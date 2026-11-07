package oneuivpn

import (
	"tailscale.com/net/netcheck"
	"tailscale.com/types/opt"
	"time"
)

// ConnectivityReport deliberately excludes addresses, ports and peer identities.
// A service being present is distinct from a successful router port mapping.
type ConnectivityReport struct {
	Available        bool   `json:"available"`
	Stale            bool   `json:"stale"`
	MeasuredAtUnixMS int64  `json:"measuredAtUnixMs,omitempty"`
	UDP              *bool  `json:"udp"`
	IPv4             *bool  `json:"ipv4"`
	IPv6             *bool  `json:"ipv6"`
	IPv6CanSend      *bool  `json:"ipv6CanSend"`
	MappingVaries    *bool  `json:"mappingVaries"`
	UPnP             *bool  `json:"upnp"`
	PMP              *bool  `json:"pmp"`
	PCP              *bool  `json:"pcp"`
	UnderlayType     string `json:"underlayType,omitempty"`
	NetworkChanges   uint64 `json:"networkChanges"`
	SocketRebinds    uint64 `json:"socketRebinds"`
}

func knownBool(v bool) *bool { return &v }
func optionalBool(v opt.Bool) *bool {
	value, ok := v.Get()
	if !ok {
		return nil
	}
	return knownBool(value)
}
func connectivityReport(r *netcheck.Report) ConnectivityReport {
	if r == nil || r.Now.IsZero() {
		return ConnectivityReport{}
	}
	// No IPv6 STUN response is not a failed round trip unless a packet could
	// actually be sent. An IPv4-only probe directory leaves IPv6 untested.
	var ipv6 *bool
	if r.IPv6 || r.IPv6CanSend {
		ipv6 = knownBool(r.IPv6)
	}
	return ConnectivityReport{Available: true, Stale: time.Since(r.Now) > 2*time.Minute,
		MeasuredAtUnixMS: r.Now.UnixMilli(), UDP: knownBool(r.UDP), IPv4: knownBool(r.IPv4),
		IPv6: ipv6, IPv6CanSend: knownBool(r.IPv6CanSend),
		MappingVaries: optionalBool(r.MappingVariesByDestIP), UPnP: optionalBool(r.UPnP),
		PMP: optionalBool(r.PMP), PCP: optionalBool(r.PCP)}
}
