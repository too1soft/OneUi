package oneuivpn

import (
	"context"
	"errors"
	"fmt"

	"github.com/tailscale/wireguard-go/tun"
	"tailscale.com/net/dns"
	"tailscale.com/net/netmon"
	"tailscale.com/tsd"
	"tailscale.com/types/logger"
	"tailscale.com/wgengine"
	"tailscale.com/wgengine/router"
)

// Platform supplies native system services. In particular, Router and DNS must
// acknowledge real native updates; a successful no-op is not a mobile adapter.
// The optional engine dependency stays in this Go SDK, outside the UI runtime.
type Platform struct {
	OpenTunnel func(context.Context) (tun.Device, error)
	NewRouter  func(tun.Device, *netmon.Monitor, *tsd.System) (router.Router, error)
	NewDNS     func(string, *tsd.System) (dns.OSConfigurator, error)
}

type EngineResources struct {
	Engine           wgengine.Engine
	Monitor          *netmon.Monitor
	DeviceName       string
	DNS              *dns.Manager
	RefreshTransport func(rebind bool)
	Connectivity     func() ConnectivityReport
}

// BuildEngine assembles a real userspace WireGuard engine around a native
// platform. It never opens a privileged desktop TUN or edits host routes itself.
// Ownership transfers to Engine on success; every partial failure rolls back.
func BuildEngine(ctx context.Context, system *tsd.System, logf logger.Logf, platform Platform) (_ EngineResources, resultErr error) {
	if system == nil || logf == nil || platform.OpenTunnel == nil || platform.NewRouter == nil || platform.NewDNS == nil {
		return EngineResources{}, errors.New("OneUI VPN native services are required")
	}
	if err := ctx.Err(); err != nil {
		return EngineResources{}, err
	}
	monitor, err := netmon.New(system.Bus.Get(), logf)
	if err != nil {
		return EngineResources{}, fmt.Errorf("OneUI network monitor: %w", err)
	}
	system.Set(monitor)
	defer func() {
		if resultErr != nil {
			monitor.Close()
		}
	}()
	device, err := platform.OpenTunnel(ctx)
	if err != nil {
		return EngineResources{}, fmt.Errorf("OneUI native TUN: %w", err)
	}
	if device == nil {
		return EngineResources{}, errors.New("OneUI native TUN is nil")
	}
	owned := true
	defer func() {
		if owned {
			_ = device.Close()
		}
	}()
	name, err := device.Name()
	if err != nil {
		return EngineResources{}, fmt.Errorf("OneUI TUN name: %w", err)
	}
	routes, err := platform.NewRouter(device, monitor, system)
	if err != nil {
		return EngineResources{}, fmt.Errorf("OneUI route adapter: %w", err)
	}
	if routes == nil {
		return EngineResources{}, errors.New("OneUI route adapter is nil")
	}
	defer func() {
		if owned {
			_ = routes.Close()
		}
	}()
	resolver, err := platform.NewDNS(name, system)
	if err != nil {
		return EngineResources{}, fmt.Errorf("OneUI DNS adapter: %w", err)
	}
	if resolver == nil {
		return EngineResources{}, errors.New("OneUI DNS adapter is nil")
	}
	defer func() {
		if owned {
			_ = resolver.Close()
		}
	}()
	if err := ctx.Err(); err != nil {
		return EngineResources{}, err
	}
	system.Set(routes)
	core, err := wgengine.NewUserspaceEngine(logf, wgengine.Config{
		Tun: device, Router: routes, DNS: resolver, NetMon: monitor,
		HealthTracker: system.HealthTracker.Get(), Metrics: system.UserMetricsRegistry(),
		ControlKnobs: system.ControlKnobs(), EventBus: system.Bus.Get(), SetSubsystem: system.Set,
	})
	if err != nil {
		return EngineResources{}, fmt.Errorf("OneUI packet engine: %w", err)
	}
	if err := ctx.Err(); err != nil {
		core.Close()
		owned = false // NewUserspaceEngine owns device/router/DNS after success.
		return EngineResources{}, err
	}
	owned = false
	magic := system.MagicSock.Get()
	return EngineResources{Engine: core, Monitor: monitor, DeviceName: name, DNS: system.DNSManager.Get(),
		RefreshTransport: func(rebind bool) {
			if rebind {
				magic.Rebind()
			}
			magic.ReSTUN("native-underlay-refresh")
		},
		Connectivity: func() ConnectivityReport {
			return connectivityReport(magic.GetLastNetcheckReport(context.Background()))
		},
	}, nil
}
