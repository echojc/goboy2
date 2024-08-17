package main

import (
	"fmt"
	"io/ioutil"
	"strings"
)
import "C"

type Debugger struct {
	Z *CPU

	breakpoints map[uint16]bool
	cpuState    CPUState
	romBanks    []*C.uchar
	dasmCache   map[uint16]Dasm
}

func NewDebugger() *Debugger {
	return &Debugger{
		Z: NewCPU(),
		breakpoints: map[uint16]bool{
			0xc000: true,
		},
		dasmCache: make(map[uint16]Dasm),
	}
}

func (d *Debugger) Load(file string) error {
	bytes, err := ioutil.ReadFile(file)
	if err != nil {
		return err
	}

	var count = len(bytes) / 0x4000
	if len(bytes)%0x4000 != 0 {
		count++
	}

	d.romBanks = make([]*C.uchar, count)
	for i := range d.romBanks {
		var slice []C.uchar
		d.romBanks[i], slice = malloc[C.uchar](0x4000)
		for j := range slice {
			offset := i*0x4000 + j
			if offset >= len(bytes) {
				break
			}
			slice[j] = C.uchar(bytes[offset])
		}
	}

	d.Z.CPU.rom = d.romBanks[0]
	if len(d.romBanks) > 1 {
		d.Z.CPU.xrom = d.romBanks[1]
	}

	return nil
}

type CPUState struct {
	B, C, D, E, H, L, A          uint8
	FZ, FN, FH, FC               bool
	SP, PC                       uint16
	Cycles                       uint32
	Halted, Stopped, IntsEnabled bool
}

func (d *Debugger) CPUState() CPUState {
	z := d.Z.CPU
	if uint32(z.cycles) == d.cpuState.Cycles {
		return d.cpuState
	}

	d.cpuState = CPUState{
		B:           uint8(z.b),
		C:           uint8(z.c),
		D:           uint8(z.d),
		E:           uint8(z.e),
		H:           uint8(z.h),
		L:           uint8(z.l),
		A:           uint8(z.a),
		FZ:          z.f&(1<<7) > 0,
		FN:          z.f&(1<<6) > 0,
		FH:          z.f&(1<<5) > 0,
		FC:          z.f&(1<<4) > 0,
		SP:          uint16(z.sp),
		PC:          uint16(z.pc),
		Cycles:      uint32(z.cycles),
		Halted:      z.halted > 0,
		Stopped:     z.stopped > 0,
		IntsEnabled: z.ints_enabled > 0,
	}
	return d.cpuState
}

func (d *Debugger) StepInto() {
	d.Z.Step()
}

func (d *Debugger) StepOver() {
	if isCall(d.PCBytes()[0]) {
		next := d.NextAddr(d.PC())
		for d.PC() != next {
			err := d.Z.Step()
			if err == ErrBreak || d.breakpoints[d.PC()] {
				break
			}
		}
	} else {
		d.Z.Step()
	}
}

func (d *Debugger) Run() {
	for {
		err := d.Z.Step()
		if err == ErrBreak || d.breakpoints[d.PC()] {
			break
		}
	}
}

func (d *Debugger) PC() uint16 {
	return uint16(d.Z.CPU.pc)
}

func (d *Debugger) PCBytes() []byte {
	return d.Disassemble(d.PC()).Bytes
}

func (d *Debugger) Read(addr uint16) byte {
	return d.Z.Read(C.ushort(addr))
}

func (d *Debugger) ToggleBreakpoint(addr uint16) {
	d.breakpoints[addr] = !d.breakpoints[addr]
}

func (d *Debugger) IsBreakpoint(addr uint16) bool {
	return d.breakpoints[addr]
}

func (d *Debugger) Disassemble(addr uint16) Dasm {
	if dasm, ok := d.dasmCache[addr]; ok {
		return dasm
	}

	var bytes = make([]byte, 3)
	for j := uint16(0); j < 3; j++ {
		bytes[j] = d.Z.Read(C.ushort(addr + j))
	}

	decoded, count := decode(bytes)
	d.dasmCache[addr] = Dasm{
		Addr:    addr,
		Bytes:   bytes[:count],
		Decoded: decoded,
	}
	return d.dasmCache[addr]
}

func (d *Debugger) NextAddr(addr uint16) uint16 {
	return addr + uint16(len(d.Disassemble(addr).Bytes))
}

// PrevAddr returns the earliest valid address that, after execution, results
// in PC being set to addr. If there is no instruction that can do so, addr is
// returned.
func (d *Debugger) PrevAddr(addr uint16) uint16 {
	// check cache directly for already disassembled addresses first, because it
	// is likely these are correct addresses
	if dasm, ok := d.dasmCache[addr-3]; ok && len(dasm.Bytes) == 3 {
		return addr - 3
	} else if dasm, ok := d.dasmCache[addr-2]; ok && len(dasm.Bytes) == 2 {
		return addr - 2
	} else if dasm, ok := d.dasmCache[addr-1]; ok && len(dasm.Bytes) == 1 {
		return addr - 1
	}

	// if no previous address was previously disassembled, search for the
	// earliest valid address. DO NOT store these in the cache, because they
	// might not be valid.
	var bytes = make([]byte, 3)
	for j := uint16(0); j < 3; j++ {
		bytes[j] = d.Z.Read(C.ushort(addr + j - 3))
	}
	for i := 3; i > 0; i-- {
		_, count := decode(bytes[:i])
		if count == i {
			return addr - uint16(i)
		}
	}

	return addr
}

func (d *Debugger) InvalidateDasms(start, end uint16) {
	for addr := start; addr < end; addr++ {
		delete(d.dasmCache, addr)
	}
}

type Dasm struct {
	Addr    uint16
	Bytes   []byte
	Decoded string
}

func (d *Dasm) String() string {
	var sb strings.Builder
	fmt.Fprintf(&sb, "%04X ", d.Addr)
	for i := 0; i < 3; i++ {
		if i < len(d.Bytes) {
			fmt.Fprintf(&sb, "%02X ", d.Bytes[i])
		} else {
			fmt.Fprintf(&sb, "   ")
		}
	}
	fmt.Fprintf(&sb, "%s", d.Decoded)
	return sb.String()
}
