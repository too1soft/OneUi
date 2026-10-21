package net.too1.oneui;

import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.inputmethod.InputConnection;

/** Framework handle artwork over the native GPU surface. Text geometry and
 * selection live in OneUI; this class owns only Android gesture presentation. */
final class EditorSelection extends Drawable {
    private final EditorSurface surface;
    private final Drawable insertion, left, right;
    private final RectF[] targets = {new RectF(), new RectF()};
    private final RectF[] artwork = {new RectF(), new RectF()};
    private EditorSurface.SelectionGeometry geometry;
    private boolean visible, swallowing;
    private int dragged = -1, pointer, startAnchor, startCaret;
    private long session;
    private Integer tint;
    private float grabX, grabY;
    private final float density;
    EditorSelection(EditorSurface surface) {
        this.surface = surface;
        density = surface.getResources().getDisplayMetrics().density;
        TypedArray a = surface.getContext().obtainStyledAttributes(new int[]{
            android.R.attr.textSelectHandle, android.R.attr.textSelectHandleLeft,
            android.R.attr.textSelectHandleRight});
        insertion = drawable(a, 0); left = drawable(a, 1); right = drawable(a, 2);
        a.recycle();
        surface.getOverlay().add(this);
    }
    private static Drawable drawable(TypedArray a, int index) {
        Drawable value = a.getDrawable(index);
        return value == null ? null : value.mutate();
    }
    void color(int color) {
        if (tint != null && tint == color) return;
        tint = color;
        for (Drawable d : new Drawable[]{insertion, left, right}) if (d != null) d.setTint(color);
        invalidateSelf();
    }
    void show() { visible = true; update(); }
    void hide() {
        cancel(); visible = false; geometry = null;
        for (RectF r : targets) r.setEmpty();
        invalidateSelf();
    }
    boolean active() { return dragged >= 0; }
    void cancelGesture() { if (active()) hide(); }
    boolean dismiss() { boolean was = visible; hide(); return was; }
    void update() {
        if (!visible) return;
        setBounds(0, 0, surface.getWidth(), surface.getHeight());
        EditorSurface.SelectionGeometry next = surface.selectionGeometry();
        if (next == null || (active() && next.session != session)) { hide(); return; }
        if (geometry != null && geometry.same(next)) return;
        geometry = next;
        for (int i = 0; i < 2; ++i) layout(i);
        invalidateSelf();
    }
    private RectF caret(int i) { return i == 0 ? geometry.anchorRect : geometry.caretRect; }
    private Drawable art(int i) {
        if (geometry.anchor == geometry.caret) return insertion;
        return (i == 0) == (geometry.anchor < geometry.caret) ? left : right;
    }
    private void layout(int i) {
        targets[i].setEmpty(); artwork[i].setEmpty();
        if (i == 1 && geometry.anchor == geometry.caret) return;
        RectF r = caret(i), clip = geometry.clip;
        Drawable d = art(i);
        if (d == null || r.left < clip.left - 1 || r.left > clip.right + 1 ||
            r.centerY() < clip.top || r.centerY() > clip.bottom) return;
        float w = Math.max(16 * density, d.getIntrinsicWidth());
        float h = Math.max(20 * density, d.getIntrinsicHeight());
        // Framework selection artwork has its tip at 3/4 (start) or 1/4 (end)
        // of the drawable, not at its outer edge. Match Android Editor hotspots.
        float hotspot = geometry.anchor == geometry.caret ? .5f : d == left ? .75f : .25f;
        float x = r.left - w * hotspot;
        artwork[i].set(x, r.bottom, x + w, r.bottom + h);
        // Touch slop extends below the text; tapping the glyph itself continues
        // to position the caret instead of accidentally grabbing an old handle.
        float center = artwork[i].centerX();
        targets[i].set(center - 24*density, r.bottom, center + 24*density, r.bottom + 48*density);
    }
    float[] bounds() {
        update();
        return new float[]{targets[0].left, targets[0].top, targets[0].right, targets[0].bottom,
                           targets[1].left, targets[1].top, targets[1].right, targets[1].bottom};
    }
    private int hit(float x, float y) {
        int best = -1; float distance = Float.MAX_VALUE;
        for (int i = 0; i < 2; ++i) if (targets[i].contains(x, y)) {
            float dx = x-artwork[i].centerX(), dy = y-artwork[i].centerY();
            if (dx*dx+dy*dy < distance) { best = i; distance = dx*dx+dy*dy; }
        }
        return best;
    }
    boolean touch(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            swallowing = false; update();
            int index = visible && geometry != null ? hit(event.getX(), event.getY()) : -1;
            if (index < 0) { hide(); return false; }
            surface.dismissActionMenu();
            dragged = geometry.anchor == geometry.caret ? 0 : index + 1;
            pointer = event.getPointerId(0); session = geometry.session;
            startAnchor = geometry.anchor; startCaret = geometry.caret;
            RectF c = caret(index);
            grabX = event.getX() - c.left; grabY = event.getY() - c.centerY();
            swallowing = true; return true;
        }
        if (!swallowing) return false;
        int index = event.findPointerIndex(pointer);
        if (action == MotionEvent.ACTION_CANCEL || event.getPointerCount() != 1 || index < 0) {
            cancel();
        } else if (active() && action == MotionEvent.ACTION_MOVE) {
            if (!surface.selectAt(session, dragged, event.getX(index)-grabX, event.getY(index)-grabY)) hide();
            else update();
        } else if (active() && action == MotionEvent.ACTION_UP) {
            dragged = -1;
            update();
            if (geometry != null) surface.showActionMenu(geometry.caretRect.centerX(), geometry.caretRect.top);
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) swallowing = false;
        return true;
    }
    private void cancel() {
        if (!active()) return;
        dragged = -1;
        EditorSurface.Snapshot live = surface.editing();
        if (live != null && live.session == session) {
            InputConnection connection = surface.editorConnection();
            if (connection != null) connection.setSelection(startAnchor, startCaret);
        }
    }
    @Override public void draw(Canvas canvas) {
        if (!visible || geometry == null) return;
        for (int i = 0; i < 2; ++i) if (!targets[i].isEmpty()) {
            Drawable d = art(i); RectF b = artwork[i];
            d.setBounds(Math.round(b.left), Math.round(b.top), Math.round(b.right), Math.round(b.bottom));
            d.draw(canvas);
        }
    }
    @Override public void setAlpha(int alpha) { }
    @Override public void setColorFilter(ColorFilter filter) { }
    @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
}
