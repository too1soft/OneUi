package net.too1.oneui.documents;

import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsProvider;
import java.io.File;
import java.io.FileNotFoundException;
import java.nio.file.Files;
import java.util.UUID;
import org.json.JSONObject;

/** Persistent, opaque-ID, same-UID SAF fixture. Never accesses product data. */
public final class OwnedReceiveProvider extends DocumentsProvider {
    static final String AUTHORITY = "net.too1.oneui.documents.receivefixture";
    static volatile boolean blockOpen;
    static volatile java.util.concurrent.CountDownLatch openEntered = new java.util.concurrent.CountDownLatch(1);
    private static final Object LOCK = new Object();
    private static final String[] COLUMNS = {Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
        Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE, Document.COLUMN_LAST_MODIFIED, Document.COLUMN_FLAGS};
    @Override public boolean onCreate() { return true; }
    static File bytes(Context context, String id) {
        File folder = new File(context.getFilesDir(), "provider-bytes");
        folder.mkdirs();
        return new File(folder, id);
    }
    private static JSONObject nodes(Context context) throws Exception {
        return new JSONObject(context.getSharedPreferences("provider-nodes", 0).getString("nodes", "{}"));
    }
    private static void save(Context context, JSONObject nodes) throws Exception {
        if (!context.getSharedPreferences("provider-nodes", 0).edit().putString("nodes", nodes.toString()).commit())
            throw new java.io.IOException("fixture metadata commit failed");
    }
    private static JSONObject node(String parent, String name, boolean directory) throws Exception {
        return new JSONObject().put("parent", parent).put("name", name).put("directory", directory);
    }
    static void reset(Context context) throws Exception {
        synchronized (LOCK) {
            JSONObject old = nodes(context);
            for (var ids = old.keys(); ids.hasNext();) Files.deleteIfExists(bytes(context, ids.next()).toPath());
            save(context, new JSONObject().put("root", node("", "root", true))
                .put("selected", node("root", "selected", true)));
            context.getSharedPreferences("provider-controls", 0).edit().clear().commit();
        }
    }
    static void control(Context context, String name, boolean value) {
        if (!context.getSharedPreferences("provider-controls", 0).edit().putBoolean(name, value).commit())
            throw new AssertionError("fixture control commit failed");
    }
    static String find(Context context, String parent, String name) throws Exception {
        synchronized (LOCK) {
            JSONObject all = nodes(context);
            for (var ids = all.keys(); ids.hasNext();) {
                String id = ids.next(); JSONObject item = all.getJSONObject(id);
                if (parent.equals(item.getString("parent")) && name.equals(item.getString("name"))) return id;
            }
            return null;
        }
    }
    static byte[] content(Context context, String parent, String name) throws Exception {
        String id = find(context, parent, name);
        return id == null ? null : Files.readAllBytes(bytes(context, id).toPath());
    }
    static void replaceBytes(Context context, String parent, String name, byte[] content) throws Exception {
        Files.write(bytes(context, find(context, parent, name)).toPath(), content);
    }
    static int children(Context context, String parent) throws Exception {
        synchronized (LOCK) {
            int count = 0; JSONObject all = nodes(context);
            for (var ids = all.keys(); ids.hasNext();) if (parent.equals(all.getJSONObject(ids.next()).getString("parent"))) count++;
            return count;
        }
    }
    private JSONObject required(JSONObject all, String id) throws Exception {
        JSONObject item = all.optJSONObject(id);
        if (item == null) throw new FileNotFoundException("fixture document absent");
        return item;
    }
    private void row(MatrixCursor cursor, String id, JSONObject item) throws Exception {
        Object[] values = new Object[cursor.getColumnCount()];
        boolean directory = item.getBoolean("directory");
        for (int i = 0; i < values.length; i++) values[i] = switch (cursor.getColumnNames()[i]) {
            case Document.COLUMN_DOCUMENT_ID -> id;
            case Document.COLUMN_DISPLAY_NAME -> item.getString("name");
            case Document.COLUMN_MIME_TYPE -> directory ? Document.MIME_TYPE_DIR : "application/octet-stream";
            case Document.COLUMN_SIZE -> directory ? 0L : bytes(getContext(), id).length();
            case Document.COLUMN_LAST_MODIFIED -> directory ? 1700000000123L : bytes(getContext(), id).lastModified();
            case Document.COLUMN_FLAGS -> directory ? Document.FLAG_DIR_SUPPORTS_CREATE
                : Document.FLAG_SUPPORTS_WRITE | Document.FLAG_SUPPORTS_RENAME | Document.FLAG_SUPPORTS_DELETE;
            default -> null;
        };
        cursor.addRow(values);
    }
    @Override public Cursor queryRoots(String[] projection) { return new MatrixCursor(new String[]{"root_id"}); }
    @Override public Cursor queryDocument(String id, String[] projection) throws FileNotFoundException {
        synchronized (LOCK) {
            try {
                MatrixCursor cursor = new MatrixCursor(projection == null ? COLUMNS : projection);
                JSONObject item = nodes(getContext()).optJSONObject(id);
                if (item != null) row(cursor, id, item);
                return cursor;
            } catch (Exception error) { throw new FileNotFoundException(error.toString()); }
        }
    }
    @Override public Cursor queryChildDocuments(String parent, String[] projection, String sort) throws FileNotFoundException {
        synchronized (LOCK) {
            try {
                JSONObject all = nodes(getContext());
                if (!required(all, parent).getBoolean("directory")) throw new FileNotFoundException("not a directory");
                MatrixCursor cursor = new MatrixCursor(projection == null ? COLUMNS : projection);
                for (var ids = all.keys(); ids.hasNext();) {
                    String id = ids.next(); JSONObject item = all.getJSONObject(id);
                    if (parent.equals(item.getString("parent"))) row(cursor, id, item);
                }
                return cursor;
            } catch (Exception error) { throw new FileNotFoundException(error.toString()); }
        }
    }
    @Override public ParcelFileDescriptor openDocument(String id, String mode, CancellationSignal cancellation) throws FileNotFoundException {
        synchronized (LOCK) {
            try {
                if (cancellation != null) cancellation.throwIfCanceled();
                if (required(nodes(getContext()), id).getBoolean("directory")) throw new FileNotFoundException("not a file");
                if (blockOpen) {
                    openEntered.countDown();
                    while (blockOpen) { if (cancellation != null) cancellation.throwIfCanceled(); Thread.sleep(10); }
                }
                if (getContext().getSharedPreferences("provider-controls", 0).getBoolean("close-error", false)) {
                    ParcelFileDescriptor[] pair = ParcelFileDescriptor.createReliableSocketPair();
                    pair[1].closeWithError("injected provider close error");
                    return pair[0];
                }
                return ParcelFileDescriptor.open(bytes(getContext(), id), ParcelFileDescriptor.parseMode(mode));
            } catch (android.os.OperationCanceledException error) { throw error;
            } catch (FileNotFoundException error) { throw error; }
            catch (Exception error) { throw new FileNotFoundException(error.toString()); }
        }
    }
    @Override public String createDocument(String parent, String mime, String name) throws FileNotFoundException {
        synchronized (LOCK) {
            try {
                JSONObject all = nodes(getContext());
                if (!required(all, parent).getBoolean("directory") || find(getContext(), parent, name) != null)
                    throw new FileNotFoundException("invalid or duplicate destination");
                String id = UUID.randomUUID().toString(); boolean directory = Document.MIME_TYPE_DIR.equals(mime);
                if (!directory) Files.write(bytes(getContext(), id).toPath(), new byte[0]);
                save(getContext(), all.put(id, node(parent, name, directory)));
                return id;
            } catch (Exception error) { throw new FileNotFoundException(error.toString()); }
        }
    }
    @Override public String renameDocument(String id, String name) throws FileNotFoundException {
        synchronized (LOCK) {
            try {
                JSONObject all = nodes(getContext()), item = required(all, id);
                String parent = item.getString("parent"), previous = item.getString("name");
                boolean publishing = previous.startsWith(".oneui-") && previous.contains(".download") && !name.startsWith(".oneui-");
                var controls = getContext().getSharedPreferences("provider-controls", 0);
                if (publishing && controls.getBoolean("fail-publish", false)) throw new java.io.IOException("injected provider rename failure");
                if (find(getContext(), parent, name) != null) throw new java.io.IOException("fixture rename never overwrites");
                item.put("name", name);
                if (controls.getBoolean("change-ids", false)) {
                    String replacement = UUID.randomUUID().toString();
                    if (!item.getBoolean("directory")) Files.move(bytes(getContext(), id).toPath(), bytes(getContext(), replacement).toPath());
                    all.remove(id); all.put(replacement, item); id = replacement;
                }
                save(getContext(), all);
                if (publishing && controls.getBoolean("kill-publish", false)) {
                    control(getContext(), "kill-publish", false);
                    control(getContext(), "crash-occurred", true);
                    android.os.Process.killProcess(android.os.Process.myPid());
                }
                return id;
            } catch (Exception error) { throw new FileNotFoundException(error.toString()); }
        }
    }
    @Override public void deleteDocument(String id) throws FileNotFoundException {
        synchronized (LOCK) {
            try {
                JSONObject all = nodes(getContext()); required(all, id);
                if (all.getJSONObject(id).getString("name").contains(".backup")
                        && getContext().getSharedPreferences("provider-controls", 0).getBoolean("fail-delete-backup", false))
                    throw new java.io.IOException("injected backup deletion failure");
                if (children(getContext(), id) != 0) throw new java.io.IOException("fixture folder not empty");
                all.remove(id); save(getContext(), all); Files.deleteIfExists(bytes(getContext(), id).toPath());
            } catch (Exception error) { throw new FileNotFoundException(error.toString()); }
        }
    }
    @Override public boolean isChildDocument(String parent, String id) {
        synchronized (LOCK) {
            try {
                JSONObject all = nodes(getContext());
                while (!id.equals(parent)) { id = required(all, id).getString("parent"); if (id.isEmpty()) return false; }
                return true;
            } catch (Exception error) { return false; }
        }
    }
}
