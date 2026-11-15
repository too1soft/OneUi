package net.too1.oneui.documents;

import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.IOException;
import java.util.Objects;
import java.util.UUID;
import org.json.JSONObject;

/** Shared rollback preserves published/uncertain copies and removes only owned siblings. */
final class DocumentRecovery {
    static void discard(DocumentTree tree, String id, JSONObject value, DocumentJournal journal) throws Exception {
        String path = value.getString("path"), parent = DocumentAccess.parent(path);
        String temporaryName = DocumentAccess.name(value.getString("temporary"));
        String backupName = DocumentAccess.name(value.getString("backup"));
        Uri temporary = tree.resolve(parent + "/" + temporaryName, false);
        Uri backup = tree.resolve(parent + "/" + backupName, false);
        if (backup != null) {
            if (value.optString("stage").equals("receiving")) throw new IOException("An unexpected recovery sibling exists");
            String recordedBackup = value.optString("backupUri");
            if (!recordedBackup.isEmpty() && !backup.toString().equals(recordedBackup))
                throw new IOException("The original recovery copy changed");
            JSONObject snapshot = value.optJSONObject("backupSnapshot");
            if (snapshot != null && !DocumentPublication.same(snapshot, tree.metadata(backup)))
                throw new IOException("The recovery copy changed");
            Uri target = tree.resolve(path, false);
            if (target == null) tree.renameChecked(backup, path.substring(path.lastIndexOf('/') + 1));
            else if (value.optString("stage").equals("published")
                    && DocumentPublication.same(value.optJSONObject("publishedSnapshot"), tree.metadata(target)))
                delete(tree, backup);
            else {
                String recovered = "Recovered-" + UUID.randomUUID().toString().substring(0, 8) + "-" + path.substring(path.lastIndexOf('/') + 1);
                if (tree.resolve(parent + "/" + recovered, false) != null) throw new IOException("A recovery copy already exists");
                tree.renameChecked(backup, recovered);
            }
        }
        if (temporary != null) {
            String recorded = value.optString("temporaryUri");
            if (!recorded.isEmpty() && !Objects.equals(temporary.toString(), recorded))
                throw new IOException("The receiving sibling changed before recovery");
            if (tree.metadata(temporary).getBoolean("directory")) throw new IOException("The receiving sibling is no longer a file");
            delete(tree, temporary);
        }
        journal.remove(id);
    }
    private static void delete(DocumentTree tree, Uri uri) throws IOException {
        if (!DocumentsContract.deleteDocument(tree.resolver, uri)) throw new IOException("Unable to remove an interrupted receiving file");
    }
}
