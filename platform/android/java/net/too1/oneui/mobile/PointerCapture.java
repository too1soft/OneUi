package net.too1.oneui.mobile;

import android.graphics.RectF;
import android.view.MotionEvent;
import org.json.JSONObject;
import java.nio.charset.StandardCharsets;
import java.util.function.Consumer;

/** Raw multi-pointer capture for shared remote canvases. Coordinates are dp. */
public final class PointerCapture {
    private final RectF bounds = new RectF();
    private boolean captured;
    private RectF[] exclusions=new RectF[0];
    public void exclusions(float[] rectangles) {
        if(rectangles==null||rectangles.length%4!=0||rectangles.length>64)throw new IllegalArgumentException("Bounded exclusion rectangles required");
        RectF[] next=new RectF[rectangles.length/4];
        for(int i=0;i<next.length;++i){int n=i*4;
            for(int j=0;j<4;++j)if(!Float.isFinite(rectangles[n+j]))throw new IllegalArgumentException("Finite rectangle required");
            if(rectangles[n+2]<0||rectangles[n+3]<0)throw new IllegalArgumentException("Positive rectangle required");
            next[i]=new RectF(rectangles[n],rectangles[n+1],rectangles[n]+rectangles[n+2],rectangles[n+1]+rectangles[n+3]);}
        exclusions=next;
    }
    public void bounds(float x, float y, float width, float height) { bounds.set(x, y, x+width, y+height); }
    public void clear() { bounds.setEmpty(); captured = false; exclusions=new RectF[0]; }
    public boolean dispatch(MotionEvent event, float density, Consumer<byte[]> sink) {
        if (!(density > 0)) return false;
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN){
            captured = bounds.contains(event.getX()/density, event.getY()/density);
            for(RectF rectangle:exclusions)if(rectangle.contains(event.getX()/density,event.getY()/density))captured=false;
        }
        if (!captured) return false;
        int phase = action == MotionEvent.ACTION_CANCEL ? 3 : action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP ? 2 :
                    action == MotionEvent.ACTION_MOVE ? 1 : 0;
        int first = action == MotionEvent.ACTION_MOVE ? 0 : event.getActionIndex();
        int end = action == MotionEvent.ACTION_MOVE ? Math.min(10, event.getPointerCount()) : first + 1;
        try {
            for (int i=first; i<end; ++i) sink.accept(new JSONObject().put("type", "pointer")
                .put("id", event.getPointerId(i)).put("phase", phase).put("time", event.getEventTime())
                .put("x", Math.round((event.getX(i)/density - bounds.left)*1000))
                .put("y", Math.round((event.getY(i)/density - bounds.top)*1000))
                .toString().getBytes(StandardCharsets.UTF_8));
        } catch (org.json.JSONException error) { clear(); }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) captured = false;
        return true;
    }
}
