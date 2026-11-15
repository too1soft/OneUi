package net.too1.oneui.documents;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import org.json.JSONObject;
import android.system.Os;
import android.system.OsConstants;
import java.io.FileDescriptor;

/** Per-transaction atomic metadata and kernel leases survive process restart. */
final class DocumentReceiveJournal implements DocumentJournal {
    // POSIX record locks belong to the process: closing a second FD for the
    // same inode can release the first FD's lock. Reject local overlap BEFORE
    // opening another descriptor, not after FileChannel.tryLock rejects it.
    private static final Set<String> LOCAL_LEASES = ConcurrentHashMap.newKeySet();
    private final File directory;
    DocumentReceiveJournal(File directory) { this.directory = directory; }
    private File file(String id, String suffix) throws IOException {
        if (!id.matches("receive-[0-9a-f]{64}")) throw new IOException("Invalid receive record");
        if (!directory.isDirectory() && !directory.mkdirs() && !directory.isDirectory())
            throw new IOException("Unable to create receive metadata directory");
        return new File(directory, id + suffix);
    }
    @Override public JSONObject read(String id) throws Exception {
        File record = file(id, ".json");
        if (!record.exists()) return null;
        if (record.length() > 64 * 1024) throw new IOException("Receive metadata exceeds its limit");
        try (FileInputStream input = new FileInputStream(record)) {
            byte[] bytes = new byte[64 * 1024 + 1];
            int count = 0, read;
            while (count < bytes.length && (read = input.read(bytes, count, bytes.length - count)) != -1) count += read;
            if (count > 64 * 1024) throw new IOException("Receive metadata exceeds its limit");
            return new JSONObject(new String(bytes, 0, count, StandardCharsets.UTF_8));
        }
    }
    @Override public void save(String id, JSONObject value) throws Exception {
        byte[] bytes = value.toString().getBytes(StandardCharsets.UTF_8);
        if (bytes.length > 64 * 1024) throw new IOException("Receive metadata exceeds its limit");
        File temporary = file(id, ".new"), target = file(id, ".json");
        try (FileOutputStream output = new FileOutputStream(temporary)) {
            output.write(bytes);
            output.getFD().sync();
        }
        Files.move(temporary.toPath(), target.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        syncDirectory();
    }
    @Override public void remove(String id) throws Exception {
        File record = file(id, ".json");
        if (record.exists() && !record.delete()) throw new IOException("Unable to remove receive metadata");
        File temporary = file(id, ".new");
        if (temporary.exists() && !temporary.delete()) throw new IOException("Unable to remove unfinished receive metadata");
        syncDirectory();
    }
    private void syncDirectory() throws Exception {
        FileDescriptor fd = Os.open(directory.getPath(), OsConstants.O_RDONLY | OsConstants.O_CLOEXEC, 0);
        try {
            if (!OsConstants.S_ISDIR(Os.fstat(fd).st_mode)) throw new IOException("Receive metadata is not a directory");
            Os.fsync(fd);
        } finally { Os.close(fd); }
    }
    Lease lease(String id) throws IOException {
        File lock = file(id, ".lock");
        String key = lock.getCanonicalPath();
        if (!LOCAL_LEASES.add(key)) throw new IOException("The receiving document is already open");
        RandomAccessFile handle = null;
        try {
            handle = new RandomAccessFile(lock, "rw");
            FileLock held = handle.getChannel().tryLock();
            if (held == null) throw new IOException("The receiving document is already open");
            return new Lease(handle, held, key);
        } catch (IOException | RuntimeException failure) {
            try { if (handle != null) handle.close(); }
            catch (IOException closing) { failure.addSuppressed(closing); }
            finally { LOCAL_LEASES.remove(key); }
            throw new IOException("The receiving document is already open", failure);
        }
    }
    static final class Lease implements AutoCloseable {
        private final RandomAccessFile file;
        private final FileLock lock;
        private final String key;
        private boolean closed;
        Lease(RandomAccessFile file, FileLock lock, String key) { this.file = file; this.lock = lock; this.key = key; }
        @Override public synchronized void close() throws IOException {
            if (closed) return;
            closed = true;
            try { lock.release(); }
            finally { try { file.close(); } finally { LOCAL_LEASES.remove(key); } }
        }
    }
}
