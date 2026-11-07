package net.too1.oneui.vpntest;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.accessibility.AccessibilityNodeInfo;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import net.too1.oneui.vpn.OneUiVpnManager;

/** Touches only this installed fixture and its emulator VPN permission. */
public final class SystemVpnChecks extends Instrumentation {
    private OneUiVpnManager owner;
    private static void require(boolean value, String reason) { if (!value) throw new AssertionError(reason); }
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }
    private int state() {
        AtomicInteger value = new AtomicInteger(); runOnMainSync(() -> value.set(owner.snapshot().state)); return value.get();
    }
    private void waitState(int expected) {
        long deadline = SystemClock.uptimeMillis() + 20000;
        while (SystemClock.uptimeMillis() < deadline) { if (state() == expected) return; SystemClock.sleep(100); }
        throw new AssertionError("VPN state expected " + expected + " got " + state());
    }
    private void consent(boolean allow) {
        long deadline = SystemClock.uptimeMillis() + 8000;
        while (SystemClock.uptimeMillis() < deadline) {
            AccessibilityNodeInfo root = getUiAutomation().getRootInActiveWindow();
            if (root != null) {
                List<AccessibilityNodeInfo> buttons = root.findAccessibilityNodeInfosByViewId("android:id/button" + (allow ? "1" : "2"));
                for (AccessibilityNodeInfo button : buttons) if (button.isEnabled() && button.performAction(AccessibilityNodeInfo.ACTION_CLICK)) return;
            }
            SystemClock.sleep(100);
        }
        throw new AssertionError("VPN consent dialog unavailable");
    }
    private String counters(String name) throws Exception {
        if (name == null || !name.matches("[a-zA-Z0-9_-]{1,32}")) return "invalid interface";
        try (android.os.ParcelFileDescriptor result = getUiAutomation().executeShellCommand("ip -s link show dev " + name);
             java.io.FileInputStream input = new java.io.FileInputStream(result.getFileDescriptor())) {
            byte[] bytes = new byte[2048]; int count = input.read(bytes); return count < 0 ? "" : new String(bytes, 0, count, StandardCharsets.UTF_8);
        }
    }
    private android.net.Network currentVpn(android.net.ConnectivityManager connectivity) {
        android.net.Network newest = null;
        for (android.net.Network network : connectivity.getAllNetworks()) {
            android.net.NetworkCapabilities capabilities = connectivity.getNetworkCapabilities(network);
            android.net.LinkProperties properties = connectivity.getLinkProperties(network);
            if (capabilities == null || properties == null || properties.getInterfaceName() == null ||
                !capabilities.hasTransport(android.net.NetworkCapabilities.TRANSPORT_VPN) ||
                capabilities.getOwnerUid() != android.os.Process.myUid()) continue;
            boolean destination = false;
            for (android.net.RouteInfo route : properties.getRoutes()) {
                if (route.getDestination().toString().equals("100.64.10.2/32")) destination = true;
            }
            if (destination && (newest == null || network.getNetworkHandle() > newest.getNetworkHandle())) newest = network;
        }
        return newest;
    }
    @Override public void onStart() {
        Bundle result = new Bundle(); FixtureActivity activity = null;
        try {
            activity = (FixtureActivity)startActivitySync(new Intent(getTargetContext(), FixtureActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            owner = activity.owner;
            runOnMainSync(() -> owner.connect(FixtureActivity.CONFIG)); consent(false); waitState(6);
            runOnMainSync(() -> owner.connect(FixtureActivity.CONFIG)); consent(true); waitState(4);
            android.net.ConnectivityManager connectivity = getTargetContext().getSystemService(android.net.ConnectivityManager.class);
            net.too1.oneui.vpn.OneUiUnderlay.Selection underlay = net.too1.oneui.vpn.OneUiUnderlay.select(getTargetContext());
            require(underlay.network != null && !underlay.identity().isEmpty(), "Physical network identity unavailable");
            require(!connectivity.getNetworkCapabilities(underlay.network).hasTransport(android.net.NetworkCapabilities.TRANSPORT_VPN), "VPN selected as its own underlay");
            org.json.JSONObject networkSnapshot = new org.json.JSONObject(new String(
                net.too1.oneui.vpn.OneUiNetworkState.snapshot(getTargetContext()), StandardCharsets.UTF_8));
            require(networkSnapshot.getString("underlayId").equals(underlay.identity()), "Socket and native netmon use different networks");
            int sends = 0;
            for (int i = 0; i < 2; ++i) {
                byte[] expected = ("OneUI encrypted kernel UDP " + i).getBytes(StandardCharsets.UTF_8);
                boolean echoed = false;
                String last = "no routed VPN network";
                long deadline = SystemClock.uptimeMillis() + 12000;
                while (!echoed && SystemClock.uptimeMillis() < deadline) {
                    require(state() == 4, "VPN left connected state=" + state() + " stats=" + FixtureService.stats());
                    android.net.Network vpn = currentVpn(connectivity);
                    if (vpn == null) { SystemClock.sleep(100); continue; }
                    // Reconfiguration can replace an initially bindable netId.
                    // Rebind after a timeout; never inject diagnostic packets.
                    try (java.net.DatagramSocket socket = new java.net.DatagramSocket(null)) {
                        vpn.bindSocket(socket); socket.setSoTimeout(1000);
                        socket.connect(java.net.InetAddress.getByName("100.64.10.2"), 8443);
                        socket.send(new java.net.DatagramPacket(expected, expected.length)); ++sends;
                        byte[] bytes = new byte[1024]; java.net.DatagramPacket response = new java.net.DatagramPacket(bytes, bytes.length);
                        socket.receive(response);
                        require(java.util.Arrays.equals(expected, java.util.Arrays.copyOf(bytes, response.getLength())), "decrypted kernel UDP differs");
                        echoed = true;
                    } catch (java.net.SocketException | java.net.SocketTimeoutException error) {
                        android.net.LinkProperties properties = connectivity.getLinkProperties(vpn);
                        last = error.getClass().getSimpleName() + " routes=" + properties + " counters=" + counters(properties == null ? null : properties.getInterfaceName());
                    }
                }
                require(echoed, "kernel VPN echo failed stats=" + FixtureService.stats() + " last=" + last);
            }
            require(FixtureService.stats().getInt("echoed") >= 2, "loopback WireGuard peer did not decrypt both OS packets");
            FixtureActivity page = activity; runOnMainSync(page::finish); SystemClock.sleep(300);
            require(state() == 4, "finishing the page stopped its application-owned VPN");
            runOnMainSync(owner::disconnect); waitState(0);
            result.putInt("passed", 1);
            result.putInt("kernelUdpSends", sends);
            result.putString("checks", "Actual consent denial/retry, foreground VPN, VPN-bound kernel UDP WireGuard roundtrip, page exit persistence and cleanup passed");
            finish(Activity.RESULT_OK, result);
        } catch (Throwable error) {
            result.putString("failure", error.getClass().getName() + ": " + error.getMessage()); finish(Activity.RESULT_CANCELED, result);
        } finally {
            if (owner != null) runOnMainSync(() -> { try { owner.disconnect(); } catch (Exception ignored) { } });
            if (activity != null) { FixtureActivity page = activity; runOnMainSync(page::finish); }
        }
    }
}
