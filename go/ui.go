package main

import (
	"fmt"
	"log"
	"slices"
	"strings"

	"github.com/jroimartin/gocui"
)

type CLI struct {
	Debugger *Debugger

	dasmStartAddr uint16
	dasmAddrs     []uint16
	dasmCursor    int
	memStartAddr  uint16

	g *gocui.Gui
}

const (
	ViewCPU         = "cpu"
	ViewDisassembly = "disassembly"
	ViewSerial      = "serial"
	ViewMemory      = "memory"
)

func NewCLI(d *Debugger) (*CLI, error) {
	g, err := gocui.NewGui(gocui.OutputNormal)
	if err != nil {
		return nil, err
	}

	cli := &CLI{
		Debugger: d,

		dasmStartAddr: 0x0100,
		memStartAddr:  0xc000,

		g: g,
	}

	g.SetManager(
		gocui.ManagerFunc(cli.RenderCPU),
		gocui.ManagerFunc(cli.RenderDisassembly),
		gocui.ManagerFunc(cli.RenderSerial),
		gocui.ManagerFunc(cli.RenderMemory),
	)
	go cli.readSerial()

	cli.bind('q', func() { cli.g.Update(func(_ *gocui.Gui) error { return gocui.ErrQuit }) })
	cli.bind('j', func() { cli.dasmCursor++ })
	cli.bind('k', func() { cli.dasmCursor-- })
	cli.bind(gocui.KeyCtrlE, func() { cli.dasmStartAddr = cli.Debugger.NextAddr(cli.dasmStartAddr) })
	cli.bind(gocui.KeyCtrlY, func() { cli.dasmStartAddr = cli.Debugger.PrevAddr(cli.dasmStartAddr) })
	cli.bind(gocui.KeyCtrlD, func() { cli.memStartAddr += 0x100 })
	cli.bind(gocui.KeyCtrlU, func() { cli.memStartAddr -= 0x100 })
	cli.bind('b', func() { cli.Debugger.ToggleBreakpoint(cli.dasmAddrs[cli.dasmCursor]) })
	cli.bind('i', func() { cli.Debugger.StepInto(); cli.JumpToDasm() })
	cli.bind('n', func() { cli.Debugger.StepOver(); cli.JumpToDasm() })
	cli.bind('r', func() { cli.Debugger.Run(); cli.JumpToDasm() })

	return cli, nil
}

func (cli *CLI) RenderCPU(g *gocui.Gui) error {
	v, err := g.View(ViewCPU)
	if err != nil {
		if err != gocui.ErrUnknownView {
			return err
		}
		v, _ = g.SetView(ViewCPU, 0, 0, 35, 4)
		v.Title = ViewCPU
	}
	v.Clear()

	z := cli.Debugger.CPUState()

	fmt.Fprintf(v, " b %02X c %02X d %02X e %02X\n"+
		" h %02X l %02X a %02X f %c%c%c%c\n"+
		" sp %04X pc %04X %c %cI #%d\n\n",
		z.B, z.C, z.D, z.E,
		z.H, z.L, z.A,
		tern(z.FZ, 'Z', 'z'),
		tern(z.FN, 'N', 'n'),
		tern(z.FH, 'H', 'h'),
		tern(z.FC, 'C', 'c'),
		z.SP, z.PC,
		tern(z.Stopped, 'S', tern(z.Halted, 'H', 'R')),
		tern(z.IrqEnabled, 'E', 'D'),
		z.Cycles,
	)

	return nil
}

func (cli *CLI) JumpToDasm() {
	pc := cli.Debugger.PC()
	if !slices.Contains(cli.dasmAddrs, pc) {
		cli.dasmStartAddr = pc
	}
}

func (cli *CLI) RenderDisassembly(g *gocui.Gui) error {
	v, err := g.View(ViewDisassembly)
	if err != nil {
		if err != gocui.ErrUnknownView {
			return err
		}
		_, maxY := g.Size()
		v, _ = g.SetView(ViewDisassembly, 0, 5, 35, maxY-10)
		v.Title = ViewDisassembly
	}
	v.Clear()
	_, maxY := v.Size()

	pc := cli.Debugger.PC()

	// check bounds
	cli.dasmCursor = max(0, min(maxY, cli.dasmCursor))
	if len(cli.dasmAddrs) != maxY {
		cli.dasmAddrs = make([]uint16, maxY)
	}

	addr := cli.dasmStartAddr
	for i := 0; i < maxY; i++ {
		dasm := cli.Debugger.Disassemble(addr)
		cli.dasmAddrs[i] = addr

		var c byte = tern[byte](addr == pc, '>', ' ')

		color := ""
		if i == cli.dasmCursor {
			color = "\x1b[37;44m"
		} else if addr == pc {
			color = "\x1b[37;42m"
		} else if cli.Debugger.IsBreakpoint(addr) {
			color = "\x1b[37;41m"
		}

		fmt.Fprintf(v, "%s%c %s                     \n\x1b[0m", color, c, dasm.String())

		addr += uint16(len(dasm.Bytes))
	}

	return nil
}

func (cli *CLI) RenderSerial(g *gocui.Gui) error {
	v, err := g.View(ViewSerial)
	if err != nil {
		if err != gocui.ErrUnknownView {
			return err
		}
		_, maxY := g.Size()
		v, _ = g.SetView(ViewSerial, 0, maxY-9, 35, maxY-1)
		v.Title = ViewSerial
		v.Wrap = true
		v.Autoscroll = true
	}
	return nil
}

func (cli *CLI) readSerial() {
	for {
		b := <-cli.Debugger.Z.Serial
		cli.g.Update(func(g *gocui.Gui) error {
			v, err := g.View(ViewSerial)
			if err != nil {
				log.Fatal(err)
			}
			fmt.Fprintf(v, "%c", b)
			return nil
		})
	}
}

func (cli *CLI) RenderMemory(g *gocui.Gui) error {
	v, err := g.View(ViewMemory)
	if err != nil {
		if err != gocui.ErrUnknownView {
			return err
		}
		maxX, maxY := g.Size()
		v, _ = g.SetView(ViewMemory, 36, 0, maxX-1, maxY-1)
		v.Title = ViewMemory
	}
	v.Clear()
	_, maxY := v.Size()

	cli.memStartAddr &= 0xfff0
	maxAddr := cli.memStartAddr + uint16(maxY*0x10)
	if maxAddr < cli.memStartAddr { // wrapped
		maxAddr = 0xffff
	}

	for y := cli.memStartAddr; y < maxAddr; y += 0x10 {
		fmt.Fprintf(v, "%04X ", y)

		var sb strings.Builder
		for j := uint16(0); j < 0x10; j++ {
			addr := y + j
			b := cli.Debugger.Read(addr)

			if addr == cli.Debugger.PC() {
				fmt.Fprintf(v, "\x1b[37;42m%02X\x1b[0m ", b)
			} else {
				fmt.Fprintf(v, "%02X ", b)
			}

			if b >= 0x20 && b < 0x80 {
				fmt.Fprintf(&sb, "%c", b)
			} else {
				fmt.Fprintf(&sb, ".")
			}
		}
		fmt.Fprintf(v, "%s\n", sb.String())

		if y == 0xfff0 { // wrapping
			break
		}
	}

	return nil
}

func (cli *CLI) bind(key any, handler func()) {
	cli.g.SetKeybinding("", key, gocui.ModNone, func(_ *gocui.Gui, _ *gocui.View) error {
		handler()
		return nil
	})
}

func (cli *CLI) MainLoop() error {
	return cli.g.MainLoop()
}

func (cli *CLI) Close() {
	cli.g.Close()
}
