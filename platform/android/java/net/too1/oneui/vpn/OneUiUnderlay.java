package net.too1.oneui.vpn;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.ParcelFileDescriptor;
import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

/** Physical network selection shared by socket protection and native netmon. */
public final class OneUiUnderlay {
    private OneUiUnderlay() { }
    public static final class Selection {
        public final Set<String> interfaces = new HashSet<>();
        public Network network;
        public LinkProperties properties;
        public String transport = "";
        public String identity() {
            return network == null ? "" : Long.toUnsignedString(network.getNetworkHandle()) + ":" + transport;
        }
    }
    @SuppressWarnings("deprecation")
    public static Selection select(Context context) {
        ConnectivityManager manager = context.getSystemService(ConnectivityManager.class);
        Selection result = new Selection();
        Network preferred = manager.getActiveNetwork();
        int best = -1;
        for (Network network : manager.getAllNetworks()) {
            NetworkCapabilities caps = manager.getNetworkCapabilities(network);
            LinkProperties properties = manager.getLinkProperties(network);
            if (caps == null || properties == null || properties.getInterfaceName() == null ||
                    caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) continue;
            result.interfaces.add(properties.getInterfaceName());
            int score = (network.equals(preferred) ? 1000 : 0)
                + (caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED) ? 100 : 0)
                + (caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) ? 100 : 0)
                + (caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) ? 40 : 0)
                + (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ? 30 : 0)
                + (caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) ? 20 : 0);
            if (score > best) {
                best = score; result.network = network; result.properties = properties;
                result.transport = caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) ? "cellular"
                    : caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ? "wifi"
                    : caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) ? "ethernet" : "other";
            }
        }
        return result;
    }
    /** Duplicate only the descriptor; closing it never closes the engine's socket. */
    public static boolean bindSocket(Context context, int fd) {
        try {
            Network network = select(context).network;
            if (network == null) return true; // No usable link yet; a later change retries discovery.
            try (ParcelFileDescriptor copy = ParcelFileDescriptor.fromFd(fd)) {
                network.bindSocket(copy.getFileDescriptor());
            }
            return true;
        } catch (IOException | SecurityException error) { return false; }
    }
}
