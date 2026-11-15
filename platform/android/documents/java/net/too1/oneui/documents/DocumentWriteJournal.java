package net.too1.oneui.documents;

import android.content.SharedPreferences;
import java.io.IOException;
import java.util.UUID;
import org.json.JSONObject;

/** Durable ownership of receiving siblings. Recovery never scans or deletes unrelated files. */
final class DocumentWriteJournal implements DocumentJournal {
    static final String PROCESS = UUID.randomUUID().toString();
    static final Object LOCK = new Object();
    private final SharedPreferences preferences;
    DocumentWriteJournal(SharedPreferences preferences) { this.preferences=preferences; }
    JSONObject begin(String id, String path, String temporary, String backup) throws Exception {
        JSONObject value=new JSONObject().put("owner",PROCESS).put("path",path).put("temporary",temporary)
            .put("backup",backup).put("stage","receiving");
        save(id,value);return value;
    }
    public void save(String id, JSONObject value) throws Exception {
        synchronized(LOCK) {
            if(!preferences.edit().putString(id,value.toString()).commit())throw new IOException("Unable to record document recovery");
        }
    }
    public void remove(String id) throws IOException {
        synchronized(LOCK) {
            if(!preferences.edit().remove(id).commit())throw new IOException("Unable to complete document recovery");
        }
    }
    public JSONObject read(String id) throws Exception {
        synchronized (LOCK) {
            String value = preferences.getString(id, null);
            return value == null ? null : new JSONObject(value);
        }
    }
    void recover(DocumentTree tree) throws Exception {
        synchronized(LOCK) {
            for(var entry:preferences.getAll().entrySet()) {
                JSONObject value=new JSONObject((String)entry.getValue());
                if(value.optBoolean("managed"))continue;
                if(PROCESS.equals(value.optString("owner"))||!value.getString("path").startsWith(tree.prefix+"/"))continue;
                discard(tree,entry.getKey(),value);
            }
        }
    }
    void discard(DocumentTree tree, String id, JSONObject value) throws Exception {
        DocumentRecovery.discard(tree,id,value,this);
    }
}
