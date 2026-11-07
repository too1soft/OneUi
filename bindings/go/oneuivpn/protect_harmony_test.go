//go:build openharmony || ohos || oneui_harmonyos

package oneuivpn

import (
	"errors"
	"testing"
)

func TestHarmonyRuntimeIsFailClosed(t *testing.T) {
	called := false
	release, err := installSocketProtector(func(int) error { called = true; return nil }, func() ([]byte, error) {
		return []byte(`{"version":1,"interfaces":[]}`), nil
	})
	if release != nil || !errors.Is(err, ErrHarmonyRuntime) || called {
		t.Fatal("unverified native runtime must not acquire a successful protector lease")
	}
	if !errors.Is(refreshNativeNetwork(), ErrHarmonyRuntime) {
		t.Fatal("unverified runtime must not report a successful refresh")
	}
}

func TestHarmonyProcessProtectionLease(t *testing.T) {
	protect := func(int) error {
		t.Fatal("NetworkKit already protected the process")
		return nil
	}
	read := func() ([]byte, error) {
		return []byte(`{"version":1,"vpnProcessProtected":true,"interfaces":[]}`), nil
	}
	release, err := installSocketProtector(protect, read)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(release)
	if err := refreshNativeNetwork(); err != nil {
		t.Fatal(err)
	}
	if duplicate, err := installSocketProtector(protect, read); duplicate != nil || err == nil {
		t.Fatal("duplicate process lease accepted")
	}
	release()
	if !errors.Is(refreshNativeNetwork(), ErrHarmonyRuntime) {
		t.Fatal("released lease is still active")
	}
}
