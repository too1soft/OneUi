package net.too1.oneui.vpn;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.net.VpnService;
import android.os.ParcelFileDescriptor;
import java.io.IOException;

/** System VPN lifecycle shared by products. Subclasses supply a packet engine,
 * notification presentation and forward events to their owner-thread OneUI session.
 * The engine must synchronously cancel/join its work in stop(). */
public abstract class OneUiVpnService extends VpnService {
    public static final String ACTION_START = "net.too1.oneui.vpn.START";
    public static final String ACTION_STOP = "net.too1.oneui.vpn.STOP";
    public static final String EXTRA_CONFIGURATION = "oneui.vpn.configuration";
    public static final String EXTRA_ATTEMPT = "oneui.vpn.attempt";
    public static final int AUTHORIZED = 1, DENIED = 2, TUNNEL_READY = 3,
        ENGINE_CONNECTED = 4, FAILED = 5, STOPPED = 6, REVOKED = 7;
    private ParcelFileDescriptor tunnel;
    private Engine engine;
    private long attempt;
    private String configuration;
    private volatile boolean active;
    private final Object tunnelLock = new Object();
    private final android.os.Handler owner = new android.os.Handler(android.os.Looper.getMainLooper());
    private int cleanupRetries;

    public interface SocketProtector { boolean protect(int fd); }
    public interface EngineEvents { void connected(); void failed(int code); }
    public interface Engine {
        /** Takes ownership of fd only on normal return. No raw keys in config. */
        void start(int fd, SocketProtector protector, EngineEvents events) throws Exception;
        void stop();
    }
    protected abstract Engine createEngine() throws Exception;
    protected abstract Notification notification(String label);
    protected abstract int notificationId();
    protected abstract void report(long attempt, int event, int code);
    protected final String configuration() { return configuration; }
    protected final long currentAttempt() { return attempt; }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) { stopSelf(startId); return Service.START_NOT_STICKY; }
        long requested = intent.getLongExtra(EXTRA_ATTEMPT, 0);
        if (ACTION_STOP.equals(intent.getAction())) {
            if (requested == attempt) { if (closeTunnel()) stopSelf(startId); }
            else if (!active) stopSelf(startId);
            return Service.START_NOT_STICKY;
        }
        if (!ACTION_START.equals(intent.getAction()) || requested <= 0 || active || engine != null) {
            if (!active && engine == null) stopSelf(startId);
            return Service.START_NOT_STICKY;
        }
        attempt = requested;
        active = true;
        cleanupRetries = 0;
        try {
            configuration = intent.getStringExtra(EXTRA_CONFIGURATION);
            VpnConfiguration config = VpnConfiguration.parse(configuration);
            startForeground(notificationId(), notification(config.label));
            if (VpnService.prepare(this) != null) {
                report(attempt, DENIED, 1); if (closeTunnel()) stopSelf(startId);
                return Service.START_NOT_STICKY;
            }
            report(attempt, AUTHORIZED, 0);
            synchronized (tunnelLock) { tunnel = config.apply(new Builder()).establish(); }
            if (tunnel == null) throw new IOException("VPN permission unavailable");
            report(attempt, TUNNEL_READY, 0);
            engine = createEngine();
            final long generation = attempt;
            int fd = ParcelFileDescriptor.dup(tunnel.getFileDescriptor()).detachFd();
            try {
                engine.start(fd, this::protect, new EngineEvents() {
                    public void connected() { onEngineEvent(generation, ENGINE_CONNECTED, 0); }
                    public void failed(int code) { onEngineEvent(generation, FAILED, code); }
                });
            } catch (Exception error) {
                ParcelFileDescriptor.adoptFd(fd).close();
                throw error;
            }
        } catch (Exception error) {
            // Products receive a stable code, never a raw exception containing keys.
            report(attempt, FAILED, 1); if (closeTunnel()) stopSelf(startId);
        }
        return Service.START_NOT_STICKY;
    }
    private void onEngineEvent(long generation, int event, int code) {
        new android.os.Handler(getMainLooper()).post(() -> {
            if (!active || generation != attempt) return;
            report(generation, event, code);
            if (event == FAILED && closeTunnel()) stopSelf();
        });
    }
    /** Called from the protocol worker. Returns an owned duplicate for OneUI's
     * packet pump, or -1 after removing this generation's native interface. */
    protected final int replaceTunnel(String json) throws Exception {
        org.json.JSONObject value = new org.json.JSONObject(json);
        if (value.getInt("version") != 1) throw new IOException("VPN settings version");
        synchronized (tunnelLock) {
            if (!active) {
                if (value.getJSONArray("addresses").length() == 0) return -1;
                throw new IOException("VPN session stopped");
            }
            if (value.getJSONArray("addresses").length() == 0) {
                if (tunnel != null) tunnel.close();
                tunnel = null;
                return -1;
            }
            ParcelFileDescriptor replacement = VpnConfiguration.parseNetworkSettings(json).apply(new Builder()).establish();
            if (replacement == null) throw new IOException("VPN replacement unavailable");
            try {
                int fd = ParcelFileDescriptor.dup(replacement.getFileDescriptor()).detachFd();
                if (tunnel != null) tunnel.close();
                tunnel = replacement;
                return fd;
            } catch (Exception error) { replacement.close(); throw error; }
        }
    }
    private boolean closeTunnel() {
        if (!active && engine == null && tunnel == null) return true;
        active = false;
        boolean complete = true;
        try { if (engine != null) engine.stop(); }
        catch (RuntimeException error) { complete = false; report(attempt, FAILED, 2); }
        finally {
            synchronized (tunnelLock) {
                try { if (tunnel != null) tunnel.close(); } catch (IOException ignored) { }
                tunnel = null;
            }
        }
        if (!complete) {
            // Retain failed engine ownership and the foreground stop action.
            // Never acknowledge cleanup while callbacks/work may still exist.
            if (cleanupRetries++ < 3) {
                final long generation = attempt;
                owner.postDelayed(() -> { if (generation == attempt && !active && engine != null && closeTunnel()) stopSelf(); }, 1000);
            }
            return false;
        }
        engine = null; configuration = null;
        stopForeground(Service.STOP_FOREGROUND_REMOVE);
        report(attempt, STOPPED, 0);
        return true;
    }
    @Override public void onRevoke() {
        if (active) report(attempt, REVOKED, 1);
        if (closeTunnel()) stopSelf();
    }
    @Override public void onDestroy() { closeTunnel(); super.onDestroy(); }
}
