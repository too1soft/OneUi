package net.too1.oneui.documents;

import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;

/** Read-only roots pin provider document IDs, rather than resolving a mutable parent path again. */
final class DocumentSources {
    private record Root(String selected, Uri uri) { }
    private final ConcurrentHashMap<String, Root> roots = new ConcurrentHashMap<>();

    Object execute(DocumentAccess owner, DocumentDescriptors descriptors, JSONObject request,
                   CancellationSignal cancellation) throws Exception {
        String operation = request.getString("op");
        if (operation.equals("source-root")) return pin(owner, request.getString("path"), cancellation);
        String token = request.getString("source");
        if (operation.equals("source-close")) { roots.remove(token); return true; }
        Root root = roots.get(token);
        if (root == null) throw new IOException("The selected source root is closed");
        // Recheck the grant on every provider operation, including already pinned roots.
        DocumentTree tree = owner.tree(root.selected, cancellation);
        Uri target = resolve(tree, root.uri, request.optString("path"));
        if (target == null) {
            if (operation.equals("source-stat")) return JSONObject.NULL;
            throw new FileNotFoundException("The source document no longer exists");
        }
        return switch (operation) {
            case "source-stat" -> tree.metadata(target);
            case "source-list" -> {
                if (!tree.metadata(target).getBoolean("directory")) throw new IOException("The source is not a folder");
                yield tree.children(target, request.getInt("limit"));
            }
            case "source-open" -> {
                JSONObject entry = tree.metadata(target);
                if (entry == null || entry.getBoolean("directory")) throw new IOException("The source is not a regular document");
                ParcelFileDescriptor file = tree.resolver.openFileDescriptor(target, "r", cancellation);
                if (file == null) throw new IOException("Unable to open source document");
                yield descriptors.take(file).put("entry", entry);
            }
            default -> throw new IOException("Unsupported source operation");
        };
    }

    private synchronized JSONObject pin(DocumentAccess owner, String path, CancellationSignal cancellation) throws Exception {
        if (roots.size() >= 32) throw new IOException("Too many open source roots");
        DocumentTree tree = owner.tree(path, cancellation);
        Uri uri = tree.required(path);
        JSONObject entry = tree.metadata(uri);
        if (entry == null || !entry.getBoolean("directory")) throw new IOException("Select a source folder");
        String token = UUID.randomUUID().toString();
        roots.put(token, new Root(path, uri));
        return new JSONObject().put("source", token).put("entry", entry);
    }

    private Uri resolve(DocumentTree tree, Uri root, String relative) throws Exception {
        if (relative.isEmpty()) return root;
        String[] parts = relative.split("/", -1);
        for (String part : parts) DocumentAccess.name(part);
        Uri current = root;
        for (String part : parts) {
            JSONObject entry = tree.metadata(current);
            if (entry == null || !entry.getBoolean("directory")) return null;
            current = tree.findChild(current, part);
            if (current == null) return null;
        }
        return current;
    }
}
