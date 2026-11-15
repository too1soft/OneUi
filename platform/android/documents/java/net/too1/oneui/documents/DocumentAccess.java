package net.too1.oneui.documents;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.os.CancellationSignal;
import android.util.Base64;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;

/** User-granted Android document storage for native consumers. Call provider operations off the UI thread. */
public final class DocumentAccess {
    private static final int BASE64 = Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING;
    private final Context context;
    private final SharedPreferences preferences;
    private final String prefix;
    private final ConcurrentHashMap<Uri, Integer> transientGrants = new ConcurrentHashMap<>();
    private final DocumentDescriptors descriptors = new DocumentDescriptors();
    private final DocumentWrites writes;
    private final DocumentSources sources = new DocumentSources();
    private final DocumentReceives receives;
    public DocumentAccess(Context context, String preferences, String prefix) {
        this.context = context.getApplicationContext();
        this.preferences = this.context.getSharedPreferences(preferences, Context.MODE_PRIVATE);
        DocumentWriteJournal journal = new DocumentWriteJournal(this.context.getSharedPreferences(preferences + "-receiving", Context.MODE_PRIVATE));
        writes = new DocumentWrites(journal);
        try {
            receives = new DocumentReceives(new DocumentReceiveJournal(new java.io.File(this.context.getFilesDir(),
                ".oneui-receive-" + DocumentReceives.digest(preferences))));
        } catch (Exception failure) { throw new IllegalArgumentException("Unable to create receive metadata", failure); }
        if (!prefix.startsWith("/") || !prefix.endsWith("/")) throw new IllegalArgumentException("Invalid document prefix");
        this.prefix = prefix;
    }
    public JSONObject select(Uri uri, int flags) throws Exception {
        if (!"content".equals(uri.getScheme())) throw new IOException("Invalid document selection");
        int grant = flags & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        if ((grant & Intent.FLAG_GRANT_READ_URI_PERMISSION) == 0) throw new IOException("The selected folder is not readable");
        if ((flags & Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION) != 0)
            context.getContentResolver().takePersistableUriPermission(uri, grant);
        else transientGrants.put(uri, grant);
        String root = prefix + Base64.encodeToString(uri.toString().getBytes(StandardCharsets.UTF_8), BASE64);
        DocumentTree tree = new DocumentTree(context.getContentResolver(), uri, root);
        String name = name(tree.rootName()), path = root + "/" + name;
        if (!preferences.edit().putString("selected", path).commit()) throw new IOException("Unable to remember the selected folder");
        return new JSONObject().put("path", path).put("name", name);
    }
    public String home() { return preferences.getString("selected", null); }
    DocumentTree tree(String path) throws Exception {
        return tree(path, null);
    }
    DocumentTree tree(String path, CancellationSignal cancellation) throws Exception {
        return tree(path, cancellation, false);
    }
    DocumentTree tree(String path, CancellationSignal cancellation, boolean writable) throws Exception {
        if (!path.startsWith(prefix)) throw new IOException("Invalid document location");
        String[] parts = path.substring(prefix.length()).split("/", -1);
        if (parts.length < 2) throw new IOException("Invalid document location");
        for (String part : parts) name(part);
        Uri grant = Uri.parse(new String(Base64.decode(parts[0], BASE64), StandardCharsets.UTF_8));
        int flags = transientGrants.getOrDefault(grant, 0);
        for (android.content.UriPermission permission : context.getContentResolver().getPersistedUriPermissions())
            if (permission.getUri().equals(grant)) {
                if (permission.isReadPermission()) flags |= Intent.FLAG_GRANT_READ_URI_PERMISSION;
                if (permission.isWritePermission()) flags |= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
            }
        if (!"content".equals(grant.getScheme()) || (flags & Intent.FLAG_GRANT_READ_URI_PERMISSION) == 0
                || writable && (flags & Intent.FLAG_GRANT_WRITE_URI_PERMISSION) == 0)
            throw new SecurityException("Folder access was revoked. Select the folder again.");
        return new DocumentTree(context.getContentResolver(), grant, prefix + parts[0], cancellation);
    }
    public String request(String request) {
        return request(request, null);
    }
    /** Optional cancellation is propagated to provider queries and read opens. */
    public String request(String request, CancellationSignal cancellation) {
        try {
            if (cancellation != null) cancellation.throwIfCanceled();
            return new JSONObject().put("value", execute(new JSONObject(request), cancellation)).toString();
        }
        catch (Exception error) {
            try { return new JSONObject().put("error", error.getMessage() == null ? "Document operation failed" : error.getMessage())
                .put("errorType", error.getClass().getSimpleName()).toString(); }
            catch (Exception ignored) { return "{\"error\":\"Document operation failed\"}"; }
        }
    }
    private Object execute(JSONObject request, CancellationSignal cancellation) throws Exception {
        String operation = request.getString("op");
        if (operation.equals("capabilities")) return new JSONObject().put("version", 2)
            .put("boundedListing", true).put("readCancellation", true).put("preciseModified", true).put("pinnedSources", true)
            .put("receiveTransactions", 1).put("receiveStorage", 1);
        if (operation.equals("home")) return home() == null ? JSONObject.NULL : home();
        if (operation.equals("close")) return descriptors.close(request);
        if (operation.startsWith("source-")) return sources.execute(this, descriptors, request, cancellation);
        if (operation.startsWith("receive-")) return receives.execute(this, descriptors, request, cancellation);
        String path = request.getString("path");
        DocumentTree tree = tree(path, cancellation);
        writes.recover(tree);
        return switch (operation) {
            case "allowed" -> true;
            case "list" -> tree.children(tree.required(path), request.has("limit") ? request.getInt("limit") : Integer.MAX_VALUE);
            case "stat" -> { JSONObject value = tree.stat(path); yield value == null ? JSONObject.NULL : value; }
            case "open" -> {
                ParcelFileDescriptor file = tree.resolver.openFileDescriptor(tree.required(path), "r", cancellation);
                if (file == null) throw new IOException("Unable to open document");
                yield descriptors.take(file);
            }
            case "begin" -> writes.begin(tree, path, request.getBoolean("overwrite"), descriptors);
            case "finish" -> writes.finish(request.getJSONObject("token"));
            case "discard" -> writes.discard(request.getJSONObject("token"));
            case "mkdir" -> { tree.mkdir(path); yield true; }
            case "delete" -> { tree.delete(path); yield true; }
            case "rename" -> { tree.rename(path, name(request.getString("name"))); yield true; }
            default -> throw new IOException("Unsupported document operation");
        };
    }
    static String name(String value) throws IOException {
        if (value == null || value.isEmpty() || value.equals(".") || value.equals("..") ||
            value.indexOf('/') >= 0 || value.indexOf('\\') >= 0 || value.indexOf('\0') >= 0)
            throw new IOException("Invalid document name");
        return value;
    }
    static String parent(String path) throws IOException {
        int index = path.lastIndexOf('/');
        if (index <= 0) throw new IOException("Invalid document parent");
        return path.substring(0, index);
    }
}
