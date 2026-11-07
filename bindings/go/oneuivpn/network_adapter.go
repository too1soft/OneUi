package oneuivpn

import (
	"encoding/json"
	"errors"
	"net/netip"
	"slices"
	"sync"

	"tailscale.com/net/dns"
	"tailscale.com/wgengine/router"
)

type Prefix struct {
	Address string `json:"address"`
	Prefix  int    `json:"prefix"`
}

// NetworkSettings contains only OS settings, never credentials or peer keys.
// Empty addresses means remove this host's VPN settings. Apply must acknowledge
// the actual OS operation before returning; errors leave the last state intact.
type NetworkSettings struct {
	Version           int      `json:"version"`
	Label             string   `json:"label"`
	MTU               int      `json:"mtu"`
	AllowDefaultRoute bool     `json:"allowDefaultRoute"`
	Addresses         []Prefix `json:"addresses"`
	Routes            []Prefix `json:"routes"`
	ExcludedRoutes    []Prefix `json:"excludedRoutes"`
	DNSServers        []string `json:"dnsServers"`
	DNSDomains        []string `json:"dnsDomains"`
	DNSMatchDomains   []string `json:"dnsMatchDomains"`
}

type NetworkOptions struct {
	Label             string
	MTU               int
	AllowDefaultRoute bool
	SupportsSplitDNS  bool
	Apply             func(NetworkSettings) error
	ReadNetworkState  func() ([]byte, error)
}

// NetworkAdapter serializes route and DNS updates into acknowledged snapshots.
// The native host owns OS APIs; this adapter never edits desktop host settings.
type NetworkAdapter struct {
	mu       sync.Mutex
	options  NetworkOptions
	settings NetworkSettings
	closed   bool
}

func NewNetworkAdapter(options NetworkOptions) (*NetworkAdapter, error) {
	if options.Label == "" || len(options.Label) > 128 || options.MTU < 1280 || options.MTU > 9000 || options.Apply == nil {
		return nil, errors.New("invalid OneUI native network adapter")
	}
	return &NetworkAdapter{options: options, settings: NetworkSettings{
		Version: 1, Label: options.Label, MTU: options.MTU, AllowDefaultRoute: options.AllowDefaultRoute,
		Addresses: []Prefix{}, Routes: []Prefix{}, ExcludedRoutes: []Prefix{},
		DNSServers: []string{}, DNSDomains: []string{}, DNSMatchDomains: []string{},
	}}, nil
}

func (s NetworkSettings) JSON() ([]byte, error) { return json.Marshal(s) }

func (s NetworkSettings) clone() NetworkSettings {
	s.Addresses, s.Routes, s.ExcludedRoutes = slices.Clone(s.Addresses), slices.Clone(s.Routes), slices.Clone(s.ExcludedRoutes)
	s.DNSServers, s.DNSDomains, s.DNSMatchDomains = slices.Clone(s.DNSServers), slices.Clone(s.DNSDomains), slices.Clone(s.DNSMatchDomains)
	return s
}

func prefixes(values []netip.Prefix, routes, defaultAllowed bool) ([]Prefix, error) {
	if len(values) > 256 {
		return nil, errors.New("native VPN prefix limit exceeded")
	}
	out := make([]Prefix, 0, len(values))
	for _, p := range values {
		if !p.IsValid() || p.Addr().Is4In6() || (routes && p.Bits() == 0 && !defaultAllowed) {
			return nil, errors.New("native VPN prefix is not permitted")
		}
		if routes {
			p = p.Masked()
		}
		out = append(out, Prefix{Address: p.Addr().String(), Prefix: p.Bits()})
	}
	return out, nil
}

func (a *NetworkAdapter) Up() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.closed {
		return ErrClosed
	}
	return nil
}

func (a *NetworkAdapter) Set(config *router.Config) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.closed {
		return ErrClosed
	}
	if config == nil {
		config = &router.Config{}
	}
	if len(config.SubnetRoutes) != 0 {
		return errors.New("native VPN forwarding is unsupported")
	}
	next := a.settings.clone()
	if config.NewMTU != 0 && config.NewMTU != a.options.MTU {
		return errors.New("native MTU changes require a new VPN host")
	}
	if next.MTU < 1280 || next.MTU > 9000 || len(config.LocalAddrs) > 16 {
		return errors.New("invalid native VPN interface settings")
	}
	var err error
	if next.Addresses, err = prefixes(config.LocalAddrs, false, true); err != nil {
		return err
	}
	if next.Routes, err = prefixes(config.Routes, true, next.AllowDefaultRoute); err != nil {
		return err
	}
	if next.ExcludedRoutes, err = prefixes(config.LocalRoutes, true, true); err != nil {
		return err
	}
	for _, route := range next.Routes {
		addr := netip.MustParseAddr(route.Address)
		if !slices.ContainsFunc(next.Addresses, func(p Prefix) bool { return netip.MustParseAddr(p.Address).Is4() == addr.Is4() }) {
			return errors.New("native route has no configured address family")
		}
	}
	return a.commit(next)
}

func (a *NetworkAdapter) commit(next NetworkSettings) error {
	if err := a.options.Apply(next.clone()); err != nil {
		return err
	}
	a.settings = next
	return nil
}

func (a *NetworkAdapter) SetDNS(config dns.OSConfig) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.closed {
		return ErrClosed
	}
	if len(config.Hosts) != 0 || len(config.Nameservers) > 8 || len(config.SearchDomains) > 16 || len(config.MatchDomains) > 256 {
		return errors.New("unsupported native VPN DNS settings")
	}
	if len(config.MatchDomains) != 0 && !a.options.SupportsSplitDNS {
		return errors.New("native split DNS is unsupported")
	}
	next := a.settings.clone()
	next.DNSServers, next.DNSDomains, next.DNSMatchDomains = []string{}, []string{}, []string{}
	for _, addr := range config.Nameservers {
		if !addr.IsValid() || addr.Is4In6() {
			return errors.New("invalid native VPN DNS address")
		}
		next.DNSServers = append(next.DNSServers, addr.String())
	}
	for _, domain := range config.SearchDomains {
		next.DNSDomains = append(next.DNSDomains, domain.WithoutTrailingDot())
	}
	for _, domain := range config.MatchDomains {
		next.DNSMatchDomains = append(next.DNSMatchDomains, domain.WithoutTrailingDot())
	}
	if slices.Equal(next.DNSServers, a.settings.DNSServers) && slices.Equal(next.DNSDomains, a.settings.DNSDomains) &&
		slices.Equal(next.DNSMatchDomains, a.settings.DNSMatchDomains) {
		return nil
	}
	return a.commit(next)
}

func (a *NetworkAdapter) SupportsSplitDNS() bool { return a.options.SupportsSplitDNS }
func (a *NetworkAdapter) GetBaseConfig() (dns.OSConfig, error) {
	return a.baseDNS()
}

func (a *NetworkAdapter) Close() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.closed {
		return nil
	}
	next := a.settings.clone()
	next.Addresses, next.Routes, next.ExcludedRoutes = []Prefix{}, []Prefix{}, []Prefix{}
	next.DNSServers, next.DNSDomains, next.DNSMatchDomains = []string{}, []string{}, []string{}
	if err := a.commit(next); err != nil {
		return err
	}
	a.closed = true
	return nil
}
