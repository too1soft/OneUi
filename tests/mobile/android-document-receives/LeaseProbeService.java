package net.too1.oneui.documents;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import org.json.JSONObject;

/** A separate app process probes the actual kernel lease, after local overlap attempts. */
public final class LeaseProbeService extends Service {
    @Override public IBinder onBind(Intent intent) {
        return new Binder() {
            @Override protected boolean onTransact(int code, Parcel input, Parcel output, int flags) {
                if (code != IBinder.FIRST_CALL_TRANSACTION || Binder.getCallingUid() != getApplicationInfo().uid) return false;
                try {
                    ReceiveContracts helper = new ReceiveContracts(getApplicationContext());
                    DocumentAccess access = helper.access("contract"); String root = helper.pin(access).getString("root");
                    JSONObject result = helper.raw(access, "receive-open", "root", root, "key", "resume", "path", "中文😀.bin",
                        "overwrite", false, "expected", JSONObject.NULL);
                    if (result.has("value")) {
                        JSONObject token = result.getJSONObject("value");
                        if (token.has("fd")) {
                            ParcelFileDescriptor.adoptFd(token.getInt("fd")).close();
                            helper.close(access, token, "lease probe was unexpectedly admitted");
                        }
                    }
                    output.writeString(new JSONObject().put("pid", android.os.Process.myPid()).put("result", result).toString());
                } catch (Exception error) { output.writeString("{\"probeError\":" + JSONObject.quote(error.toString()) + "}"); }
                return true;
            }
        };
    }
}
