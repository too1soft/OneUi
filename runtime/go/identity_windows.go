package wincompat

import (
	"fmt"
	"runtime"
	"syscall"
	"unsafe"
)

// Detect uses the real kernel version, never compatibility-manifest helpers.
// Native architecture is deliberately separate from this process's bitness.
func Detect() (Identity, error) {
	var version struct {
		Size, Major, Minor, Build, Platform       uint32
		CSD                                       [128]uint16
		ServicePackMajor, ServicePackMinor, Suite uint16
		ProductType, Reserved                     byte
	}
	version.Size = uint32(unsafe.Sizeof(version))
	proc := syscall.NewLazyDLL("ntdll.dll").NewProc("RtlGetVersion")
	if err := proc.Find(); err != nil {
		return Identity{}, err
	}
	status, _, _ := proc.Call(uintptr(unsafe.Pointer(&version)))
	if status != 0 {
		return Identity{}, fmt.Errorf("RtlGetVersion failed: %x", status)
	}
	var system struct {
		Architecture, Reserved        uint16
		PageSize                      uint32
		Minimum, Maximum, Mask        uintptr
		Processors, Type, Granularity uint32
		Level, Revision               uint16
	}
	syscall.NewLazyDLL("kernel32.dll").NewProc("GetNativeSystemInfo").Call(uintptr(unsafe.Pointer(&system)))
	i := Identity{Major: version.Major, Minor: version.Minor, Build: version.Build, ServicePack: version.ServicePackMajor}
	switch system.Architecture {
	case 0:
		i.NativeArchitecture = "x86"
	case 9:
		i.NativeArchitecture = "x86_64"
	default:
		return Identity{}, fmt.Errorf("unsupported native architecture %d", system.Architecture)
	}
	switch runtime.GOARCH {
	case "386":
		i.ProcessArchitecture = "x86"
	case "amd64":
		i.ProcessArchitecture = "x86_64"
	default:
		return Identity{}, fmt.Errorf("unsupported process architecture %s", runtime.GOARCH)
	}
	var err error
	i.DefaultProfile, err = profile(i.Major, i.Minor, i.Build, i.ServicePack)
	return i, err
}
