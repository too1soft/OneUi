package net.too1.oneui.network;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.CookieHandler;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Bounded single-origin HTTPS transport. No cookies, redirects, logging,
 * custom trust store or automatic retries. The caller owns response bytes. */
public final class PinnedHttpClient {
    public static final class Cancellation {
        private final AtomicBoolean cancelled = new AtomicBoolean();
        private final AtomicReference<HttpURLConnection> connection = new AtomicReference<>();
        public void cancel() {
            cancelled.set(true);
            HttpURLConnection active = connection.getAndSet(null);
            if (active != null) active.disconnect();
        }
        public void check() throws IOException {
            if (cancelled.get()) throw new IOException("request_cancelled");
        }
        private void attach(HttpURLConnection value) throws IOException {
            check(); connection.set(value);
            if (cancelled.get()) { value.disconnect(); throw new IOException("request_cancelled"); }
        }
    }
    public static final class Response {
        public final int status, retryAfter;
        public final byte[] bytes;
        private Response(int status, int retryAfter, byte[] bytes) {
            this.status = status; this.retryAfter = retryAfter; this.bytes = bytes;
        }
        public String text() { return new String(bytes, StandardCharsets.UTF_8); }
    }
    private final URI origin;

    public PinnedHttpClient(String base) { this(base, false); }
    public PinnedHttpClient(String base, boolean allowDebugLoopback) {
        URI value = URI.create(base);
        boolean loopback = "http".equals(value.getScheme()) && allowDebugLoopback &&
            ("127.0.0.1".equals(value.getHost()) || "localhost".equals(value.getHost()) ||
             "10.0.2.2".equals(value.getHost()) || "[::1]".equals(value.getHost()));
        if ((!"https".equals(value.getScheme()) && !loopback) || value.getHost() == null ||
            value.getRawUserInfo() != null || value.getRawQuery() != null || value.getRawFragment() != null ||
            !(value.getRawPath().isEmpty() || value.getRawPath().equals("/")))
            throw new IllegalArgumentException("https_origin_required");
        origin = URI.create(base.replaceAll("/+$", ""));
    }
    public String origin() { return origin.toASCIIString(); }
    public Response request(String method, String path, String authorization, String contentType,
                            byte[] body, Cancellation cancellation) throws IOException {
        if (!(method.equals("GET") || method.equals("POST")) || path == null || !path.startsWith("/") ||
            path.startsWith("//") || path.contains("\\") || path.contains("..") || path.contains("#") ||
            path.contains("?") || path.length() > 2048 || (body != null && body.length > 65536) ||
            (authorization != null && (authorization.contains("\r") || authorization.contains("\n"))))
            throw new IllegalArgumentException("invalid_pinned_request");
        if (CookieHandler.getDefault() != null) throw new IOException("cookie_handler_not_supported");
        cancellation.check();
        HttpURLConnection connection = (HttpURLConnection) origin.resolve(path).toURL().openConnection();
        try {
            cancellation.attach(connection);
            connection.setConnectTimeout(12000); connection.setReadTimeout(12000);
            connection.setInstanceFollowRedirects(false); connection.setUseCaches(false);
            connection.setRequestMethod(method);
            connection.setRequestProperty("Accept", "application/json");
            connection.setRequestProperty("Cache-Control", "no-store");
            if (authorization != null && !authorization.isEmpty()) connection.setRequestProperty("Authorization", authorization);
            if (body != null) {
                connection.setDoOutput(true); connection.setFixedLengthStreamingMode(body.length);
                connection.setRequestProperty("Content-Type", contentType);
                try (java.io.OutputStream output = connection.getOutputStream()) { output.write(body); }
            }
            int status = connection.getResponseCode();
            if (status >= 300 && status < 400) throw new IOException("redirect_rejected");
            int retry = 0;
            try { retry = Math.min(86400, Math.max(0, Integer.parseInt(connection.getHeaderField("Retry-After")))); }
            catch (Exception ignored) { }
            InputStream input = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            byte[] bytes = new byte[0];
            if (input != null) try (InputStream stream = input; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[8192]; int count;
                while ((count = stream.read(buffer)) != -1) {
                    cancellation.check();
                    if (output.size() + count > 1024 * 1024) throw new IOException("response_too_large");
                    output.write(buffer, 0, count);
                }
                bytes = output.toByteArray();
            }
            cancellation.check();
            return new Response(status, retry, bytes);
        } finally {
            cancellation.connection.compareAndSet(connection, null);
            connection.disconnect();
        }
    }
}
