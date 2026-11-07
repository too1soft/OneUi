// Android emulator-only native VPN fixture. Never include this command in a
// product package: it creates an ephemeral loopback WireGuard echo peer.
package main

/*
#include <stdint.h>
*/
import "C"

import (
	"context"
	"encoding/json"
	"fmt"
	"net/netip"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"unsafe"

	"github.com/tailscale/wireguard-go/device"
	vpn "github.com/too1soft/OneUi/bindings/go/oneuivpn"
	"github.com/too1soft/OneUi/bindings/go/oneuivpn/native"
	"tailscale.com/net/dns"
	"tailscale.com/tailcfg"
	"tailscale.com/tsd"
	"tailscale.com/types/key"
	"tailscale.com/types/netmap"
	"tailscale.com/wgengine"
	"tailscale.com/wgengine/filter"
	"tailscale.com/wgengine/router"
	"tailscale.com/wgengine/wgcfg"
)

type fixture struct {
	host     *vpn.Host
	cancel   context.CancelFunc
	peer     *device.Device
	packets  *vpn.PacketDevice
	echoed   atomic.Int64
	workers  sync.WaitGroup
	system   *tsd.System
	engine   wgengine.Engine
	peerKey  key.NodePublic
	received atomic.Int64
}

var active struct {
	sync.Mutex
	value *fixture
}

func start(hostID uint64) (err error) {
	host, err := native.Lookup(hostID)
	if err != nil {
		return err
	}
	ctx, cancel := context.WithCancel(context.Background())
	f := &fixture{host: host, cancel: cancel}
	active.Lock()
	if active.value != nil {
		active.Unlock()
		cancel()
		return fmt.Errorf("fixture already running")
	}
	active.value = f
	active.Unlock()
	defer func() {
		if err != nil {
			closeFixture()
		}
	}()
	logf := func(string, ...any) {}
	system := tsd.NewSystem()
	f.system = system
	resources, err := host.Build(ctx, system, logf)
	if err != nil {
		return err
	}
	f.engine = resources.Engine
	local, peer := key.NewNode(), key.NewNode()
	f.packets, err = vpn.NewPacketDevice(vpn.PacketOptions{Name: "loopback-echo", MTU: 1280})
	if err != nil {
		return err
	}
	f.peer = device.NewDevice(f.packets, &peerBind{monitor: resources.Monitor}, device.NewLogger(device.LogLevelError, ""))
	if err = f.peer.IpcSet(fmt.Sprintf("private_key=%s\nlisten_port=0\nreplace_peers=true\npublic_key=%s\nendpoint=127.0.0.1:%d\nreplace_allowed_ips=true\nallowed_ip=100.64.10.1/32\n", peer.UntypedHexString(), local.Public().UntypedHexString(), system.MagicSock.Get().LocalPort())); err != nil {
		return err
	}
	if err = f.peer.Up(); err != nil {
		return err
	}
	settings, err := f.peer.IpcGet()
	if err != nil {
		return err
	}
	port := ""
	for _, line := range strings.Split(settings, "\n") {
		if value, ok := strings.CutPrefix(line, "listen_port="); ok {
			port = value
		}
	}
	endpoint, err := netip.ParseAddrPort("127.0.0.1:" + port)
	if err != nil {
		return err
	}
	f.peerKey = peer.Public()
	address := netip.MustParsePrefix("100.64.10.1/32")
	target := netip.MustParsePrefix("100.64.10.2/32")
	selfNode := &tailcfg.Node{ID: 1, Key: local.Public(), Addresses: []netip.Prefix{address}, AllowedIPs: []netip.Prefix{address}}
	peerNode := &tailcfg.Node{ID: 2, Key: peer.Public(), Addresses: []netip.Prefix{target}, AllowedIPs: []netip.Prefix{target}, Endpoints: []netip.AddrPort{endpoint}, IsWireGuardOnly: true}
	world := &netmap.NetworkMap{NodeKey: local.Public(), SelfNode: selfNode.View(), Peers: []tailcfg.NodeView{peerNode.View()}}
	resources.Engine.SetNetworkMap(world)
	system.MagicSock.Get().SetNetworkMap(world.SelfNode, world.Peers)
	resources.Engine.SetFilter(filter.NewAllowAllForTest(logf))
	// The engine asks its owner which configured peer owns each destination.
	// Product integrations supply this through their network-map projection.
	if lookup, ok := resources.Engine.(interface {
		SetPeerByIPPacketFunc(func(netip.Addr) (key.NodePublic, bool))
	}); ok {
		lookup.SetPeerByIPPacketFunc(func(ip netip.Addr) (key.NodePublic, bool) { return peer.Public(), target.Contains(ip) })
	}
	err = resources.Engine.Reconfig(&wgcfg.Config{PrivateKey: local, Addresses: []netip.Prefix{address}, MTU: 1280,
		Peers: []wgcfg.Peer{{PublicKey: peer.Public(), AllowedIPs: []netip.Prefix{target}}}},
		&router.Config{LocalAddrs: []netip.Prefix{address}, Routes: []netip.Prefix{target}, NewMTU: 1280}, &dns.Config{})
	if err != nil {
		return err
	}
	system.Tun.Get().Start()
	f.workers.Add(1)
	go func() {
		defer f.workers.Done()
		for {
			packet, err := f.packets.Receive(ctx)
			if err != nil {
				return
			}
			f.received.Add(1)
			response := echo(packet)
			if response == nil {
				continue
			}
			if f.packets.Inject(ctx, response) != nil {
				return
			}
			f.echoed.Add(1)
		}
	}()
	return nil
}

