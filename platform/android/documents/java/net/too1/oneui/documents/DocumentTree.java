package net.too1.oneui.documents;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.os.CancellationSignal;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import org.json.JSONArray;
import org.json.JSONObject;

/** Resolves names only inside one user-granted document tree. No raw storage paths. */
final class DocumentTree {
    final ContentResolver resolver;
    final Uri grant;
    final String prefix;
    private final CancellationSignal cancellation;
    private final Uri boundRoot;
    private static final String[] COLUMNS = {Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
        Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE, Document.COLUMN_LAST_MODIFIED};
    DocumentTree(ContentResolver resolver, Uri grant, String prefix) {
        this(resolver, grant, prefix, null);
    }
    DocumentTree(ContentResolver resolver, Uri grant, String prefix, CancellationSignal cancellation) {
        this(resolver, grant, prefix, cancellation, null);
    }
    private DocumentTree(ContentResolver resolver, Uri grant, String prefix, CancellationSignal cancellation, Uri boundRoot) {
        this.resolver = resolver; this.grant = grant; this.prefix = prefix;
        this.cancellation = cancellation;
        this.boundRoot = boundRoot;
    }
    DocumentTree bound(Uri root, String prefix) { return new DocumentTree(resolver, grant, prefix, cancellation, root); }
    Uri root() { return boundRoot != null ? boundRoot : DocumentsContract.buildDocumentUriUsingTree(grant, DocumentsContract.getTreeDocumentId(grant)); }
    String rootName() throws Exception {
        if (boundRoot != null) return "root";
        JSONObject entry = metadata(root());
        if (entry == null) throw new FileNotFoundException("The selected folder no longer exists");
        return entry.getString("name");
    }
    JSONObject metadata(Uri uri) throws Exception {
        try (Cursor cursor = resolver.query(uri, COLUMNS, null, null, null, cancellation)) {
            if (cursor == null) throw new IOException("Unable to query the selected document");
            checkListing(cursor);
            return cursor.moveToFirst() ? entry(cursor, uri) : null;
        }
    }
    private JSONObject entry(Cursor cursor, Uri uri) throws Exception {
        return new JSONObject().put("name", DocumentAccess.name(cursor.getString(1)))
            .put("directory", Document.MIME_TYPE_DIR.equals(cursor.getString(2)))
            .put("size", cursor.isNull(3) ? -1 : cursor.getLong(3))
            .put("modified", cursor.isNull(4) ? 0 : Math.max(0, cursor.getLong(4) / 1000))
            .put("modifiedMillis", cursor.isNull(4) ? 0 : Math.max(0, cursor.getLong(4)))
            .put("uri", uri.toString());
    }
    JSONArray children(Uri uri) throws Exception {
        return children(uri, Integer.MAX_VALUE);
    }
    JSONArray children(Uri uri, int limit) throws Exception {
        if (limit < 1) throw new IOException("Invalid directory limit");
        Uri query = DocumentsContract.buildChildDocumentsUriUsingTree(grant, DocumentsContract.getDocumentId(uri));
        JSONArray entries = new JSONArray();
        try (Cursor cursor = resolver.query(query, COLUMNS, null, null, null, cancellation)) {
            if (cursor == null) throw new IOException("Unable to list the selected folder");
            checkListing(cursor);
            while (cursor.moveToNext()) {
                if (cancellation != null) cancellation.throwIfCanceled();
                if (entries.length() >= limit) throw new IOException("The folder exceeds the requested item limit");
                Uri child = DocumentsContract.buildDocumentUriUsingTree(grant, cursor.getString(0));
                entries.put(entry(cursor, child));
            }
        }
        return entries;
    }
    static void checkListing(Cursor cursor) throws IOException {
        String error = cursor.getExtras().getString(DocumentsContract.EXTRA_ERROR);
        if (error != null) throw new IOException(error);
        if (cursor.getExtras().getBoolean(DocumentsContract.EXTRA_LOADING))
            throw new IOException("The selected folder is still loading. Try again.");
    }
    String[] relative(String path) throws Exception {
        if (!path.startsWith(prefix + "/")) throw new IOException("Invalid document path");
        String[] parts = path.substring(prefix.length() + 1).split("/", -1);
        for (String part : parts) DocumentAccess.name(part);
        if (parts.length == 0 || !parts[0].equals(rootName()))
            throw new IOException("The selected folder was moved or renamed. Select it again.");
        return java.util.Arrays.copyOfRange(parts, 1, parts.length);
    }
    Uri resolve(String path, boolean createParents) throws Exception {
        Uri current = root();
        for (String name : relative(path)) {
            Uri found = findChild(current, name);
            if (found == null && createParents) {
                found = DocumentsContract.createDocument(resolver, current, Document.MIME_TYPE_DIR, name);
                if (found == null || !name.equals(metadata(found).getString("name")))
                    throw new IOException("The provider could not preserve the folder name");
            }
            if (found == null) return null;
            current = found;
        }
        return current;
    }
    // Resolving one name must not allocate a JSON object for every sibling.
    Uri findChild(Uri parent, String name) throws Exception {
        Uri query = DocumentsContract.buildChildDocumentsUriUsingTree(grant, DocumentsContract.getDocumentId(parent));
        Uri found = null;
        try (Cursor cursor = resolver.query(query, COLUMNS, null, null, null, cancellation)) {
            if (cursor == null) throw new IOException("Unable to list the selected folder");
            checkListing(cursor);
            while (cursor.moveToNext()) {
                if (cancellation != null) cancellation.throwIfCanceled();
                String current = DocumentAccess.name(cursor.getString(1));
                if (!name.equals(current)) continue;
                if (found != null) throw new IOException("The provider returned duplicate document names");
                found = DocumentsContract.buildDocumentUriUsingTree(grant, cursor.getString(0));
            }
        }
        return found;
    }
    Uri required(String path) throws Exception {
        Uri result = resolve(path, false);
        if (result == null) throw new FileNotFoundException("The document no longer exists");
        return result;
    }
    JSONObject stat(String path) throws Exception {
        Uri uri = resolve(path, false); return uri == null ? null : metadata(uri);
    }
    void delete(String path) throws Exception {
        if (relative(path).length == 0) throw new IOException("Cannot delete a selected root folder");
        if (!DocumentsContract.deleteDocument(resolver, required(path))) throw new IOException("Unable to delete document");
    }
    void mkdir(String path) throws Exception {
        Uri created = resolve(path, true);
        if (created == null || !metadata(created).getBoolean("directory")) throw new IOException("The destination is not a folder");
    }
    void rename(String path, String name) throws Exception {
        if (relative(path).length == 0) throw new IOException("Cannot rename a selected root folder");
        DocumentAccess.name(name);
        if (resolve(DocumentAccess.parent(path) + "/" + name, false) != null) throw new IOException("A document already exists with this name");
        renameChecked(required(path), name);
    }
    Uri renameChecked(Uri uri, String name) throws Exception {
        Uri result = DocumentsContract.renameDocument(resolver, uri, name);
        if (result == null || !name.equals(metadata(result).getString("name")))
            throw new IOException("Unable to preserve the saved filename");
        return result;
    }
}
