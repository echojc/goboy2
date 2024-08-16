package main

import (
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"reflect"
	"slices"
	"strings"
	"unsafe"

	"github.com/jroimartin/gocui"
)

// #cgo LDFLAGS: -L../build -lcgoboy
// #include "../build/libcgoboy.h"
import "C"

var breakpoints = map[C.ushort]bool{
	0xc000: true,
	//0xc2ab: true,
	//0xc2da: true,
	//0xc2fd: true,
	//0xc357: true,
}

var ErrBreakpoint = errors.New("breakpoint")

func main() {
	var rom *C.uchar
	var slice []C.uchar

	if len(os.Args) > 1 {
		data, err := ioutil.ReadFile(os.Args[1])
		if err != nil {
			fmt.Println("Could not read", os.Args[1], ":", err)
			os.Exit(1)
		}

		rom, slice = malloc[C.uchar](len(data))
		for i := range data {
			slice[i] = C.uchar(data[i])
		}
	} else {
		rom, slice = malloc[C.uchar](0x100 + 0x100)
		data := []C.uchar{
			0x06, 0xff, // ld b
			0x0e, 0x11, // ld c
			0x16, 0x04, // ld d
			0x1e, 0x10, // ld e
			0x26, 0xff, // ld h
			0x2e, 0xff, // ld l
			0x3e, 0x0a, // ld a
			0xfe, 0x00,
			0xfe, 0x01,
			0xfe, 0x02,
			0xfe, 0x03,
			0xfe, 0x04,
			0xfe, 0x05,
			0xfe, 0x06,
			0xfe, 0x07,
			0xfe, 0x08,
			0xfe, 0x09,
			0xfe, 0x0a,
			0xfe, 0x0b,
			0xfe, 0x0c,
			0xfe, 0x0d,
			0xfe, 0x0e,
			0xfe, 0x0f,
		}
		for i := range data {
			slice[i+0x100] = data[i]
		}
	}

	var z C.cpu
	z.rom = rom
	if len(slice) > 0x4000 {
		z.xrom = (*C.uchar)(unsafe.Add(unsafe.Pointer(rom), 0x4000))
	}
	C.cpu_init(&z)

	g, err := gocui.NewGui(gocui.OutputNormal)
	if err != nil {
		log.Fatal(err)
	}
	defer g.Close()

	maxX, maxY := g.Size()
	v, _ := g.SetView("cpu", 0, 0, 35, 4)
	v.Title = "cpu"
	v, _ = g.SetView("disassembly", 0, 5, 35, maxY-10)
	v.Title = "disassembly"
	v, _ = g.SetView("serial", 0, maxY-9, 35, maxY-1)
	v.Title = "serial"
	v.Wrap = true
	v.Autoscroll = true
	v, _ = g.SetView("memory", 36, 0, maxX-1, maxY-1)
	v.Title = "memory"

	g.SetKeybinding("", 'q', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		return gocui.ErrQuit
	})

	g.SetKeybinding("", 'j', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		cursor = min(DISPLAY_DASMS-1, cursor+1)
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", 'k', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		cursor = max(0, cursor-1)
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", gocui.KeyCtrlE, gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		dasmStartAddr += C.ushort(len(dasms[dasmStartAddr].bytes))
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", gocui.KeyCtrlY, gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		if d, ok := dasms[dasmStartAddr-1]; ok && len(d.bytes) == 1 {
			dasmStartAddr -= 1
		} else if d, ok := dasms[dasmStartAddr-2]; ok && len(d.bytes) == 2 {
			dasmStartAddr -= 2
		} else if d, ok := dasms[dasmStartAddr-3]; ok && len(d.bytes) == 3 {
			dasmStartAddr -= 3
		} else {
			var addr C.ushort
			for addr = dasmStartAddr - 3; addr < dasmStartAddr; addr++ {
				dasm := decodeAddr(&z, addr)
				if addr+C.ushort(len(dasm.bytes)) == dasmStartAddr {
					break
				}
			}
			dasmStartAddr = addr
		}

		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", gocui.KeyCtrlD, gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		memAddr += 0x100
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", gocui.KeyCtrlU, gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		memAddr = max(0, memAddr-0x100)
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", 'b', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		breakpoints[dasmAddrs[cursor]] = !breakpoints[dasmAddrs[cursor]]
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", 'i', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		// step
		step(&z, g)
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", 'r', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		// run
		for {
			err := step(&z, g)
			if err == ErrBreakpoint {
				cursor = 0
				break
			} else if err == gocui.ErrQuit {
				break
			}
		}
		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", 'n', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		// over
		ins := *C.ptr(&z, z.pc)
		var nextInsAddr C.ushort = 0xffff
		switch ins {
		case 0xc4, 0xcc, 0xcd, 0xd4, 0xdc: // call
			nextInsAddr = z.pc + 3
		}

		if nextInsAddr != 0xffff {
			for z.pc != nextInsAddr {
				err := step(&z, g)
				if err == ErrBreakpoint {
					cursor = 0
					break
				} else if err == gocui.ErrQuit {
					break
				}
			}
		} else {
			step(&z, g)
		}

		debug2(&z, g)
		return nil
	})

	g.SetKeybinding("", '0', gocui.ModNone, func(g *gocui.Gui, v *gocui.View) error {
		// until ret
		for {
			err := step(&z, g)
			if err == ErrBreakpoint {
				cursor = 0
				break
			} else if err == gocui.ErrQuit {
				break
			}

			ins := *C.ptr(&z, z.pc)
			if ins == 0xc0 || ins == 0xc8 || ins == 0xc9 ||
				ins == 0xd0 || ins == 0xd8 || ins == 0xd9 { //ret
				break
			}
		}

		debug2(&z, g)
		return nil
	})

	debug2(&z, g)
	if err := g.MainLoop(); err != nil && err != gocui.ErrQuit {
		log.Fatal(err)
	}
	return

	//debug(&z)
	////
	//// stdout := ""
	////
	//for {
	//	if z.pc == 0xc266 {
	//		break
	//	}
	//	ins := *C.ptr(&z, z.pc)
	//	arg1 := *C.ptr(&z, z.pc+1)
	//	//arg2 := *C.ptr(&z, z.pc+2)

	//	if ins == 0xe0 && arg1 == 0x01 { // ldh SB, a
	//		//	for !strings.HasSuffix(stdout, "06-ld r,r\n\n") {
	//		//stdout += string(z.a)
	//		fmt.Printf("%c", z.a)

	//		//		C.cpu_step(&z)
	//		//		ins = *C.ptr(&z, z.pc)
	//		//		arg = *C.ptr(&z, z.pc+1)
	//		//		for ins != 0xe0 || arg != 0x01 {
	//		//			C.cpu_step(&z)
	//		//			ins = *C.ptr(&z, z.pc)
	//		//			arg = *C.ptr(&z, z.pc+1)
	//		//		}
	//		//	}
	//		//	break
	//	} else if ins == 0x18 && arg1 == 0xfe { // jr -2
	//		//debug(&z)
	//		return
	//	}
	//	C.cpu_step(&z)
	//}
	////
	//// //for z.pc != 0xcc50 {
	//// //	C.cpu_step(&z)
	//// //}
	////
	//r := bufio.NewReader(os.Stdin)
	//// //for {
	//// //	for z.pc != 0xc0cd {
	//// //		C.cpu_step(&z)
	//// //	}
	//// //	debug(&z)
	//// //	r.ReadBytes('\n')
	//// //	C.cpu_step(&z)
	//// //	debug(&z)
	//// //	s, _ := r.ReadString('\n')
	//// //	if strings.HasPrefix(s, "d") {
	//// //		break
	//// //	}
	//// //}
	////
	//for {
	//	debug(&z)
	//	r.ReadBytes('\n')
	//	C.cpu_step(&z)
	//}
}

