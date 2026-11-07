package net.too1.oneui.vpn;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import org.json.JSONObject;

/** Complete native service plumbing. Products provide their protocol and library
 * identity; permissions, storage, transport, notifications and events are OneUI. */
public abstract class OneUiNativeVpnService extends OneUiVpnService {
    protected abstract String backendLibrary();
    protected abstract String storageNamespace();
    protected abstract OneUiNativeVpnEngine.Protocol createProtocol() throws Exception;
    @Override protected final Engine createEngine() throws Exception {
        JSONObject config = new JSONObject(configuration());
        return new OneUiNativeVpnEngine(this, backendLibrary(), config.getString("label"), config.getInt("mtu"),
            config.getBoolean("allowDefaultRoute"), createProtocol(), this::replaceTunnel,
            new OneUiSecureBlobStore(this, storageNamespace()));
    }
    @Override protected final int notificationId() { return 0x6f31; }
    @Override protected final Notification notification(String label) {
        String channel = "oneui.vpn.connection";
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel(channel, "VPN 连接", NotificationManager.IMPORTANCE_LOW));
        Intent stop = new Intent(this, getClass()).setAction(ACTION_STOP).putExtra(EXTRA_ATTEMPT, currentAttempt());
        Notification.Builder builder = new Notification.Builder(this, channel).setSmallIcon(getApplicationInfo().icon)
            .setContentTitle(label).setContentText("VPN 连接正在运行").setOngoing(true).setCategory(Notification.CATEGORY_SERVICE)
            .addAction(new Notification.Action.Builder(null, "断开连接", PendingIntent.getService(this, 2, stop,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE)).build());
        Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        if (launch != null) builder.setContentIntent(PendingIntent.getActivity(this, 1, launch,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE));
        return builder.build();
    }
    @Override protected final void report(long attempt, int event, int code) {
        OneUiVpnManager.serviceEvent(getClass(), attempt, event, code);
    }
}
