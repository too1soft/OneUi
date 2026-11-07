package net.too1.oneui.network;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.Base64;

/** Provider-neutral OAuth state, nonce and S256 proof. Memory only. */
public final class PkceProof {
    public final String state, nonce, verifier, challenge;
    public PkceProof() {
        state = random(); nonce = random(); verifier = random();
        try {
            challenge = encode(MessageDigest.getInstance("SHA-256").digest(verifier.getBytes(StandardCharsets.US_ASCII)));
        } catch (java.security.GeneralSecurityException error) { throw new IllegalStateException("secure_hash_unavailable"); }
    }
    private static String encode(byte[] bytes) { return Base64.getUrlEncoder().withoutPadding().encodeToString(bytes); }
    private static String random() {
        byte[] bytes = new byte[32]; new SecureRandom().nextBytes(bytes);
        String value = encode(bytes); Arrays.fill(bytes, (byte)0); return value;
    }
    public boolean matchesState(String supplied) {
        return supplied != null && MessageDigest.isEqual(state.getBytes(StandardCharsets.US_ASCII), supplied.getBytes(StandardCharsets.US_ASCII));
    }
}
