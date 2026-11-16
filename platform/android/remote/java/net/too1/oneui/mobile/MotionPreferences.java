package net.too1.oneui.mobile;

import android.animation.ValueAnimator;

/** Reads Android's system animation preference without changing settings. */
public final class MotionPreferences {
    private MotionPreferences() { }
    public static boolean reduced() { return !ValueAnimator.areAnimatorsEnabled(); }
}
