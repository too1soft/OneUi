package main

import (
	"context"
	"encoding/binary"
	"fmt"
	"net"
	"net/netip"
	"sync"

	"github.com/tailscale/wireguard-go/conn"
	"tailscale.com/net/netmon"
	"tailscale.com/net/netns"
)

// The in-process echo peer is also a VPN application's transport socket. Use
// the SDK-installed protect hook so its replies cannot re-enter the test VPN.
type peerBind struct {
	mu      sync.Mutex
	socket  *net.UDPConn
	monitor *netmon.Monitor
}

func (b *peerBind) Open(port uint16) ([]conn.ReceiveFunc, uint16, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.socket != nil {
		return nil, 0, conn.ErrBindAlreadyOpen
	}
	listener := netns.Listener(func(string, ...any) {}, b.monitor)
	value, err := listener.ListenPacket(context.Background(), "udp4", fmt.Sprintf("127.0.0.1:%d", port))
	if err != nil {
		return nil, 0, err
	}
	socket := value.(*net.UDPConn)
	b.socket = socket
	receive := func(buffers [][]byte, sizes []int, endpoints []conn.Endpoint) (int, error) {
		size, address, err := socket.ReadFromUDPAddrPort(buffers[0])
		if err != nil {
			return 0, err
		}
		sizes[0] = size
		endpoints[0] = peerEndpoint{address}
		return 1, nil
	}
	return []conn.ReceiveFunc{receive}, uint16(socket.LocalAddr().(*net.UDPAddr).Port), nil
}
func (b *peerBind) Close() error {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.socket == nil {
		return nil
	}
	err := b.socket.Close()
	b.socket = nil
	return err
}
func (b *peerBind) SetMark(uint32) error { return nil }
func (b *peerBind) BatchSize() int       { return 1 }
func (b *peerBind) ParseEndpoint(text string) (conn.Endpoint, error) {
	value, err := netip.ParseAddrPort(text)
	if err != nil {
		return nil, err
	}
	if !value.Addr().IsLoopback() {
		return nil, fmt.Errorf("fixture peer must be local")
	}
	return peerEndpoint{value}, nil
}
func (b *peerBind) Send(buffers [][]byte, endpoint conn.Endpoint, offset int) error {
	b.mu.Lock()
	socket := b.socket
	b.mu.Unlock()
	if socket == nil {
		return net.ErrClosed
	}
	target := endpoint.(peerEndpoint).address
	for _, buffer := range buffers {
		if _, err := socket.WriteToUDPAddrPort(buffer[offset:], target); err != nil {
			return err
		}
	}
	return nil
}

type peerEndpoint struct{ address netip.AddrPort }

func (e peerEndpoint) ClearSrc()           {}
func (e peerEndpoint) SrcToString() string { return "127.0.0.1" }
func (e peerEndpoint) DstToString() string { return e.address.String() }
func (e peerEndpoint) DstIP() netip.Addr   { return e.address.Addr() }
func (e peerEndpoint) SrcIP() netip.Addr   { return netip.MustParseAddr("127.0.0.1") }
func (e peerEndpoint) DstToBytes() []byte {
	value := e.address.Addr().As4()
	result := append([]byte(nil), value[:]...)
	return binary.BigEndian.AppendUint16(result, e.address.Port())
}
