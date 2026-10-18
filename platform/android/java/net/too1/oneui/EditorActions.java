package net.too1.oneui;

import android.graphics.Rect;
import android.view.ActionMode;
import android.view.ViewConfiguration;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.InputConnection;
import java.text.BreakIterator;

/** Native floating selection menu and locale-aware long-press word selection. */
final class EditorActions extends ActionMode.Callback2 {
    private final EditorSurface surface;
    private final int touchSlop;
    private final Runnable longPress = this::longPress;
    private float pressX, pressY;
    private ActionMode mode;
    private long session;
    private long downSession;
    private boolean handled;
    private int x, y;
    EditorActions(EditorSurface surface) {
        this.surface = surface;
        touchSlop = ViewConfiguration.get(surface.getContext()).getScaledTouchSlop();
    }
    private void longPress() {
        if (!surface.containsEditor(pressX, pressY)) return;
        EditorSurface.Snapshot state = surface.editing();
        if (state == null || state.session == 0 || state.session != downSession) return;
        if (!state.sensitive && surface.selectAt(state.session, 0, pressX, pressY)) {
            state = surface.editing();
            if (state != null && !state.text.isEmpty()) {
                int at = Math.min(state.caret, state.text.length()-1);
                BreakIterator words = BreakIterator.getWordInstance(surface.getResources().getConfiguration().getLocales().get(0));
                words.setText(state.text);
                int start = words.preceding(at+1), end = words.following(at);
                InputConnection c = surface.editorConnection();
                if (c != null && start >= 0 && end > start) c.setSelection(start, end);
            }
        }
        show(pressX, pressY);
        handled = mode != null;
        if (handled) surface.captureEditingGesture();
        surface.showSelection();
    }
    void show(float px, float py) {
        dismiss();
        EditorSurface.Snapshot state = surface.editing();
        if (state == null || state.session == 0) return;
        session = state.session; x = (int)px; y = (int)py;
        mode = surface.startActionMode(this, ActionMode.TYPE_FLOATING);
    }
    boolean touch(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            dismiss(); handled = false;
            EditorSurface.Snapshot state = surface.editing();
            downSession = state == null ? 0 : state.session;
            pressX = event.getX(); pressY = event.getY();
            // As with View's long-click timer, count from dispatch rather than
            // an aged event timestamp after a busy layout. Queued UP cancels it.
            if (downSession != 0) surface.postDelayed(longPress, ViewConfiguration.getLongPressTimeout());
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL ||
                   event.getPointerCount() != 1 || Math.hypot(event.getX()-pressX, event.getY()-pressY) > touchSlop) {
            surface.removeCallbacks(longPress); downSession = 0;
        }
        return handled;
    }
    boolean captured(MotionEvent event) {
        int action = event.getActionMasked();
        if (!handled || action == MotionEvent.ACTION_DOWN) return false;
        touch(event);
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) handled = false;
        return true;
    }
    void dismiss() {
        cancelPress();
        if (mode != null) { mode.finish(); mode = null; }
    }
    void cancelPress() { surface.removeCallbacks(longPress); downSession = 0; }
    boolean back() {
        if (mode == null) return false;
        dismiss(); return true;
    }
    @Override public boolean onCreateActionMode(ActionMode mode, Menu menu) { return populate(menu); }
    @Override public boolean onPrepareActionMode(ActionMode mode, Menu menu) { return populate(menu); }
    private boolean populate(Menu menu) {
        EditorSurface.Snapshot state = surface.editing();
        if (state == null || state.session != session) return false;
        menu.clear();
        menu.add(0, android.R.id.selectAll, 0, android.R.string.selectAll);
        if (!state.sensitive && state.anchor != state.caret) {
            menu.add(0, android.R.id.copy, 1, android.R.string.copy);
            menu.add(0, android.R.id.cut, 2, android.R.string.cut);
        }
        menu.add(0, android.R.id.paste, 3, android.R.string.paste);
        return true;
    }
    @Override public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        EditorSurface.Snapshot state = surface.editing();
        if (state == null || state.session != session) { dismiss(); return false; }
        InputConnection connection = surface.editorConnection();
        if (connection == null || !connection.performContextMenuAction(item.getItemId())) return false;
        if (item.getItemId() == android.R.id.selectAll) { mode.invalidate(); surface.showSelection(); }
        else dismiss();
        return true;
    }
    @Override public void onGetContentRect(ActionMode mode, View view, Rect rect) {
        rect.set(x - 1, y - 1, x + 1, y + 1);
    }
    @Override public void onDestroyActionMode(ActionMode destroyed) { if (mode == destroyed) mode = null; }
}
