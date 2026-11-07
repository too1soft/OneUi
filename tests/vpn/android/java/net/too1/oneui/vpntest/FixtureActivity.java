package net.too1.oneui.vpntest;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.TextView;
import net.too1.oneui.vpn.OneUiVpnManager;

public final class FixtureActivity extends Activity {
    static final String CONFIG = "{\"version\":1,\"label\":\"OneUI VPN runtime check\",\"mtu\":1280,\"allowDefaultRoute\":false,\"addresses\":[{\"address\":\"100.64.10.1\",\"prefix\":32}],\"routes\":[{\"address\":\"100.64.10.2\",\"prefix\":32}],\"dnsServers\":[],\"dnsDomains\":[]}";
    OneUiVpnManager owner;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state); owner = OneUiVpnManager.get(this, FixtureService.class);
        TextView text = new TextView(this); text.setText("OneUI emulator VPN runtime check"); setContentView(text);
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        if (!owner.onActivityResult(request, result)) super.onActivityResult(request, result, data);
    }
    @Override protected void onDestroy() { owner.detach(this); super.onDestroy(); }
}
