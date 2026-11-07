package net.too1.oneui.vpn;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

/** Application-scoped tunnel owner. Views subscribe without owning VPN lifetime.
 * Service events stay inside this application process, without exported broadcasts. */
public final class OneUiVpnManager {
    public interface Listener { void changed(OneUiVpnSession.Snapshot state); }
    private static final Map<Class<?>, OneUiVpnManager> owners = new HashMap<>();
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Set<Listener> listeners = new LinkedHashSet<>();
    private final OneUiVpnController controller;
    private final OneUiVpnSession session;
    private static void checkThread() {
        if (Looper.myLooper() != Looper.getMainLooper()) throw new IllegalStateException("VPN manager requires main thread");
    }
    public static OneUiVpnManager get(Activity activity, Class<? extends OneUiVpnService> service) {
        checkThread();
        OneUiVpnManager owner = owners.get(service);
        if (owner == null) { owner = new OneUiVpnManager(activity, service); owners.put(service, owner); }
        owner.controller.attach(activity);
        return owner;
    }
    private OneUiVpnManager(Activity activity, Class<? extends OneUiVpnService> service) {
        controller = new OneUiVpnController(activity, service, 0x6f31, this::report);
        session = new OneUiVpnSession(new OneUiVpnSession.Host() {
            public void start(long attempt, String json) {
                try { controller.start(attempt, json); }
                catch (Exception error) { main.post(() -> { report(attempt, OneUiVpnService.FAILED, 1); report(attempt, OneUiVpnService.STOPPED, 0); }); }
            }
            public void stop(long attempt) {
                try { controller.stop(attempt); }
                catch (RuntimeException error) { main.post(() -> report(attempt, OneUiVpnService.FAILED, 2)); }
            }
        });
    }
    public void attach(Activity owner) { checkThread(); controller.attach(owner); }
    public void detach(Activity owner) {
        checkThread();
        if (owner.isFinishing() && controller.authorizing()) { session.disconnect(); publish(); }
        controller.detach(owner);
    }
    public void listen(Listener listener) { checkThread(); listeners.add(listener); listener.changed(session.snapshot()); }
    public void remove(Listener listener) { checkThread(); listeners.remove(listener); }
    public void connect(String configuration) { checkThread(); session.connect(configuration); publish(); }
    public void disconnect() { checkThread(); session.disconnect(); publish(); }
    public OneUiVpnSession.Snapshot snapshot() { checkThread(); return session.snapshot(); }
    public boolean onActivityResult(int request, int result) { checkThread(); return controller.onActivityResult(request, result); }
    private void publish() {
        OneUiVpnSession.Snapshot state = session.snapshot();
        for (Listener listener : listeners.toArray(new Listener[0])) listener.changed(state);
    }
    private void report(long attempt, int event, int code) {
        checkThread(); if (session.report(attempt, event, code)) publish();
    }
    static void serviceEvent(Class<?> service, long attempt, int event, int code) {
        new Handler(Looper.getMainLooper()).post(() -> {
            OneUiVpnManager owner = owners.get(service);
            if (owner != null) owner.report(attempt, event, code);
        });
    }
}
