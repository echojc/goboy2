package main

import (
	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"
)

type GUI struct {
	Debugger *Debugger

	window fyne.Window
}

func NewGUI(d *Debugger) *GUI {
	a := app.New()
	w := a.NewWindow("main")

	tiles := canvas.NewImageFromImage(d.Tiles())
	tiles.SetMinSize(fyne.NewSize(512, 768))
	tiles.ScaleMode = canvas.ImageScalePixels
	tiles.Refresh()

	w.SetContent(container.NewVBox(
		widget.NewButton("tiles", func() {
			tiles.Image = d.Tiles()
			tiles.Refresh()
		}),
		tiles,
	))

	return &GUI{
		Debugger: d,
		window:   w,
	}
}

func (gui *GUI) ShowAndRun() {
	gui.window.ShowAndRun()
}

func (gui *GUI) Close() {
	gui.window.Close()
}