func checksum(data []byte) uint16 {
	var sum uint32
	for len(data) > 1 {
		sum += uint32(data[0])<<8 | uint32(data[1])
		data = data[2:]
	}
	if len(data) > 0 {
		sum += uint32(data[0]) << 8
	}
	for sum>>16 != 0 {
		sum = (sum & 65535) + (sum >> 16)
	}
	return ^uint16(sum)
}
func echo(packet []byte) []byte {
	if len(packet) < 28 || packet[0]>>4 != 4 || (packet[9] != 1 && packet[9] != 17) {
		return nil
	}
	header := int(packet[0]&15) * 4
	if header < 20 || len(packet) < header+8 || (packet[9] == 1 && packet[header] != 8) || string(packet[16:20]) != string([]byte{100, 64, 10, 2}) {
		return nil
	}
	reply := append([]byte(nil), packet...)
	copy(reply[12:16], packet[16:20])
	copy(reply[16:20], packet[12:16])
	if packet[9] == 1 {
		reply[header] = 0
	} else {
		reply[header], reply[header+1] = packet[header+2], packet[header+3]
		reply[header+2], reply[header+3] = packet[header], packet[header+1]
	}
	reply[10] = 0
	reply[11] = 0
	sum := checksum(reply[:header])
	reply[10] = byte(sum >> 8)
	reply[11] = byte(sum)
	if packet[9] == 1 {
		reply[header+2] = 0
		reply[header+3] = 0
		sum = checksum(reply[header:])
		reply[header+2] = byte(sum >> 8)
		reply[header+3] = byte(sum)
	} else {
		reply[header+6] = 0
		reply[header+7] = 0
	}
	return reply
}
func closeFixture() {
	active.Lock()
	f := active.value
	active.value = nil
	active.Unlock()
	if f == nil {
		return
	}
	f.cancel()
	if f.peer != nil {
		f.peer.Close()
	}
	f.workers.Wait()
	_ = f.host.Close()
	if f.system != nil {
		f.system.Bus.Get().Close()
	}
}

//export oneui_fixture_dispatch_v1
func oneui_fixture_dispatch_v1(command C.int, input unsafe.Pointer, length C.int, output unsafe.Pointer, capacity C.int) C.int {
	if input == nil || length < 1 || length > 1024 || output == nil || capacity < 1024 {
		return -2
	}
	var request struct {
		Host string `json:"host"`
	}
	if json.Unmarshal(unsafe.Slice((*byte)(input), int(length)), &request) != nil {
		return -2
	}
	code := 0
	var echoed int64
	diagnostics := map[string]any{}
	switch int(command) {
	case 1:
		id, err := strconv.ParseUint(request.Host, 10, 64)
		if err != nil || start(id) != nil {
			code = -4
		}
	case 2:
		closeFixture()
	case 3:
		active.Lock()
		if active.value != nil {
			active.value.cancel()
		}
		active.Unlock()
	case 4:
		active.Lock()
		current := active.value
		active.Unlock()
		if current != nil {
			echoed = current.echoed.Load()
			diagnostics["received"] = current.received.Load()
			if peer, ok := current.engine.PeerByKey(current.peerKey); ok {
				diagnostics["clientRx"] = peer.RxBytes()
				diagnostics["clientTx"] = peer.TxBytes()
				diagnostics["clientHandshake"] = !peer.LastHandshake().IsZero()
			}
			if config, err := current.peer.IpcGet(); err == nil {
				for _, line := range strings.Split(config, "\n") {
					name, value, ok := strings.Cut(line, "=")
					if ok && (name == "rx_bytes" || name == "tx_bytes" || name == "last_handshake_time_sec") {
						diagnostics[name] = value
					}
				}
			}
		}
	default:
		code = -2
	}
	diagnostics["code"] = code
	diagnostics["echoed"] = echoed
	result, _ := json.Marshal(diagnostics)
	copy(unsafe.Slice((*byte)(output), int(capacity)), result)
	return C.int(len(result))
}
func main() {}