func step(z *C.cpu, g *gocui.Gui) error {
	C.cpu_step(z)

	if breakpoints[z.pc] {
		return ErrBreakpoint
	}

	ins := *C.ptr(z, z.pc)
	arg1 := *C.ptr(z, z.pc+1)
	//arg2 := *C.ptr(z, z.pc+2)

	// intercept serial
	switch ins {
	case 0xe0:
		if arg1 == 0x01 { // ldh SB, a
			v, _ := g.View("serial")
			fmt.Fprintf(v, "%c", z.a)
		}
	case 0x18:
		if arg1 == 0xfe { // jr -2
			return gocui.ErrQuit
		}
	}

	return nil
}

type dasm struct {
	addr    C.ushort
	bytes   []C.uchar
	decoded string
}

func (d *dasm) String() string {
	var sb strings.Builder
	fmt.Fprintf(&sb, "%04X ", d.addr)
	for _, b := range d.bytes {
		fmt.Fprintf(&sb, "%02X ", b)
	}
	for i := 0; i < 3-len(d.bytes); i++ {
		fmt.Fprintf(&sb, "   ")
	}
	fmt.Fprintf(&sb, "%s", d.decoded)
	return sb.String()
}

const DISPLAY_DASMS = 0x20

var dasms = map[C.ushort]dasm{}
var dasmStartAddr C.ushort = 0x100
var dasmAddrs = make([]C.ushort, DISPLAY_DASMS)
var cursor int
var memAddr C.ushort = 0xff00

