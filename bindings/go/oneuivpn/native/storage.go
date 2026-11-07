package native

/*
#include <stdlib.h>
#include "native_bridge.h"
*/
import "C"

import (
	"context"
	"errors"
	"strings"
	"unsafe"

	vpn "github.com/too1soft/OneUi/bindings/go/oneuivpn"
)

type SecureBlobStore struct{ entry *entry }

func Storage(handle uint64) (*SecureBlobStore, error) {
	hosts.RLock()
	e := hosts.values[handle]
	hosts.RUnlock()
	if e == nil {
		return nil, vpn.ErrClosed
	}
	if e.callbacks.secure_read == nil || e.callbacks.secure_write == nil {
		return nil, errors.New("native secure storage callbacks are required")
	}
	return &SecureBlobStore{entry: e}, nil
}

func storageKey(key string) bool {
	if len(key) == 0 || len(key) > 128 || strings.ContainsAny(key, "/\\\x00") {
		return false
	}
	return true
}

func (s *SecureBlobStore) Read(ctx context.Context, key string) ([]byte, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if !storageKey(key) {
		return nil, errors.New("invalid secure-storage key")
	}
	s.entry.RLock()
	defer s.entry.RUnlock()
	if s.entry.closed {
		return nil, vpn.ErrClosed
	}
	name := C.CString(key)
	defer C.free(unsafe.Pointer(name))
	buffer := make([]byte, 1024*1024)
	n := C.oneui_vpn_call_secure_read(s.entry.callbacks, name, unsafe.Pointer(unsafe.SliceData(buffer)), C.int(len(buffer)))
	if n < 0 || int(n) > len(buffer) {
		clear(buffer)
		return nil, errors.New("native secure-storage read failed")
	}
	data := append([]byte(nil), buffer[:int(n)]...)
	clear(buffer)
	return data, nil
}

func (s *SecureBlobStore) Write(ctx context.Context, key string, data []byte) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if !storageKey(key) || len(data) == 0 || len(data) > 1024*1024 {
		return errors.New("invalid secure-storage write")
	}
	s.entry.RLock()
	defer s.entry.RUnlock()
	if s.entry.closed {
		return vpn.ErrClosed
	}
	name := C.CString(key)
	defer C.free(unsafe.Pointer(name))
	if C.oneui_vpn_call_secure_write(s.entry.callbacks, name, unsafe.Pointer(unsafe.SliceData(data)), C.int(len(data))) != 0 {
		return errors.New("native secure-storage write failed")
	}
	return nil
}
