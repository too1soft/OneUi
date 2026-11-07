package oneuivpn

import (
	"encoding/json"
	"errors"
	"net/netip"
	"unicode/utf8"

	"tailscale.com/net/dns"
	"tailscale.com/util/dnsname"
)

// baseDNS uses an OS-authorized underlay snapshot, never the resolver settings
// installed by this VPN. Platforms without this additive field retain their
// existing explicit unsupported behavior (including native Apple split DNS).
func (a *NetworkAdapter) baseDNS() (dns.OSConfig, error) {
	a.mu.Lock()
	if a.closed {
		a.mu.Unlock()
		return dns.OSConfig{}, ErrClosed
	}
	read := a.options.ReadNetworkState
	a.mu.Unlock()
	if read == nil {
		return dns.OSConfig{}, dns.ErrGetBaseConfigNotSupported
	}
	data, err := read()
	if err != nil {
		return dns.OSConfig{}, err
	}
	var snapshot struct {
		Version int `json:"version"`
		BaseDNS *struct {
			Servers       []string `json:"servers"`
			SearchDomains []string `json:"searchDomains"`
		} `json:"baseDNS"`
	}
	invalid := errors.New("invalid native base DNS configuration")
	if len(data) > 65536 || !utf8.Valid(data) || json.Unmarshal(data, &snapshot) != nil || snapshot.Version != 1 {
		return dns.OSConfig{}, invalid
	}
	if snapshot.BaseDNS == nil {
		return dns.OSConfig{}, dns.ErrGetBaseConfigNotSupported
	}
	base := snapshot.BaseDNS
	if len(base.Servers) == 0 || len(base.Servers) > 8 || len(base.SearchDomains) > 16 {
		return dns.OSConfig{}, invalid
	}
	config := dns.OSConfig{}
	for _, text := range base.Servers {
		address, err := netip.ParseAddr(text)
		if err != nil || address.Is4In6() || address.IsUnspecified() || address.IsMulticast() ||
			address == netip.MustParseAddr("100.100.100.100") || address == netip.MustParseAddr("fd7a:115c:a1e0::53") {
			return dns.OSConfig{}, invalid
		}
		config.Nameservers = append(config.Nameservers, address)
	}
	for _, text := range base.SearchDomains {
		domain, err := dnsname.ToFQDN(text)
		if err != nil || domain == "." {
			return dns.OSConfig{}, invalid
		}
		config.SearchDomains = append(config.SearchDomains, domain)
	}
	return config, nil
}
