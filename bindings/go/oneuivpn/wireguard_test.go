package oneuivpn

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/netip"
	"strings"
	"testing"
	"time"

	"github.com/tailscale/wireguard-go/conn"
	"github.com/tailscale/wireguard-go/device"
	"github.com/tailscale/wireguard-go/tun/tuntest"
	"golang.org/x/crypto/curve25519"
)

// Uses real loopback UDP sockets and the production WireGuard handshake/cipher.
// No privileged TUN, desktop routes, accounts or production peers are touched.
func TestEncryptedPacketRoundTrip(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 8*time.Second)
	defer cancel()
	var hosts [2]*PacketDevice
	var engines [2]*device.Device
	var privateKeys, publicKeys [2]string
	for i := range hosts {
		key := make([]byte, 32)
		if _, err := rand.Read(key); err != nil {
			t.Fatal(err)
		}
		pub, err := curve25519.X25519(key, curve25519.Basepoint)
		if err != nil {
			t.Fatal(err)
		}
		privateKeys[i], publicKeys[i] = hex.EncodeToString(key), hex.EncodeToString(pub)
		hosts[i], err = NewPacketDevice(PacketOptions{Name: fmt.Sprintf("native-%d", i), MTU: 1280})
		if err != nil {
			t.Fatal(err)
		}
		engines[i] = device.NewDevice(hosts[i], conn.NewStdNetBind(), device.NewLogger(device.LogLevelError, ""))
		t.Cleanup(engines[i].Close)
	}
	var ports [2]string
	for i, engine := range engines {
		if err := engine.IpcSet(fmt.Sprintf("private_key=%s\nlisten_port=0\nreplace_peers=true\npublic_key=%s\nreplace_allowed_ips=true\nallowed_ip=100.64.0.%d/32\n", privateKeys[i], publicKeys[i^1], (i^1)+1)); err != nil {
			t.Fatal(err)
		}
		if err := engine.Up(); err != nil {
			t.Fatal(err)
		}
		config, err := engine.IpcGet()
		if err != nil {
			t.Fatal(err)
		}
		for _, line := range strings.Split(config, "\n") {
			if p, ok := strings.CutPrefix(line, "listen_port="); ok {
				ports[i] = p
			}
		}
		if ports[i] == "" || ports[i] == "0" {
			t.Fatal("WireGuard did not bind a UDP port")
		}
	}
	for i, engine := range engines {
		if err := engine.IpcSet(fmt.Sprintf("public_key=%s\nendpoint=127.0.0.1:%s\n", publicKeys[i^1], ports[i^1])); err != nil {
			t.Fatal(err)
		}
	}
	for _, sender := range []int{0, 1} {
		source := netip.MustParseAddr(fmt.Sprintf("100.64.0.%d", sender+1))
		target := netip.MustParseAddr(fmt.Sprintf("100.64.0.%d", (sender^1)+1))
		packet := tuntest.Ping(target, source)
		if err := hosts[sender].Inject(ctx, packet); err != nil {
			t.Fatal(err)
		}
		received, err := hosts[sender^1].Receive(ctx)
		if err != nil {
			t.Fatal(err)
		}
		if !bytes.Equal(received, packet) {
			t.Fatal("decrypted native packet differs from sent IP packet")
		}
	}
}
