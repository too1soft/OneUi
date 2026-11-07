package oneuivpn

import (
	"encoding/json"
	"net/netip"
	"strings"
	"tailscale.com/net/netcheck"
	"tailscale.com/types/opt"
	"testing"
	"time"
)

func TestConnectivityDistinguishesUnknownAndFailedWithoutAddresses(t *testing.T) {
	if r := connectivityReport(nil); r.Available || r.UDP != nil || r.MappingVaries != nil {
		t.Fatal("unknown reported as failed")
	}
	r := connectivityReport(&netcheck.Report{Now: time.Now(), UDP: true, IPv4: true, IPv6: false, IPv6CanSend: true,
		MappingVariesByDestIP: opt.Bool("true"), GlobalV4: netip.MustParseAddrPort("198.51.100.5:5000")})
	if !r.Available || r.UDP == nil || !*r.UDP || r.IPv6 == nil || *r.IPv6 || r.MappingVaries == nil || !*r.MappingVaries || r.UPnP != nil {
		t.Fatal("incorrect connectivity summary")
	}
	raw, _ := json.Marshal(r)
	if strings.Contains(string(raw), "198.51.100") || strings.Contains(string(raw), "5000") {
		t.Fatal("endpoint leaked")
	}
	if !connectivityReport(&netcheck.Report{Now: time.Now().Add(-3 * time.Minute)}).Stale {
		t.Fatal("old result not marked stale")
	}
}

func TestConnectivityDoesNotInventIPv6FailureWithoutAProbe(t *testing.T) {
	r := connectivityReport(&netcheck.Report{Now: time.Now(), UDP: true, IPv4: true, OSHasIPv6: true})
	if !r.Available || r.IPv6 != nil {
		t.Fatal("IPv4-only probe results were presented as an IPv6 failure")
	}
	r = connectivityReport(&netcheck.Report{Now: time.Now(), IPv6: true})
	if r.IPv6 == nil || !*r.IPv6 {
		t.Fatal("verified IPv6 response must remain successful")
	}
}
