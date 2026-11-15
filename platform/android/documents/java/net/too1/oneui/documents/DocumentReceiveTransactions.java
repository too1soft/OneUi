package net.too1.oneui.documents;

import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.webkit.MimeTypeMap;
import java.io.IOException;
import java.util.Locale;
import java.util.UUID;
import org.json.JSONObject;

/** Provider siblings plus durable ownership metadata; file bytes stay in SAF. */
final class DocumentReceiveTransactions {
    private final DocumentReceiveJournal journal;
    DocumentReceiveTransactions(DocumentReceiveJournal journal) { this.journal = journal; }

    private String id(DocumentTree tree, String key) throws Exception {
        if (!key.matches("[A-Za-z0-9_-]{1,128}")) throw new IOException("Invalid receive key");
        return "receive-" + DocumentReceives.digest(tree.prefix + "\n" + key);
    }
    private JSONObject existing(DocumentTree tree, String id) throws Exception {
        JSONObject value = journal.read(id);
        if (value != null && (!value.optBoolean("managed") || !tree.prefix.equals(value.optString("scope"))))
            throw new IOException("Receive state belongs to a different folder");
        return value;
    }
    JSONObject open(DocumentTree tree, String path, JSONObject request, DocumentDescriptors descriptors, CancellationSignal cancellation) throws Exception {
        String id = id(tree, request.getString("key"));
        DocumentReceiveJournal.Lease lease = journal.lease(id);
        try { return openLocked(tree, path, id, request, descriptors, cancellation, lease); }
        catch (Exception failure) { lease.close(); throw failure; }
    }

    private JSONObject openLocked(DocumentTree tree, String path, String id, JSONObject request,
                                  DocumentDescriptors descriptors, CancellationSignal cancellation, DocumentReceiveJournal.Lease lease) throws Exception {
        if (tree.relative(path).length == 0 || !request.has("expected")) throw new IOException("A receive target and snapshot are required");
        JSONObject value = existing(tree, id);
        boolean created = value == null;
        if (value != null && !path.equals(value.getString("path"))) throw new IOException("The receive key changed its target");
        if (value != null && value.getString("stage").equals("publishing")) DocumentPublication.recover(tree, id, value, journal);
        if (value != null && value.getString("stage").equals("published")) {
            JSONObject result = new JSONObject().put("stage", "published").put("transaction", id).put("entry", DocumentPublication.published(tree, value));
            lease.close();
            return result;
        }
        if (value == null) {
            JSONObject expected = request.isNull("expected") ? null : request.getJSONObject("expected");
            JSONObject original = tree.stat(path);
            if (!DocumentPublication.same(expected, original)) throw new IOException("The receive target changed before opening");
            if (original != null && (!request.getBoolean("overwrite") || original.getBoolean("directory")))
                throw new IOException("Replacing this document requires confirmation");
            String name = path.substring(path.lastIndexOf('/') + 1);
            String extension = name.contains(".") ? name.substring(name.lastIndexOf('.')) : "";
            value = new JSONObject().put("owner", DocumentWriteJournal.PROCESS).put("managed", true).put("scope", tree.prefix)
                .put("path", path).put("stage", "receiving").put("original", original == null ? JSONObject.NULL : original)
                .put("parentUri", tree.required(DocumentAccess.parent(path)).toString())
                .put("temporary", ".oneui-" + UUID.randomUUID() + ".download" + extension)
                .put("backup", ".oneui-" + UUID.randomUUID() + ".backup" + extension);
            journal.save(id, value);
        }
        Uri parent = tree.required(DocumentAccess.parent(path));
        if (!parent.toString().equals(value.getString("parentUri"))) throw new IOException("The receive parent changed");
        Uri temporary = tree.resolve(DocumentAccess.parent(path) + "/" + value.getString("temporary"), false);
        if (temporary == null) {
            if (!value.optString("temporaryUri").isEmpty()) throw new IOException("The interrupted receiving document disappeared");
            String extension = path.contains(".") ? path.substring(path.lastIndexOf('.') + 1).toLowerCase(Locale.ROOT) : "";
            String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            temporary = DocumentsContract.createDocument(tree.resolver, parent, mime == null ? "application/octet-stream" : mime, value.getString("temporary"));
            if (temporary == null) throw new IOException("Unable to create receiving document");
            created = true;
            value.put("temporaryUri", temporary.toString());
            journal.save(id, value);
        }
        if (!value.optString("temporaryUri").isEmpty() && !temporary.toString().equals(value.getString("temporaryUri")))
            throw new IOException("The receiving document identity changed");
        JSONObject entry = tree.metadata(temporary);
        if (entry == null || entry.getBoolean("directory") || !entry.getString("name").equals(value.getString("temporary")))
            throw new IOException("The receiving document changed");
        value.put("temporaryUri", temporary.toString()).put("owner", DocumentWriteJournal.PROCESS).put("ioFailed", true);
        journal.save(id, value);
        ParcelFileDescriptor file = tree.resolver.openFileDescriptor(temporary, "rw", cancellation);
        if (file == null) throw new IOException("Unable to reopen receiving document");
        JSONObject token = descriptors.take(file, success -> {
            try {
                JSONObject current = journal.read(id);
                if (current != null) journal.save(id, current.put("ioFailed", !success));
            } finally { lease.close(); }
        });
        return token.put("transaction", id).put("path", path).put("stage", "receiving").put("created", created).put("entry", entry);
    }
    JSONObject finish(DocumentTree tree, String key) throws Exception {
        String id = id(tree, key);
        try (DocumentReceiveJournal.Lease held = journal.lease(id)) {
            JSONObject value = existing(tree, id);
            if (value == null || value.optBoolean("ioFailed")) throw new IOException("Receiving I/O has not completed successfully");
            boolean clean = DocumentPublication.publish(tree, id, value, journal);
            return new JSONObject().put("stage", "published").put("cleanupPending", !clean).put("entry", DocumentPublication.published(tree, value));
        }
    }
    JSONObject recover(DocumentTree tree, String key) throws Exception {
        String id = id(tree, key);
        try (DocumentReceiveJournal.Lease held = journal.lease(id)) {
            JSONObject value = existing(tree, id);
            if (value == null) throw new IOException("Receiving state is unavailable");
            DocumentPublication.recover(tree, id, value, journal);
            return new JSONObject().put("stage", value.getString("stage"));
        }
    }
    boolean discard(DocumentTree tree, String key) throws Exception {
        String id = id(tree, key);
        try (DocumentReceiveJournal.Lease held = journal.lease(id)) {
            JSONObject value = existing(tree, id);
            if (value != null) {
                DocumentPublication.checkParent(tree, value);
                DocumentRecovery.discard(tree, id, value, journal);
            }
            return true;
        }
    }
    boolean forget(DocumentTree tree, String key) throws Exception {
        String id = id(tree, key);
        try (DocumentReceiveJournal.Lease held = journal.lease(id)) {
            JSONObject value = existing(tree, id);
            if (value == null) return true;
            if (!value.getString("stage").equals("published")) throw new IOException("The receiving document is not published");
            if (!DocumentPublication.publish(tree, id, value, journal)) return false;
            journal.remove(id);
            return true;
        }
    }
}
