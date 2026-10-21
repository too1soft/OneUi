package net.too1.oneui;

import android.content.Context;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.os.Looper;
import android.text.Editable;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.SurfaceView;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import java.util.function.BooleanSupplier;

/** Real Android IME connection to a OneUI GPU editor; no hidden native EditText.
 * Calls stay on the UI thread. Bridge ownership stays with the Activity.
 * Call sync(true) after a user touch, sync(false) after native edits/layout,
 * suspend() before host background/destruction, and resume() on activation.
 */
public final class EditorSurface extends SurfaceView {
    public static final class SelectionGeometry {
        public final long session;
        public final int anchor, caret;
        public final android.graphics.RectF clip, anchorRect, caretRect;
        public SelectionGeometry(long session, int anchor, int caret, float[] rects) {
            if (rects == null || rects.length != 12) throw new IllegalArgumentException("Expected three rectangles");
            this.session = session; this.anchor = anchor; this.caret = caret;
            clip = rect(rects, 0); anchorRect = rect(rects, 4); caretRect = rect(rects, 8);
        }
        private static android.graphics.RectF rect(float[] r, int i) {
            return new android.graphics.RectF(r[i], r[i+1], r[i+2], r[i+3]);
        }
        boolean same(SelectionGeometry b) {
            return session == b.session && anchor == b.anchor && caret == b.caret &&
                clip.equals(b.clip) && anchorRect.equals(b.anchorRect) && caretRect.equals(b.caretRect);
        }
    }
    public static final class Snapshot {
        public final long session;
        public final String text;
        public final int anchor, caret, composingStart, composingEnd;
        public final boolean multiline, sensitive;
        public Snapshot(long session, String text, int anchor, int caret, int start, int end, boolean multiline) {
            this(session, text, anchor, caret, start, end, multiline, false);
        }
        public Snapshot(long session, String text, int anchor, int caret, int start, int end, boolean multiline, boolean sensitive) {
            this.session = session; this.text = text; this.anchor = anchor; this.caret = caret;
            composingStart = start; composingEnd = end; this.multiline = multiline; this.sensitive = sensitive;
        }
    }
    public interface Bridge {
        Snapshot snapshot();
        boolean apply(long session, String text, int anchor, int caret, int start, int end);
        default boolean sensitive(long session, int command, String text, int first, int second) { return false; }
        default boolean containsEditor(float x, float y) { return false; }
        default SelectionGeometry selectionGeometry() { return null; }
        default boolean selectAt(long session, int edge, float x, float y) { return false; }
        default void cancelPointers() { }
        void changed();
    }
    private final Bridge bridge;
    private final InputMethodManager manager;
    private Snapshot current;
    private InputConnection connection;
    private final EditorActions actions;
    private final EditorSelection selection;
    private boolean active = true;
    public EditorSurface(Context context, Bridge bridge) {
        super(context);
        this.bridge = bridge;
        actions = new EditorActions(this);
        manager = (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
        selection = new EditorSelection(this);
        setFocusable(true); setFocusableInTouchMode(true);
        // Stylus handwriting/geometry is not exposed by this adapter yet.
        if (android.os.Build.VERSION.SDK_INT >= 33) setAutoHandwritingEnabled(false);
    }
    private static void requireUi() {
        if (Looper.myLooper() != Looper.getMainLooper())
            throw new IllegalStateException("EditorSurface requires the UI thread");
    }
    public void resume() { requireUi(); active = true; sync(false); }
    public void suspend() {
        requireUi(); selection.hide(); active = false;
        if (connection != null) connection.closeConnection();
        actions.dismiss();
        connection = null; current = null;
        manager.hideSoftInputFromWindow(getWindowToken(), 0);
        manager.restartInput(this);
    }
    public void sync(boolean showKeyboard) {
        requireUi();
        if (!active) return;
        Snapshot next = bridge.snapshot();
        long previous = current == null ? 0 : current.session;
        current = next;
        long session = next == null ? 0 : next.session;
        if (previous != session) {
            selection.hide();
            actions.dismiss();
            if (connection != null) connection.closeConnection();
            connection = null;
            if (session != 0) requestFocus();
            manager.restartInput(this);
            if (session == 0) manager.hideSoftInputFromWindow(getWindowToken(), 0);
        }
        if (showKeyboard && session != 0) {
            requestFocus(); manager.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT);
        }
        selection.update();
    }
    @Override public boolean onCheckIsTextEditor() { return active && current != null && current.session != 0; }
    @Override public InputConnection onCreateInputConnection(EditorInfo info) {
        requireUi();
        if (!onCheckIsTextEditor()) return null;
        info.inputType = InputType.TYPE_CLASS_TEXT | (current.sensitive ?
            InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS :
            current.multiline ? InputType.TYPE_TEXT_FLAG_MULTI_LINE : 0);
        info.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN |
            EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING |
            (current.multiline ? EditorInfo.IME_ACTION_NONE : EditorInfo.IME_ACTION_DONE);
        info.initialSelStart = current.anchor; info.initialSelEnd = current.caret;
        if (connection != null) connection.closeConnection();
        connection = current.sensitive ? new SensitiveConnection(this, current) : new Connection(current);
        return connection;
    }
    @Override public boolean onKeyDown(int key, KeyEvent event) {
        if (connection != null && connection.sendKeyEvent(event)) return true;
        return super.onKeyDown(key, event);
    }
    @Override public boolean onKeyUp(int key, KeyEvent event) {
        if (connection != null && connection.sendKeyEvent(event)) return true;
        return super.onKeyUp(key, event);
    }
    /** Call after native pointer handling and sync(false). True consumes the
     * keyboard request on finger-up, so a long press keeps its menu visible. */
    public boolean onEditorTouch(android.view.MotionEvent event) {
        boolean handled = actions.touch(event);
        if (event.getActionMasked() == android.view.MotionEvent.ACTION_UP &&
            containsEditor(event.getX(), event.getY())) selection.show();
        return handled;
    }
    /** Call before forwarding native pointers. A grabbed handle owns the stream. */
    public boolean beforeEditorTouch(android.view.MotionEvent event) {
        return active && (selection.touch(event) || actions.captured(event));
    }
    public void cancelSelectionGesture() { requireUi(); actions.cancelPress(); selection.cancelGesture(); }
    public void setSelectionHandleColor(int color) { requireUi(); selection.color(color); }
    /** View-local physical pixels, two L/T/R/B rectangles; empty means hidden. */
    public float[] getSelectionHandleBounds() { requireUi(); return selection.bounds(); }
    public boolean isSelectionGestureActive() { return selection.active(); }
    public SelectionGeometry getSelectionGeometry() { requireUi(); return selectionGeometry(); }
    /** Give the edit menu priority over application page navigation. */
    public boolean dismissEditingActions() {
        requireUi(); boolean menu = actions.back(); return selection.dismiss() || menu;
    }
    void dismissActionMenu() { actions.dismiss(); }
    void showActionMenu(float x, float y) { actions.show(x, y); }
    void showSelection() { selection.show(); }
    void captureEditingGesture() { bridge.cancelPointers(); }
    SelectionGeometry selectionGeometry() { return active ? bridge.selectionGeometry() : null; }
    boolean selectAt(long session, int edge, float x, float y) {
        if (!active || !bridge.selectAt(session, edge, x, y)) return false;
        Snapshot next = editing();
        if (next == null || next.session != session) return false;
        InputConnection c = editorConnection();
        if (c == null || !c.setSelection(next.anchor, next.caret)) return false;
        current = next; bridge.changed(); return true;
    }
    Snapshot editing() { return active ? bridge.snapshot() : null; }
    boolean containsEditor(float x, float y) { return active && bridge.containsEditor(x, y); }
    InputConnection editorConnection() {
        if (connection == null) onCreateInputConnection(new EditorInfo());
        return connection;
    }
    boolean sensitiveEdit(long session, int command, String text, int first, int second) {
        requireUi();
        return active && bridge.sensitive(session, command, text, first, second);
    }
    void editingChanged(Snapshot next) {
        current = next;
        manager.updateSelection(this, next.anchor, next.caret, next.composingStart, next.composingEnd);
        bridge.changed();
    }
    void hideKeyboard() { manager.hideSoftInputFromWindow(getWindowToken(), 0); }
    private final class Connection extends BaseInputConnection {
        final long session;
        final Editable editable;
        boolean closed, constructing = true;
        int batch;
        Connection(Snapshot state) {
            super(EditorSurface.this, true);
            session = state.session;
            editable = new SpannableStringBuilder(state.text);
            Selection.setSelection(editable, state.anchor, state.caret);
            if (state.composingStart >= 0) super.setComposingRegion(state.composingStart, state.composingEnd);
            constructing = false;
        }
        boolean valid() {
            requireUi();
            if (closed || !active) return false;
            Snapshot live = bridge.snapshot();
            return live != null && live.session == session;
        }
        @Override public Editable getEditable() { return editable; }
        @Override public CharSequence getTextBeforeCursor(int length, int flags) {
            return valid() ? super.getTextBeforeCursor(length, flags) : null;
        }
        @Override public CharSequence getTextAfterCursor(int length, int flags) {
            return valid() ? super.getTextAfterCursor(length, flags) : null;
        }
        @Override public CharSequence getSelectedText(int flags) {
            return valid() ? super.getSelectedText(flags) : null;
        }
        @Override public int getCursorCapsMode(int modes) {
            return valid() ? super.getCursorCapsMode(modes) : 0;
        }
        boolean flush() {
            if (constructing || batch > 0) return true;
            if (!valid()) return false;
            int anchor = Selection.getSelectionStart(editable), caret = Selection.getSelectionEnd(editable);
            int start = getComposingSpanStart(editable), end = getComposingSpanEnd(editable);
            boolean accepted = bridge.apply(session, editable.toString(), anchor, caret, start, end);
            if (accepted) {
                current = bridge.snapshot();
                if (current != null && current.composingStart < 0 &&
                    (!current.text.contentEquals(editable) || current.anchor != anchor || current.caret != caret)) {
                    editable.replace(0, editable.length(), current.text);
                    Selection.setSelection(editable, current.anchor, current.caret);
                    anchor = current.anchor; caret = current.caret;
                }
                manager.updateSelection(EditorSurface.this, anchor, caret, start, end);
                bridge.changed();
            } else {
                closed = true;
                sync(false); // Reject malformed/stale state, never keep a divergent IME buffer.
                manager.restartInput(EditorSurface.this);
            }
            return accepted;
        }
        boolean mutate(BooleanSupplier change) {
            if (!valid()) return false;
            ++batch;
            boolean result;
            try { result = change.getAsBoolean(); } finally { --batch; }
            return flush() && result;
        }
        @Override public boolean beginBatchEdit() { if (!constructing && !valid()) return false; ++batch; return true; }
        @Override public boolean endBatchEdit() {
            if (batch <= 0) return false;
            --batch; boolean ok = flush(); return ok && batch > 0;
        }
        @Override public boolean commitText(CharSequence value, int cursor) { return mutate(() -> super.commitText(value, cursor)); }
        @Override public boolean setComposingText(CharSequence value, int cursor) { return mutate(() -> super.setComposingText(value, cursor)); }
        @Override public boolean setComposingRegion(int start, int end) { return mutate(() -> super.setComposingRegion(start, end)); }
        @Override public boolean finishComposingText() { return mutate(() -> super.finishComposingText()); }
        @Override public boolean setSelection(int start, int end) { return mutate(() -> super.setSelection(start, end)); }
        @Override public boolean deleteSurroundingText(int before, int after) { return mutate(() -> super.deleteSurroundingText(before, after)); }
        @Override public boolean deleteSurroundingTextInCodePoints(int before, int after) {
            return mutate(() -> super.deleteSurroundingTextInCodePoints(before, after));
        }
        @Override public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
            if (!valid()) return null;
            ExtractedText result = new ExtractedText();
            result.text = editable.toString(); result.startOffset = 0;
            result.partialStartOffset = result.partialEndOffset = -1;
            result.selectionStart = Selection.getSelectionStart(editable);
            result.selectionEnd = Selection.getSelectionEnd(editable);
            result.flags = current.multiline ? 0 : ExtractedText.FLAG_SINGLE_LINE;
            return result;
        }
        @Override public boolean performEditorAction(int action) {
            if (!valid()) return false;
            if (action == EditorInfo.IME_ACTION_DONE) {
                finishComposingText(); manager.hideSoftInputFromWindow(getWindowToken(), 0); return true;
            }
            return false;
        }
        @Override public boolean sendKeyEvent(KeyEvent event) {
            if (!valid()) return false;
            int key = event.getKeyCode();
            boolean supported = key == KeyEvent.KEYCODE_DEL || key == KeyEvent.KEYCODE_FORWARD_DEL ||
                key == KeyEvent.KEYCODE_ENTER || key == KeyEvent.KEYCODE_DPAD_LEFT ||
                key == KeyEvent.KEYCODE_DPAD_RIGHT || event.getUnicodeChar() > 0;
            if (event.getAction() == KeyEvent.ACTION_UP) return supported;
            if (event.getAction() != KeyEvent.ACTION_DOWN || !supported) return false;
            int a = Selection.getSelectionStart(editable), b = Selection.getSelectionEnd(editable);
            if (key == KeyEvent.KEYCODE_DEL || key == KeyEvent.KEYCODE_FORWARD_DEL) {
                if (a != b) return commitText("", 1);
                return deleteSurroundingTextInCodePoints(key == KeyEvent.KEYCODE_DEL ? 1 : 0,
                                                        key == KeyEvent.KEYCODE_FORWARD_DEL ? 1 : 0);
            }
            if (key == KeyEvent.KEYCODE_ENTER)
                return current.multiline ? commitText("\n", 1) : performEditorAction(EditorInfo.IME_ACTION_DONE);
            if (key == KeyEvent.KEYCODE_DPAD_LEFT || key == KeyEvent.KEYCODE_DPAD_RIGHT) {
                int direction = key == KeyEvent.KEYCODE_DPAD_LEFT ? -1 : 1;
                int caret = b;
                if ((direction < 0 && b > 0) || (direction > 0 && b < editable.length()))
                    caret = Character.offsetByCodePoints(editable, b, direction);
                return setSelection(event.isShiftPressed() ? a : caret, caret);
            }
            int code = event.getUnicodeChar();
            return Character.isValidCodePoint(code) && commitText(new String(Character.toChars(code)), 1);
        }
        @Override public boolean performContextMenuAction(int id) {
            if (!valid()) return false;
            if (id == android.R.id.selectAll) return setSelection(0, editable.length());
            ClipboardManager clipboard = (ClipboardManager) getContext().getSystemService(Context.CLIPBOARD_SERVICE);
            int start = Math.min(Selection.getSelectionStart(editable), Selection.getSelectionEnd(editable));
            int end = Math.max(Selection.getSelectionStart(editable), Selection.getSelectionEnd(editable));
            if (id == android.R.id.copy || id == android.R.id.cut) {
                clipboard.setPrimaryClip(ClipData.newPlainText("", editable.subSequence(start, end)));
                return id == android.R.id.copy || commitText("", 1);
            }
            if (id == android.R.id.paste) {
                ClipData clip = clipboard.getPrimaryClip();
                return clip != null && clip.getItemCount() > 0 && commitText(clip.getItemAt(0).coerceToText(getContext()), 1);
            }
            return false;
        }
        @Override public void closeConnection() { closed = true; batch = 0; editable.clear(); }
    }
}