func decodeAddr(z *C.cpu, addr C.ushort) dasm {
	if d, ok := dasms[addr]; ok {
		return d
	}

	var next = make([]C.uchar, 3)
	for j := 0; j < 3; j++ {
		next[j] = *C.ptr(z, addr+C.ushort(j))
	}
	decoded, count := decode(next)

	dasms[addr] = dasm{
		addr:    addr,
		bytes:   next[:count],
		decoded: decoded,
	}
	return dasms[addr]
}

func debug2(z *C.cpu, g *gocui.Gui) {
	if !slices.Contains(dasmAddrs, z.pc) {
		dasmStartAddr = z.pc
	}

	v, _ := g.View("cpu")
	v.Clear()

	var intsEnabled = 'D'
	if z.ints_enabled > 0 {
		intsEnabled = 'E'
	}

	fmt.Fprintf(v,
		" b %02X c %02X d %02X e %02X\n"+
			" h %02X l %02X a %02X f %c%c%c%c\n"+
			" sp %04X pc %04X #%d %cI\n\n",
		z.b, z.c, z.d, z.e,
		z.h, z.l, z.a,
		'z'-((z.f>>7&1)*0x20),
		'n'-((z.f>>6&1)*0x20),
		'h'-((z.f>>5&1)*0x20),
		'c'-((z.f>>4&1)*0x20),
		z.sp, z.pc, z.cycles,
		intsEnabled,
	)

	v, _ = g.View("disassembly")
	v.Clear()

	// 32 instructions ahead
	var addr = dasmStartAddr
	for i := 0; i < DISPLAY_DASMS; i++ {
		dasm := decodeAddr(z, addr)
		dasmAddrs[i] = addr

		var c byte = ' '
		if addr == z.pc {
			c = '>'
		}

		if i == cursor {
			fmt.Fprintf(v, "\x1b[37;44m%c %s                 \n\x1b[0m", c, dasm.String())
		} else if addr == z.pc {
			fmt.Fprintf(v, "\x1b[37;42m%c %s                 \n\x1b[0m", c, dasm.String())
		} else if breakpoints[addr] {
			fmt.Fprintf(v, "\x1b[37;41m%c %s                 \n\x1b[0m", c, dasm.String())
		} else {
			fmt.Fprintf(v, "%c %s                           \n", c, dasm.String())
		}

		addr += C.ushort(len(dasm.bytes))
	}

	v, _ = g.View("memory")
	v.Clear()
	_, maxY := v.Size()

	maxAddr := memAddr + C.ushort(maxY*0x10)
	if maxAddr < memAddr {
		maxAddr = 0xffff
	}
	for y := memAddr; y < maxAddr; y += 0x10 {
		fmt.Fprintf(v, "%04X ", y)
		for j := 0; j < 0x10; j++ {
			addr := y + C.ushort(j)
			if addr == z.pc {
				fmt.Fprintf(v, "\x1b[37;42m%02X\x1b[0m ", *C.ptr(z, addr))
			} else {
				fmt.Fprintf(v, "%02X ", *C.ptr(z, addr))
			}
		}
		for j := 0; j < 0x10; j++ {
			addr := y + C.ushort(j)
			c := *C.ptr(z, addr)
			if c < 0x20 || c >= 0x80 {
				c = '.'
			}
			fmt.Fprintf(v, "%c", c)
		}
		fmt.Fprintf(v, "\n")

		if y == 0xfff0 {
			break
		}
	}
}

