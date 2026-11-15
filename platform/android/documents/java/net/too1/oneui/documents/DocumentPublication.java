package net.too1.oneui.documents;

import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.IOException;
import java.util.Objects;
import org.json.JSONObject;

/** Shared legacy/resumable publication. Provider rename is not a filesystem CAS. */
final class DocumentPublication {
    static boolean same(JSONObject expected, JSONObject actual) {
        return expected == null ? actual == null : actual != null
            && Objects.equals(expected.optString("uri"), actual.optString("uri")) && sameContents(expected, actual);
    }
    private static boolean sameContents(JSONObject a, JSONObject b) {
        return a != null && b != null && a.optBoolean("directory") == b.optBoolean("directory")
            && a.optLong("size", -1) == b.optLong("size", -1)
            && a.optLong("modifiedMillis", -1) == b.optLong("modifiedMillis", -1);
    }
    static JSONObject published(DocumentTree tree, JSONObject value) throws Exception {
        checkParent(tree, value);
        JSONObject actual = tree.stat(value.getString("path"));
        if (!same(value.optJSONObject("publishedSnapshot"), actual)) throw new IOException("The published document changed");
        return actual;
    }
    static boolean publish(DocumentTree tree, String id, JSONObject value, DocumentJournal journal) throws Exception {
        String path = value.getString("path");
        if (value.getString("stage").equals("publishing")) recover(tree, id, value, journal);
        if (value.getString("stage").equals("published")) {
            published(tree, value);
            return cleanup(tree, value);
        }
        String name = path.substring(path.lastIndexOf('/') + 1);
        String parent = DocumentAccess.parent(path);
        Uri directory = tree.required(parent);
        if (!directory.toString().equals(value.getString("parentUri"))) throw new IOException("The destination folder changed");
        JSONObject original = value.optJSONObject("original");
        if (!same(original, tree.stat(path))) throw new IOException("The destination changed during the transfer");
        Uri temporary = tree.resolve(parent + "/" + value.getString("temporary"), false);
        if (temporary == null || !temporary.toString().equals(value.getString("temporaryUri")))
            throw new IOException("The receiving document changed");
        JSONObject entry = tree.metadata(temporary);
        if (entry == null || entry.getBoolean("directory")) throw new IOException("The receiving document is unavailable");
        if (tree.resolve(parent + "/" + value.getString("backup"), false) != null)
            throw new IOException("The recovery name is already used");
        value.put("stage", "publishing").put("temporarySnapshot", entry);
        journal.save(id, value);
        Uri backup = original == null ? null : tree.renameChecked(Uri.parse(original.getString("uri")), value.getString("backup"));
        if (backup != null) {
            value.put("backupUri", backup.toString()).put("backupSnapshot", tree.metadata(backup));
            journal.save(id, value);
        }
        Uri result;
        try { result = tree.renameChecked(temporary, name); }
        catch (Exception error) {
            try { recover(tree, id, value, journal); }
            catch (Exception recovery) { error.addSuppressed(recovery); }
            throw error;
        }
        value.put("stage", "published").put("publishedUri", result.toString()).put("publishedSnapshot", tree.metadata(result));
        journal.save(id, value);
        return cleanup(tree, value);
    }
    // Resolve only outcomes supported by identity evidence. In particular a
    // path-based ID reused by a new target cannot prove who published it.
    static void recover(DocumentTree tree, String id, JSONObject value, DocumentJournal journal) throws Exception {
        if (!value.getString("stage").equals("publishing")) return;
        checkParent(tree, value);
        String path = value.getString("path"), parent = DocumentAccess.parent(path);
        JSONObject target = tree.stat(path);
        Uri temporary = tree.resolve(parent + "/" + value.getString("temporary"), false);
        Uri backup = tree.resolve(parent + "/" + value.getString("backup"), false);
        if (temporary == null && target != null && target.optString("uri").equals(value.optString("temporaryUri"))
                && sameContents(value.optJSONObject("temporarySnapshot"), target)) {
            value.put("stage", "published").put("publishedUri", target.getString("uri")).put("publishedSnapshot", target);
            journal.save(id, value);
            return;
        }
        if (temporary == null || !temporary.toString().equals(value.optString("temporaryUri")))
            throw new IOException("Publication needs recovery; existing copies were preserved");
        if (target == null && backup != null) {
            JSONObject snapshot = value.optJSONObject("backupSnapshot");
            if (snapshot != null ? !same(snapshot, tree.metadata(backup)) : !sameContents(value.optJSONObject("original"), tree.metadata(backup)))
                throw new IOException("The recovery copy changed");
            tree.renameChecked(backup, path.substring(path.lastIndexOf('/') + 1));
            target = tree.stat(path);
            if (!sameContents(value.optJSONObject("original"), target)) throw new IOException("The recovered original changed");
            value.put("original", target);
        } else if (backup != null || !same(value.optJSONObject("original"), target)) {
            throw new IOException("Publication needs recovery; existing copies were preserved");
        }
        value.put("stage", "receiving").remove("backupUri");
        value.remove("backupSnapshot");
        journal.save(id, value);
    }
    private static boolean cleanup(DocumentTree tree, JSONObject value) throws Exception {
        String parent = DocumentAccess.parent(value.getString("path"));
        Uri backup = tree.resolve(parent + "/" + value.getString("backup"), false);
        if (backup == null) return true;
        if (!same(value.optJSONObject("backupSnapshot"), tree.metadata(backup)))
            throw new IOException("The previous copy changed; it was preserved");
        return DocumentsContract.deleteDocument(tree.resolver, backup);
    }
    static void checkParent(DocumentTree tree, JSONObject value) throws Exception {
        if (!tree.required(DocumentAccess.parent(value.getString("path"))).toString().equals(value.getString("parentUri")))
            throw new IOException("The receiving parent changed");
    }
}
