// Read-only native trust diagnostic. Uses the OS Authenticode policy, including
// revocation checking. Never inserts certificates into a machine/user store.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"syscall"
	"time"
	"unsafe"

	wincompat "github.com/too1soft/windows-compat/go"
)

type guid struct {
	Data1        uint32
	Data2, Data3 uint16
	Data4        [8]byte
}

type fileInfo struct {
	Size         uint32
	Path         *uint16
	Handle       uintptr
	KnownSubject *guid
}

// The pre-KB3033929 layout ends at UIContext. The newer optional signature
// settings pointer must not become a hidden Win7 update prerequisite.
type trustData struct {
	Size                                    uint32
	Policy, SIP                             unsafe.Pointer
	UIChoice, RevocationChecks, UnionChoice uint32
	File                                    *fileInfo
	StateAction                             uint32
	State                                   uintptr
	URL                                     *uint16
	ProviderFlags, UIContext                uint32
}

func verify(path string) (uint32, error) {
	var system [32768]uint16
	n, _, err := syscall.NewLazyDLL("kernel32.dll").NewProc("GetSystemDirectoryW").Call(uintptr(unsafe.Pointer(&system[0])), uintptr(len(system)))
	if n == 0 || n >= uintptr(len(system)) {
		return 0, fmt.Errorf("GetSystemDirectoryW: %w", err)
	}
	// System directory only; no dependency probing in CWD or PATH.
	proc := syscall.NewLazyDLL(filepath.Join(syscall.UTF16ToString(system[:n]), "wintrust.dll")).NewProc("WinVerifyTrust")
	if err := proc.Find(); err != nil {
		return 0, err
	}
	name, err := syscall.UTF16PtrFromString(path)
	if err != nil {
		return 0, err
	}
	info := fileInfo{Path: name}
	info.Size = uint32(unsafe.Sizeof(info))
	data := trustData{UIChoice: 2, RevocationChecks: 1, UnionChoice: 1, File: &info, StateAction: 1, ProviderFlags: 0x80 | 0x2000}
	data.Size = uint32(unsafe.Sizeof(data))
	action := guid{0x00aac56b, 0xcd44, 0x11d0, [8]byte{0x8c, 0xc2, 0x00, 0xc0, 0x4f, 0xc2, 0x95, 0xee}}
	code, _, _ := proc.Call(^uintptr(0), uintptr(unsafe.Pointer(&action)), uintptr(unsafe.Pointer(&data)))
	data.StateAction = 2 // WTD_STATEACTION_CLOSE also on failure.
	proc.Call(^uintptr(0), uintptr(unsafe.Pointer(&action)), uintptr(unsafe.Pointer(&data)))
	runtime.KeepAlive(name)
	return uint32(code), nil
}

func reason(code uint32) string {
	switch code {
	case 0:
		return "trusted"
	case 0x800b0109:
		return "untrusted_root"
	case 0x800b010a:
		return "incomplete_certificate_chain"
	case 0x80092013:
		return "revocation_server_unavailable"
	case 0x800b010e:
		return "revocation_check_failed"
	case 0x800b0101:
		return "certificate_expired_or_not_yet_valid"
	case 0x800b010c:
		return "certificate_revoked"
	case 0x80096010:
		return "signature_digest_mismatch"
	case 0x800b0100:
		return "signature_missing"
	default:
		return "os_trust_rejected"
	}
}

func run() int {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: authenticode-probe <file.exe>")
		return 2
	}
	path, err := filepath.Abs(os.Args[1])
	if err != nil {
		return 2
	}
	identity, err := wincompat.Detect()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	file, err := os.Open(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	defer file.Close()
	hash := sha256.New()
	if _, err := io.Copy(hash, file); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	data := map[string]any{"time": time.Now().UTC().Format(time.RFC3339), "identity": identity,
		"scope": "native-authenticode-only", "subject": filepath.Base(path), "subjectSHA256": hex.EncodeToString(hash.Sum(nil)),
		"revocationChecking": true, "systemTrustStoreUnchanged": true, "stage": "verifying"}
	json.NewEncoder(os.Stdout).Encode(data)
	started := time.Now()
	code, err := verify(path)
	data["elapsedMs"] = time.Since(started).Milliseconds()
	data["stage"] = "complete"
	data["passed"] = err == nil && code == 0
	if err != nil {
		data["error"] = err.Error()
	} else {
		data["status"] = fmt.Sprintf("0x%08x", code)
		data["reason"] = reason(code)
	}
	json.NewEncoder(os.Stdout).Encode(data)
	if err != nil || code != 0 {
		return 1
	}
	return 0
}

func main() { os.Exit(run()) }
