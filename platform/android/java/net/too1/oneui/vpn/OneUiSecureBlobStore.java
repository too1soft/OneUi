package net.too1.oneui.vpn;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.AtomicFile;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.concurrent.ConcurrentHashMap;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** App-private authenticated blobs. Keys stay in AndroidKeyStore; ciphertext
 * uses AtomicFile in noBackupFilesDir. Background VPN access needs no biometric
 * prompt. Namespace/key are authenticated so blobs cannot be interchanged. */
public final class OneUiSecureBlobStore {
    private static final int MAX_BYTES = 1024 * 1024;
    private static final ConcurrentHashMap<String, Object> LOCKS = new ConcurrentHashMap<>();
    private final File directory;
    private final String namespace, alias;
    private final Object lock;

    public OneUiSecureBlobStore(Context context, String namespace) throws GeneralSecurityException, IOException {
        if (namespace == null || namespace.isEmpty() || namespace.length() > 128) throw new IllegalArgumentException("Secure namespace");
        this.namespace = namespace;
        String digest = digest(namespace);
        alias = "net.too1.oneui.secure." + digest;
        lock = LOCKS.computeIfAbsent(alias, unused -> new Object());
        directory = new File(context.getNoBackupFilesDir(), "oneui-secure/" + digest);
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Secure directory unavailable");
    }
    private static String digest(String value) throws GeneralSecurityException {
        byte[] hash = MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
        StringBuilder out = new StringBuilder(64);
        for (byte b : hash) out.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
        return out.toString();
    }
    private AtomicFile file(String key) throws GeneralSecurityException {
        if (key == null || key.isEmpty() || key.length() > 128) throw new IllegalArgumentException("Secure key");
        return new AtomicFile(new File(directory, digest(key)));
    }
    private SecretKey key(boolean create) throws GeneralSecurityException, IOException {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (store.containsAlias(alias)) return ((KeyStore.SecretKeyEntry)store.getEntry(alias, null)).getSecretKey();
        if (!create) throw new GeneralSecurityException("Secure key unavailable");
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setKeySize(256).setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true).setUserAuthenticationRequired(false).build());
        return generator.generateKey();
    }
    private byte[] aad(String name) { return (namespace + "\u0000" + name).getBytes(StandardCharsets.UTF_8); }

    /** Returns a new plaintext array or null if absent; caller clears the array. */
    public byte[] read(String name) throws GeneralSecurityException, IOException {
        synchronized (lock) { return readLocked(name); }
    }
    private byte[] readLocked(String name) throws GeneralSecurityException, IOException {
        AtomicFile target = file(name);
        byte[] data;
        try (FileInputStream input = target.openRead(); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] block = new byte[4096];
            int count;
            while ((count = input.read(block)) != -1) {
                if (output.size() + count > MAX_BYTES + 64) throw new IOException("Secure blob exceeds limit");
                output.write(block, 0, count);
            }
            data = output.toByteArray();
        } catch (java.io.FileNotFoundException missing) {
            if (target.getBaseFile().exists()) throw missing;
            return null;
        }
        if (data.length < 30 || data[0] != 1 || data[1] != 12) throw new GeneralSecurityException("Secure blob format");
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, key(false), new GCMParameterSpec(128, data, 2, 12));
        cipher.updateAAD(aad(name));
        return cipher.doFinal(data, 14, data.length - 14);
    }
    public void write(String name, byte[] plaintext) throws GeneralSecurityException, IOException {
        synchronized (lock) { writeLocked(name, plaintext); }
    }
    private void writeLocked(String name, byte[] plaintext) throws GeneralSecurityException, IOException {
        if (plaintext == null || plaintext.length == 0 || plaintext.length > MAX_BYTES) throw new IllegalArgumentException("Secure blob size");
        AtomicFile target = file(name);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, key(true));
        cipher.updateAAD(aad(name));
        byte[] encrypted = cipher.doFinal(plaintext), iv = cipher.getIV();
        if (iv.length != 12) throw new GeneralSecurityException("Secure IV length");
        FileOutputStream output = null;
        try {
            output = target.startWrite();
            output.write(new byte[]{1, 12}); output.write(iv); output.write(encrypted);
            target.finishWrite(output); output = null;
        } finally {
            if (output != null) target.failWrite(output);
            Arrays.fill(encrypted, (byte)0);
        }
    }
    public void delete(String name) throws GeneralSecurityException { synchronized (lock) { file(name).delete(); } }
}
