package oneuivpn

import (
	"errors"
	"sync"
	"sync/atomic"
	"tailscale.com/net/netmon"
)

type appleNetworkLease struct {
	mu     sync.RWMutex
	closed bool
	read   func() ([]byte, error)
}

var appleNetwork atomic.Pointer[appleNetworkLease]

func (lease *appleNetworkLease) snapshot() (nativeNetworkState, error) {
	lease.mu.RLock()
	defer lease.mu.RUnlock()
	if lease.closed {
		return nativeNetworkState{}, ErrClosed
	}
	data, err := lease.read()
	if err != nil {
		return nativeNetworkState{}, err
	}
	return decodeNetworkState(data)
}

func init() {
	netmon.RegisterInterfaceGetter(func() ([]netmon.Interface, error) {
		lease := appleNetwork.Load()
		if lease == nil {
			return nil, ErrClosed
		}
		state, err := lease.snapshot()
		return state.interfaces, err
	})
}

func refreshNativeNetwork() error {
	lease := appleNetwork.Load()
	if lease == nil {
		return nil
	}
	state, err := lease.snapshot()
	if err != nil {
		return err
	}
	// Updating the default may enumerate interfaces. Hold no lease lock here.
	netmon.UpdateLastKnownDefaultRouteInterface(state.defaultInterface)
	return nil
}

func installSocketProtector(_ func(int) error, read func() ([]byte, error)) (func(), error) {
	if read == nil {
		return nil, errors.New("Apple native network state is required")
	}
	lease := &appleNetworkLease{read: read}
	if !appleNetwork.CompareAndSwap(nil, lease) {
		return nil, errors.New("another native VPN owns Apple network hooks")
	}
	release := func() {
		appleNetwork.CompareAndSwap(lease, nil)
		lease.mu.Lock()
		lease.closed = true
		lease.mu.Unlock()
	}
	if err := refreshNativeNetwork(); err != nil {
		release()
		return nil, err
	}
	return release, nil
}
