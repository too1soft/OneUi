package oneuivpn

import (
	"fmt"
	"sync/atomic"
	"testing"
	"time"
)

func TestSameAddressNetworkReplacementRebindsAndCloseCancelsRetry(t *testing.T) {
	identity := "100:wifi"
	read := func() ([]byte, error) {
		return []byte(fmt.Sprintf(`{"version":1,"defaultInterface":"eth0","underlayId":%q,"underlayType":"wifi","interfaces":[{"name":"eth0","index":1,"mtu":1500,"up":true,"addresses":["192.0.2.2/24"]}]}`, identity)), nil
	}
	h, err := NewHost(NetworkOptions{Label: "network-test", MTU: 1280, Apply: func(NetworkSettings) error { return nil }, ReadNetworkState: read}, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer h.Close()
	var calls, rebinds atomic.Int32
	h.resources.RefreshTransport = func(rebind bool) {
		calls.Add(1)
		if rebind {
			rebinds.Add(1)
		}
	}
	h.initializeNetworkLocked()
	identity = "101:cellular"
	if err = h.NetworkChanged(); err != nil {
		t.Fatal(err)
	}
	if calls.Load() != 1 || rebinds.Load() != 1 || h.refresh.changes != 1 {
		t.Fatal("replacement with same interface and IP was missed")
	}
	if err = h.RefreshConnectivity(); err != nil {
		t.Fatal(err)
	}
	if rebinds.Load() != 1 {
		t.Fatal("rapid retry reopened sockets again")
	}
	if err = h.Close(); err != nil {
		t.Fatal(err)
	}
	before := calls.Load()
	time.Sleep(1100 * time.Millisecond)
	if calls.Load() != before {
		t.Fatal("discovery continued after close")
	}
}

func TestRestoredSameNetworkIdentityRenewsDiscovery(t *testing.T) {
	available := true
	read := func() ([]byte, error) {
		if !available {
			return []byte(`{"version":1,"interfaces":[]}`), nil
		}
		return []byte(`{"version":1,"defaultInterface":"eth0","underlayId":"same:wifi","interfaces":[{"name":"eth0","index":1,"mtu":1500,"up":true,"addresses":["192.0.2.2/24"]}]}`), nil
	}
	h, err := NewHost(NetworkOptions{Label: "network-test", MTU: 1280, Apply: func(NetworkSettings) error { return nil }, ReadNetworkState: read}, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer h.Close()
	h.initializeNetworkLocked()
	rebinds := 0
	h.resources.RefreshTransport = func(rebind bool) {
		if rebind {
			rebinds++
		}
	}
	available = false
	if err = h.NetworkChanged(); err != nil {
		t.Fatal(err)
	}
	available = true
	if err = h.NetworkChanged(); err != nil {
		t.Fatal(err)
	}
	if rebinds != 1 {
		t.Fatal("restoring same virtual network did not reopen sockets")
	}
}
