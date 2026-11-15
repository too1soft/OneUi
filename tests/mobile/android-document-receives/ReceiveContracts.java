package net.too1.oneui.documents;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.Os;
import android.system.OsConstants;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import org.json.JSONArray;
import org.json.JSONObject;

/** Assertions exercise public JSON operations and actual provider descriptors. */
final class ReceiveContracts {
    final Context context;
    final ArrayList<String> passed = new ArrayList<>();
    String phase = "start";
    ReceiveContracts(Context context) { this.context = context; }
    void check(boolean condition, String name) {
        phase = name;
        if (!condition) throw new AssertionError(name);
        passed.add(name);
    }
    JSONObject raw(DocumentAccess access, String operation, Object... fields) throws Exception {
        JSONObject request = new JSONObject().put("op", operation);
        for (int i = 0; i < fields.length; i += 2) request.put((String)fields[i], fields[i + 1]);
        return new JSONObject(access.request(request.toString()));
    }
    JSONObject object(DocumentAccess access, String operation, Object... fields) throws Exception {
        JSONObject result = raw(access, operation, fields);
        if (result.has("error")) throw new AssertionError(operation + ": " + result);
        return result.getJSONObject("value");
    }
    void success(DocumentAccess access, String operation, Object... fields) throws Exception {
        JSONObject result = raw(access, operation, fields);
        if (result.has("error")) throw new AssertionError(operation + ": " + result);
    }
    String select(DocumentAccess access, boolean writable) throws Exception {
        Uri grant = DocumentsContract.buildTreeDocumentUri(OwnedReceiveProvider.AUTHORITY, "root");
        return access.select(grant, Intent.FLAG_GRANT_READ_URI_PERMISSION
            | (writable ? Intent.FLAG_GRANT_WRITE_URI_PERMISSION : 0)).getString("path") + "/selected";
    }
    JSONObject pin(DocumentAccess access) throws Exception { return object(access, "receive-root", "path", select(access, true)); }
    DocumentAccess access(String name) { return new DocumentAccess(context, name, "/@receive/tree/"); }
    JSONObject open(DocumentAccess access, String root, String key, String path, boolean overwrite, Object expected) throws Exception {
        return object(access, "receive-open", "root", root, "key", key, "path", path,
            "overwrite", overwrite, "expected", expected == null ? JSONObject.NULL : expected);
    }
    byte[] bytes(String value) { return value.getBytes(StandardCharsets.UTF_8); }
    void data(JSONObject token, byte[] prefix, byte[] suffix) throws Exception {
        try (ParcelFileDescriptor descriptor = ParcelFileDescriptor.adoptFd(token.getInt("fd"))) {
            byte[] actual = new byte[prefix.length]; int count = 0;
            while (count < actual.length) {
                int read = Os.read(descriptor.getFileDescriptor(), actual, count, actual.length - count);
                if (read == 0) break;
                count += read;
            }
            check(count == prefix.length && Arrays.equals(actual, prefix), "actual_prefix_" + passed.size());
            check(Os.fstat(descriptor.getFileDescriptor()).st_size == prefix.length, "untruncated_length_" + passed.size());
            Os.lseek(descriptor.getFileDescriptor(), prefix.length, OsConstants.SEEK_SET);
            int written = 0;
            while (written < suffix.length) written += Os.write(descriptor.getFileDescriptor(), suffix, written, suffix.length - written);
            Os.fsync(descriptor.getFileDescriptor());
        }
    }
    void close(DocumentAccess access, JSONObject token, String error) throws Exception {
        success(access, "close", "descriptor", token.getString("descriptor"), "error", error);
    }
    void write(DocumentAccess access, JSONObject token, byte[] prefix, byte[] suffix) throws Exception {
        data(token, prefix, suffix); close(access, token, "");
    }
    JSONObject stat(DocumentAccess access, String root, String path) throws Exception {
        return object(access, "receive-stat", "root", root, "path", path);
    }
    void finish(DocumentAccess access, String root, String key) throws Exception {
        object(access, "receive-finish", "root", root, "key", key);
    }
    private void crossProcessLease() throws Exception {
        var ready = new java.util.concurrent.CountDownLatch(1);
        android.os.IBinder[] remote = new android.os.IBinder[1];
        android.content.ServiceConnection connection = new android.content.ServiceConnection() {
            @Override public void onServiceConnected(android.content.ComponentName name, android.os.IBinder binder) { remote[0] = binder; ready.countDown(); }
            @Override public void onServiceDisconnected(android.content.ComponentName name) { }
        };
        check(context.bindService(new Intent(context, LeaseProbeService.class), connection, Context.BIND_AUTO_CREATE), "separate_lease_process_bound");
        try {
            check(ready.await(2, java.util.concurrent.TimeUnit.SECONDS), "separate_lease_process_ready");
            android.os.Parcel input = android.os.Parcel.obtain(), output = android.os.Parcel.obtain();
            try {
                check(remote[0].transact(android.os.IBinder.FIRST_CALL_TRANSACTION, input, output, 0), "separate_lease_probe_called");
                JSONObject response = new JSONObject(output.readString());
                check(response.getInt("pid") != android.os.Process.myPid(), "lease_probe_has_different_actual_pid");
                check(response.getJSONObject("result").has("error"), "local_overlap_does_not_release_cross_process_kernel_lease");
            } finally { input.recycle(); output.recycle(); }
        } finally { context.unbindService(connection); }
    }
    void ownershipAndResume() throws Exception {
        DocumentAccess access = access("contract");
        JSONObject cap = object(access, "capabilities");
        check(cap.getInt("version") == 2 && cap.getInt("receiveTransactions") == 1 && cap.getBoolean("pinnedSources"), "additive_capability");
        String selected = select(access, false);
        check(raw(access, "receive-root", "path", selected).optString("errorType").equals("SecurityException"), "write_grant_required");
        String root = pin(access).getString("root"), path = "中文😀.bin", key = "resume";
        JSONObject token = open(access, root, key, path, false, null);
        check(token.getBoolean("created"), "new_temporary_created");
        check(raw(access, "receive-stat", "root", root, "path", path).isNull("value"), "target_absent_before_publication");
        check(raw(access, "receive-finish", "root", root, "key", key).has("error"), "cannot_finish_open_descriptor");
        DocumentAccess other = access("contract"); String otherRoot = pin(other).getString("root");
        check(raw(other, "receive-open", "root", otherRoot, "key", key, "path", path, "overwrite", false,
            "expected", JSONObject.NULL).has("error"), "cross_instance_lease_exclusion");
        crossProcessLease();
        byte[] first = bytes("prefix 中文😀"); write(access, token, new byte[0], first);
        check(raw(access, "close", "descriptor", token.getString("descriptor")).has("error"), "descriptor_cannot_close_twice");
        check(raw(access, "receive-open", "root", root, "key", key, "path", "different.bin", "overwrite", false,
            "expected", JSONObject.NULL).has("error"), "receive_key_cannot_change_target");
        token = open(other, otherRoot, key, path, false, null);
        check(!token.getBoolean("created"), "fresh_access_reopens_same_transaction");
        write(other, token, first, bytes(" tail")); finish(other, otherRoot, key);
        check(Arrays.equals(OwnedReceiveProvider.content(context, "selected", path), bytes("prefix 中文😀 tail")), "provider_target_has_complete_content");
        JSONObject replay = open(access, root, key, path, false, null);
        check(replay.getString("stage").equals("published") && !replay.has("fd"), "published_receipt_replay_has_no_writer");
        finish(access, root, key); success(access, "receive-forget", "root", root, "key", key);
        check(raw(access, "receive-recover", "root", root, "key", key).has("error"), "forget_removes_completed_receipt");
        check(OwnedReceiveProvider.children(context, "selected") == 1, "no_receiving_siblings_after_publish");
        for (String unsafe : new String[]{"..", "/outside", "a//b", "a\\b", "./file", "a/"})
            check(raw(access, "receive-stat", "root", root, "path", unsafe).has("error"), "unsafe_relative_rejected_" + passed.size());
        check(raw(access, "receive-list", "root", root, "path", path, "limit", 10).has("error"), "listing_requires_directory");
        success(access, "receive-mkdir", "root", root, "path", "folder/sub");
        check(stat(access, root, "folder/sub").getBoolean("directory"), "nested_directories_created");
        check(raw(access, "receive-list", "root", root, "path", "", "limit", 1).has("error"), "bounded_receiver_listing");
        success(access, "receive-root-close", "root", root);
        check(raw(access, "receive-stat", "root", root, "path", path).has("error"), "closed_root_rejected");
        success(other, "receive-root-close", "root", otherRoot);
    }
    void conflictsAndFailures() throws Exception {
        DocumentAccess access = access("failures"); String root = pin(access).getString("root");
        JSONObject token = open(access, root, "initial", "existing.bin", false, null);
        write(access, token, new byte[0], bytes("original")); finish(access, root, "initial");
        JSONObject original = stat(access, root, "existing.bin");
        check(raw(access, "receive-open", "root", root, "key", "stale", "path", "existing.bin",
            "overwrite", true, "expected", JSONObject.NULL).has("error"), "stale_target_snapshot_rejected_before_open");
        check(raw(access, "receive-open", "root", root, "key", "no-confirm", "path", "existing.bin",
            "overwrite", false, "expected", original).has("error"), "overwrite_requires_explicit_confirmation");
        token = open(access, root, "changed", "existing.bin", true, original);
        write(access, token, new byte[0], bytes("replacement"));
        OwnedReceiveProvider.replaceBytes(context, "selected", "existing.bin", bytes("outside edit longer"));
        check(raw(access, "receive-finish", "root", root, "key", "changed").has("error"), "target_change_prevents_publication");
        success(access, "receive-discard", "root", root, "key", "changed");
        check(Arrays.equals(OwnedReceiveProvider.content(context, "selected", "existing.bin"), bytes("outside edit longer")), "discard_preserves_external_target");
        token = open(access, root, "retry", "existing.bin", true, stat(access, root, "existing.bin"));
        write(access, token, new byte[0], bytes("replacement"));
        OwnedReceiveProvider.control(context, "fail-publish", true);
        check(raw(access, "receive-finish", "root", root, "key", "retry").has("error"), "provider_publication_failure_propagated");
        check(Arrays.equals(OwnedReceiveProvider.content(context, "selected", "existing.bin"), bytes("outside edit longer")), "rename_failure_restores_original");
        OwnedReceiveProvider.control(context, "fail-publish", false); finish(access, root, "retry");
        check(Arrays.equals(OwnedReceiveProvider.content(context, "selected", "existing.bin"), bytes("replacement")), "publication_retry_succeeds");
        success(access, "receive-forget", "root", root, "key", "retry");
        token = open(access, root, "io-error", "failed.bin", false, null);
        data(token, new byte[0], bytes("partial")); close(access, token, "injected write failure");
        check(raw(access, "receive-finish", "root", root, "key", "io-error").has("error"), "failed_io_cannot_publish");
        check(OwnedReceiveProvider.find(context, "selected", "failed.bin") == null, "failed_io_target_absent");
        token = open(access, root, "io-error", "failed.bin", false, null);
        write(access, token, bytes("partial"), bytes(" complete")); finish(access, root, "io-error");
        check(Arrays.equals(OwnedReceiveProvider.content(context, "selected", "failed.bin"), bytes("partial complete")), "failed_io_can_reopen_for_verified_resume");
        // Revoking the locally recorded write permission must stop mutations even for a permissive same-UID provider.
        select(access, false);
        check(raw(access, "receive-stat", "root", root, "path", "existing.bin").optString("errorType").equals("SecurityException"), "grant_rechecked_for_every_receive_operation");
        success(access, "receive-root-close", "root", root);
    }
    void providerErrorsAndCancellation() throws Exception {
        DocumentAccess access = access("provider-errors"); String root = pin(access).getString("root");
        OwnedReceiveProvider.control(context, "close-error", true);
        JSONObject token = open(access, root, "close-error", "provider-error.bin", false, null);
        try (ParcelFileDescriptor data = ParcelFileDescriptor.adoptFd(token.getInt("fd"))) { }
        check(raw(access, "close", "descriptor", token.getString("descriptor")).has("error"), "actual_provider_error_channel_propagated");
        OwnedReceiveProvider.control(context, "close-error", false);
        check(raw(access, "receive-finish", "root", root, "key", "close-error").has("error"), "provider_close_error_blocks_publication");
        success(access, "receive-discard", "root", root, "key", "close-error");
        check(OwnedReceiveProvider.find(context, "selected", "provider-error.bin") == null, "provider_error_target_never_published");
        var executor = java.util.concurrent.Executors.newSingleThreadExecutor();
        android.os.CancellationSignal cancellation = new android.os.CancellationSignal();
        OwnedReceiveProvider.openEntered = new java.util.concurrent.CountDownLatch(1);
        OwnedReceiveProvider.blockOpen = true;
        try {
            String request = new JSONObject().put("op", "receive-open").put("root", root).put("key", "cancel")
                .put("path", "cancel.bin").put("overwrite", false).put("expected", JSONObject.NULL).toString();
            var result = executor.submit(() -> access.request(request, cancellation));
            check(OwnedReceiveProvider.openEntered.await(2, java.util.concurrent.TimeUnit.SECONDS), "actual_provider_write_open_entered");
            cancellation.cancel();
            check(new JSONObject(result.get(2, java.util.concurrent.TimeUnit.SECONDS)).optString("errorType").equals("OperationCanceledException"), "actual_provider_write_open_cancelled");
        } finally { OwnedReceiveProvider.blockOpen = false; cancellation.cancel(); executor.shutdownNow(); }
        check(raw(access, "receive-finish", "root", root, "key", "cancel").has("error"), "cancelled_open_cannot_publish");
        token = open(access, root, "cancel", "cancel.bin", false, null);
        write(access, token, new byte[0], bytes("retried")); finish(access, root, "cancel");
        check(Arrays.equals(OwnedReceiveProvider.content(context, "selected", "cancel.bin"), bytes("retried")), "cancelled_open_releases_lease_for_retry");
        success(access, "receive-root-close", "root", root);
    }
    void changedPublishedTargetKeepsBackup() throws Exception {
        DocumentAccess access = access("backup-safety"); String root = pin(access).getString("root");
        JSONObject token = open(access, root, "original", "safe.bin", false, null);
        write(access, token, new byte[0], bytes("safe original")); finish(access, root, "original");
        token = open(access, root, "replace", "safe.bin", true, stat(access, root, "safe.bin"));
        write(access, token, new byte[0], bytes("new content"));
        OwnedReceiveProvider.control(context, "fail-delete-backup", true);
        check(raw(access, "receive-finish", "root", root, "key", "replace").has("error"), "failed_backup_cleanup_retains_published_receipt");
        OwnedReceiveProvider.replaceBytes(context, "selected", "safe.bin", bytes("outside change after publication"));
        OwnedReceiveProvider.control(context, "fail-delete-backup", false);
        success(access, "receive-discard", "root", root, "key", "replace");
        JSONArray entries = raw(access, "receive-list", "root", root, "path", "", "limit", 100).getJSONArray("value");
        boolean preserved = false;
        for (int i = 0; i < entries.length(); i++) {
            String name = entries.getJSONObject(i).getString("name");
            if (name.startsWith("Recovered-") && name.endsWith("-safe.bin"))
                preserved |= Arrays.equals(OwnedReceiveProvider.content(context, "selected", name), bytes("safe original"));
        }
        check(preserved, "modified_published_target_preserves_original_backup");
        check(Arrays.equals(OwnedReceiveProvider.content(context, "selected", "safe.bin"), bytes("outside change after publication")), "discard_keeps_modified_published_target");
        success(access, "receive-root-close", "root", root);
    }
    void pinsLimitsAndLegacy() throws Exception {
        DocumentAccess access = access("pins"); String selected = select(access, true);
        JSONObject pin = object(access, "receive-root", "path", selected); String root = pin.getString("root");
        Uri old = DocumentsContract.buildDocumentUriUsingTree(DocumentsContract.buildTreeDocumentUri(OwnedReceiveProvider.AUTHORITY, "root"), "selected");
        DocumentsContract.renameDocument(context.getContentResolver(), old, "moved");
        Uri parent = DocumentsContract.buildDocumentUriUsingTree(DocumentsContract.buildTreeDocumentUri(OwnedReceiveProvider.AUTHORITY, "root"), "root");
        String replacement = DocumentsContract.getDocumentId(DocumentsContract.createDocument(context.getContentResolver(), parent,
            DocumentsContract.Document.MIME_TYPE_DIR, "selected"));
        JSONObject newer = object(access, "receive-root", "path", selected);
        check(!pin.getString("scope").equals(newer.getString("scope")), "replaced_selection_has_distinct_scope");
        JSONObject token = open(access, root, "pinned", "bound.bin", false, null);
        write(access, token, new byte[0], bytes("bound")); finish(access, root, "pinned");
        check(OwnedReceiveProvider.find(context, "selected", "bound.bin") != null
            && OwnedReceiveProvider.find(context, replacement, "bound.bin") == null, "writer_stays_in_original_bound_folder");
        success(access, "receive-root-close", "root", root); success(access, "receive-root-close", "root", newer.getString("root"));
        ArrayList<String> roots = new ArrayList<>();
        for (int i = 0; i < 32; i++) roots.add(pin(access).getString("root"));
        check(raw(access, "receive-root", "path", selected).has("error"), "receive_root_capacity_bounded");
        for (String held : roots) success(access, "receive-root-close", "root", held);
        root = pin(access).getString("root"); check(!root.isEmpty(), "receive_root_capacity_recovered");
        success(access, "receive-root-close", "root", root);
        String legacyPath = selected + "/legacy.bin";
        token = object(access, "begin", "path", legacyPath, "overwrite", false);
        write(access, token, new byte[0], bytes("legacy"));
        success(access, "finish", "path", legacyPath, "token", token);
        check(Arrays.equals(OwnedReceiveProvider.content(context, replacement, "legacy.bin"), bytes("legacy")), "legacy_begin_finish_compatible");
        token = object(access, "begin", "path", legacyPath, "overwrite", true);
        write(access, token, new byte[0], bytes("discarded"));
        success(access, "discard", "path", legacyPath, "token", token);
        check(Arrays.equals(OwnedReceiveProvider.content(context, replacement, "legacy.bin"), bytes("legacy")), "legacy_discard_keeps_original");
    }
    void run() throws Exception {
        OwnedReceiveProvider.reset(context); ownershipAndResume(); conflictsAndFailures(); providerErrorsAndCancellation();
        changedPublishedTargetKeepsBackup(); pinsLimitsAndLegacy();
    }
}
