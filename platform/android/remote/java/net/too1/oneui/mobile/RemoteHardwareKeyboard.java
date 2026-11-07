package net.too1.oneui.mobile;

import static android.view.KeyEvent.*;

import android.view.KeyEvent;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.function.BiConsumer;

/** Maps hardware keyboard events to the USB HID usages used by remote desktops. */
public final class RemoteHardwareKeyboard {
    @FunctionalInterface
    public interface Sink {
        void accept(int usage, boolean pressed, int locks);
    }
    private static final int KEYBOARD_PAGE = 0x070000;
    private final Set<Integer> held = new LinkedHashSet<>();
    private final Sink sink;
    private boolean enabled;
    private int locks;

    public RemoteHardwareKeyboard(BiConsumer<Integer, Boolean> sink) {
        this((key, pressed, locks) -> sink.accept(key, pressed));
    }

    public RemoteHardwareKeyboard(Sink sink) {
        this.sink = sink;
    }

    public void setEnabled(boolean value) {
        enabled = value;
        if (!value) release();
    }

    public void release() {
        for (int key : held) sink.accept(key, false, locks);
        held.clear();
    }

    public boolean dispatch(KeyEvent event) {
        if (!enabled || (event.getFlags() & FLAG_SOFT_KEYBOARD) != 0) return false;
        int usage = keyboardUsage(event.getKeyCode());
        if (usage == 0) return false;
        locks = (event.isCapsLockOn() ? 2 : 0) | (event.isNumLockOn() ? 4 : 0);
        int key = KEYBOARD_PAGE | usage;
        if (event.getAction() == ACTION_DOWN) {
            // A repeated down after focus loss must not re-arm a released key.
            if (event.getRepeatCount() == 0) held.add(key);
            if (held.contains(key)) sink.accept(key, true, locks);
            return true;
        }
        if (event.getAction() != ACTION_UP) return false;
        if (held.remove(key)) sink.accept(key, false, locks);
        return true;
    }

    private static int keyboardUsage(int key) {
        if (key >= KEYCODE_A && key <= KEYCODE_Z) return 0x04 + key - KEYCODE_A;
        if (key >= KEYCODE_1 && key <= KEYCODE_9) return 0x1e + key - KEYCODE_1;
        if (key >= KEYCODE_F1 && key <= KEYCODE_F12) return 0x3a + key - KEYCODE_F1;
        if (key >= KEYCODE_NUMPAD_1 && key <= KEYCODE_NUMPAD_9) return 0x59 + key - KEYCODE_NUMPAD_1;
        return switch (key) {
            case KEYCODE_0 -> 0x27;
            case KEYCODE_ENTER -> 0x28;
            case KEYCODE_ESCAPE -> 0x29;
            case KEYCODE_DEL -> 0x2a;
            case KEYCODE_TAB -> 0x2b;
            case KEYCODE_SPACE -> 0x2c;
            case KEYCODE_MINUS -> 0x2d;
            case KEYCODE_EQUALS -> 0x2e;
            case KEYCODE_LEFT_BRACKET -> 0x2f;
            case KEYCODE_RIGHT_BRACKET -> 0x30;
            case KEYCODE_BACKSLASH -> 0x31;
            case KEYCODE_SEMICOLON -> 0x33;
            case KEYCODE_APOSTROPHE -> 0x34;
            case KEYCODE_GRAVE -> 0x35;
            case KEYCODE_COMMA -> 0x36;
            case KEYCODE_PERIOD -> 0x37;
            case KEYCODE_SLASH -> 0x38;
            case KEYCODE_CAPS_LOCK -> 0x39;
            case KEYCODE_SYSRQ -> 0x46;
            case KEYCODE_SCROLL_LOCK -> 0x47;
            case KEYCODE_BREAK -> 0x48;
            case KEYCODE_INSERT -> 0x49;
            case KEYCODE_MOVE_HOME -> 0x4a;
            case KEYCODE_PAGE_UP -> 0x4b;
            case KEYCODE_FORWARD_DEL -> 0x4c;
            case KEYCODE_MOVE_END -> 0x4d;
            case KEYCODE_PAGE_DOWN -> 0x4e;
            case KEYCODE_DPAD_RIGHT -> 0x4f;
            case KEYCODE_DPAD_LEFT -> 0x50;
            case KEYCODE_DPAD_DOWN -> 0x51;
            case KEYCODE_DPAD_UP -> 0x52;
            case KEYCODE_NUM_LOCK -> 0x53;
            case KEYCODE_NUMPAD_DIVIDE -> 0x54;
            case KEYCODE_NUMPAD_MULTIPLY -> 0x55;
            case KEYCODE_NUMPAD_SUBTRACT -> 0x56;
            case KEYCODE_NUMPAD_ADD -> 0x57;
            case KEYCODE_NUMPAD_ENTER -> 0x58;
            case KEYCODE_NUMPAD_0 -> 0x62;
            case KEYCODE_NUMPAD_DOT -> 0x63;
            case KEYCODE_CTRL_LEFT -> 0xe0;
            case KEYCODE_SHIFT_LEFT -> 0xe1;
            case KEYCODE_ALT_LEFT -> 0xe2;
            case KEYCODE_META_LEFT -> 0xe3;
            case KEYCODE_CTRL_RIGHT -> 0xe4;
            case KEYCODE_SHIFT_RIGHT -> 0xe5;
            case KEYCODE_ALT_RIGHT -> 0xe6;
            case KEYCODE_META_RIGHT -> 0xe7;
            default -> 0;
        };
    }
}
