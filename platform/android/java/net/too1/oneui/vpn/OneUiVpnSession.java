package net.too1.oneui.vpn;

import android.os.Looper;
import java.nio.charset.StandardCharsets;

/** Owner-thread binding of the portable OneUI VPN state machine. */
public final class OneUiVpnSession implements AutoCloseable {
    public interface Host { void start(long attempt, String configuration); void stop(long attempt); }
    public static final class Snapshot {
        public final int state, error;
        public final long attempt, revision;
        private Snapshot(long[] values) {
            state = (int)values[0]; error = (int)values[1]; attempt = values[2]; revision = values[3];
        }
    }
    private final Host host;
    private long handle;
    static { System.loadLibrary("oneui_vpn_native"); }
    public OneUiVpnSession(Host host) {
        checkThread();
        if (host == null) throw new IllegalArgumentException("VPN host required");
        this.host = host;
        handle = create();
        if (handle == 0) throw new IllegalStateException("VPN session unavailable");
    }
    private static void checkThread() {
        if (Looper.myLooper() != Looper.getMainLooper()) throw new IllegalStateException("VPN session requires main thread");
    }
    private void check() { checkThread(); if (handle == 0) throw new IllegalStateException("VPN session closed"); }
    public void connect(String configuration) {
        check(); require(connect(handle, configuration.getBytes(StandardCharsets.UTF_8)));
    }
    public void disconnect() { check(); require(disconnect(handle)); }
    /** Stale native events are ignored by the shared state machine. */
    public boolean report(long attempt, int event, int code) { check(); return report(handle, attempt, event, code) == 0; }
    public Snapshot snapshot() { check(); return new Snapshot(snapshot(handle)); }
    @Override public void close() { check(); require(destroy(handle)); handle = 0; }
    private static void require(int result) { if (result != 0) throw new IllegalStateException("VPN session result " + result); }
    // Invoked synchronously by the shared owner-thread state machine.
    private void begin(long attempt, byte[] json) { host.start(attempt, new String(json, StandardCharsets.UTF_8)); }
    private void end(long attempt) { host.stop(attempt); }
    private native long create();
    private static native int connect(long handle, byte[] configuration);
    private static native int disconnect(long handle);
    private static native int report(long handle, long attempt, int event, int code);
    private static native long[] snapshot(long handle);
    private static native int destroy(long handle);
}
