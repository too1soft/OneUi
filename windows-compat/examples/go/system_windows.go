package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
	"unsafe"
)

func systemInfo() string {
	var info struct {
		Size, Major, Minor, Build, Platform uint32
		ServicePack                         [128]uint16
	}
	info.Size = uint32(unsafe.Sizeof(info))
	status, _, _ := syscall.NewLazyDLL("ntdll.dll").NewProc("RtlGetVersion").Call(uintptr(unsafe.Pointer(&info)))
	if status != 0 {
		return "unknown"
	}
	return fmt.Sprintf("%d.%d.%d %s", info.Major, info.Minor, info.Build, syscall.UTF16ToString(info.ServicePack[:]))
}

func checkJunctionBoundary(parent, shared string, root *os.Root) error {
	outside := filepath.Join(parent, "outside")
	if err := os.Mkdir(outside, 0700); err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(outside, "secret.txt"), []byte("keep"), 0600); err != nil {
		return err
	}
	link := filepath.Join(shared, "junction")
	if output, err := exec.Command("cmd.exe", "/d", "/c", "mklink", "/J", link, outside).CombinedOutput(); err != nil {
		return fmt.Errorf("create test junction: %v: %s", err, output)
	}
	if file, err := root.Open("junction/secret.txt"); err == nil {
		file.Close()
		return fmt.Errorf("junction escaped Root")
	}
	if err := root.RemoveAll("junction"); err != nil {
		return fmt.Errorf("remove junction: %w", err)
	}
	if data, err := os.ReadFile(filepath.Join(outside, "secret.txt")); err != nil || string(data) != "keep" {
		return fmt.Errorf("junction removal modified outside data: %v", err)
	}
	return nil
}
