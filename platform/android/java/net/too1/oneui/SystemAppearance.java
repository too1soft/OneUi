package net.too1.oneui;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Build;
import android.os.Looper;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;

/** Android API 28+ adapter for OneUI mobile Appearance.
 * Own one per Activity, place the GPU Surface inside content(), and forward
 * configuration/resume events through systemTheme(). Native code resolves
 * requests and calls apply(); no timer, global listener or hidden JNI ownership.
 * Content is inset once; the Surface's C++ viewport safe insets must remain zero.
 */
public final class SystemAppearance extends FrameLayout {
    private final Activity activity;
    private final FrameLayout content;
    private final Paint paint = new Paint();
    private int left, top, right, bottom, imeBottom;
    private int statusColor = 0xfff5f7f4, navigationColor = 0xfff5f7f4;
    private int appliedIcons;

    public SystemAppearance(Activity owner) {
        super(owner);
        requireUi();
        activity = owner;
        setWillNotDraw(false);
        setBackgroundColor(statusColor);
        content = new FrameLayout(owner);
        addView(content, new FrameLayout.LayoutParams(-1, -1));
        Window window = owner.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        if (Build.VERSION.SDK_INT >= 30) Api30.edgeToEdge(window);
        else window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        // Deprecated only on API 35+ where the OS makes the bars transparent.
        if (Build.VERSION.SDK_INT < 35) {
            window.setStatusBarColor(Color.TRANSPARENT);
            window.setNavigationBarColor(Color.TRANSPARENT);
        }
        if (Build.VERSION.SDK_INT >= 29) Api29.configure(window, this);
        setOnApplyWindowInsetsListener((view, insets) -> {
            if (Build.VERSION.SDK_INT >= 30) {
                android.graphics.Insets safe = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                left = safe.left; top = safe.top; right = safe.right; bottom = safe.bottom;
                imeBottom = insets.getInsets(WindowInsets.Type.ime()).bottom;
            } else {
                left = insets.getSystemWindowInsetLeft(); top = insets.getSystemWindowInsetTop();
                right = insets.getSystemWindowInsetRight(); bottom = insets.getSystemWindowInsetBottom();
                if (insets.getDisplayCutout() != null) {
                    left = Math.max(left, insets.getDisplayCutout().getSafeInsetLeft());
                    top = Math.max(top, insets.getDisplayCutout().getSafeInsetTop());
                    right = Math.max(right, insets.getDisplayCutout().getSafeInsetRight());
                    bottom = Math.max(bottom, insets.getDisplayCutout().getSafeInsetBottom());
                }
            }
            setPadding(left, top, right, Math.max(bottom, imeBottom));
            invalidate();
            return insets;
        });
    }
    public FrameLayout content() { return content; }
    /** 0 = unknown, 1 = light, 2 = dark. Does not change device preferences. */
    public int systemTheme() {
        requireUi();
        int night = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        return night == Configuration.UI_MODE_NIGHT_YES ? 2 :
            night == Configuration.UI_MODE_NIGHT_NO ? 1 : 0;
    }
    /** Returns adapter success, not proof of compositor presentation. */
    public boolean apply(int status, int navigation, boolean lightStatusIcons,
                         boolean lightNavigationIcons, int contentColor) {
        requireUi();
        if (!isAttachedToWindow() || Color.alpha(status) != 255 || Color.alpha(navigation) != 255)
            return false;
        Window window = activity.getWindow();
        int mask = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        int flags = (!lightStatusIcons ? View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR : 0) |
            (!lightNavigationIcons ? View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR : 0);
        if (Build.VERSION.SDK_INT >= 30) {
            if (!Api30.icons(window, lightStatusIcons, lightNavigationIcons)) return false;
        } else {
            View decor = window.getDecorView();
            decor.setSystemUiVisibility((decor.getSystemUiVisibility() & ~mask) | flags);
        }
        statusColor = status; navigationColor = navigation;
        appliedIcons = (lightStatusIcons ? 1 : 0) | (lightNavigationIcons ? 2 : 0);
        content.setBackgroundColor(contentColor);
        setBackgroundColor(contentColor);
        invalidate();
        return true;
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        paint.setColor(statusColor);
        canvas.drawRect(0, 0, getWidth(), top, paint);
        paint.setColor(navigationColor);
        canvas.drawRect(0, getHeight() - bottom, getWidth(), getHeight(), paint);
        canvas.drawRect(0, top, left, getHeight() - bottom, paint);
        canvas.drawRect(getWidth() - right, top, getWidth(), getHeight() - bottom, paint);
    }
    public String diagnostics() {
        return " safe=" + left + "," + top + "," + right + "," + bottom +
            " imeBottom=" + imeBottom + " statusArgb=" + Integer.toUnsignedString(statusColor) +
            " navigationArgb=" + Integer.toUnsignedString(navigationColor) +
            " barLightIcons=" + appliedIcons;
    }
    private static void requireUi() {
        if (Looper.myLooper() != Looper.getMainLooper())
            throw new IllegalStateException("SystemAppearance requires the Android UI thread");
    }
    private static final class Api29 {
        static void configure(Window w, View root) {
            w.setStatusBarContrastEnforced(false);
            w.setNavigationBarContrastEnforced(false);
            root.setForceDarkAllowed(false); // C++ owns its exact theme colors.
        }
    }
    private static final class Api30 {
        static void edgeToEdge(Window w) { w.setDecorFitsSystemWindows(false); }
        static boolean icons(Window w, boolean statusLight, boolean navigationLight) {
            WindowInsetsController c = w.getInsetsController();
            if (c == null) return false;
            int mask = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS |
                WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
            int flags = (!statusLight ? WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS : 0) |
                (!navigationLight ? WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS : 0);
            c.setSystemBarsAppearance(flags, mask);
            return (c.getSystemBarsAppearance() & mask) == flags;
        }
    }
}
