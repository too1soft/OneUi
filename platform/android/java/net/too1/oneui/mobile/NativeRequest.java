package net.too1.oneui.mobile;

public final class NativeRequest {
    public final long id;
    public final String action;
    public final byte[] payload;
    public NativeRequest(long id, String action, byte[] payload) { this.id = id; this.action = action; this.payload = payload; }
}
