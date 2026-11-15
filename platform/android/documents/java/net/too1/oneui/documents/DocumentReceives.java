package net.too1.oneui.documents;

import android.net.Uri;
import android.os.CancellationSignal;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;

/** Local write-granted receiver scopes. No peer-selected URI or disk path. */
final class DocumentReceives {
    private record Root(String selected, Uri uri, String prefix) { }
    private final ConcurrentHashMap<String, Root> roots = new ConcurrentHashMap<>();
    private final DocumentReceiveTransactions transactions;
    DocumentReceives(DocumentReceiveJournal journal) { transactions = new DocumentReceiveTransactions(journal); }

    Object execute(DocumentAccess owner, DocumentDescriptors descriptors, JSONObject request, CancellationSignal cancellation) throws Exception {
        String operation = request.getString("op");
        if (operation.equals("receive-root")) return pin(owner, request.getString("path"), cancellation);
        String token = request.getString("root");
        if (operation.equals("receive-root-close")) { roots.remove(token); return true; }
        Root root = roots.get(token);
        if (root == null) throw new IOException("The receive root is closed");
        DocumentTree tree = owner.tree(root.selected, cancellation, true).bound(root.uri, root.prefix);
        JSONObject entry = tree.metadata(root.uri);
        if (entry == null || !entry.getBoolean("directory")) throw new IOException("The receive folder changed");
        return switch (operation) {
            case "receive-stat" -> {
                JSONObject value = tree.stat(location(root, request.optString("path")));
                yield value == null ? JSONObject.NULL : value;
            }
            case "receive-list" -> {
                Uri directory = tree.required(location(root, request.optString("path")));
                if (!tree.metadata(directory).getBoolean("directory")) throw new IOException("The receive location is not a folder");
                yield tree.children(directory, request.getInt("limit"));
            }
            case "receive-mkdir" -> { tree.mkdir(location(root, request.getString("path"))); yield true; }
            case "receive-open" -> transactions.open(tree, location(root, request.getString("path")), request, descriptors, cancellation);
            case "receive-finish" -> transactions.finish(tree, request.getString("key"));
            case "receive-recover" -> transactions.recover(tree, request.getString("key"));
            case "receive-discard" -> transactions.discard(tree, request.getString("key"));
            case "receive-forget" -> transactions.forget(tree, request.getString("key"));
            default -> throw new IOException("Unsupported receive operation");
        };
    }
    private synchronized JSONObject pin(DocumentAccess owner, String selected, CancellationSignal cancellation) throws Exception {
        if (roots.size() >= 32) throw new IOException("Too many receive roots");
        DocumentTree tree = owner.tree(selected, cancellation, true);
        Uri uri = tree.required(selected);
        JSONObject entry = tree.metadata(uri);
        if (entry == null || !entry.getBoolean("directory")) throw new IOException("Select a receive folder");
        String scope = digest(tree.grant + "\n" + uri);
        String token = UUID.randomUUID().toString();
        roots.put(token, new Root(selected, uri, "/.oneui-receive/" + scope));
        return new JSONObject().put("root", token).put("scope", scope).put("entry", entry);
    }
    private static String location(Root root, String relative) throws Exception {
        if (relative.length() > 8192) throw new IOException("Receive path is too long");
        if (!relative.isEmpty()) for (String part : relative.split("/", -1)) DocumentAccess.name(part);
        return root.prefix + "/root" + (relative.isEmpty() ? "" : "/" + relative);
    }
    static String digest(String value) throws Exception {
        byte[] bytes = MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
        char[] digits = "0123456789abcdef".toCharArray();
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) result.append(digits[(b & 255) >>> 4]).append(digits[b & 15]);
        return result.toString();
    }
}
