package net.too1.oneui.vpn;

import android.os.ParcelFileDescriptor;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructPollfd;
import java.io.Closeable;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.IntConsumer;

/** Bounded, bidirectional IP transport shared by every Android VPN product.
 * Owns the supplied duplicate descriptor. Port calls must honor their timeout.
 * Failure is delivered from a worker; dispatch teardown to the owner thread. */
public final class OneUiPacketPump implements Closeable {
    public interface Port {
        int inject(byte[] packet, int length, int timeoutMs);
        int receive(byte[] destination, int timeoutMs);
    }
    private final ParcelFileDescriptor descriptor;
    private final Port port;
    private final IntConsumer failure;
    private final AtomicBoolean running = new AtomicBoolean(true);
    private final AtomicBoolean failed = new AtomicBoolean();
    private final Thread reader, writer;
    private boolean closed;

    /** Takes ownership on success only; the caller closes fd if this throws. */
    public OneUiPacketPump(int fd, Port port, IntConsumer failure) throws IOException {
        if (fd < 0 || port == null || failure == null) throw new IllegalArgumentException("Packet transport");
        this.port = port;
        this.failure = failure;
        descriptor = ParcelFileDescriptor.adoptFd(fd);
        try {
            int flags = Os.fcntlInt(descriptor.getFileDescriptor(), OsConstants.F_GETFL, 0);
            Os.fcntlInt(descriptor.getFileDescriptor(), OsConstants.F_SETFL, flags | OsConstants.O_NONBLOCK);
        } catch (ErrnoException error) {
            descriptor.detachFd(); // Ownership remains with the caller on failure.
            throw new IOException("Packet descriptor setup failed", error);
        }
        reader = new Thread(this::readLoop, "OneUI-VPN-read");
        writer = new Thread(this::writeLoop, "OneUI-VPN-write");
        reader.start();
        writer.start();
    }
    private boolean ready(short events) throws ErrnoException {
        StructPollfd item = new StructPollfd();
        item.fd = descriptor.getFileDescriptor();
        item.events = events;
        if (Os.poll(new StructPollfd[]{item}, 200) == 0) return false;
        if ((item.revents & (OsConstants.POLLERR | OsConstants.POLLHUP | OsConstants.POLLNVAL)) != 0)
            throw new ErrnoException("VPN poll", OsConstants.EIO);
        return (item.revents & events) != 0;
    }
    private void readLoop() {
        byte[] packet = new byte[65535];
        try {
            while (running.get()) {
                if (!ready((short)OsConstants.POLLIN)) continue;
                int length;
                try { length = Os.read(descriptor.getFileDescriptor(), packet, 0, packet.length); }
                catch (ErrnoException error) { if (error.errno == OsConstants.EAGAIN || error.errno == OsConstants.EINTR) continue; throw error; }
                if (length <= 0) throw new IOException("VPN packet stream closed");
                int result;
                do { result = port.inject(packet, length, 100); }
                while (running.get() && result == -3);
                if (running.get() && result < 0) throw new IOException("VPN engine input failed");
            }
        } catch (Exception error) { fail(); }
        finally { java.util.Arrays.fill(packet, (byte)0); }
    }
    private void writeLoop() {
        byte[] packet = new byte[65535];
        try {
            while (running.get()) {
                int length = port.receive(packet, 100);
                if (length == -3) continue;
                if (length <= 0 || length > packet.length) throw new IOException("VPN engine output failed");
                while (running.get()) {
                    if (!ready((short)OsConstants.POLLOUT)) continue;
                    try {
                        if (Os.write(descriptor.getFileDescriptor(), packet, 0, length) != length)
                            throw new IOException("Partial VPN packet write");
                        break;
                    } catch (ErrnoException error) {
                        if (error.errno != OsConstants.EAGAIN && error.errno != OsConstants.EINTR) throw error;
                    }
                }
            }
        } catch (Exception error) { fail(); }
        finally { java.util.Arrays.fill(packet, (byte)0); }
    }
    private void fail() {
        if (running.getAndSet(false) && failed.compareAndSet(false, true)) failure.accept(1);
    }
    @Override public synchronized void close() throws IOException {
        if (closed) return;
        if (Thread.currentThread() == reader || Thread.currentThread() == writer)
            throw new IllegalStateException("Close packet transport on its owner thread");
        running.set(false);
        boolean interrupted = false;
        for (Thread thread : new Thread[]{reader, writer}) {
            while (thread.isAlive()) {
                try { thread.join(); } catch (InterruptedException error) { interrupted = true; }
            }
        }
        descriptor.close();
        closed = true;
        if (interrupted) Thread.currentThread().interrupt();
    }
}
