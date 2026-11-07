package net.too1.oneui.vpn;

import android.content.Context;
import org.json.JSONObject;

/** Secure preflight context for enrollment/identity queries before VPN approval.
 * Network configuration with addresses is rejected; this context cannot route traffic. */
public final class OneUiNativeContext implements AutoCloseable {
    private final OneUiNativePacketPort port;
    public OneUiNativeContext(Context context, String library, String namespace, String label, int mtu) throws Exception {
        Context app = context.getApplicationContext();
        OneUiSecureBlobStore storage = new OneUiSecureBlobStore(app, namespace);
        port = new OneUiNativePacketPort(library, label, mtu, false, false, new OneUiNativePacketPort.Services() {
            public int applyNetwork(String json) {
                try { return new JSONObject(json).getJSONArray("addresses").length() == 0 ? 0 : -1; }
                catch (Exception error) { return -1; }
            }
            public boolean protectSocket(int fd) { return false; }
            public byte[] secureRead(String key) throws Exception { return storage.read(key); }
            public void secureWrite(String key, byte[] bytes) throws Exception { storage.write(key, bytes); }
            public byte[] networkState() throws Exception { return OneUiNetworkState.snapshot(app); }
        });
    }
    public long host() { return port.handle(); }
    @Override public void close() throws java.io.IOException { port.close(); }
}
