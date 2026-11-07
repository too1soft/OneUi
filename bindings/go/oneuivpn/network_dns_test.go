package oneuivpn

import (
	"errors"
	"net/netip"
	"testing"

	"tailscale.com/health"
	"tailscale.com/net/dns"
	"tailscale.com/net/netmon"
	"tailscale.com/net/tsdial"
	"tailscale.com/types/dnstype"
	"tailscale.com/util/dnsname"
	"tailscale.com/util/eventbus"
)

func TestAndroidPrivateDNSConfigUsesUnderlay(t *testing.T) {
	base := `{"version":1,"baseDNS":{"servers":["192.0.2.53"],"searchDomains":["lan.example"]}}`
	var applied NetworkSettings
	adapter, err := NewNetworkAdapter(NetworkOptions{Label: "VPN", MTU: 1280,
		ReadNetworkState: func() ([]byte, error) { return []byte(base), nil },
		Apply:            func(settings NetworkSettings) error { applied = settings; return nil },
	})
	if err != nil {
		t.Fatal(err)
	}
	bus := eventbus.New()
	dialer := tsdial.NewDialer(netmon.NewStatic())
	t.Cleanup(func() { dialer.Close() })
	manager := dns.NewManager(t.Logf, adapter, health.NewTracker(bus), dialer, nil, nil, "android", bus)
	t.Cleanup(func() { manager.Down() })
	zone := dnsname.FQDN("private.example.")
	config := dns.Config{AcceptDNS: true, SearchDomains: []dnsname.FQDN{zone},
		Routes: map[dnsname.FQDN][]*dnstype.Resolver{zone: nil},
		Hosts:  map[dnsname.FQDN][]netip.Addr{"desktop.private.example.": {netip.MustParseAddr("100.64.0.2")}}}
	// This exercises the exact upstream non-split DNS compilation that failed
	// in the installed product; no OS routes or public DNS queries are made.
	if err := manager.Set(config); err != nil {
		t.Fatal(err)
	}
	if len(applied.DNSServers) == 0 || applied.DNSServers[0] != "100.100.100.100" || len(applied.DNSMatchDomains) != 0 {
		t.Fatalf("Android DNS was not applied as a complete proxy: %+v", applied)
	}
	underlay, err := adapter.GetBaseConfig()
	if err != nil || underlay.Nameservers[0].String() != "192.0.2.53" {
		t.Fatalf("VPN DNS leaked into the base configuration: %+v %v", underlay, err)
	}
	base = `{"version":1,"baseDNS":{"servers":["192.0.2.54"],"searchDomains":[]}}`
	if err := manager.RecompileDNSConfig(); err != nil {
		t.Fatal(err)
	}
	underlay, err = adapter.GetBaseConfig()
	if err != nil || underlay.Nameservers[0].String() != "192.0.2.54" {
		t.Fatal("underlay DNS change was not observed")
	}
}

func TestNativeBaseDNSRejectsLoopsAndMalformedSnapshots(t *testing.T) {
	raw := `{"version":1}`
	adapter, _ := NewNetworkAdapter(NetworkOptions{Label: "VPN", MTU: 1280,
		ReadNetworkState: func() ([]byte, error) { return []byte(raw), nil },
		Apply:            func(NetworkSettings) error { return nil },
	})
	if _, err := adapter.GetBaseConfig(); !errors.Is(err, dns.ErrGetBaseConfigNotSupported) {
		t.Fatal(err)
	}
	for _, value := range []string{
		`{"version":2,"baseDNS":{"servers":["192.0.2.53"]}}`,
		`{"version":1,"baseDNS":{"servers":[]}}`,
		`{"version":1,"baseDNS":{"servers":["100.100.100.100"]}}`,
		`{"version":1,"baseDNS":{"servers":["fd7a:115c:a1e0::53"]}}`,
		`{"version":1,"baseDNS":{"servers":["0.0.0.0"]}}`,
		`{"version":1,"baseDNS":{"servers":["not-an-address"]}}`,
		`{"version":1,"baseDNS":{"servers":["192.0.2.53"],"searchDomains":["."]}}`,
	} {
		raw = value
		if _, err := adapter.GetBaseConfig(); err == nil {
			t.Fatal("unsafe base DNS accepted")
		}
	}
}
