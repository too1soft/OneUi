export class OneUiNativeWorkOwner {}
interface NativeWork {
  owner: OneUiNativeWorkOwner;
  action: () => Promise<Uint8Array>;
  resolve: (value: Uint8Array) => void;
  reject: (error: Error) => void;
}
/** Two Go workers plus a bounded queue, leaving native OS work runnable while
 * a protocol call waits for its storage/network callbacks. No polling buildup. */
export class OneUiNativeWork {
  private static running: number = 0;
  private static queue: NativeWork[] = [];
  static submit(owner: OneUiNativeWorkOwner, action: () => Promise<Uint8Array>): Promise<Uint8Array> {
    if (OneUiNativeWork.queue.length >= 16) { return Promise.reject(new Error('Native work capacity')); }
    return new Promise<Uint8Array>((resolve, reject) => {
      OneUiNativeWork.queue.push({ owner: owner, action: action, resolve: resolve, reject: reject });
      OneUiNativeWork.drain();
    });
  }
  static cancel(owner: OneUiNativeWorkOwner): void {
    const retained: NativeWork[] = [];
    OneUiNativeWork.queue.forEach((task: NativeWork) => {
      if (task.owner === owner) { task.reject(new Error('Native work cancelled')); }
      else { retained.push(task); }
    });
    OneUiNativeWork.queue = retained;
  }
  private static drain(): void {
    while (OneUiNativeWork.running < 2 && OneUiNativeWork.queue.length > 0) {
      const task: NativeWork = OneUiNativeWork.queue.shift() as NativeWork;
      ++OneUiNativeWork.running;
      Promise.resolve().then(() => task.action()).then(task.resolve, task.reject).finally(() => {
        --OneUiNativeWork.running; OneUiNativeWork.drain();
      });
    }
  }
}
