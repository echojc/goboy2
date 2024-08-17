package main

import (
	"reflect"
	"unsafe"
)

import "C"

func malloc[T any](size int) (*T, []T) {
	bytes := int(reflect.TypeOf((*T)(nil)).Elem().Size())
	array := (*T)(C.malloc(C.ulong(size * bytes)))
	return array, asSlice(array, size)
}

func asSlice[T any](arr *T, size int) []T {
	var slice []T

	header := (*reflect.SliceHeader)((unsafe.Pointer(&slice)))
	header.Cap = size
	header.Len = size
	header.Data = uintptr(unsafe.Pointer(arr))

	return slice
}

func tern[T any](b bool, t T, f T) T {
	if b {
		return t
	} else {
		return f
	}
}
