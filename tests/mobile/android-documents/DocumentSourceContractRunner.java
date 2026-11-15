package net.too1.oneui.documents;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import org.json.JSONArray;
import org.json.JSONObject;

/** Actual Android ContentResolver calls against a controlled, same-UID provider. No product UI. */
public final class DocumentSourceContractRunner extends Instrumentation {
    private DocumentAccess access;
    private String root, phase = "start";
    private final ArrayList<String> passed = new ArrayList<>();
    @Override public void onCreate(Bundle args) { super.onCreate(args); start(); }
    private void check(boolean value, String name) {
        phase = name;
        if (!value) throw new AssertionError(name);
        passed.add(name);
    }
    private JSONObject request(String op, String source, String path) throws Exception {
        JSONObject input = new JSONObject().put("op", op);
        if (source != null) input.put("source", source);
        if (path != null) input.put("path", path);
        return new JSONObject(access.request(input.toString()));
    }
    private JSONObject pin(String path) throws Exception { return request("source-root", null, path).getJSONObject("value"); }
    private void ownership() throws Exception {
        JSONObject capabilities = request("capabilities", null, null).getJSONObject("value");
        check(capabilities.getInt("version") == 2 && capabilities.getBoolean("pinnedSources"), "source_capability");
        String source = pin(root + "/selected").getString("source");
        JSONObject entry = request("source-stat", source, "中文😀.txt").getJSONObject("value");
        check(entry.getLong("modified") == 1700000000L && entry.getLong("modifiedMillis") == 1700000000123L, "millisecond_metadata_and_legacy_seconds");
        OwnedDocumentProvider.replaced = true;
        entry = request("source-stat", source, "中文😀.txt").getJSONObject("value");
        check(entry.getString("uri").endsWith("original-data"), "pinned_id_survives_parent_replacement");
        String replacement = pin(root + "/selected").getString("source");
        check(request("source-stat", replacement, "中文😀.txt").getJSONObject("value").getString("uri").endsWith("replacement-data"), "new_root_uses_new_selection");
        for (String path : new String[]{"..", "/outside", "a//b", "a\\b", "./file"})
            check(request("source-stat", source, path).has("error"), "relative_path_rejected_" + passed.size());
        check(request("source-delete", source, "中文😀.txt").has("error"), "read_only_source_operations");
        byte[] expected = "provider bytes 中文😀".getBytes(StandardCharsets.UTF_8);
        JSONObject opened = request("source-open", source, "中文😀.txt").getJSONObject("value");
        try (ParcelFileDescriptor fd = ParcelFileDescriptor.adoptFd(opened.getInt("fd"));
             var input = new ParcelFileDescriptor.AutoCloseInputStream(fd)) {
            check(Arrays.equals(input.readAllBytes(), expected), "detached_descriptor_reads_original_document");
        }
        JSONObject close = new JSONObject().put("op", "close").put("descriptor", opened.getString("descriptor"));
        check(new JSONObject(access.request(close.toString())).getBoolean("value"), "retained_descriptor_closed");
        OwnedDocumentProvider.denied = true;
        check(request("source-stat", source, "中文😀.txt").getString("errorType").equals("SecurityException"), "provider_refusal_propagated");
        check(request("source-close", source, null).getBoolean("value"), "cleanup_after_permission_loss");
        OwnedDocumentProvider.denied = false;
        check(request("source-stat", source, "中文😀.txt").has("error"), "closed_source_cannot_reopen");
        request("source-close", replacement, null);
    }
    private void limitsAndCancellation() throws Exception {
        String source = pin(root + "/large").getString("source");
        OwnedDocumentProvider.rows = 0;
        JSONObject query = new JSONObject().put("op", "source-list").put("source", source).put("path", "").put("limit", 3);
        check(new JSONObject(access.request(query.toString())).has("error") && OwnedDocumentProvider.rows <= 5, "large_listing_stops_at_limit");
        int calls = OwnedDocumentProvider.rows;
        CancellationSignal cancelled = new CancellationSignal(); cancelled.cancel();
        check(new JSONObject(access.request(query.toString(), cancelled)).getString("errorType").equals("OperationCanceledException") && OwnedDocumentProvider.rows == calls, "cancelled_request_does_not_query_provider");
        var executor = Executors.newSingleThreadExecutor();
        CancellationSignal active = new CancellationSignal();
        OwnedDocumentProvider.blocked = true;
        try {
            var result = executor.submit(() -> access.request(query.toString(), active));
            check(OwnedDocumentProvider.entered.await(2, TimeUnit.SECONDS), "provider_query_started");
            active.cancel();
            check(new JSONObject(result.get(2, TimeUnit.SECONDS)).getString("errorType").equals("OperationCanceledException"), "inflight_query_cancelled");
        } finally { OwnedDocumentProvider.blocked = false; active.cancel(); executor.shutdownNow(); }
        request("source-close", source, null);
        ArrayList<String> tokens = new ArrayList<>();
        for (int i = 0; i < 32; i++) tokens.add(pin(root).getString("source"));
        check(request("source-root", null, root).has("error"), "open_roots_bounded");
        for (String token : tokens) request("source-close", token, null);
        source = pin(root).getString("source");
        check(!source.isEmpty(), "root_capacity_restored_after_close");
        request("source-close", source, null);
        check(OwnedDocumentProvider.cursorCloses > 0, "provider_cursors_closed");
    }
    @Override public void onStart() {
        Bundle report = new Bundle();
        try {
            OwnedDocumentProvider.reset();
            Files.write(new File(getTargetContext().getFilesDir(), "original-data").toPath(), "provider bytes 中文😀".getBytes(StandardCharsets.UTF_8));
            Files.write(new File(getTargetContext().getFilesDir(), "replacement-data").toPath(), "replacement".getBytes(StandardCharsets.UTF_8));
            access = new DocumentAccess(getTargetContext(), "owned-document-contract", "/@fixture/tree/");
            Uri grant = DocumentsContract.buildTreeDocumentUri(OwnedDocumentProvider.AUTHORITY, "root");
            root = access.select(grant, Intent.FLAG_GRANT_READ_URI_PERMISSION).getString("path");
            ownership(); limitsAndCancellation();
            report.putString("passed", "true");
        } catch (Throwable error) {
            report.putString("passed", "false"); report.putString("errorClass", error.getClass().getSimpleName());
        }
        report.putString("phase", phase); report.putString("checks", new JSONArray(passed).toString());
        report.putString("provider", "fixture-owned-same-uid");
        finish(Activity.RESULT_OK, report);
    }
}
