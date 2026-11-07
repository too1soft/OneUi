package oneuivpn

import (
	"encoding/json"
	"errors"
	"net"
	"net/netip"
	"unicode/utf8"

	"tailscale.com/net/netmon"
)

type nativeNetworkState struct {
	defaultInterface         string
	underlayID, underlayType string
	vpnProcessProtected      bool
	interfaces               []netmon.Interface
}

func decodeNetworkState(data []byte) (nativeNetworkState, error) {
	var value struct {
		Version             int    `json:"version"`
		DefaultInterface    string `json:"defaultInterface"`
		UnderlayID          string `json:"underlayId"`
		UnderlayType        string `json:"underlayType"`
		VPNProcessProtected bool   `json:"vpnProcessProtected"`
		Interfaces          []struct {
			Name         string   `json:"name"`
			Index        int      `json:"index"`
			MTU          int      `json:"mtu"`
			Up           bool     `json:"up"`
			Loopback     bool     `json:"loopback"`
			PointToPoint bool     `json:"pointToPoint"`
			Multicast    bool     `json:"multicast"`
			Addresses    []string `json:"addresses"`
		} `json:"interfaces"`
	}
	invalid := errors.New("invalid native network state")
	if len(data) > 65536 || !utf8.Valid(data) || json.Unmarshal(data, &value) != nil || value.Version != 1 || len(value.Interfaces) > 256 {
		return nativeNetworkState{}, invalid
	}
	if len(value.UnderlayID) > 256 || len(value.UnderlayType) > 32 {
		return nativeNetworkState{}, invalid
	}
	state := nativeNetworkState{defaultInterface: value.DefaultInterface, underlayID: value.UnderlayID,
		underlayType: value.UnderlayType, vpnProcessProtected: value.VPNProcessProtected}
	names, indexes := map[string]bool{}, map[int]bool{}
	for _, iface := range value.Interfaces {
		if iface.Name == "" || len(iface.Name) > 128 || iface.Index < 1 || iface.MTU < 1 || len(iface.Addresses) > 64 || names[iface.Name] || indexes[iface.Index] {
			return nativeNetworkState{}, invalid
		}
		names[iface.Name], indexes[iface.Index] = true, true
		flags := net.Flags(0)
		if iface.Up {
			flags |= net.FlagUp | net.FlagRunning
		}
		if iface.Loopback {
			flags |= net.FlagLoopback
		}
		if iface.PointToPoint {
			flags |= net.FlagPointToPoint
		}
		if iface.Multicast {
			flags |= net.FlagMulticast
		}
		addresses := make([]net.Addr, 0, len(iface.Addresses))
		for _, address := range iface.Addresses {
			prefix, err := netip.ParsePrefix(address)
			if err != nil || prefix.Addr().Is4In6() {
				return nativeNetworkState{}, invalid
			}
			addresses = append(addresses, &net.IPNet{IP: net.IP(prefix.Addr().AsSlice()), Mask: net.CIDRMask(prefix.Bits(), prefix.Addr().BitLen())})
		}
		state.interfaces = append(state.interfaces, netmon.Interface{Interface: &net.Interface{Name: iface.Name, Index: iface.Index, MTU: iface.MTU, Flags: flags}, AltAddrs: addresses})
	}
	if state.defaultInterface != "" && !names[state.defaultInterface] {
		return nativeNetworkState{}, invalid
	}
	return state, nil
}
