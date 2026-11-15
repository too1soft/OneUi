package net.too1.oneui.documents;

import android.app.Activity;
import android.app.Instrumentation;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.system.Os;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;
import org.json.JSONArray;
import org.json.JSONObject;

/** Independent process phases deliberately retain SDK journals between invocations. */
public final class DocumentReceiveContractRunner extends Instrumentation {
    private String mode;
    private ReceiveContracts test;
    private ParcelFileDescriptor held;
    @Override public void onCreate(Bundle args) { super.onCreate(args); mode = args.getString("phase", "contracts"); start(); }
    private void restart(boolean prepare, boolean hold) throws Exception {
        if (prepare) OwnedReceiveProvider.reset(getTargetContext());
        DocumentAccess access = test.access("restart"); String root = test.pin(access).getString("root");
        JSONObject token = test.open(access, root, "restart", "restart.bin", false, null);
        if (prepare && hold) {
            held = ParcelFileDescriptor.adoptFd(token.getInt("fd"));
            byte[] prefix = test.bytes("persisted prefix");
            test.check(Os.write(held.getFileDescriptor(), prefix, 0, prefix.length) == prefix.length, "held_prefix_written");
            Os.fsync(held.getFileDescriptor());
            Files.write(new File(getTargetContext().getFilesDir(), "hold-ready.json").toPath(), token.toString().getBytes(StandardCharsets.UTF_8));
            while (true) Thread.sleep(1000); // Driver observes the marker and force-stops this owned package.
        }
        if (prepare) {
            test.write(access, token, new byte[0], test.bytes("persisted prefix"));
            test.check(OwnedReceiveProvider.find(getTargetContext(), "selected", "restart.bin") == null, "restart_preparation_keeps_target_absent");
        } else {
            test.check(!token.getBoolean("created"), "new_process_reopens_previous_temporary");
            test.write(access, token, test.bytes("persisted prefix"), test.bytes(" and suffix"));
            test.finish(access, root, "restart");
            test.check(Arrays.equals(OwnedReceiveProvider.content(getTargetContext(), "selected", "restart.bin"),
                test.bytes("persisted prefix and suffix")), "restart_publishes_complete_provider_content");
            test.success(access, "receive-forget", "root", root, "key", "restart");
            test.check(OwnedReceiveProvider.children(getTargetContext(), "selected") == 1, "restart_removes_owned_siblings");
        }
    }
    private void crash(boolean prepare, boolean changingIds) throws Exception {
        if (prepare) OwnedReceiveProvider.reset(getTargetContext());
        DocumentAccess access = test.access(changingIds ? "changing-crash" : "opaque-crash");
        String root = test.pin(access).getString("root"); String key = "crash";
        if (prepare) {
            JSONObject token = test.open(access, root, "original", "crash.bin", false, null);
            test.write(access, token, new byte[0], test.bytes("old bytes")); test.finish(access, root, "original");
            token = test.open(access, root, key, "crash.bin", true, test.stat(access, root, "crash.bin"));
            test.write(access, token, new byte[0], test.bytes("new complete bytes"));
            OwnedReceiveProvider.control(getTargetContext(), "change-ids", changingIds);
            OwnedReceiveProvider.control(getTargetContext(), "kill-publish", true);
            test.finish(access, root, key);
            throw new AssertionError("fixture did not kill process after persisted publication");
        }
        test.check(getTargetContext().getSharedPreferences("provider-controls", 0).getBoolean("crash-occurred", false), "provider_confirmed_crash_after_rename");
        JSONObject recovery = test.raw(access, "receive-recover", "root", root, "key", key);
        if (changingIds) {
            test.check(recovery.has("error"), "ambiguous_changed_id_requires_recovery");
            test.check(OwnedReceiveProvider.children(getTargetContext(), "selected") == 2, "ambiguous_publication_preserves_both_copies");
            test.success(access, "receive-discard", "root", root, "key", key);
            test.check(OwnedReceiveProvider.children(getTargetContext(), "selected") == 2, "discard_preserves_uncertain_original_as_recovered_copy");
        } else {
            test.check(recovery.getJSONObject("value").getString("stage").equals("published"), "opaque_identity_recovers_published_receipt");
            test.finish(access, root, key);
            test.success(access, "receive-forget", "root", root, "key", key);
            test.check(OwnedReceiveProvider.children(getTargetContext(), "selected") == 1, "recovered_receipt_cleans_verified_backup");
        }
        test.check(Arrays.equals(OwnedReceiveProvider.content(getTargetContext(), "selected", "crash.bin"), test.bytes("new complete bytes")), "crash_recovery_keeps_published_content");
    }
    @Override public void onStart() {
        Bundle report = new Bundle(); test = new ReceiveContracts(getTargetContext());
        try {
            switch (mode) {
                case "contracts" -> test.run();
                case "prepare-resume" -> restart(true, false);
                case "hold-open" -> restart(true, true);
                case "resume" -> restart(false, false);
                case "crash-opaque" -> crash(true, false);
                case "recover-opaque" -> crash(false, false);
                case "crash-changing" -> crash(true, true);
                case "recover-changing" -> crash(false, true);
                default -> throw new AssertionError("unknown phase");
            }
            report.putString("passed", "true");
        } catch (Throwable error) {
            report.putString("passed", "false"); report.putString("error", android.util.Log.getStackTraceString(error));
        }
        report.putString("phase", mode + ":" + test.phase); report.putString("checks", new JSONArray(test.passed).toString());
        report.putString("provider", "fixture-owned-same-uid"); finish(Activity.RESULT_OK, report);
    }
}
