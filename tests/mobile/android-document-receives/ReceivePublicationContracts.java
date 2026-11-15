package net.too1.oneui.documents;

import android.os.ParcelFileDescriptor;
import android.system.Os;
import android.system.OsConstants;
import java.nio.file.Files;
import java.util.Arrays;
import org.json.JSONObject;

/** Real SDK choices needed by the shared Go Rename/Ask receiver algorithm. */
final class ReceivePublicationContracts {
    private final ReceiveContracts test;
    ReceivePublicationContracts(ReceiveContracts test) { this.test = test; }
    void run() throws Exception { publication(); directories(); }
    private JSONObject publish(DocumentAccess access, String root, String key, String from, String target, boolean overwrite, Object expected) throws Exception {
        return test.raw(access, "receive-publish", "root", root, "key", key, "from", from, "path", target,
            "overwrite", overwrite, "expected", expected == null ? JSONObject.NULL : expected);
    }
    private void publication() throws Exception {
        DocumentAccess access = test.access("publication-adapter"); String root = test.pin(access).getString("root");
        test.check(test.object(access, "capabilities").getInt("receiveStorage") == 1, "receive_storage_capability");
        test.check(test.object(access, "receive-status", "root", root, "key", "missing").getString("stage").equals("absent"), "absent_transaction_status");
        JSONObject token = test.open(access, root, "seed", "collision.bin", false, null);
        test.write(access, token, new byte[0], test.bytes("keep collision")); test.finish(access, root, "seed");
        JSONObject original = test.stat(access, root, "collision.bin");
        token = test.open(access, root, "rename", "collision.bin", true, original);
        test.data(token, new byte[0], test.bytes("incoming rename"));
        JSONObject entry = test.object(access, "receive-stage-stat", "root", root, "key", "rename", "descriptor", token.getString("descriptor"));
        test.check(entry.getLong("size") == test.bytes("incoming rename").length, "live_stage_stat_with_held_lease");
        JSONObject other = test.open(access, root, "other-live-writer", "other-live.bin", false, null);
        test.check(test.raw(access, "receive-stage-stat", "root", root, "key", "other-live-writer", "descriptor", token.getString("descriptor")).has("error"), "stage_descriptor_cannot_cross_transaction");
        test.close(access, other, "");
        test.success(access, "receive-discard", "root", root, "key", "other-live-writer");
        test.check(publish(access, root, "rename", "collision.bin", "collision (1).bin", false, null).has("error"), "publish_waits_for_writer_close");
        test.close(access, token, "");
        test.check(publish(access, root, "rename", "collision.bin", "collision.bin", false, null).optString("errorType").equals("FileAlreadyExistsException"), "rename_collision_is_distinct_exists_error");
        test.check(publish(access, root, "rename", "different.bin", "collision (1).bin", false, null).has("error"), "target_rebind_requires_current_from");
        test.success(access, "receive-mkdir", "root", root, "path", "other-parent");
        test.check(publish(access, root, "rename", "collision.bin", "other-parent/collision.bin", false, null).has("error"), "target_rebind_cannot_move_parent");
        test.check(!publish(access, root, "rename", "collision.bin", "collision (1).bin", false, null).has("error"), "rename_publishes_existing_bytes_to_new_leaf");
        test.check(Arrays.equals(OwnedReceiveProvider.content(test.context, "selected", "collision.bin"), test.bytes("keep collision")), "retarget_preserves_collision");
        test.check(Arrays.equals(OwnedReceiveProvider.content(test.context, "selected", "collision (1).bin"), test.bytes("incoming rename")), "retarget_has_incoming_content");
        JSONObject status = test.object(access, "receive-status", "root", root, "key", "rename");
        test.check(status.getString("stage").equals("published") && status.getString("target").equals("collision (1).bin"), "status_recovers_published_target_binding");
        JSONObject reopened = test.object(access, "receive-reopen", "root", root, "key", "rename");
        test.check(reopened.getString("stage").equals("published") && !reopened.has("fd"), "reopen_completed_key_has_no_writer");
        test.check(!publish(access, root, "rename", "collision.bin", "collision (1).bin", false, null).has("error"), "publication_receipt_is_idempotent");
        test.check(publish(access, root, "rename", "collision (1).bin", "moved-again.bin", false, null).has("error"), "published_receipt_cannot_retarget_again");
        JSONObject read = test.object(access, "receive-read", "root", root, "path", "collision (1).bin");
        try (var stream = new ParcelFileDescriptor.AutoCloseInputStream(ParcelFileDescriptor.adoptFd(read.getInt("fd")))) {
            test.check(Arrays.equals(stream.readAllBytes(), test.bytes("incoming rename")), "published_read_uses_actual_provider_fd");
        }
        test.close(access, read, "");
        token = test.open(access, root, "ask-updated", "collision.bin", true, original);
        test.write(access, token, new byte[0], test.bytes("confirmed replacement"));
        OwnedReceiveProvider.replaceBytes(test.context, "selected", "collision.bin", test.bytes("outside changed original"));
        test.check(publish(access, root, "ask-updated", "collision.bin", "collision.bin", true, original).has("error"), "publish_rejects_stale_confirmation_snapshot");
        JSONObject current = test.stat(access, root, "collision.bin");
        test.check(!publish(access, root, "ask-updated", "collision.bin", "collision.bin", true, current).has("error"), "new_confirmation_can_publish_same_staged_bytes");
        test.check(Arrays.equals(OwnedReceiveProvider.content(test.context, "selected", "collision.bin"), test.bytes("confirmed replacement")), "new_confirmation_replaces_only_current_target");
        token = test.open(access, root, "reopen", "reopen-adapter.bin", false, null);
        test.write(access, token, new byte[0], test.bytes("prefix"));
        reopened = test.object(access, "receive-reopen", "root", root, "key", "reopen");
        test.check(reopened.getString("target").equals("reopen-adapter.bin"), "key_reopen_preserves_bound_target");
        test.write(access, reopened, test.bytes("prefix"), test.bytes(" tail"));
        test.success(access, "receive-discard", "root", root, "key", "reopen");
        test.success(access, "receive-root-close", "root", root);
    }
    private void directories() throws Exception {
        DocumentAccess access = test.access("directory-adapter"); String root = test.pin(access).getString("root");
        JSONObject entry = test.object(access, "receive-mkdir-exclusive", "root", root, "path", "exclusive", "key", "directory");
        test.check(entry.getBoolean("directory"), "exclusive_directory_created");
        JSONObject replay = test.object(access, "receive-mkdir-exclusive", "root", root, "path", "exclusive", "key", "directory");
        test.check(entry.getString("uri").equals(replay.getString("uri")), "owned_directory_create_is_idempotent");
        test.check(test.raw(access, "receive-mkdir-exclusive", "root", root, "path", "exclusive", "key", "another-owner").optString("errorType").equals("FileAlreadyExistsException"), "unowned_existing_directory_is_not_adopted");
        OwnedReceiveProvider.control(test.context, "directory-as-file", true);
        try {
            test.check(test.raw(access, "receive-directory-handles", "root", root, "path", "exclusive", "key", "directory").optString("errorType").equals("UnsupportedOperationException"), "regular_provider_fd_cannot_authorize_directory_cleanup");
        } finally { OwnedReceiveProvider.control(test.context, "directory-as-file", false); }
        JSONObject handles = test.object(access, "receive-directory-handles", "root", root, "path", "exclusive", "key", "directory");
        JSONObject parent = handles.getJSONObject("parent"), directory = handles.getJSONObject("directory");
        try (ParcelFileDescriptor p = ParcelFileDescriptor.adoptFd(parent.getInt("fd"));
             ParcelFileDescriptor d = ParcelFileDescriptor.adoptFd(directory.getInt("fd"))) {
            test.check(OsConstants.S_ISDIR(Os.fstat(p.getFileDescriptor()).st_mode)
                && OsConstants.S_ISDIR(Os.fstat(d.getFileDescriptor()).st_mode), "actual_parent_and_directory_fds_are_directories");
        }
        test.close(access, parent, "");
        test.check(test.raw(access, "receive-directory-forget", "root", root, "path", "exclusive", "key", "directory").has("error"), "directory_lease_waits_for_both_error_channels");
        test.close(access, directory, "");
        // Only the native backend may perform directory-only removal. Here the fixture performs the known empty-directory effect.
        Files.delete(OwnedReceiveProvider.bytes(test.context, OwnedReceiveProvider.find(test.context, "selected", "exclusive")).toPath());
        test.success(access, "receive-directory-forget", "root", root, "path", "exclusive", "key", "directory");
        test.check(test.raw(access, "receive-directory-handles", "root", root, "path", "exclusive", "key", "directory").has("error"), "forgotten_directory_cannot_reopen_handles");
        test.object(access, "receive-mkdir-exclusive", "root", root, "path", "replaced-directory", "key", "replace");
        String id = OwnedReceiveProvider.find(test.context, "selected", "replaced-directory");
        var file = OwnedReceiveProvider.bytes(test.context, id).toPath(); Files.delete(file); Files.write(file, test.bytes("preserve external file"));
        test.check(test.raw(access, "receive-directory-handles", "root", root, "path", "replaced-directory", "key", "replace").has("error"), "replaced_owned_directory_cannot_be_deleted_through_handles");
        test.check(Arrays.equals(Files.readAllBytes(file), test.bytes("preserve external file")), "directory_api_preserves_external_replacement");
        test.success(access, "receive-root-close", "root", root);
    }
}
