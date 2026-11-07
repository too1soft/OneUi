package net.too1.oneui.vpntest;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicReference;

/** Test-only bounded TCP echo on an OS VPN address; no native packet injection. */
public final class KernelTcpEcho implements AutoCloseable {
    private final ServerSocket listener = new ServerSocket();
    private final AtomicReference<Socket> active = new AtomicReference<>();
    private final Thread worker;
    private volatile boolean closed;
    private volatile long echoed;
    private volatile int exchanges;
    private final String allowedSource;
    private final int maximumExchanges;

    public KernelTcpEcho(String localIpv4, String allowedSource) throws Exception {
        this(localIpv4, allowedSource, 4);
    }
    public KernelTcpEcho(String localIpv4, String allowedSource, int maximumExchanges) throws Exception {
        if (maximumExchanges < 1 || maximumExchanges > 256) throw new IllegalArgumentException("Bounded echo exchanges");
        this.maximumExchanges = maximumExchanges;
        this.allowedSource = allowedSource;
        listener.bind(new InetSocketAddress(InetAddress.getByName(localIpv4), 0));
        listener.setSoTimeout(500);
        worker = new Thread(this::serve, "OneUI-kernel-echo");
        worker.setDaemon(true);
        worker.start();
    }

    private void serve() {
        while (!closed && exchanges < maximumExchanges) {
            try (Socket socket = listener.accept()) {
                active.set(socket);
                if (!allowedSource.equals(socket.getInetAddress().getHostAddress()))
                    continue;
                socket.setSoTimeout(5000);
                DataInputStream input = new DataInputStream(socket.getInputStream());
                int length = input.readInt();
                if (length < 1 || length > 4 * 1024 * 1024)
                    continue;
                byte[] payload = new byte[length];
                input.readFully(payload);
                DataOutputStream output = new DataOutputStream(socket.getOutputStream());
                output.writeInt(length);
                output.write(payload);
                output.flush();
                echoed += length;
                exchanges++;
            } catch (java.io.IOException ignored) {
                // Timeout/close is bounded; failed transfers are never counted.
            } finally {
                active.set(null);
            }
        }
    }

    public int port() { return listener.getLocalPort(); }
    public long echoedBytes() { return echoed; }
    public int exchanges() { return exchanges; }

    @Override
    public void close() throws Exception {
        closed = true;
        listener.close();
        Socket socket = active.get();
        if (socket != null)
            socket.close();
        worker.join(2000);
        if (worker.isAlive())
            throw new IllegalStateException("Kernel echo worker did not close");
    }
}
