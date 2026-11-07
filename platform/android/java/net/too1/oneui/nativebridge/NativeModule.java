package net.too1.oneui.nativebridge;

import java.io.Closeable;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Generic worker-call bridge to an app-packaged OneUiNativeModuleCallV1 symbol.
 * Closing releases the function handle; native libraries stay loaded for the
 * process lifetime, as required by embedded language runtimes. */
public final class NativeModule implements Closeable {
    static { System.loadLibrary("oneui_native_services"); }
    private long handle;
    public NativeModule(String library, String symbol) throws IOException {
        handle = open(library.getBytes(StandardCharsets.UTF_8), symbol.getBytes(StandardCharsets.UTF_8));
        if (handle == 0) throw new IOException("Native module unavailable");
    }
    public byte[] call(int command, byte[] request) throws IOException {
        long value;
        synchronized (this) { value = handle; }
        if (value == 0) throw new IOException("Native module closed");
        byte[] result = invoke(value, command, request);
        if (result == null) throw new IOException("Native module call failed");
        return result;
    }
    @Override public synchronized void close() { if (handle != 0) { release(handle); handle = 0; } }
    private static native long open(byte[] library, byte[] symbol);
    private static native byte[] invoke(long handle, int command, byte[] request);
    private static native void release(long handle);
}
