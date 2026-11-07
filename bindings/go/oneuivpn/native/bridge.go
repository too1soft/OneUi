// Package native exports the reusable packet/network ABI to native VPN hosts.
// Application main packages import it when producing a Go c-shared/c-archive.
package native

/*
#include "native_bridge.h"
*/
import "C"

import (
	"context"
	"errors"
	"sync"
	"time"
	"unsafe"

	vpn "github.com/too1soft/OneUi/bindings/go/oneuivpn"
)

type entry struct {
	sync.RWMutex
	host      *vpn.Host
	callbacks C.OneUiVpnNativeCallbacks
	closed    bool
}

var hosts = struct {
	sync.RWMutex
	next   uint64
	values map[uint64]*entry
}{values: make(map[uint64]*entry)}

func Lookup(handle uint64) (*vpn.Host, error) {
	hosts.RLock()
	e := hosts.values[handle]
	hosts.RUnlock()
	if e == nil {
		return nil, vpn.ErrClosed
	}
	return e.host, nil
}

func result(err error) C.int {
	if err == nil {
		return 0
	}
	if errors.Is(err, vpn.ErrClosed) {
		return -1
	}
	if errors.Is(err, vpn.ErrInvalidPacket) {
		return -2
	}
	if errors.Is(err, context.DeadlineExceeded) {
		return -3
	}
	return -4
}

//export oneui_vpn_go_abi_version
func oneui_vpn_go_abi_version() C.uint32_t { return 1 }

//export oneui_vpn_go_host_create
func oneui_vpn_go_host_create(label *C.char, mtu, flags C.int, supplied *C.OneUiVpnNativeCallbacks) C.uint64_t {
	if supplied == nil || supplied.struct_size != C.sizeof_OneUiVpnNativeCallbacks || supplied.abi_version != 1 {
		return 0
	}
	callbacks := *supplied
	if label == nil || flags < 0 || flags > 3 || callbacks.apply_network == nil {
		return 0
	}
	// Bounded scan avoids an unbounded C.GoString on a malformed native label.
	length := 0
	for length <= 128 && *(*byte)(unsafe.Add(unsafe.Pointer(label), length)) != 0 {
		length++
	}
	if length == 0 || length > 128 {
		return 0
	}
	options := vpn.NetworkOptions{Label: C.GoStringN(label, C.int(length)), MTU: int(mtu), AllowDefaultRoute: flags&1 != 0, SupportsSplitDNS: flags&2 != 0,
		ReadNetworkState: func() ([]byte, error) {
			buffer := make([]byte, 65536)
			n := C.oneui_vpn_call_network_state(callbacks, unsafe.Pointer(unsafe.SliceData(buffer)), C.int(len(buffer)))
			if n < 1 || n > 65536 {
				return nil, errors.New("native network state unavailable")
			}
			return buffer[:int(n)], nil
		},
		Apply: func(s vpn.NetworkSettings) error {
			data, err := s.JSON()
			if err != nil {
				return err
			}
			if C.oneui_vpn_call_apply(callbacks, (*C.char)(unsafe.Pointer(unsafe.SliceData(data))), C.int(len(data))) != 0 {
				return errors.New("native network update failed")
			}
			return nil
		},
	}
	h, err := vpn.NewHost(options, func(fd int) error {
		if C.oneui_vpn_call_protect(callbacks, C.int(fd)) != 0 {
			return errors.New("native socket protection failed")
		}
		return nil
	})
	if err != nil {
		return 0
	}
	hosts.Lock()
	defer hosts.Unlock()
	if hosts.next == ^uint64(0) {
		_ = h.Close()
		return 0
	}
	hosts.next++
	hosts.values[hosts.next] = &entry{host: h, callbacks: callbacks}
	return C.uint64_t(hosts.next)
}

//export oneui_vpn_go_network_changed
func oneui_vpn_go_network_changed(handle C.uint64_t) C.int {
	h, err := Lookup(uint64(handle))
	if err != nil {
		return result(err)
	}
	return result(h.NetworkChanged())
}

//export oneui_vpn_go_packet_inject
func oneui_vpn_go_packet_inject(handle C.uint64_t, packet unsafe.Pointer, length, timeout C.int) C.int {
	if packet == nil || length < 1 || length > 65535 || timeout < 1 || timeout > 30000 {
		return -2
	}
	h, err := Lookup(uint64(handle))
	if err != nil {
		return result(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(timeout)*time.Millisecond)
	defer cancel()
	// Inject copies before returning; native memory never escapes this call.
	return result(h.Inject(ctx, unsafe.Slice((*byte)(packet), int(length))))
}

//export oneui_vpn_go_packet_receive
func oneui_vpn_go_packet_receive(handle C.uint64_t, buffer unsafe.Pointer, capacity, timeout C.int) C.int {
	if buffer == nil || capacity < 65535 || timeout < 1 || timeout > 30000 {
		return -2
	}
	h, err := Lookup(uint64(handle))
	if err != nil {
		return result(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(timeout)*time.Millisecond)
	defer cancel()
	packet, err := h.Receive(ctx)
	if err != nil {
		return result(err)
	}
	copy(unsafe.Slice((*byte)(buffer), int(capacity)), packet)
	return C.int(len(packet))
}

//export oneui_vpn_go_host_close
func oneui_vpn_go_host_close(handle C.uint64_t) C.int {
	hosts.RLock()
	e := hosts.values[uint64(handle)]
	hosts.RUnlock()
	if e == nil {
		return -1
	}
	e.Lock()
	defer e.Unlock()
	if err := e.host.Close(); err != nil {
		return result(err)
	}
	e.closed = true
	hosts.Lock()
	delete(hosts.values, uint64(handle))
	hosts.Unlock()
	return 0
}
