package net.too1.oneui.nativebridge;

import android.os.Handler;
import android.os.Looper;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/** Bounded native worker calls, with cancellation and UI-owner result delivery. */
public final class NativeTaskScope implements AutoCloseable {
    public interface Work { byte[] run() throws Exception; }
    public interface Reply { void complete(int code, byte[] result); }
    private static final ThreadPoolExecutor workers = new ThreadPoolExecutor(2, 2, 30, TimeUnit.SECONDS,
        new ArrayBlockingQueue<>(16), work -> { Thread t = new Thread(work, "OneUI-native-service"); t.setDaemon(true); return t; });
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Set<Task> pending = new HashSet<>();
    private boolean closed;
	private final java.util.List<AutoCloseable> resources = new java.util.ArrayList<>();
	private boolean cleaning;
	public synchronized <T extends AutoCloseable> T use(T resource) {
		if (closed) throw new IllegalStateException("Native task scope closed");
		resources.add(resource); return resource;
	}
    public final class Task {
        private final Runnable cancellation;
        private boolean cancelled;
        private Task(Runnable cancellation) { this.cancellation = cancellation; }
        public void cancel() {
            synchronized (NativeTaskScope.this) { if (cancelled) return; cancelled = true; }
            if (cancellation != null) cancellation.run();
        }
    }
    public synchronized Task submit(Work work, Runnable cancellation, Reply reply) {
        if (closed) throw new IllegalStateException("Native task scope closed");
        Task task = new Task(cancellation); pending.add(task);
        try { workers.execute(() -> {
            byte[] result = new byte[0]; int code = -1;
            try { boolean run; synchronized (this) { run = !task.cancelled; }
                if (run) { result = work.run(); code = 0; }
            } catch (Exception error) { code = -1; }
            finally { synchronized (this) { pending.remove(task); } releaseWhenIdle(); }
            final byte[] value = result; final int status = code;
            main.post(() -> {
                boolean deliver;
                int outcome;
                synchronized (this) { deliver = !closed; outcome = task.cancelled ? -3 : status; }
                try { if (deliver) reply.complete(outcome, value); }
                finally { if (value != null) java.util.Arrays.fill(value, (byte)0); }
            });
        }); } catch (RuntimeException error) { pending.remove(task); throw error; }
        return task;
    }
    @Override public void close() {
        Task[] tasks;
        synchronized (this) { if (closed) return; closed = true; tasks = pending.toArray(new Task[0]); }
        for (Task task : tasks) task.cancel();
		releaseWhenIdle();
    }
	// Resource ownership survives UI detachment and failed cleanup. Only the
	// worker that has joined all work may close a native callback context.
	private void releaseWhenIdle() {
		synchronized (this) {
			if (!closed || !pending.isEmpty() || cleaning || resources.isEmpty()) return;
			cleaning = true;
		}
		try { workers.execute(() -> {
			try {
				while (true) {
					AutoCloseable resource;
					synchronized (this) { if (resources.isEmpty()) break; resource = resources.get(resources.size() - 1); }
					resource.close();
					synchronized (this) { resources.remove(resources.size() - 1); }
				}
			} catch (Exception retry) { main.postDelayed(this::releaseWhenIdle, 1000); }
			finally { synchronized (this) { cleaning = false; } }
		}); } catch (RuntimeException retry) {
			synchronized (this) { cleaning = false; }
			main.postDelayed(this::releaseWhenIdle, 1000);
		}
	}
}