func debug(z *C.cpu) {
	var stack []C.uchar = make([]C.uchar, 0x100-(z.sp%0x100))
	for i := range stack {
		stack[i] = *C.ptr(z, C.ushort(int(z.sp)+len(stack)-i-1))
	}

	//var hl []C.uchar = make([]C.uchar, 0x10)
	//for i := range hl {
	//	hl[i] = *C.ptr(z, (C.ushort(z.h)<<8)+C.ushort(z.l)+C.ushort(i))
	//}

	var next [0x10]C.uchar
	for i := 0; i < 0x10; i++ {
		next[i] = *C.ptr(z, z.pc+C.ushort(i))
	}
	var decoded [5]string
	n := 0
	for i := 0; i < 5; i++ {
		var count int
		decoded[i], count = decode(next[n:])
		n += count
	}

	fmt.Printf(
		"b %02X c %02X d %02X e %02X\n"+
			"h %02X l %02X a %02X f %c%c%c%c\n"+
			"sp %04X pc %04X # %d\n"+
			"stack % X\n"+
			//"hl % X\n"+
			"> %s\n  %s\n  %s\n  %s\n  %s\n\n",
		z.b, z.c, z.d, z.e,
		z.h, z.l, z.a,
		'z'-((z.f>>7&1)*0x20),
		'n'-((z.f>>6&1)*0x20),
		'h'-((z.f>>5&1)*0x20),
		'c'-((z.f>>4&1)*0x20),
		z.sp, z.pc, z.cycles,
		stack,
		//hl,
		decoded[0], decoded[1], decoded[2], decoded[3], decoded[4],
	)
}

func malloc[T any](size int) (*T, []T) {
	array := (*T)(C.malloc(C.ulong(size)))
	return array, arrayToSlice(array, size)
}

func arrayToSlice[T any](arr *T, size int) []T {
	var slice []T

	header := (*reflect.SliceHeader)((unsafe.Pointer(&slice)))
	header.Cap = size
	header.Len = size
	header.Data = uintptr(unsafe.Pointer(arr))

	return slice
}

