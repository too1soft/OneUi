// Package oneuivpn provides the renderer-independent packet boundary between
// native VPN hosts and packet engines. No account, control service or product
// network identifiers enter this package.
package oneuivpn

import (
	"context"
	"errors"
	"io"
	"os"
	"sync"
	"sync/atomic"

	"github.com/tailscale/wireguard-go/tun"
)

var ErrClosed = errors.New("OneUI VPN packet device closed")
var ErrInvalidPacket = errors.New("invalid OneUI VPN packet")

type PacketOptions struct {
	Name          string
	MTU           int
	QueueDepth    int
	MaxPacketSize int
}

// PacketDevice is a real userspace TUN implementation: native hosts supply
// outbound IP packets with Inject and drain decrypted packets with Receive.
// The bounded queues apply backpressure. Close wakes every blocked operation.
// Native OS descriptors remain owned by the Android/HarmonyOS host; iOS uses
// public NEPacketTunnelFlow rather than a private utun descriptor.
type PacketDevice struct {
	name      string
	mtu       int
	maxPacket int
	incoming  chan []byte
	outgoing  chan []byte
	done      chan struct{}
	events    chan tun.Event
	closed    atomic.Bool
	once      sync.Once
}

var _ tun.Device = (*PacketDevice)(nil)

func NewPacketDevice(options PacketOptions) (*PacketDevice, error) {
	if options.Name == "" || len(options.Name) > 128 || options.MTU < 1280 || options.MTU > 9000 {
		return nil, errors.New("invalid OneUI VPN packet device options")
	}
	if options.QueueDepth == 0 {
		options.QueueDepth = 16
	}
	if options.MaxPacketSize == 0 {
		options.MaxPacketSize = 65535
	}
	if options.QueueDepth < 1 || options.QueueDepth > 64 || options.MaxPacketSize < options.MTU || options.MaxPacketSize > 65535 {
		return nil, errors.New("invalid OneUI VPN packet queue limits")
	}
	d := &PacketDevice{name: options.Name, mtu: options.MTU, maxPacket: options.MaxPacketSize,
		incoming: make(chan []byte, options.QueueDepth), outgoing: make(chan []byte, options.QueueDepth),
		done: make(chan struct{}), events: make(chan tun.Event, 2)}
	d.events <- tun.EventUp
	return d, nil
}

func (d *PacketDevice) send(ctx context.Context, queue chan []byte, packet []byte) error {
	if d.closed.Load() {
		return ErrClosed
	}
	if len(packet) == 0 || len(packet) > d.maxPacket {
		return ErrInvalidPacket
	}
	if err := ctx.Err(); err != nil {
		return err
	}
	copy := append([]byte(nil), packet...)
	select {
	case <-d.done:
		return ErrClosed
	case <-ctx.Done():
		return ctx.Err()
	case queue <- copy:
		if d.closed.Load() {
			return ErrClosed
		}
		return nil
	}
}

func (d *PacketDevice) receive(ctx context.Context, queue chan []byte) ([]byte, error) {
	if d.closed.Load() {
		return nil, ErrClosed
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	select {
	case <-d.done:
		return nil, ErrClosed
	case <-ctx.Done():
		return nil, ctx.Err()
	case packet := <-queue:
		if d.closed.Load() {
			return nil, ErrClosed
		}
		return packet, nil
	}
}

// Inject copies a packet read by the native OS host into the encrypted engine.
func (d *PacketDevice) Inject(ctx context.Context, packet []byte) error {
	return d.send(ctx, d.incoming, packet)
}

// Receive transfers ownership of one decrypted packet to the native OS host.
func (d *PacketDevice) Receive(ctx context.Context) ([]byte, error) {
	return d.receive(ctx, d.outgoing)
}

func (d *PacketDevice) Read(buffers [][]byte, sizes []int, offset int) (int, error) {
	if len(buffers) == 0 || len(sizes) < len(buffers) || offset < 0 || offset > len(buffers[0]) {
		return 0, io.ErrShortBuffer
	}
	packet, err := d.receive(context.Background(), d.incoming)
	if err != nil {
		return 0, err
	}
	if len(buffers[0])-offset < len(packet) {
		return 0, io.ErrShortBuffer
	}
	sizes[0] = copy(buffers[0][offset:], packet)
	return 1, nil
}

func (d *PacketDevice) Write(buffers [][]byte, offset int) (int, error) {
	for i, buffer := range buffers {
		if offset < 0 || offset >= len(buffer) {
			return i, io.ErrShortBuffer
		}
		if err := d.send(context.Background(), d.outgoing, buffer[offset:]); err != nil {
			return i, err
		}
	}
	return len(buffers), nil
}

func (d *PacketDevice) File() *os.File           { return nil }
func (d *PacketDevice) Name() (string, error)    { return d.name, nil }
func (d *PacketDevice) MTU() (int, error)        { return d.mtu, nil }
func (d *PacketDevice) Events() <-chan tun.Event { return d.events }
func (d *PacketDevice) BatchSize() int           { return 1 }
func (d *PacketDevice) Close() error {
	d.once.Do(func() {
		d.closed.Store(true)
		close(d.done)
		d.events <- tun.EventDown
		close(d.events)
	})
	return nil
}
