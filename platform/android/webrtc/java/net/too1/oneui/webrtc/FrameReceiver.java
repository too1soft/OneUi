package net.too1.oneui.webrtc;

import org.webrtc.VideoFrame;
import org.webrtc.VideoSink;
import org.webrtc.VideoTrack;
import org.webrtc.YuvHelper;
import java.nio.ByteBuffer;

/** Optional WebRTC adapter; no signaling, authentication or product code. */
public final class FrameReceiver implements VideoSink, AutoCloseable {
    private final Object lock = new Object();
    private long mailbox;
    private VideoTrack track;
    private boolean enabled = true;
    private ByteBuffer rgba;
    private final Runnable changed;
    public FrameReceiver(long mailbox, Runnable changed) { this.mailbox = mailbox; this.changed = changed; }
    public void attach(VideoTrack value) {
        final VideoTrack previous;
        synchronized(lock) { previous = track; track = mailbox != 0 ? value : null; }
        // Never hold the frame lock across WebRTC: removeSink can wait for a
        // decoder callback which itself needs that lock.
        if (previous != null) previous.removeSink(this);
        if (track != null) track.addSink(this);
    }
    @Override public void onFrame(VideoFrame frame) {
        boolean accepted = false;
        synchronized(lock) {
            if (mailbox == 0 || !enabled) return;
            VideoFrame.I420Buffer i420 = frame.getBuffer().toI420();
            try {
                int w = i420.getWidth(), h = i420.getHeight();
                if (w < 2 || h < 2 || w > 5120 || h > 2880 || w % 2 != 0 || h % 2 != 0) return;
                int bytes = w * h * 3 / 2;
                if (rgba == null || rgba.capacity() != bytes) rgba = ByteBuffer.allocateDirect(bytes);
                rgba.clear();
                YuvHelper.I420ToNV12(i420.getDataY(), i420.getStrideY(), i420.getDataU(), i420.getStrideU(),
                    i420.getDataV(), i420.getStrideV(), rgba, w, h);
                accepted = submit(mailbox, rgba, w, h, w);
            } finally { i420.release(); }
        }
        if (accepted) changed.run();
    }
    @Override public void close() {
        final VideoTrack previous;
        synchronized(lock) { previous = track; track = null; mailbox = 0; rgba = null; }
        if (previous != null) previous.removeSink(this);
    }
    public void setEnabled(boolean value) { synchronized(lock) { enabled = value; } }
    private static native boolean submit(long mailbox, ByteBuffer bytes, int width, int height, int stride);
}
