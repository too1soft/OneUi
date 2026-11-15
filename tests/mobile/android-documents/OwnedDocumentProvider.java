package net.too1.oneui.documents;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.AbstractCursor;
import android.database.Cursor;
import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/** A fixture-owned provider. This does not grant access to a system SAF tree. */
public final class OwnedDocumentProvider extends ContentProvider {
    static final String AUTHORITY = "net.too1.oneui.documents.fixture";
    static volatile boolean replaced, denied, blocked;
    static volatile int rows, contentQueries, cursorCloses, opened;
    static volatile CountDownLatch entered = new CountDownLatch(1);
    private static final long MODIFIED = 1700000000123L;
    private static final String[] COLUMNS = {Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
        Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE, Document.COLUMN_LAST_MODIFIED};
    record Node(String id, String name, boolean directory) { }
    @Override public boolean onCreate() { return true; }
    static void reset() { replaced = denied = blocked = false; rows = contentQueries = cursorCloses = opened = 0; entered = new CountDownLatch(1); }
    private Node node(String id) {
        return switch (id) {
            case "root" -> new Node(id, "chosen", true);
            case "original" -> new Node(id, replaced ? "moved" : "selected", true);
            case "replacement" -> new Node(id, "selected", true);
            case "original-data" -> new Node(id, "中文😀.txt", false);
            case "replacement-data" -> new Node(id, "中文😀.txt", false);
            case "large" -> new Node(id, "large", true);
            default -> new Node(id, id, false);
        };
    }
    @Override public Cursor query(Uri uri, String[] projection, String selection, String[] args, String order) {
        return query(uri, projection, selection, args, order, null);
    }
    @Override public Cursor query(Uri uri, String[] projection, String selection, String[] args, String order, CancellationSignal cancellation) {
        if (denied) throw new SecurityException("fixture read denied");
        if (blocked) {
            entered.countDown();
            while (cancellation == null || !cancellation.isCanceled()) {
                try { Thread.sleep(1); } catch (InterruptedException error) { Thread.currentThread().interrupt(); break; }
            }
            if (cancellation != null) cancellation.throwIfCanceled();
        }
        String id = DocumentsContract.getDocumentId(uri);
        boolean children = uri.getLastPathSegment().equals("children");
        List<Node> values = new ArrayList<>();
        int count;
        if (children && id.equals("large")) count = 100000;
        else {
            if (!children) values.add(node(id));
            else if (id.equals("root")) {
                values.add(node(replaced ? "replacement" : "original")); values.add(node("large"));
            } else if (id.equals("original")) values.add(node("original-data"));
            else if (id.equals("replacement")) values.add(node("replacement-data"));
            count = values.size();
        }
        final int size = count;
        return new AbstractCursor() {
            @Override public int getCount() { return size; }
            @Override public String[] getColumnNames() { return COLUMNS; }
            private Node current() { return values.isEmpty() ? node("item-" + getPosition()) : values.get(getPosition()); }
            @Override public boolean onMove(int old, int next) { rows++; return true; }
            @Override public String getString(int column) {
                Node value = current();
                return switch (column) {
                    case 0 -> value.id; case 1 -> value.name;
                    case 2 -> value.directory ? Document.MIME_TYPE_DIR : "text/plain";
                    default -> Long.toString(getLong(column));
                };
            }
            @Override public long getLong(int column) {
                contentQueries++;
                if (column == 4) return MODIFIED;
                if (current().directory) return 0;
                return (current().id.equals("original-data") ? "provider bytes 中文😀" : "replacement")
                    .getBytes(java.nio.charset.StandardCharsets.UTF_8).length;
            }
            @Override public int getInt(int column) { return (int)getLong(column); }
            @Override public short getShort(int column) { return (short)getLong(column); }
            @Override public float getFloat(int column) { return getLong(column); }
            @Override public double getDouble(int column) { return getLong(column); }
            @Override public boolean isNull(int column) { return false; }
            @Override public void close() { cursorCloses++; super.close(); }
        };
    }
    @Override public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        return openFile(uri, mode, null);
    }
    @Override public ParcelFileDescriptor openFile(Uri uri, String mode, CancellationSignal cancellation) throws FileNotFoundException {
        if (denied) throw new SecurityException("fixture open denied");
        if (cancellation != null) cancellation.throwIfCanceled();
        if (!mode.equals("r")) throw new FileNotFoundException("read-only fixture");
        opened++;
        return ParcelFileDescriptor.open(new File(getContext().getFilesDir(), DocumentsContract.getDocumentId(uri)), ParcelFileDescriptor.MODE_READ_ONLY);
    }
    @Override public String getType(Uri uri) { return "text/plain"; }
    @Override public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException(); }
    @Override public int delete(Uri uri, String selection, String[] args) { throw new UnsupportedOperationException(); }
    @Override public int update(Uri uri, ContentValues values, String selection, String[] args) { throw new UnsupportedOperationException(); }
}
