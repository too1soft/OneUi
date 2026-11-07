package net.too1.oneui.vpn;

import android.os.ParcelFileDescriptor;
import android.content.Context;
import java.io.IOException;
import org.json.JSONObject;

/** Reusable lifecycle connecting native packet transport, OS network updates,
 * secure blobs and an application protocol. Products supply Protocol only. */
public final class OneUiNativeVpnEngine implements OneUiVpnService.Engine {
    public interface Protocol {
        /** Blocking worker call; returns only after actual encrypted readiness. */
        void start(long oneuiHost) throws Exception;
        /** Must wake a running start, including cancellation before creation. */
        void cancel();
        /** Called after the start worker has joined. Must be idempotent. */
        void close() throws Exception;
    }
    public interface TunnelControl { int apply(String networkSettings) throws Exception; }
    private final Object lock = new Object();
    private final String label;
    private final String backendLibrary;
    private final int mtu;
    private final boolean defaultRoute;
    private final Protocol protocol;
    private final TunnelControl tunnelControl;
    private final OneUiSecureBlobStore storage;
    private final Context context;
    private OneUiNetworkState.Observer observer;
    private volatile boolean active;
    private OneUiNativePacketPort port;
    private OneUiPacketPump pump;
    private OneUiVpnService.EngineEvents events;
    private Thread worker;
    private boolean protocolClosed;

    public OneUiNativeVpnEngine(Context context, String backendLibrary, String label, int mtu, boolean defaultRoute, Protocol protocol,
            TunnelControl tunnelControl, OneUiSecureBlobStore storage) {
        if (protocol == null || tunnelControl == null || storage == null) throw new IllegalArgumentException("Native VPN dependencies");
        this.label = label; this.mtu = mtu; this.defaultRoute = defaultRoute;
        this.backendLibrary = backendLibrary;
        this.protocol = protocol; this.tunnelControl = tunnelControl; this.storage = storage;
        this.context = context.getApplicationContext();
    }
    @Override public void start(int fd, OneUiVpnService.SocketProtector protector, OneUiVpnService.EngineEvents events) throws Exception {
        try { synchronized (lock) {
            if (worker != null || active) throw new IllegalStateException("Native VPN engine is single use");
            this.events = events;
            active = true;
            try {
                port = new OneUiNativePacketPort(backendLibrary, label, mtu, defaultRoute, false, new OneUiNativePacketPort.Services() {
                    public int applyNetwork(String json) { return reconfigure(json); }
                    public boolean protectSocket(int socket) {
                        return active && protector.protect(socket) && OneUiUnderlay.bindSocket(context, socket);
                    }
                    public byte[] secureRead(String key) throws Exception { return storage.read(key); }
                    public void secureWrite(String key, byte[] bytes) throws Exception { storage.write(key, bytes); }
                    public byte[] networkState() throws Exception { return OneUiNetworkState.snapshot(context); }
                });
                OneUiNativePacketPort observed = port;
                observer = new OneUiNetworkState.Observer(context, () -> {
                    if (!active) return;
                    try { observed.networkChanged(); }
                    catch (IllegalStateException error) { if (active) events.failed(1); }
                });
                pump = new OneUiPacketPump(fd, port, code -> { if (active) events.failed(code); });
            } catch (Exception error) {
                active = false;
                throw error;
            }
            worker = new Thread(() -> {
                try { protocol.start(port.handle()); if (active) events.connected(); }
                catch (Exception error) { if (active) events.failed(1); }
            }, "OneUI-VPN-protocol");
            worker.start();
        } } catch (Exception error) {
            OneUiNativePacketPort abandoned;
            synchronized (lock) { abandoned = port; port = null; }
            if (observer != null) { observer.close(); observer = null; }
            if (abandoned != null) abandoned.close();
            throw error;
        }
    }
    private int reconfigure(String json) {
        synchronized (lock) {
            try {
                boolean clearing = new JSONObject(json).getJSONArray("addresses").length() == 0;
                if (!active) return clearing ? 0 : -1; // Service teardown owns the remaining OS descriptor.
                if (pump != null) { pump.close(); pump = null; }
                int fd = tunnelControl.apply(json);
                if (clearing) return fd < 0 ? 0 : closeUnexpected(fd);
                if (fd < 0) return -1;
                try { pump = new OneUiPacketPump(fd, port, code -> { if (active) events.failed(code); }); }
                catch (Exception error) { ParcelFileDescriptor.adoptFd(fd).close(); throw error; }
                return 0;
            } catch (Exception error) { return -1; }
        }
    }
    private int closeUnexpected(int fd) throws IOException { ParcelFileDescriptor.adoptFd(fd).close(); return -1; }
    @Override public void stop() {
        active = false;
        if (observer != null) { observer.close(); observer = null; }
        protocol.cancel();
        Thread pending;
        synchronized (lock) { pending = worker; }
        boolean interrupted = false;
        if (pending != null) {
            while (pending.isAlive()) {
                try { pending.join(); } catch (InterruptedException error) { interrupted = true; }
            }
        }
        Exception failure = null;
        try { if (!protocolClosed) { protocol.close(); protocolClosed = true; } }
        catch (Exception error) { failure = error; }
        OneUiPacketPump previousPump;
        OneUiNativePacketPort previousPort;
        synchronized (lock) { previousPump = pump; previousPort = port; }
        // Native close invokes applyNetwork; hold no Java monitor across it.
        try { if (previousPump != null) previousPump.close(); } catch (Exception error) { failure = error; }
        // A failed protocol can still call native services. Keep its callback
        // context alive until both protocol and packet-pump cleanup succeed.
        if (failure == null) {
            try { if (previousPort != null) previousPort.close(); } catch (Exception error) { failure = error; }
        }
        if (interrupted) Thread.currentThread().interrupt();
        if (failure != null) throw new IllegalStateException("Native VPN teardown failed", failure);
        synchronized (lock) { pump = null; port = null; }
    }
}
