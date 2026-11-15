package net.too1.oneui.documents;

import android.os.ParcelFileDescriptor;
import java.io.IOException;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;

/** Retains the provider error channel while handing a duplicated descriptor to native code. */
final class DocumentDescriptors {
    private final ConcurrentHashMap<String, ParcelFileDescriptor> open = new ConcurrentHashMap<>();
    JSONObject take(ParcelFileDescriptor descriptor) throws Exception {
        String id = UUID.randomUUID().toString();
        try {
            ParcelFileDescriptor copy = ParcelFileDescriptor.dup(descriptor.getFileDescriptor());
            open.put(id, descriptor);
            return new JSONObject().put("fd", copy.detachFd()).put("descriptor", id);
        } catch (Exception error) {
            open.remove(id);
            descriptor.closeWithError("Unable to open document");
            throw error;
        }
    }
    boolean close(JSONObject request) throws Exception {
        ParcelFileDescriptor descriptor = open.remove(request.getString("descriptor"));
        if (descriptor == null) throw new IOException("The document was already closed");
        String error = request.optString("error");
        if (!error.isEmpty()) descriptor.closeWithError(error);
        else try (descriptor) { if (descriptor.canDetectErrors()) descriptor.checkError(); }
        return true;
    }
}
