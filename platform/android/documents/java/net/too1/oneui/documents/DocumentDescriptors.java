package net.too1.oneui.documents;

import android.os.ParcelFileDescriptor;
import java.io.IOException;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;

/** Retains the provider error channel while handing a duplicated descriptor to native code. */
final class DocumentDescriptors {
    interface Closed { void accept(boolean success) throws Exception; }
    private record Held(ParcelFileDescriptor file, Closed closed, String owner) { }
    private final ConcurrentHashMap<String, Held> open = new ConcurrentHashMap<>();
    JSONObject take(ParcelFileDescriptor descriptor) throws Exception {
        return take(descriptor, null);
    }
    JSONObject take(ParcelFileDescriptor descriptor, Closed closed) throws Exception {
        return take(descriptor, closed, "");
    }
    JSONObject take(ParcelFileDescriptor descriptor, Closed closed, String owner) throws Exception {
        String id = UUID.randomUUID().toString();
        try {
            ParcelFileDescriptor copy = ParcelFileDescriptor.dup(descriptor.getFileDescriptor());
            open.put(id, new Held(descriptor, closed, owner));
            return new JSONObject().put("fd", copy.detachFd()).put("descriptor", id);
        } catch (Exception error) {
            open.remove(id);
            descriptor.closeWithError("Unable to open document");
            throw error;
        }
    }
    boolean belongsTo(String descriptor, String owner) {
        Held held = open.get(descriptor);
        return held != null && owner.equals(held.owner);
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
    void reclaim(JSONObject token) throws Exception {
        Exception failed = null;
        try (ParcelFileDescriptor file = ParcelFileDescriptor.adoptFd(token.getInt("fd"))) { }
        catch (Exception error) { failed = error; }
        try { close(new JSONObject().put("descriptor", token.getString("descriptor")).put("error", "Document response was not delivered")); }
        catch (Exception error) { if (failed == null) failed = error; else failed.addSuppressed(error); }
        if (failed != null) throw failed;
    }
}
