package main

import (
	"log"
	"os"

	"github.com/jroimartin/gocui"
)

func main() {
	d := NewDebugger()
	err := d.Load(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}

	ui, err := NewUI(d)
	if err != nil {
		log.Fatal(err)
	}
	defer ui.Close()

	if err := ui.MainLoop(); err != nil && err != gocui.ErrQuit {
		log.Fatal(err)
	}
}
