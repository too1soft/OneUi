package net.too1.oneui.documents;

import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.webkit.MimeTypeMap;
import java.io.IOException;
import java.util.Locale;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONObject;

/** Receives into a temporary sibling, publishing only after native I/O has succeeded. */
final class DocumentWrites {
    private record Pending(DocumentTree tree, String path, Uri temporary, Uri original, JSONObject journal) { }
    private final ConcurrentHashMap<String, Pending> pending = new ConcurrentHashMap<>();
    private final DocumentWriteJournal journal;
    DocumentWrites(DocumentWriteJournal journal) { this.journal=journal; }
    void recover(DocumentTree tree) throws Exception { journal.recover(tree); }
    synchronized JSONObject begin(DocumentTree tree, String path, boolean overwrite, DocumentDescriptors descriptors) throws Exception {
        Uri original = tree.resolve(path, false);
        if (original != null && (!overwrite || tree.metadata(original).getBoolean("directory")))
            throw new IOException("Replacing this document requires confirmation");
        String name = DocumentAccess.name(path.substring(path.lastIndexOf('/') + 1));
        Uri parent = tree.resolve(DocumentAccess.parent(path), true);
        String extension = name.contains(".") ? name.substring(name.lastIndexOf('.') + 1).toLowerCase(Locale.ROOT) : "";
        String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
        String id=UUID.randomUUID().toString(), receivingName=temporaryName(name,"download");
        JSONObject recovery=journal.begin(id,path,receivingName,temporaryName(name,"backup"));
        recovery.put("original", original == null ? JSONObject.NULL : tree.metadata(original)).put("parentUri", parent.toString());
        journal.save(id,recovery);
        Uri temporary = null;
        try {
            temporary=DocumentsContract.createDocument(tree.resolver,parent,mime==null?"application/octet-stream":mime,receivingName);
            if(temporary==null)throw new IOException("Unable to create the receiving document");
            journal.save(id,recovery.put("temporaryUri",temporary.toString()));
            if(!receivingName.equals(tree.metadata(temporary).getString("name")))throw new IOException("Unable to preserve the receiving filename");
            ParcelFileDescriptor file = tree.resolver.openFileDescriptor(temporary, "rw");
            if (file == null) throw new IOException("Unable to open the receiving document");
            JSONObject token = descriptors.take(file);
            pending.put(id, new Pending(tree, path, temporary, original,recovery));
            return token.put("transaction", id).put("path", path);
        } catch (Exception error) {
            if (temporary!=null&&!DocumentsContract.deleteDocument(tree.resolver, temporary))
                throw new IOException("Unable to remove the failed receiving document", error);
            journal.remove(id);
            throw error;
        }
    }
    synchronized boolean finish(JSONObject token) throws Exception {
        String id = token.getString("transaction");
        Pending item = pending.get(id);
        if (item == null || !item.path.equals(token.getString("path"))) throw new IOException("Invalid document transaction");
        if (!DocumentPublication.publish(item.tree,id,item.journal,journal))
            throw new IOException("File saved, but the previous copy could not be removed");
        journal.remove(id);pending.remove(id);
        return true;
    }
    synchronized boolean discard(JSONObject token) throws Exception {
        String id = token.getString("transaction");
        Pending item = pending.get(id);
        if (item == null) return true;
        if(!item.path.equals(token.getString("path")))throw new IOException("Invalid document transaction");
        journal.discard(item.tree,id,item.journal);pending.remove(id);
        return true;
    }
    private static String temporaryName(String name, String purpose) {
        String extension = name.contains(".") ? name.substring(name.lastIndexOf('.')) : "";
        return ".oneui-" + UUID.randomUUID() + "." + purpose + extension;
    }
}
