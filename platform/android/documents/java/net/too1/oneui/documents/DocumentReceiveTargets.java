package net.too1.oneui.documents;

import java.io.IOException;
import java.nio.file.FileAlreadyExistsException;
import org.json.JSONObject;

/** Publication choices are local compare-and-set requests, separate from opening content. */
final class DocumentReceiveTargets {
    static void bind(DocumentTree tree, String id, JSONObject value, String from, String target,
                     JSONObject request, DocumentJournal journal) throws Exception {
        if (!value.getString("stage").equals("receiving") || value.optBoolean("ioFailed"))
            throw new IOException("Receiving I/O has not completed successfully");
        if (!value.getString("path").equals(from) || !DocumentAccess.parent(from).equals(DocumentAccess.parent(target))
                || tree.relative(target).length == 0 || !request.has("expected"))
            throw new IOException("The publication target changed or moved outside its parent");
        DocumentPublication.checkParent(tree, value);
        String name = target.substring(target.lastIndexOf('/') + 1);
        if (name.equals(value.getString("temporary")) || name.equals(value.getString("backup")))
            throw new IOException("A recovery sibling cannot be a publication target");
        JSONObject actual = tree.stat(target);
        boolean overwrite = request.getBoolean("overwrite");
        if (actual != null && !overwrite) throw new FileAlreadyExistsException("The receive target already exists");
        JSONObject expected = request.isNull("expected") ? null : request.getJSONObject("expected");
        if (!DocumentPublication.same(expected, actual) || actual != null && actual.getBoolean("directory"))
            throw new IOException("The confirmed publication snapshot changed");
        value.put("path", target).put("original", actual == null ? JSONObject.NULL : actual);
        journal.save(id, value);
    }
}
