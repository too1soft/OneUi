package net.too1.oneui.vpn;

import android.app.Activity;
import android.content.Intent;
import android.net.VpnService;
import android.os.Build;
import android.content.Context;
import java.lang.ref.WeakReference;

/** Foreground Activity permission bridge. The caller forwards onActivityResult.
 * Do not report authorization from startService's return value. */
public final class OneUiVpnController {
    public interface Events { void report(long attempt, int event, int code); }
    private final Context context;
    private WeakReference<Activity> activity = new WeakReference<>(null);
    private final Class<? extends OneUiVpnService> service;
    private final Events events;
    private final int requestCode;
    private long attempt;
    private String configuration;
    private boolean permissionPending;
    private boolean cancelled;

    public OneUiVpnController(Activity activity, Class<? extends OneUiVpnService> service,
                              int requestCode, Events events) {
        this.context = activity.getApplicationContext(); this.activity = new WeakReference<>(activity); this.service = service;
        this.requestCode = requestCode; this.events = events;
    }
    public void attach(Activity owner) { activity = new WeakReference<>(owner); }
    public boolean authorizing() { return permissionPending; }
    public void detach(Activity owner) {
        if (activity.get() != owner) return;
        if (owner.isFinishing() && permissionPending) {
            cancelled = true; permissionPending = false; configuration = null;
            owner.finishActivity(requestCode);
            events.report(attempt, OneUiVpnService.STOPPED, 0);
        }
        activity.clear();
    }
    public void start(long id, String json) throws Exception {
        if (id <= 0 || permissionPending) throw new IllegalStateException("VPN authorization pending");
        VpnConfiguration.parse(json);
        attempt = id; configuration = json; cancelled = false;
        Activity owner = activity.get();
        if (owner == null || owner.isFinishing()) throw new IllegalStateException("VPN authorization requires a foreground Activity");
        Intent permission = VpnService.prepare(owner);
        if (permission != null) {
            permissionPending = true;
            owner.startActivityForResult(permission, requestCode);
        } else { startService(); }
    }
    public boolean onActivityResult(int request, int result) {
        if (request != requestCode || !permissionPending) return false;
        permissionPending = false;
        if (cancelled) {
            events.report(attempt, OneUiVpnService.STOPPED, 0);
            return true;
        }
        if (result != Activity.RESULT_OK) {
            cancelled = true;
            events.report(attempt, OneUiVpnService.DENIED, 1);
            events.report(attempt, OneUiVpnService.STOPPED, 0);
            configuration = null;
        } else {
            try { startService(); }
            catch (RuntimeException error) {
                cancelled = true;
                events.report(attempt, OneUiVpnService.FAILED, 1);
                events.report(attempt, OneUiVpnService.STOPPED, 0);
                configuration = null;
            }
        }
        return true;
    }
    private void startService() {
        Intent intent = new Intent(context, service).setAction(OneUiVpnService.ACTION_START)
            .putExtra(OneUiVpnService.EXTRA_ATTEMPT, attempt)
            .putExtra(OneUiVpnService.EXTRA_CONFIGURATION, configuration);
        if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(intent);
        else context.startService(intent);
        configuration = null;
    }
    public void stop(long id) {
        if (id != attempt) return;
        cancelled = true;
        if (permissionPending) {
            configuration = null;
            // Wait for the outstanding system permission result before allowing
            // another attempt; a late approval must never start a cancelled VPN.
        } else {
            context.startService(new Intent(context, service)
                .setAction(OneUiVpnService.ACTION_STOP)
                .putExtra(OneUiVpnService.EXTRA_ATTEMPT, attempt));
        }
    }
}
