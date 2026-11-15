package net.too1.oneui.documents;

import org.json.JSONObject;

/** Ownership metadata only. No file payloads belong in this journal. */
interface DocumentJournal {
    JSONObject read(String id) throws Exception;
    void save(String id, JSONObject value) throws Exception;
    void remove(String id) throws Exception;
}
