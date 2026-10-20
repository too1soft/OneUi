package main

import (
	"testing"
	"unsafe"
)

func TestWin7ABILayout(t *testing.T) {
	wantFile, wantData := uintptr(16), uintptr(48)
	if unsafe.Sizeof(uintptr(0)) == 8 {
		wantFile, wantData = 32, 80
	}
	if unsafe.Sizeof(fileInfo{}) != wantFile || unsafe.Sizeof(trustData{}) != wantData {
		t.Fatal("Win7 SDK structure layout mismatch")
	}
}

func TestOnlyZeroIsTrusted(t *testing.T) {
	if reason(0) != "trusted" {
		t.Fatal("success not identified")
	}
	for _, code := range []uint32{1, 0x800b0109, 0x80096010, 0x800b0100, 0xffffffff} {
		if reason(code) == "trusted" {
			t.Fatal("a nonzero status must not be accepted")
		}
	}
}
