package main

import (
	"fmt"
	"log"
	"slices"
	"strings"

	"github.com/jroimartin/gocui"
)

type UI struct {
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

func NewUI(d *Debugger) (*UI, error) {
	g, err := gocui.NewGui(gocui.OutputNormal)
	if err != nil {
		return nil, err
	}

	ui := &UI{
		Debugger: d,

		dasmStartAddr: 0x0100,
		memStartAddr:  0xc000,

		g: g,
	}

	g.SetManager(
		gocui.ManagerFunc(ui.RenderCPU),
		gocui.ManagerFunc(ui.RenderDisassembly),
		gocui.ManagerFunc(ui.RenderSerial),
		gocui.ManagerFunc(ui.RenderMemory),
	)
	go ui.readSerial()

	ui.bind('q', func() { ui.g.Update(func(_ *gocui.Gui) error { return gocui.ErrQuit }) })
	ui.bind('j', func() { ui.dasmCursor++ })
	ui.bind('k', func() { ui.dasmCursor-- })
	ui.bind(gocui.KeyCtrlE, func() { ui.dasmStartAddr = ui.Debugger.NextAddr(ui.dasmStartAddr) })
	ui.bind(gocui.KeyCtrlY, func() { ui.dasmStartAddr = ui.Debugger.PrevAddr(ui.dasmStartAddr) })
	ui.bind(gocui.KeyCtrlD, func() { ui.memStartAddr += 0x100 })
	ui.bind(gocui.KeyCtrlU, func() { ui.memStartAddr -= 0x100 })
	ui.bind('b', func() { ui.Debugger.ToggleBreakpoint(ui.dasmAddrs[ui.dasmCursor]) })
	ui.bind('i', func() { ui.Debugger.StepInto(); ui.JumpToDasm() })
	ui.bind('n', func() { ui.Debugger.StepOver(); ui.JumpToDasm() })
	ui.bind('r', func() { ui.Debugger.Run(); ui.JumpToDasm() })

	return ui, nil
}

func (ui *UI) RenderCPU(g *gocui.Gui) error {
	v, err := g.View(ViewCPU)
	if err != nil {
		if err != gocui.ErrUnknownView {
			return err
		}
		v, _ = g.SetView(ViewCPU, 0, 0, 35, 4)
		v.Title = ViewCPU
	}
	v.Clear()

	z := ui.Debugger.CPUState()

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
		tern(z.IntsEnabled, 'E', 'D'),
		z.Cycles,
	)

	return nil
}

func (ui *UI) JumpToDasm() {
	pc := ui.Debugger.PC()
	if !slices.Contains(ui.dasmAddrs, pc) {
		ui.dasmStartAddr = pc
		ui.dasmCursor = 0
	}
}

func (ui *UI) RenderDisassembly(g *gocui.Gui) error {
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

	pc := ui.Debugger.PC()

	// check bounds
	ui.dasmCursor = max(0, min(maxY, ui.dasmCursor))
	if len(ui.dasmAddrs) != maxY {
		ui.dasmAddrs = make([]uint16, maxY)
	}

	addr := ui.dasmStartAddr
	for i := 0; i < maxY; i++ {
		dasm := ui.Debugger.Disassemble(addr)
		ui.dasmAddrs[i] = addr

		var c byte = tern[byte](addr == pc, '>', ' ')

		color := ""
		if i == ui.dasmCursor {
			color = "\x1b[37;44m"
		} else if addr == pc {
			color = "\x1b[37;42m"
		} else if ui.Debugger.IsBreakpoint(addr) {
			color = "\x1b[37;41m"
		}

		fmt.Fprintf(v, "%s%c %s                     \n\x1b[0m", color, c, dasm.String())

		addr += uint16(len(dasm.Bytes))
	}

	return nil
}

func (ui *UI) RenderSerial(g *gocui.Gui) error {
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

func (ui *UI) readSerial() {
	for {
		b := <-ui.Debugger.Z.Serial
		ui.g.Update(func(g *gocui.Gui) error {
			v, err := g.View(ViewSerial)
			if err != nil {
				log.Fatal(err)
			}
			fmt.Fprintf(v, "%c", b)
			return nil
		})
	}
}

func (ui *UI) RenderMemory(g *gocui.Gui) error {
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

	ui.memStartAddr &= 0xfff0
	maxAddr := ui.memStartAddr + uint16(maxY*0x10)
	if maxAddr < ui.memStartAddr { // wrapped
		maxAddr = 0xffff
	}

	for y := ui.memStartAddr; y < maxAddr; y += 0x10 {
		fmt.Fprintf(v, "%04X ", y)

		var sb strings.Builder
		for j := uint16(0); j < 0x10; j++ {
			addr := y + j
			b := ui.Debugger.Read(addr)

			if addr == ui.Debugger.PC() {
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

func (ui *UI) bind(key any, handler func()) {
	ui.g.SetKeybinding("", key, gocui.ModNone, func(_ *gocui.Gui, _ *gocui.View) error {
		handler()
		return nil
	})
}

func (ui *UI) MainLoop() error {
	return ui.g.MainLoop()
}

func (ui *UI) Close() {
	ui.g.Close()
}
