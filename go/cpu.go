package main

// #cgo LDFLAGS: -L../build -lcgoboy
// #include "../build/libcgoboy.h"
import "C"
import (
	"errors"
)

var (
	ErrBreak = errors.New("break")
)

type CPU struct {
	CPU    C.cpu
	Serial chan byte
}

func NewCPU() *CPU {
	var z CPU
	C.cpu_init(&z.CPU)
	z.Serial = make(chan byte)
	return &z
}

// check breakpoints immediately after step
func (z *CPU) Step() error {
	C.cpu_step(&z.CPU)

	ins := z.Read(z.CPU.pc)
	arg1 := z.Read(z.CPU.pc + 1)
	arg2 := z.Read(z.CPU.pc + 2)

	switch ins {
	case 0xe0: // intercept serial data
		// TODO should be intercepting b7 on SC
		if arg1 == 0x01 { // ldh SB, a
			z.Serial <- byte(z.CPU.a)
		}
	case 0x18: // intercept while true loop
		if arg1 == 0xfe { // jr -2
			return ErrBreak
		}
	case 0xc3: // intercept while true loop
		addr := C.ushort(arg1) + (C.ushort(arg2) << 8)
		if addr == z.CPU.pc { // label: jp label
			return ErrBreak
		}
	}

	return nil
}

func (z *CPU) Read(addr C.ushort) byte {
	return byte(C.cpu_read(&z.CPU, addr))
}

type tileMode C.ushort

const (
	TileMode8000 = 0x8000
	TileMode9000 = 0x9000
)

func (z *CPU) TileData(baseAddr tileMode, tileID C.uchar, y C.uchar) uint16 {
	return uint16(C.tile_data(&z.CPU, C.ushort(baseAddr), tileID, y))
}
