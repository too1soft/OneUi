package net.too1.oneui.vpn;

import android.net.VpnService;
import android.net.IpPrefix;
import android.system.OsConstants;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/** Wire format produced by oneui_vpn_config_json_v1; contains no engine secrets. */
public final class VpnConfiguration {
    private final JSONObject value;
    public final String label;
    private VpnConfiguration(JSONObject value, boolean dynamic) throws JSONException {
        this.value = value;
        if (value.getInt("version") != 1) throw new JSONException("VPN configuration version");
        label = value.getString("label");
        if (label.isEmpty() || label.length() > 128) throw new JSONException("VPN label");
        int mtu = value.getInt("mtu");
        if (mtu < 1280 || mtu > 9000) throw new JSONException("VPN MTU");
        bounded("addresses", 1, 16); bounded("routes", dynamic ? 0 : 1, 256);
        bounded("dnsServers", 0, 8); bounded("dnsDomains", 0, 16);
        JSONArray excluded = value.optJSONArray("excludedRoutes"), matches = value.optJSONArray("dnsMatchDomains");
        if ((excluded != null && excluded.length() > 256) || (matches != null && matches.length() != 0))
            throw new JSONException("Unsupported VPN route or DNS settings");
    }
    public static VpnConfiguration parse(String json) throws JSONException {
        if (json == null || json.length() > 65536) throw new JSONException("VPN configuration size");
        return new VpnConfiguration(new JSONObject(json), false);
    }
    public static VpnConfiguration parseNetworkSettings(String json) throws JSONException {
        if (json == null || json.length() > 65536) throw new JSONException("VPN configuration size");
        return new VpnConfiguration(new JSONObject(json), true);
    }
    private void bounded(String key, int min, int max) throws JSONException {
        int size = value.getJSONArray(key).length();
        if (size < min || size > max) throw new JSONException("VPN configuration count");
    }
    public VpnService.Builder apply(VpnService.Builder builder) throws JSONException {
        builder.setSession(label).setMtu(value.getInt("mtu")).setBlocking(true);
        JSONArray addresses = value.getJSONArray("addresses");
        for (int i = 0; i < addresses.length(); ++i) {
            JSONObject p = addresses.getJSONObject(i);
            builder.addAddress(p.getString("address"), p.getInt("prefix"));
        }
        JSONArray routes = value.getJSONArray("routes");
        for (int i = 0; i < routes.length(); ++i) {
            JSONObject p = routes.getJSONObject(i);
            if (p.getInt("prefix") == 0 && !value.getBoolean("allowDefaultRoute"))
                throw new JSONException("Default route requires explicit consent");
            builder.addRoute(p.getString("address"), p.getInt("prefix"));
        }
        JSONArray excluded = value.optJSONArray("excludedRoutes");
        if (excluded != null && excluded.length() != 0) {
            if (android.os.Build.VERSION.SDK_INT < 33) throw new JSONException("Excluded VPN routes require Android 13");
            Api33.exclude(builder, excluded);
        }
        JSONArray dns = value.getJSONArray("dnsServers");
        for (int i = 0; i < dns.length(); ++i) builder.addDnsServer(dns.getString(i));
        JSONArray domains = value.getJSONArray("dnsDomains");
        for (int i = 0; i < domains.length(); ++i) builder.addSearchDomain(domains.getString(i));
        if (!value.getBoolean("allowDefaultRoute")) {
            // Families without overlay routes must retain normal Internet access.
            builder.allowFamily(OsConstants.AF_INET).allowFamily(OsConstants.AF_INET6);
        }
        return builder;
    }
    private static final class Api33 {
        static void exclude(VpnService.Builder builder, JSONArray values) throws JSONException {
            for (int i = 0; i < values.length(); ++i) {
                JSONObject p = values.getJSONObject(i);
                builder.excludeRoute(new IpPrefix(android.net.InetAddresses.parseNumericAddress(p.getString("address")), p.getInt("prefix")));
            }
        }
    }
}
