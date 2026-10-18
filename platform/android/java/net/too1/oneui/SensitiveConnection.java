package net.too1.oneui;

import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.Context;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.SurroundingText;

/** Password commands only. Never mirrors the native password into an Editable.
 * Incoming characters necessarily pass through the IME; existing contents do not.
 */
final class SensitiveConnection extends BaseInputConnection {
    private final EditorSurface surface;
    private final long session;
    private EditorSurface.Snapshot state;
    private boolean closed;
    private int batch;
    SensitiveConnection(EditorSurface surface, EditorSurface.Snapshot state) {
        super(surface, true);
        this.surface = surface; this.state = state; session = state.session;
    }
    private boolean valid() {
        EditorSurface.Snapshot live = surface.editing();
        return !closed && live != null && live.sensitive && live.session == session;
    }
    private boolean edit(int command, CharSequence text, int a, int b) {
        if (!valid() || text == null) return false;
        if (!surface.sensitiveEdit(session, command, text.toString(), a, b)) return false;
        state = surface.editing();
        if (!valid()) { closeConnection(); surface.sync(false); return false; }
        if (batch == 0) surface.editingChanged(state);
        return true;
    }
    @Override public Editable getEditable() { return null; }
    @Override public CharSequence getTextBeforeCursor(int n, int flags) { return null; }
    @Override public CharSequence getTextAfterCursor(int n, int flags) { return null; }
    @Override public CharSequence getSelectedText(int flags) { return null; }
    @Override public SurroundingText getSurroundingText(int before, int after, int flags) { return null; }
    @Override public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) { return null; }
    @Override public int getCursorCapsMode(int modes) { return 0; }
    @Override public boolean commitText(CharSequence text, int cursor) { return edit(0, text, cursor, 0); }
    @Override public boolean setComposingText(CharSequence text, int cursor) { return edit(1, text, cursor, 0); }
    @Override public boolean setSelection(int a, int b) { return edit(2, "", a, b); }
    @Override public boolean setComposingRegion(int a, int b) { return edit(3, "", a, b); }
    @Override public boolean finishComposingText() { return edit(4, "", 0, 0); }
    @Override public boolean deleteSurroundingText(int a, int b) { return edit(5, "", a, b); }
    @Override public boolean deleteSurroundingTextInCodePoints(int a, int b) { return edit(6, "", a, b); }
    @Override public boolean beginBatchEdit() { if (!valid()) return false; ++batch; return true; }
    @Override public boolean endBatchEdit() {
        if (!valid() || batch == 0) return false;
        if (--batch == 0) surface.editingChanged(state);
        return batch > 0;
    }
    @Override public boolean performContextMenuAction(int id) {
        if (!valid()) return false;
        if (id == android.R.id.selectAll) return setSelection(0, state.text.length());
        // Even a direct InputConnection call cannot copy or cut passwords.
        if (id != android.R.id.paste) return false;
        ClipboardManager clipboard = (ClipboardManager) surface.getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clip = clipboard.getPrimaryClip();
        return clip != null && clip.getItemCount() > 0 &&
            commitText(clip.getItemAt(0).coerceToText(surface.getContext()), 1);
    }
    @Override public boolean performEditorAction(int action) {
        if (!valid() || action != EditorInfo.IME_ACTION_DONE || !finishComposingText()) return false;
        surface.hideKeyboard(); return true;
    }
    @Override public boolean sendKeyEvent(KeyEvent event) {
        if (!valid()) return false;
        int key = event.getKeyCode(), unicode = event.getUnicodeChar();
        boolean supported = key == KeyEvent.KEYCODE_DEL || key == KeyEvent.KEYCODE_FORWARD_DEL ||
            key == KeyEvent.KEYCODE_DPAD_LEFT || key == KeyEvent.KEYCODE_DPAD_RIGHT ||
            key == KeyEvent.KEYCODE_ENTER || unicode > 0;
        if (event.getAction() == KeyEvent.ACTION_UP) return supported;
        if (event.getAction() != KeyEvent.ACTION_DOWN || !supported) return false;
        if (key == KeyEvent.KEYCODE_DEL || key == KeyEvent.KEYCODE_FORWARD_DEL) {
            if (state.anchor != state.caret) return commitText("", 1);
            return deleteSurroundingTextInCodePoints(key == KeyEvent.KEYCODE_DEL ? 1 : 0,
                                                    key == KeyEvent.KEYCODE_FORWARD_DEL ? 1 : 0);
        }
        if (key == KeyEvent.KEYCODE_DPAD_LEFT || key == KeyEvent.KEYCODE_DPAD_RIGHT)
            return edit(7, "", key == KeyEvent.KEYCODE_DPAD_LEFT ? -1 : 1, event.isShiftPressed() ? 1 : 0);
        if (key == KeyEvent.KEYCODE_ENTER) return performEditorAction(EditorInfo.IME_ACTION_DONE);
        return Character.isValidCodePoint(unicode) && commitText(new String(Character.toChars(unicode)), 1);
    }
    @Override public void closeConnection() { closed = true; state = null; batch = 0; }
}
