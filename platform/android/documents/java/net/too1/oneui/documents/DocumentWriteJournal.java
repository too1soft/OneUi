package net.too1.oneui.documents;

import android.content.SharedPreferences;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.IOException;
import java.util.Objects;
import java.util.UUID;
import org.json.JSONObject;

/** Durable ownership of receiving siblings. Recovery never scans or deletes unrelated files. */
final class DocumentWriteJournal {
    private static final String PROCESS = UUID.randomUUID().toString();
    private static final Object LOCK = new Object();
    private final SharedPreferences preferences;
    DocumentWriteJournal(SharedPreferences preferences) { this.preferences=preferences; }
    JSONObject begin(String id, String path, String temporary, String backup) throws Exception {
        JSONObject value=new JSONObject().put("owner",PROCESS).put("path",path).put("temporary",temporary)
            .put("backup",backup).put("stage","receiving");
        save(id,value);return value;
    }
    void save(String id, JSONObject value) throws Exception {
        synchronized(LOCK) {
            if(!preferences.edit().putString(id,value.toString()).commit())throw new IOException("Unable to record document recovery");
        }
    }
    void remove(String id) throws IOException {
        synchronized(LOCK) {
            if(!preferences.edit().remove(id).commit())throw new IOException("Unable to complete document recovery");
        }
    }
    void recover(DocumentTree tree) throws Exception {
        synchronized(LOCK) {
            for(var entry:preferences.getAll().entrySet()) {
                JSONObject value=new JSONObject((String)entry.getValue());
                if(PROCESS.equals(value.optString("owner"))||!value.getString("path").startsWith(tree.prefix+"/"))continue;
                discard(tree,entry.getKey(),value);
            }
        }
    }
    void discard(DocumentTree tree, String id, JSONObject value) throws Exception {
        String path=value.getString("path"),parent=DocumentAccess.parent(path);
        String temporaryName=DocumentAccess.name(value.getString("temporary"));
        String backupName=DocumentAccess.name(value.getString("backup"));
        Uri temporary=tree.resolve(parent+"/"+temporaryName,false);
        Uri backup=tree.resolve(parent+"/"+backupName,false);
        if(backup!=null) {
            if(value.optString("stage").equals("receiving"))throw new IOException("An unexpected recovery sibling exists");
            String recordedBackup=value.optString("backupUri");
            if(!recordedBackup.isEmpty()&&!backup.toString().equals(recordedBackup))
                throw new IOException("The original recovery copy changed");
            Uri target=tree.resolve(path,false);
            if(target==null)tree.renameChecked(backup,path.substring(path.lastIndexOf('/')+1));
            else if(value.optString("stage").equals("published")&&target.toString().equals(value.optString("publishedUri")))
                delete(tree,backup);
            else {
                // Publication may have completed before its journal commit. Keep both copies visible.
                String recovered="Recovered-"+UUID.randomUUID().toString().substring(0,8)+"-"+path.substring(path.lastIndexOf('/')+1);
                if(tree.resolve(parent+"/"+recovered,false)!=null)throw new IOException("A recovery copy already exists");
                tree.renameChecked(backup,recovered);
            }
        }
        if(temporary!=null) {
            String recorded=value.optString("temporaryUri");
            if(!recorded.isEmpty()&&!Objects.equals(temporary.toString(),recorded))
                throw new IOException("The receiving sibling changed before recovery");
            if(tree.metadata(temporary).getBoolean("directory"))throw new IOException("The receiving sibling is no longer a file");
            delete(tree,temporary);
        }
        remove(id);
    }
    private static void delete(DocumentTree tree, Uri uri) throws IOException {
        if(!DocumentsContract.deleteDocument(tree.resolver,uri))throw new IOException("Unable to remove an interrupted receiving file");
    }
}
