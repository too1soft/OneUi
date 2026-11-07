package oneuivpn

import (
	"context"
	"errors"
	"sync"

	"github.com/tailscale/wireguard-go/tun"
	"tailscale.com/net/dns"
	"tailscale.com/net/netmon"
	"tailscale.com/tsd"
	"tailscale.com/types/logger"
	"tailscale.com/wgengine/router"
)

// Host owns one native VPN generation. Reconnection creates a fresh Host; an
// old handle can never inject into a new tunnel. Products supply their protocol
// identity separately and never put private keys in OS VPN configuration.
type Host struct {
	mu             sync.Mutex
	device         *PacketDevice
	network        *NetworkAdapter
	protect        func(int) error
	releaseProtect func()
	resources      EngineResources
	started        bool
	closed         bool
	refresh        nativeRefreshState
}

func NewHost(options NetworkOptions, protect func(int) error) (*Host, error) {
	network, err := NewNetworkAdapter(options)
	if err != nil {
		return nil, err
	}
	device, err := NewPacketDevice(PacketOptions{Name: options.Label, MTU: options.MTU})
	if err != nil {
		return nil, err
	}
	return &Host{network: network, device: device, protect: protect}, nil
}

func (h *Host) Inject(ctx context.Context, packet []byte) error { return h.device.Inject(ctx, packet) }
func (h *Host) Receive(ctx context.Context) ([]byte, error)     { return h.device.Receive(ctx) }

func (h *Host) Build(ctx context.Context, system *tsd.System, logf logger.Logf) (EngineResources, error) {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.closed {
		return EngineResources{}, ErrClosed
	}
	if h.started {
		return EngineResources{}, errors.New("native VPN host is single use")
	}
	h.started = true
	release, err := installSocketProtector(h.protect, h.network.options.ReadNetworkState)
	if err != nil {
		return EngineResources{}, err
	}
	h.releaseProtect = release
	resources, err := BuildEngine(ctx, system, logf, Platform{
		OpenTunnel: func(context.Context) (tun.Device, error) { return h.device, nil },
		NewRouter:  func(tun.Device, *netmon.Monitor, *tsd.System) (router.Router, error) { return h.network, nil },
		NewDNS:     func(string, *tsd.System) (dns.OSConfigurator, error) { return h.network, nil },
	})
	if err != nil {
		release()
		h.releaseProtect = nil
		return EngineResources{}, err
	}
	h.resources = resources
	h.initializeNetworkLocked()
	return resources, nil
}

// Close waits for in-flight network callbacks and wakes blocked packet calls.
// Native callback context must remain alive until this method returns.
func (h *Host) Close() error {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.closed {
		return nil
	}
	h.stopNetworkRefreshLocked()
	_ = h.device.Close()
	if h.resources.Engine != nil {
		h.resources.Engine.Close()
	}
	if h.resources.Monitor != nil {
		h.resources.Monitor.Close()
	}
	if h.releaseProtect != nil {
		h.releaseProtect()
		h.releaseProtect = nil
	}
	if err := h.network.Close(); err != nil {
		return err
	}
	h.closed = true
	return nil
}
