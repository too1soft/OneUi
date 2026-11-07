package net.too1.oneui.vpntest;

import net.too1.oneui.nativebridge.NativeModule;
import net.too1.oneui.vpn.OneUiNativeVpnEngine;
import net.too1.oneui.vpn.OneUiNativeVpnService;
import java.nio.charset.StandardCharsets;
import org.json.JSONObject;

/** Emulator-only ephemeral WireGuard echo peer. Not shipped in a product. */
public final class FixtureService extends OneUiNativeVpnService {
    static JSONObject call(NativeModule module, int command, JSONObject request) throws Exception {
        return new JSONObject(new String(module.call(command, request.toString().getBytes(StandardCharsets.UTF_8)), StandardCharsets.UTF_8));
    }
    static JSONObject stats() throws Exception {
        try (NativeModule module = new NativeModule("oneui_vpn_fixture", "oneui_fixture_dispatch_v1")) { return call(module, 4, new JSONObject()); }
    }
    @Override protected String backendLibrary() { return "oneui_vpn_fixture"; }
    @Override protected String storageNamespace() { return "oneui.emulator.vpn.fixture"; }
    @Override protected OneUiNativeVpnEngine.Protocol createProtocol() throws Exception {
        NativeModule module = new NativeModule("oneui_vpn_fixture", "oneui_fixture_dispatch_v1");
        return new OneUiNativeVpnEngine.Protocol() {
            public void start(long host) throws Exception {
                if (call(module, 1, new JSONObject().put("host", Long.toUnsignedString(host))).getInt("code") != 0)
                    throw new IllegalStateException("Fixture engine startup failed");
            }
            public void cancel() { try { call(module, 3, new JSONObject()); } catch (Exception ignored) { } }
            public void close() throws Exception { call(module, 2, new JSONObject()); module.close(); }
        };
    }
}
