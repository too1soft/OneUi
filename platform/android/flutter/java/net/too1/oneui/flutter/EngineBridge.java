package net.too1.oneui.flutter;

import android.content.Context;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.plugin.common.MethodChannel;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.function.Consumer;

/** Optional transport adapter. Flutter supplies an isolate/plugins, never a View. */
public final class EngineBridge implements AutoCloseable {
    public interface Reply { void complete(int code, byte[] value); }
    private final FlutterEngine engine;
    private final MethodChannel channel;
    private final ArrayDeque<Runnable> waiting = new ArrayDeque<>();
    private boolean ready, closed;
    public EngineBridge(Context context, String channelName, Consumer<byte[]> events) {
        engine = new FlutterEngine(context);
        channel = new MethodChannel(engine.getDartExecutor().getBinaryMessenger(), channelName);
        channel.setMethodCallHandler((call, result) -> {
            if (closed) { result.success(null); return; }
            if (call.method.equals("ready")) {
                ready = true;
                while (!waiting.isEmpty()) waiting.removeFirst().run();
            } else if (call.method.equals("event") && call.arguments instanceof String) {
                byte[] value = ((String)call.arguments).getBytes(StandardCharsets.UTF_8);
                if (value.length <= 1024 * 1024) events.accept(value);
            }
            result.success(null);
        });
        engine.getDartExecutor().executeDartEntrypoint(DartExecutor.DartEntrypoint.createDefault());
    }
    public void request(String action, byte[] payload, Reply reply) {
        if (closed || waiting.size() >= 16) { reply.complete(-2, null); return; }
        final String owned = new String(payload, StandardCharsets.UTF_8);
        Runnable run = () -> {
            HashMap<String, Object> args = new HashMap<>(); args.put("action", action); args.put("payload", owned);
            channel.invokeMethod("request", args, new MethodChannel.Result() {
                public void success(Object value) { reply.complete(0, (value instanceof String ? (String)value : "{}").getBytes(StandardCharsets.UTF_8)); }
                public void error(String code, String message, Object details) { reply.complete(-1, null); }
                public void notImplemented() { reply.complete(-4, null); }
            });
        };
        if (ready) run.run(); else waiting.addLast(run);
    }
    @Override public void close() {
        if (closed) return;
        closed = true; waiting.clear(); channel.setMethodCallHandler(null); engine.destroy();
    }
}