func decode(ins []C.uchar) (string, int) {
	var count int = 1
	var str string

	switch ins[0] {
	case 0x00:
		str = "nop"
	case 0x01:
		count = 3
		str = fmt.Sprintf("ld bc, $%02x%02x", ins[2], ins[1])
	case 0x02:
		str = "ld (bc), a"
	case 0x03:
		str = "inc bc"
	case 0x04:
		str = "inc b"
	case 0x05:
		str = "dec b"
	case 0x06:
		count = 2
		str = fmt.Sprintf("ld b, $%02x", ins[1])
	case 0x07:
		str = "rlca"
	case 0x08:
		count = 3
		str = fmt.Sprintf("ld ($%02x%02x), sp", ins[2], ins[1])
	case 0x09:
		str = "add hl, bc"
	case 0x0a:
		str = "ld a, (bc)"
	case 0x0b:
		str = "dec bc"
	case 0x0c:
		str = "inc c"
	case 0x0d:
		str = "dec c"
	case 0x0e:
		count = 2
		str = fmt.Sprintf("ld c, $%02x", ins[1])
	case 0x0f:
		str = "rrca"
	case 0x10:
		str = "stop"
	case 0x11:
		count = 3
		str = fmt.Sprintf("ld de, $%02x%02x", ins[2], ins[1])
	case 0x12:
		str = "ld (de), a"
	case 0x13:
		str = "inc de"
	case 0x14:
		str = "inc d"
	case 0x15:
		str = "dec d"
	case 0x16:
		count = 2
		str = fmt.Sprintf("ld d, $%02x", ins[1])
	case 0x17:
		str = "rla"
	case 0x18:
		count = 2
		str = fmt.Sprintf("jr $%02x", ins[1])
	case 0x19:
		str = "add hl, de"
	case 0x1a:
		str = "ld a, (de)"
	case 0x1b:
		str = "dec de"
	case 0x1c:
		str = "inc e"
	case 0x1d:
		str = "dec e"
	case 0x1e:
		count = 2
		str = fmt.Sprintf("ld e, $%02x", ins[1])
	case 0x1f:
		str = "rra"
	case 0x20:
		count = 2
		str = fmt.Sprintf("jr nz, $%02x", ins[1])
	case 0x21:
		count = 3
		str = fmt.Sprintf("ld hl, $%02x%02x", ins[2], ins[1])
	case 0x22:
		str = "ldi (hl), a"
	case 0x23:
		str = "inc hl"
	case 0x24:
		str = "inc h"
	case 0x25:
		str = "dec h"
	case 0x26:
		count = 2
		str = fmt.Sprintf("ld h, $%02x", ins[1])
	case 0x27:
		str = "daa"
	case 0x28:
		count = 2
		str = fmt.Sprintf("jr z, $%02x", ins[1])
	case 0x29:
		str = "add hl, hl"
	case 0x2a:
		str = "ldi a, (hl)"
	case 0x2b:
		str = "dec hl"
	case 0x2c:
		str = "inc l"
	case 0x2d:
		str = "dec l"
	case 0x2e:
		count = 2
		str = fmt.Sprintf("ld l, $%02x", ins[1])
	case 0x2f:
		str = "cpl"
	case 0x30:
		count = 2
		str = fmt.Sprintf("jr nc, $%02x", ins[1])
	case 0x31:
		count = 3
		str = fmt.Sprintf("ld sp, $%02x%02x", ins[2], ins[1])
	case 0x32:
		str = "ldd (hl), a"
	case 0x33:
		str = "inc sp"
	case 0x34:
		str = "inc (hl)"
	case 0x35:
		str = "dec (hl)"
	case 0x36:
		count = 2
		str = fmt.Sprintf("ld (hl), $%02x", ins[1])
	case 0x37:
		str = "scf"
	case 0x38:
		count = 2
		str = fmt.Sprintf("jr c, $%02x", ins[1])
	case 0x39:
		str = "add hl, sp"
	case 0x3a:
		str = "ldd a, (hl)"
	case 0x3b:
		str = "dec sp"
	case 0x3c:
		str = "inc a"
	case 0x3d:
		str = "dec a"
	case 0x3e:
		count = 2
		str = fmt.Sprintf("ld a, $%02x", ins[1])
	case 0x3f:
		str = "ccf"
	case 0x40:
		str = "ld b, b"
	case 0x41:
		str = "ld b, c"
	case 0x42:
		str = "ld b, d"
	case 0x43:
		str = "ld b, e"
	case 0x44:
		str = "ld b, h"
	case 0x45:
		str = "ld b, l"
	case 0x46:
		str = "ld b, (hl)"
	case 0x47:
		str = "ld b, a"
	case 0x48:
		str = "ld c, b"
	case 0x49:
		str = "ld c, c"
	case 0x4a:
		str = "ld c, d"
	case 0x4b:
		str = "ld c, e"
	case 0x4c:
		str = "ld c, h"
	case 0x4d:
		str = "ld c, l"
	case 0x4e:
		str = "ld c, (hl)"
	case 0x4f:
		str = "ld c, a"
	case 0x50:
		str = "ld d, b"
	case 0x51:
		str = "ld d, c"
	case 0x52:
		str = "ld d, d"
	case 0x53:
		str = "ld d, e"
	case 0x54:
		str = "ld d, h"
	case 0x55:
		str = "ld d, l"
	case 0x56:
		str = "ld d, (hl)"
	case 0x57:
		str = "ld d, a"
	case 0x58:
		str = "ld e, b"
	case 0x59:
		str = "ld e, c"
	case 0x5a:
		str = "ld e, d"
	case 0x5b:
		str = "ld e, e"
	case 0x5c:
		str = "ld e, h"
	case 0x5d:
		str = "ld e, l"
	case 0x5e:
		str = "ld e, (hl)"
	case 0x5f:
		str = "ld e, a"
	case 0x60:
		str = "ld h, b"
	case 0x61:
		str = "ld h, c"
	case 0x62:
		str = "ld h, d"
	case 0x63:
		str = "ld h, e"
	case 0x64:
		str = "ld h, h"
	case 0x65:
		str = "ld h, l"
	case 0x66:
		str = "ld h, (hl)"
	case 0x67:
		str = "ld h, a"
	case 0x68:
		str = "ld l, b"
	case 0x69:
		str = "ld l, c"
	case 0x6a:
		str = "ld l, d"
	case 0x6b:
		str = "ld l, e"
	case 0x6c:
		str = "ld l, h"
	case 0x6d:
		str = "ld l, l"
	case 0x6e:
		str = "ld l, (hl)"
	case 0x6f:
		str = "ld l, a"
	case 0x70:
		str = "ld (hl), b"
	case 0x71:
		str = "ld (hl), c"
	case 0x72:
		str = "ld (hl), d"
	case 0x73:
		str = "ld (hl), e"
	case 0x74:
		str = "ld (hl), h"
	case 0x75:
		str = "ld (hl), l"
	case 0x76:
		str = "halt"
	case 0x77:
		str = "ld (hl), a"
	case 0x78:
		str = "ld a, b"
	case 0x79:
		str = "ld a, c"
	case 0x7a:
		str = "ld a, d"
	case 0x7b:
		str = "ld a, e"
	case 0x7c:
		str = "ld a, h"
	case 0x7d:
		str = "ld a, l"
	case 0x7e:
		str = "ld a, (hl)"
	case 0x7f:
		str = "ld a, a"
	case 0x80:
		str = "add b"
	case 0x81:
		str = "add c"
	case 0x82:
		str = "add d"
	case 0x83:
		str = "add e"
	case 0x84:
		str = "add h"
	case 0x85:
		str = "add l"
	case 0x86:
		str = "add (hl)"
	case 0x87:
		str = "add a"
	case 0x88:
		str = "adc b"
	case 0x89:
		str = "adc c"
	case 0x8a:
		str = "adc d"
	case 0x8b:
		str = "adc e"
	case 0x8c:
		str = "adc h"
	case 0x8d:
		str = "adc l"
	case 0x8e:
		str = "adc (hl)"
	case 0x8f:
		str = "adc a"
	case 0x90:
		str = "sub b"
	case 0x91:
		str = "sub c"
	case 0x92:
		str = "sub d"
	case 0x93:
		str = "sub e"
	case 0x94:
		str = "sub h"
	case 0x95:
		str = "sub l"
	case 0x96:
		str = "sub (hl)"
	case 0x97:
		str = "sub a"
	case 0x98:
		str = "sbc b"
	case 0x99:
		str = "sbc c"
	case 0x9a:
		str = "sbc d"
	case 0x9b:
		str = "sbc e"
	case 0x9c:
		str = "sbc h"
	case 0x9d:
		str = "sbc l"
	case 0x9e:
		str = "sbc (hl)"
	case 0x9f:
		str = "sbc a"
	case 0xa0:
		str = "and b"
	case 0xa1:
		str = "and c"
	case 0xa2:
		str = "and d"
	case 0xa3:
		str = "and e"
	case 0xa4:
		str = "and h"
	case 0xa5:
		str = "and l"
	case 0xa6:
		str = "and (hl)"
	case 0xa7:
		str = "and a"
	case 0xa8:
		str = "xor b"
	case 0xa9:
		str = "xor c"
	case 0xaa:
		str = "xor d"
	case 0xab:
		str = "xor e"
	case 0xac:
		str = "xor h"
	case 0xad:
		str = "xor l"
	case 0xae:
		str = "xor (hl)"
	case 0xaf:
		str = "xor a"
	case 0xb0:
		str = "or b"
	case 0xb1:
		str = "or c"
	case 0xb2:
		str = "or d"
	case 0xb3:
		str = "or e"
	case 0xb4:
		str = "or h"
	case 0xb5:
		str = "or l"
	case 0xb6:
		str = "or (hl)"
	case 0xb7:
		str = "or a"
	case 0xb8:
		str = "cp b"
	case 0xb9:
		str = "cp c"
	case 0xba:
		str = "cp d"
	case 0xbb:
		str = "cp e"
	case 0xbc:
		str = "cp h"
	case 0xbd:
		str = "cp l"
	case 0xbe:
		str = "cp (hl)"
	case 0xbf:
		str = "cp a"
	case 0xc0:
		str = "ret nz"
	case 0xc1:
		str = "pop bc"
	case 0xc2:
		count = 3
		str = fmt.Sprintf("jp nz, $%02x%02x", ins[2], ins[1])
	case 0xc3:
		count = 3
		str = fmt.Sprintf("jp $%02x%02x", ins[2], ins[1])
	case 0xc4:
		count = 3
		str = fmt.Sprintf("call nz, $%02x%02x", ins[2], ins[1])
	case 0xc5:
		str = "push bc"
	case 0xc6:
		count = 2
		str = fmt.Sprintf("add $%02x", ins[1])
	case 0xc7:
		str = "rst $00"
	case 0xc8:
		str = "ret z"
	case 0xc9:
		str = "ret"
	case 0xca:
		count = 3
		str = fmt.Sprintf("jp z, $%02x%02x", ins[2], ins[1])
	case 0xcb:
		count = 2
		str = fmt.Sprintf("cb %02x", ins[1])
	case 0xcc:
		count = 3
		str = fmt.Sprintf("call z, $%02x%02x", ins[2], ins[1])
	case 0xcd:
		count = 3
		str = fmt.Sprintf("call $%02x%02x", ins[2], ins[1])
	case 0xce:
		count = 2
		str = fmt.Sprintf("adc $%02x", ins[1])
	case 0xcf:
		str = "rst $08"
	case 0xd0:
		str = "ret nc"
	case 0xd1:
		str = "pop de"
	case 0xd2:
		count = 3
		str = fmt.Sprintf("jp nc, $%02x%02x", ins[2], ins[1])
	case 0xd3:
		str = "xx"
	case 0xd4:
		count = 3
		str = fmt.Sprintf("call nc, $%02x%02x", ins[2], ins[1])
	case 0xd5:
		str = "push de"
	case 0xd6:
		count = 2
		str = fmt.Sprintf("sub $%02x", ins[1])
	case 0xd7:
		str = "rst $10"
	case 0xd8:
		str = "ret c"
	case 0xd9:
		str = "reti"
	case 0xda:
		count = 3
		str = fmt.Sprintf("jp c, $%02x%02x", ins[2], ins[1])
	case 0xdb:
		str = "xx"
	case 0xdc:
		count = 3
		str = fmt.Sprintf("call c, $%02x%02x", ins[2], ins[1])
	case 0xdd:
		str = "xx"
	case 0xde:
		count = 2
		str = fmt.Sprintf("sbc $%02x", ins[1])
	case 0xdf:
		str = "rst $18"
	case 0xe0:
		count = 2
		str = fmt.Sprintf("ldh ($ff%02x), a", ins[1])
	case 0xe1:
		str = "pop hl"
	case 0xe2:
		str = "ldh (c), a"
	case 0xe3:
		str = "xx"
	case 0xe4:
		str = "xx"
	case 0xe5:
		str = "push hl"
	case 0xe6:
		count = 2
		str = fmt.Sprintf("and $%02x", ins[1])
	case 0xe7:
		str = "rst $20"
	case 0xe8:
		count = 2
		str = fmt.Sprintf("add sp, $%02x", ins[1])
	case 0xe9:
		str = "jp hl"
	case 0xea:
		count = 3
		str = fmt.Sprintf("ld ($%02x%02x), a", ins[2], ins[1])
	case 0xeb:
		str = "xx"
	case 0xec:
		str = "xx"
	case 0xed:
		str = "xx"
	case 0xee:
		count = 2
		str = fmt.Sprintf("xor $%02x", ins[1])
	case 0xef:
		str = "rst $28"
	case 0xf0:
		count = 2
		str = fmt.Sprintf("ldh a, ($ff%02x)", ins[1])
	case 0xf1:
		str = "pop af"
	case 0xf2:
		str = "ldh a, (c)"
	case 0xf3:
		str = "di"
	case 0xf4:
		str = "xx"
	case 0xf5:
		str = "push af"
	case 0xf6:
		count = 2
		str = fmt.Sprintf("or $%02x", ins[1])
	case 0xf7:
		str = "rst $30"
	case 0xf8:
		count = 2
		str = fmt.Sprintf("ldhl sp, $%02x", ins[1])
	case 0xf9:
		str = "ld sp, hl"
	case 0xfa:
		count = 3
		str = fmt.Sprintf("ld a, ($%02x%02x)", ins[2], ins[1])
	case 0xfb:
		str = "ei"
	case 0xfc:
		str = "xx"
	case 0xfd:
		str = "xx"
	case 0xfe:
		count = 2
		str = fmt.Sprintf("cp $%02x", ins[1])
	case 0xff:
		str = "rst $38"
	}

	return str, count
}
