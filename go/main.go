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

	cli, err := NewCLI(d)
	if err != nil {
		log.Fatal(err)
	}
	defer cli.Close()

	gui := NewGUI(d)

	go func() {
		defer gui.Close()
		if err := cli.MainLoop(); err != nil && err != gocui.ErrQuit {
			log.Fatal(err)
		}
	}()

	gui.ShowAndRun()
}
