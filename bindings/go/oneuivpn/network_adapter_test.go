package oneuivpn

import (
	"errors"
	"net/netip"
	"testing"

	"tailscale.com/net/dns"
	"tailscale.com/wgengine/router"
)

func TestNativeNetworkAcknowledgementAndRollback(t *testing.T) {
	var received []NetworkSettings
	reject := false
	a, err := NewNetworkAdapter(NetworkOptions{Label: "Native", MTU: 1280, Apply: func(s NetworkSettings) error {
		if reject {
			return errors.New("OS rejected settings")
		}
		received = append(received, s)
		return nil
	}})
	if err != nil {
		t.Fatal(err)
	}
	config := &router.Config{LocalAddrs: []netip.Prefix{netip.MustParsePrefix("100.64.0.1/32")}, Routes: []netip.Prefix{netip.MustParsePrefix("100.64.0.2/32")}}
	if err := a.Set(config); err != nil {
		t.Fatal(err)
	}
	received[0].Addresses[0].Address = "caller-mutated"
	reject = true
	if err := a.Set(&router.Config{LocalAddrs: []netip.Prefix{netip.MustParsePrefix("100.64.0.3/32")}}); err == nil {
		t.Fatal("OS failure was ignored")
	}
	reject = false
	if err := a.SetDNS(dns.OSConfig{Nameservers: []netip.Addr{netip.MustParseAddr("100.64.0.53")}}); err != nil {
		t.Fatal(err)
	}
	if received[1].Addresses[0].Address != "100.64.0.1" || received[1].Routes[0].Address != "100.64.0.2" {
		t.Fatal("failed or caller-mutated state was committed")
	}
	if err := a.Close(); err != nil {
		t.Fatal(err)
	}
	if len(received[2].Addresses) != 0 || len(received[2].DNSServers) != 0 {
		t.Fatal("close did not remove native settings")
	}
	if err := a.Set(config); !errors.Is(err, ErrClosed) {
		t.Fatal(err)
	}
}

func TestNativeNetworkRejectsUnsafeOrUnsupportedRoutes(t *testing.T) {
	calls := 0
	a, _ := NewNetworkAdapter(NetworkOptions{Label: "Native", MTU: 1280, Apply: func(NetworkSettings) error { calls++; return nil }})
	for _, config := range []*router.Config{
		{LocalAddrs: []netip.Prefix{netip.MustParsePrefix("100.64.0.1/32")}, Routes: []netip.Prefix{netip.MustParsePrefix("0.0.0.0/0")}},
		{LocalAddrs: []netip.Prefix{netip.MustParsePrefix("100.64.0.1/32")}, Routes: []netip.Prefix{netip.MustParsePrefix("fd00::/64")}},
		{SubnetRoutes: []netip.Prefix{netip.MustParsePrefix("192.168.1.0/24")}},
	} {
		if err := a.Set(config); err == nil {
			t.Fatal("unsupported route accepted")
		}
	}
	if calls != 0 {
		t.Fatal("invalid settings reached native host")
	}
}
