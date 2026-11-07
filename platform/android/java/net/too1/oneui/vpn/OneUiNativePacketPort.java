package net.too1.oneui.vpn;

import java.io.Closeable;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** JNI binding to the reusable OneUI Go packet ABI, independent of product
 * enrollment, account UI and control-service endpoints. */
public final class OneUiNativePacketPort implements OneUiPacketPump.Port, Closeable {
    public interface Services {
        int applyNetwork(String json);
        boolean protectSocket(int fd);
        byte[] secureRead(String key) throws Exception;
        void secureWrite(String key, byte[] value) throws Exception;
        byte[] networkState() throws Exception;
    }
    private long handle;
    static { System.loadLibrary("oneui_vpn_native"); }
    public OneUiNativePacketPort(String backendLibrary, String label, int mtu, boolean defaultRoute, boolean splitDNS, Services services) throws IOException {
        if (backendLibrary == null || label == null || services == null) throw new IllegalArgumentException("Native VPN services");
        handle = nativeCreate(backendLibrary.getBytes(StandardCharsets.UTF_8), label.getBytes(StandardCharsets.UTF_8), mtu, (defaultRoute ? 1 : 0) | (splitDNS ? 2 : 0), services);
        if (handle == 0) throw new IOException("Native VPN host creation failed");
    }
    public synchronized long handle() { return handle; }
    public int networkChanged() { return nativeNetworkChanged(handle()); }
    @Override public int inject(byte[] packet, int length, int timeoutMs) { return nativeInject(handle(), packet, length, timeoutMs); }
    @Override public int receive(byte[] destination, int timeoutMs) { return nativeReceive(handle(), destination, timeoutMs); }
    @Override public synchronized void close() throws IOException {
        if (handle == 0) return;
        if (nativeDestroy(handle) != 0) throw new IOException("Native VPN cleanup failed");
        handle = 0;
    }
    private static native long nativeCreate(byte[] backendLibrary, byte[] label, int mtu, int flags, Services services);
    private static native int nativeInject(long host, byte[] packet, int length, int timeoutMs);
    private static native int nativeReceive(long host, byte[] buffer, int timeoutMs);
    private static native int nativeDestroy(long host);
    private static native int nativeNetworkChanged(long host);
}
