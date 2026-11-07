//go:build openharmony || ohos || oneui_harmonyos

package oneuivpn

import (
	"errors"
	"net"
	"sync"
	"sync/atomic"

	"tailscale.com/net/netmon"
	"tailscale.com/net/netns"
)

// ErrHarmonyRuntime prevents starting the engine outside a protected VPN process.
var ErrHarmonyRuntime = errors.New("OneUI HarmonyOS VPN process protection is required")

type harmonyNetworkLease struct {
	mu     sync.RWMutex
	closed bool
	read   func() ([]byte, error)
}

var harmonyNetwork atomic.Pointer[harmonyNetworkLease]

func readProtectedHarmonyNetwork(read func() ([]byte, error)) (nativeNetworkState, error) {
	data, err := read()
	if err != nil {
		return nativeNetworkState{}, err
	}
	state, err := decodeNetworkState(data)
	if err != nil {
		return nativeNetworkState{}, err
	}
	if !state.vpnProcessProtected {
		return nativeNetworkState{}, ErrHarmonyRuntime
	}
	return state, nil
}

func init() {
	netmon.RegisterInterfaceGetter(func() ([]netmon.Interface, error) {
		lease := harmonyNetwork.Load()
		if lease == nil {
			interfaces, err := net.Interfaces()
			if err != nil {
				return nil, err
			}
			result := make([]netmon.Interface, len(interfaces))
			for i := range interfaces {
				result[i].Interface = &interfaces[i]
			}
			return result, nil
		}
		lease.mu.RLock()
		defer lease.mu.RUnlock()
		if lease.closed {
			return nil, ErrClosed
		}
		state, err := readProtectedHarmonyNetwork(lease.read)
		return state.interfaces, err
	})
}

func installSocketProtector(protect func(int) error, read func() ([]byte, error)) (func(), error) {
	if protect == nil || read == nil {
		return nil, ErrHarmonyRuntime
	}
	if _, err := readProtectedHarmonyNetwork(read); err != nil {
		return nil, err
	}
	lease := &harmonyNetworkLease{read: read}
	if !harmonyNetwork.CompareAndSwap(nil, lease) {
		return nil, errors.New("another native VPN owns HarmonyOS network hooks")
	}
	// NetworkKit owns bypass routing. Desktop socket marks or binds would
	// override that policy and require Linux privileges.
	netns.SetEnabled(false)
	return sync.OnceFunc(func() {
		lease.mu.Lock()
		defer lease.mu.Unlock()
		lease.closed = true
		netns.SetEnabled(true)
		harmonyNetwork.CompareAndSwap(lease, nil)
	}), nil
}

func refreshNativeNetwork() error {
	if harmonyNetwork.Load() == nil {
		return ErrHarmonyRuntime
	}
	// The host re-reads NetworkKit state and injects a monitor event.
	return nil
}
