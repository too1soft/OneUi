package oneuivpn

import (
	"bytes"
	"context"
	"errors"
	"io"
	"testing"
	"time"
)

func packetDevice(t *testing.T) *PacketDevice {
	t.Helper()
	d, err := NewPacketDevice(PacketOptions{Name: "native-test", MTU: 1280, QueueDepth: 1})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = d.Close() })
	return d
}

func TestPacketOwnershipAndOffsets(t *testing.T) {
	d := packetDevice(t)
	ctx := context.Background()
	input := []byte{0x45, 1, 2, 3}
	if err := d.Inject(ctx, input); err != nil {
		t.Fatal(err)
	}
	input[1] = 99
	buffer, sizes := make([]byte, 32), make([]int, 1)
	n, err := d.Read([][]byte{buffer}, sizes, 16)
	if err != nil || n != 1 || sizes[0] != 4 || !bytes.Equal(buffer[16:20], []byte{0x45, 1, 2, 3}) {
		t.Fatalf("read %d %v sizes=%v packet=%x", n, err, sizes, buffer)
	}
	if _, err := d.Write([][]byte{buffer[:20]}, 16); err != nil {
		t.Fatal(err)
	}
	buffer[17] = 88
	output, err := d.Receive(ctx)
	if err != nil || !bytes.Equal(output, []byte{0x45, 1, 2, 3}) {
		t.Fatalf("receive %x %v", output, err)
	}
	if _, err := d.Read(nil, nil, 0); !errors.Is(err, io.ErrShortBuffer) {
		t.Fatal(err)
	}
	if _, err := d.Write([][]byte{{1}}, -1); !errors.Is(err, io.ErrShortBuffer) {
		t.Fatal(err)
	}
}

func TestBackpressureAndCancellation(t *testing.T) {
	d := packetDevice(t)
	if err := d.Inject(context.Background(), []byte{1}); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Millisecond)
	defer cancel()
	if err := d.Inject(ctx, []byte{2}); !errors.Is(err, context.DeadlineExceeded) {
		t.Fatal(err)
	}
	if err := d.Inject(context.Background(), nil); !errors.Is(err, ErrInvalidPacket) {
		t.Fatal(err)
	}
	if err := d.Inject(context.Background(), make([]byte, 65536)); !errors.Is(err, ErrInvalidPacket) {
		t.Fatal(err)
	}
}

func TestCloseWakesNativeAndEngineOperations(t *testing.T) {
	for _, operation := range []string{"native-read", "native-write", "engine-read", "engine-write"} {
		t.Run(operation, func(t *testing.T) {
			d := packetDevice(t)
			if operation == "native-write" {
				_ = d.Inject(context.Background(), []byte{1})
			}
			if operation == "engine-write" {
				_, _ = d.Write([][]byte{{1}}, 0)
			}
			result := make(chan error, 1)
			go func() {
				var err error
				switch operation {
				case "native-read":
					_, err = d.Receive(context.Background())
				case "native-write":
					err = d.Inject(context.Background(), []byte{2})
				case "engine-read":
					_, err = d.Read([][]byte{make([]byte, 1280)}, []int{0}, 0)
				case "engine-write":
					_, err = d.Write([][]byte{{2}}, 0)
				}
				result <- err
			}()
			_ = d.Close()
			_ = d.Close()
			select {
			case err := <-result:
				if !errors.Is(err, ErrClosed) {
					t.Fatal(err)
				}
			case <-time.After(time.Second):
				t.Fatal("Close did not release blocked packet operation")
			}
		})
	}
}
