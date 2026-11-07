package oneuivpn

import (
	"net"
	"testing"
)

func TestNativeNetworkStatePreservesAddressesAndFlags(t *testing.T) {
	state, err := decodeNetworkState([]byte(`{"version":1,"defaultInterface":"wifi0","interfaces":[{"name":"wifi0","index":3,"mtu":1500,"up":true,"multicast":true,"addresses":["192.168.0.20/24","fe80::1/64"]}]}`))
	if err != nil { t.Fatal(err) }
	if state.defaultInterface != "wifi0" || len(state.interfaces) != 1 { t.Fatal(state) }
	iface := state.interfaces[0]
	addresses, err := iface.Addrs()
	if err != nil || len(addresses) != 2 || addresses[0].String() != "192.168.0.20/24" || iface.Flags&net.FlagUp == 0 { t.Fatalf("%+v %v %v", iface, addresses, err) }
	for _, raw := range []string{
		`{"version":2}`, `{"version":1,"defaultInterface":"missing","interfaces":[]}`,
		`{"version":1,"interfaces":[{"name":"bad","index":1,"mtu":1500,"addresses":["bad"]}]}`,
		`{"version":1,"interfaces":[{"name":"duplicate","index":1,"mtu":1500},{"name":"duplicate","index":2,"mtu":1500}]}`,
	} { if _, err := decodeNetworkState([]byte(raw)); err == nil { t.Fatal("malformed native network state accepted") } }
}
