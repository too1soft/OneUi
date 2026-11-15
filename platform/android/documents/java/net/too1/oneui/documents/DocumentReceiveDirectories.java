package net.too1.oneui.documents;

import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.Os;
import android.system.OsConstants;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.file.FileAlreadyExistsException;
import java.util.concurrent.atomic.AtomicInteger;
import org.json.JSONObject;

/** Owns created directories; automatic removal is delegated to directory-only native IO. */
final class DocumentReceiveDirectories {
    private final DocumentReceiveJournal journal;
    DocumentReceiveDirectories(DocumentReceiveJournal journal) { this.journal = journal; }
    private String id(DocumentTree tree, String key) throws Exception {
        if (!key.matches("[A-Za-z0-9_-]{1,128}")) throw new IOException("Invalid receive directory key");
        return "receive-" + DocumentReceives.digest(tree.prefix + "\ndirectory\n" + key);
    }
    private JSONObject existing(DocumentTree tree, String id, String path) throws Exception {
        JSONObject value = journal.read(id);
        if (value != null && (!value.optString("kind").equals("directory") || !value.optString("scope").equals(tree.prefix)
                || !value.optString("path").equals(path))) throw new IOException("The owned directory changed its scope or path");
        return value;
    }
    private void parent(DocumentTree tree, JSONObject value) throws Exception {
        if (!tree.required(DocumentAccess.parent(value.getString("path"))).toString().equals(value.getString("parentUri")))
            throw new IOException("The owned directory parent changed");
    }
    private JSONObject owned(DocumentTree tree, JSONObject value) throws Exception {
        parent(tree, value);
        if (!value.has("uri")) throw new IOException("Directory creation needs recovery; existing copies were preserved");
        JSONObject entry = tree.metadata(Uri.parse(value.getString("uri")));
        if (entry == null) throw new FileNotFoundException("The owned directory is unavailable");
        if (!entry.getBoolean("directory")) throw new IOException("The owned directory became a file");
        return entry;
    }
    JSONObject create(DocumentTree tree, String path, String key) throws Exception {
        if (tree.relative(path).length == 0) throw new IOException("Cannot create a receive root");
        String id = id(tree, key), name = path.substring(path.lastIndexOf('/') + 1);
        try (DocumentReceiveJournal.Lease lease = journal.lease(id)) {
            JSONObject value = existing(tree, id, path);
            if (value != null && value.has("uri")) {
                JSONObject entry = owned(tree, value);
                if (!name.equals(entry.getString("name")) || !value.getString("uri").equals(String.valueOf(tree.resolve(path, false))))
                    throw new IOException("The created directory changed its name or identity");
                return entry;
            }
            if (tree.resolve(path, false) != null) {
                if (value == null) throw new FileAlreadyExistsException("The receive directory already exists");
                throw new IOException("Directory creation needs recovery; an unproven directory was preserved");
            }
            Uri parent = tree.required(DocumentAccess.parent(path));
            if (value == null) {
                value = new JSONObject().put("kind", "directory").put("scope", tree.prefix).put("path", path)
                    .put("parentUri", parent.toString());
                journal.save(id, value);
            } else parent(tree, value);
            Uri created = DocumentsContract.createDocument(tree.resolver, parent, DocumentsContract.Document.MIME_TYPE_DIR, name);
            if (created == null) throw new IOException("Unable to create the receive directory");
            value.put("uri", created.toString());
            journal.save(id, value); // retain identity even if the provider changes the requested name
            JSONObject entry = owned(tree, value);
            if (!name.equals(entry.getString("name")) || !created.equals(tree.resolve(path, false)))
                throw new IOException("The provider could not preserve the receive directory name");
            return entry;
        }
    }
    JSONObject handles(DocumentTree tree, String path, String key, DocumentDescriptors descriptors, CancellationSignal cancellation) throws Exception {
        String id = id(tree, key);
        DocumentReceiveJournal.Lease lease = journal.lease(id);
        ParcelFileDescriptor parent = null, directory = null;
        JSONObject parentToken = null, directoryToken = null;
        try {
            JSONObject value = existing(tree, id, path);
            if (value == null) throw new FileNotFoundException("No directory is owned by this receive job");
            JSONObject entry = owned(tree, value);
            Uri parentUri = Uri.parse(value.getString("parentUri"));
            Uri child = tree.findChild(parentUri, entry.getString("name"));
            if (child == null || !child.toString().equals(value.getString("uri"))) throw new IOException("The owned directory moved");
            try {
                parent = tree.resolver.openFileDescriptor(parentUri, "r", cancellation);
                directory = tree.resolver.openFileDescriptor(child, "r", cancellation);
            } catch (FileNotFoundException unsupported) {
                throw new UnsupportedOperationException("The provider cannot supply native directory handles", unsupported);
            }
            if (parent == null || directory == null || !OsConstants.S_ISDIR(Os.fstat(parent.getFileDescriptor()).st_mode)
                    || !OsConstants.S_ISDIR(Os.fstat(directory.getFileDescriptor()).st_mode))
                throw new UnsupportedOperationException("The provider cannot supply native directory handles");
            AtomicInteger remaining = new AtomicInteger(2);
            DocumentDescriptors.Closed closed = success -> { if (remaining.decrementAndGet() == 0) lease.close(); };
            parentToken = descriptors.take(parent, closed); parent = null;
            directoryToken = descriptors.take(directory, closed); directory = null;
            return new JSONObject().put("parent", parentToken).put("directory", directoryToken)
                .put("name", entry.getString("name")).put("entry", entry);
        } catch (Exception failure) {
            try { if (parentToken != null) descriptors.reclaim(parentToken); } catch (Exception closing) { failure.addSuppressed(closing); }
            try { if (directoryToken != null) descriptors.reclaim(directoryToken); } catch (Exception closing) { failure.addSuppressed(closing); }
            try { if (parent != null) parent.close(); } catch (Exception closing) { failure.addSuppressed(closing); }
            try { if (directory != null) directory.close(); } catch (Exception closing) { failure.addSuppressed(closing); }
            lease.close();
            throw failure;
        }
    }
    boolean forget(DocumentTree tree, String path, String key) throws Exception {
        String id = id(tree, key);
        try (DocumentReceiveJournal.Lease lease = journal.lease(id)) {
            if (existing(tree, id, path) != null) journal.remove(id);
            return true; // metadata only; this operation never deletes provider content
        }
    }
}
