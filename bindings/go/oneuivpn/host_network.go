package oneuivpn

import (
	"context"
	"errors"
	"strings"
	"tailscale.com/net/dns"
	"time"
)

type nativeRefreshState struct {
	identity, linkType                   string
	requestedAt, lastRequest, lastRebind time.Time
	changes, rebinds                     uint64
	lost                                 bool
	cancel                               context.CancelFunc
	stopping                             bool
}

func (h *Host) readUnderlayLocked() (nativeNetworkState, error) {
	read := h.network.options.ReadNetworkState
	if read == nil {
		return nativeNetworkState{}, nil
	}
	raw, err := read()
	if err != nil {
		return nativeNetworkState{}, err
	}
	return decodeNetworkState(raw)
}
func (h *Host) initializeNetworkLocked() {
	if state, err := h.readUnderlayLocked(); err == nil {
		h.refresh.identity, h.refresh.linkType = state.underlayID, state.underlayType
	}
}
func (h *Host) stopNetworkRefreshLocked() {
	h.refresh.stopping = true
	if h.refresh.cancel != nil {
		h.refresh.cancel()
		h.refresh.cancel = nil
	}
}

// RefreshConnectivity renews discovery without replacing the tunnel or identity.
// A user-requested retry also reopens protected UDP sockets on the current link.
func (h *Host) RefreshConnectivity() error { return h.networkChanged(true) }
func (h *Host) NetworkChanged() error      { return h.networkChanged(false) }
func (h *Host) networkChanged(explicit bool) error {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.closed || h.refresh.stopping {
		return ErrClosed
	}
	if err := refreshNativeNetwork(); err != nil {
		return err
	}
	state, err := h.readUnderlayLocked()
	if err != nil {
		return err
	}
	now := time.Now()
	changed := state.underlayID != "" && h.refresh.identity != "" &&
		(state.underlayID != h.refresh.identity || h.refresh.lost)
	if changed {
		h.refresh.changes++
	}
	if state.underlayID != "" {
		h.refresh.identity = state.underlayID
	}
	h.refresh.linkType = state.underlayType
	h.refresh.lost = state.defaultInterface == "" && h.refresh.identity != ""
	if h.refresh.lost {
		h.refresh.requestedAt = now
		if h.refresh.cancel != nil {
			h.refresh.cancel()
			h.refresh.cancel = nil
		}
	}
	if h.resources.Monitor != nil {
		h.resources.Monitor.InjectEvent()
	}
	if fn := h.resources.RefreshTransport; fn != nil && (changed || explicit || now.Sub(h.refresh.lastRequest) >= time.Second) {
		force := changed || (explicit && now.Sub(h.refresh.lastRebind) >= 2*time.Second)
		if force {
			h.refresh.rebinds++
			h.refresh.lastRebind = now
		}
		h.refresh.lastRequest = now
		if force {
			h.refresh.requestedAt = now
		}
		fn(force)
		if force {
			h.scheduleDiscoveryLocked(fn)
		}
	}
	// DNS renewals may not change an interface address.
	if h.resources.DNS != nil {
		if err := h.resources.DNS.RecompileDNSConfig(); err != nil && !errors.Is(err, dns.ErrNoDNSConfig) {
			return err
		}
	}
	return nil
}
func (h *Host) scheduleDiscoveryLocked(refresh func(bool)) {
	if h.refresh.cancel != nil {
		h.refresh.cancel()
	}
	ctx, cancel := context.WithCancel(context.Background())
	h.refresh.cancel = cancel
	go func() {
		// A handover's first probe can run before the new link has settled.
		// Three bounded follow-ups; a newer generation or Close cancels them.
		for _, delay := range []time.Duration{time.Second, 2 * time.Second, 4 * time.Second} {
			timer := time.NewTimer(delay)
			select {
			case <-ctx.Done():
				timer.Stop()
				return
			case <-timer.C:
			}
			h.mu.Lock()
			if ctx.Err() != nil || h.closed || h.refresh.stopping {
				h.mu.Unlock()
				return
			}
			refresh(false)
			h.mu.Unlock()
		}
	}()
}
func (h *Host) Connectivity() ConnectivityReport {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.closed || h.refresh.stopping || h.resources.Connectivity == nil {
		return ConnectivityReport{}
	}
	r := h.resources.Connectivity()
	r.UnderlayType = strings.TrimSpace(h.refresh.linkType)
	r.NetworkChanges, r.SocketRebinds = h.refresh.changes, h.refresh.rebinds
	if r.MeasuredAtUnixMS < h.refresh.requestedAt.UnixMilli() {
		r.Stale = true
	}
	return r
}
