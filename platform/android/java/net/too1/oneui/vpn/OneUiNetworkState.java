package net.too1.oneui.vpn;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import java.io.Closeable;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.nio.charset.StandardCharsets;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import org.json.JSONArray;
import org.json.JSONObject;

/** Android-authorized interface enumeration. Go's netlink enumeration is denied
 * in modern app sandboxes. Overlay VPN interfaces never become the underlay. */
public final class OneUiNetworkState {
    private OneUiNetworkState() { }
    @SuppressWarnings("deprecation") // One bounded snapshot; callbacks below drive subsequent refreshes.
    public static byte[] snapshot(Context context) throws Exception {
        Set<String> underlying = new HashSet<>();
        OneUiUnderlay.Selection choice = OneUiUnderlay.select(context);
        underlying.addAll(choice.interfaces);
        LinkProperties selectedProperties = choice.properties;
        String selected = selectedProperties == null ? "" : selectedProperties.getInterfaceName();
        JSONArray interfaces = new JSONArray();
        Set<String> included = new HashSet<>();
        Enumeration<NetworkInterface> values = NetworkInterface.getNetworkInterfaces();
        while (values != null && values.hasMoreElements()) {
            NetworkInterface value = values.nextElement();
            if (!value.isLoopback() && !underlying.contains(value.getName())) continue;
            JSONArray addresses = new JSONArray();
            for (InterfaceAddress address : value.getInterfaceAddresses()) {
                if (address.getAddress() == null) continue;
                String ip = address.getAddress().getHostAddress();
                int zone = ip.indexOf('%');
                if (zone >= 0) ip = ip.substring(0, zone);
                addresses.put(ip + "/" + address.getNetworkPrefixLength());
            }
            interfaces.put(new JSONObject().put("name", value.getName()).put("index", value.getIndex())
                .put("mtu", value.getMTU()).put("up", value.isUp()).put("loopback", value.isLoopback())
                .put("pointToPoint", value.isPointToPoint()).put("multicast", value.supportsMulticast()).put("addresses", addresses));
            included.add(value.getName());
        }
        if (!included.contains(selected)) selected = "";
        JSONObject result = new JSONObject().put("version", 1).put("defaultInterface", selected).put("interfaces", interfaces)
            .put("underlayId", choice.identity()).put("underlayType", choice.transport);
        if (!selected.isEmpty() && selectedProperties != null) {
            JSONArray servers = new JSONArray(), domains = new JSONArray();
            for (java.net.InetAddress server : selectedProperties.getDnsServers())
                servers.put(server.getHostAddress());
            String search = selectedProperties.getDomains();
            if (search != null) for (String domain : search.trim().split("\\s+"))
                if (!domain.isEmpty()) domains.put(domain);
            // Read the selected NOT_VPN network, even after our VPN becomes the
            // active network. Reading the VPN itself would create a DNS loop.
            result.put("baseDNS", new JSONObject().put("servers", servers).put("searchDomains", domains));
        }
        return result.toString().getBytes(StandardCharsets.UTF_8);
    }
    public static final class Observer implements Closeable {
        private final ConnectivityManager manager;
        private final ConnectivityManager.NetworkCallback callback;
        private final android.os.HandlerThread thread = new android.os.HandlerThread("OneUI-underlay");
        private final android.os.Handler owner;
        private final Runnable refresh;
        private volatile boolean closed;
        public Observer(Context context, Runnable changed) {
            manager = context.getSystemService(ConnectivityManager.class);
            thread.start();
            owner = new android.os.Handler(thread.getLooper());
            refresh = () -> { if (!closed) changed.run(); };
            callback = new ConnectivityManager.NetworkCallback() {
                @Override public void onAvailable(Network network) { schedule(); }
                @Override public void onLost(Network network) { schedule(); }
                @Override public void onLinkPropertiesChanged(Network network, LinkProperties properties) { schedule(); }
                @Override public void onCapabilitiesChanged(Network network, NetworkCapabilities capabilities) { schedule(); }
            };
            try {
                manager.registerNetworkCallback(new NetworkRequest.Builder()
                    .addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN).build(), callback, owner);
            } catch (RuntimeException error) { thread.quitSafely(); throw error; }
        }
        private void schedule() {
            if (closed) return;
            owner.removeCallbacks(refresh);
            owner.postDelayed(refresh, 250);
        }
        @Override public synchronized void close() {
            if (closed) return;
            closed = true;
            try { manager.unregisterNetworkCallback(callback); }
            finally { owner.removeCallbacksAndMessages(null); thread.quitSafely(); }
        }
    }
}
