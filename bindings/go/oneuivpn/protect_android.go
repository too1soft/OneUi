package oneuivpn

import (
	"errors"
	"sync"
	"sync/atomic"

	"tailscale.com/net/netmon"
	"tailscale.com/net/netns"
)

type socketProtector struct {
	mu      sync.RWMutex
	closed  bool
	protect func(int) error
	network func() ([]byte, error)
}

var androidProtector atomic.Pointer[socketProtector]

func refreshNativeNetwork() error { return nil }

func init() {
	netns.SetAndroidProtectFunc(func(fd int) error {
		current := androidProtector.Load()
		if current == nil {
			return ErrClosed
		}
		current.mu.RLock()
		defer current.mu.RUnlock()
		if current.closed {
			return ErrClosed
		}
		return current.protect(fd)
	})
	netmon.RegisterInterfaceGetter(func() ([]netmon.Interface, error) {
		current := androidProtector.Load()
		if current == nil {
			return nil, ErrClosed
		}
		current.mu.RLock()
		defer current.mu.RUnlock()
		if current.closed {
			return nil, ErrClosed
		}
		data, err := current.network()
		if err != nil {
			return nil, err
		}
		state, err := decodeNetworkState(data)
		if err != nil {
			return nil, err
		}
		netmon.UpdateLastKnownDefaultRouteInterface(state.defaultInterface)
		return state.interfaces, nil
	})
}

func installSocketProtector(protect func(int) error, network func() ([]byte, error)) (func(), error) {
	if protect == nil || network == nil {
		return nil, errors.New("Android socket protection and native network state are required")
	}
	lease := &socketProtector{protect: protect, network: network}
	if !androidProtector.CompareAndSwap(nil, lease) {
		return nil, errors.New("another native VPN owns Android system hooks")
	}
	return func() {
		androidProtector.CompareAndSwap(lease, nil)
		lease.mu.Lock()
		lease.closed = true
		lease.mu.Unlock()
	}, nil
}
