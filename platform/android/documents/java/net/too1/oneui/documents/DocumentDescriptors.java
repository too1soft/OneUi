package net.too1.oneui.documents;

import android.os.ParcelFileDescriptor;
import java.io.IOException;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;

/** Retains the provider error channel while handing a duplicated descriptor to native code. */
final class DocumentDescriptors {
    interface Closed { void accept(boolean success) throws Exception; }
    private record Held(ParcelFileDescriptor file, Closed closed) { }
    private final ConcurrentHashMap<String, Held> open = new ConcurrentHashMap<>();
    JSONObject take(ParcelFileDescriptor descriptor) throws Exception {
        return take(descriptor, null);
    }
    JSONObject take(ParcelFileDescriptor descriptor, Closed closed) throws Exception {
        String id = UUID.randomUUID().toString();
        try {
            ParcelFileDescriptor copy = ParcelFileDescriptor.dup(descriptor.getFileDescriptor());
            open.put(id, new Held(descriptor, closed));
            return new JSONObject().put("fd", copy.detachFd()).put("descriptor", id);
        } catch (Exception error) {
            open.remove(id);
            descriptor.closeWithError("Unable to open document");
            throw error;
        }
    }
    boolean close(JSONObject request) throws Exception {
        Held held = open.remove(request.getString("descriptor"));
        if (held == null) throw new IOException("The document was already closed");
        ParcelFileDescriptor descriptor = held.file;
        String error = request.optString("error");
        Exception failed = null;
        try {
            if (!error.isEmpty()) descriptor.closeWithError(error);
            else try (descriptor) { if (descriptor.canDetectErrors()) descriptor.checkError(); }
        } catch (Exception failure) { failed = failure; }
        if (held.closed != null) try { held.closed.accept(failed == null && error.isEmpty()); }
        catch (Exception failure) { if (failed == null) failed = failure; else failed.addSuppressed(failure); }
        if (failed != null) throw failed;
        return true;
    }
}
