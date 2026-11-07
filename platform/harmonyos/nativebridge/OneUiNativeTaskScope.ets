export interface OneUiAsyncResource { close(): Promise<void>; }

/** A task retains reverse-order resource ownership through cleanup failures.
 * Cancellation callbacks must signal promptly; they never perform OS teardown. */
export class OneUiNativeTask {
  private resources: OneUiAsyncResource[] = [];
  private cancellation: (() => void)[] = [];
  private cancelled: boolean = false;
  private finished: boolean = false;
  private work: Promise<Uint8Array> = Promise.resolve(new Uint8Array());
  private release: (() => void) | undefined = undefined;
  private cleaning: Promise<void> | undefined = undefined;
  use<T extends OneUiAsyncResource>(resource: T): T { this.resources.push(resource); return resource; }
  onCancel(callback: () => void): void { this.cancellation.push(callback); if (this.cancelled) { callback(); } }
  cancel(): void {
    if (this.cancelled) { return; } this.cancelled = true;
    this.cancellation.forEach((callback: () => void) => { try { callback(); } catch (_) {} });
  }
  begin(work: (task: OneUiNativeTask) => Promise<Uint8Array>, release: () => void): Promise<Uint8Array> {
    this.release = release;
    this.work = Promise.resolve().then(() => {
      if (this.cancelled) { throw new Error('Native task cancelled'); }
      return work(this);
    }).then((bytes: Uint8Array) => {
      if (this.cancelled) { bytes.fill(0); throw new Error('Native task cancelled'); }
      return bytes;
    }).finally(() => { this.finished = true; });
    return this.result();
  }
  private async result(): Promise<Uint8Array> {
    let bytes: Uint8Array;
    try { bytes = await this.work; }
    catch (error) { await this.cleanup(); throw error as Error; }
    try { await this.cleanup(); return bytes; }
    catch (error) { bytes.fill(0); throw error as Error; }
  }
  async cleanup(): Promise<void> {
    if (!this.finished) { return; }
    if (this.cleaning !== undefined) { return this.cleaning; }
    const pending: Promise<void> = this.closeResources(); this.cleaning = pending;
    try { await pending; } finally { this.cleaning = undefined; }
  }
  private async closeResources(): Promise<void> {
    while (this.resources.length > 0) {
      const resource: OneUiAsyncResource = this.resources[this.resources.length - 1];
      await resource.close(); this.resources.pop();
    }
    this.cancellation = []; this.release?.(); this.release = undefined;
  }
  async close(): Promise<void> {
    this.cancel(); try { await this.work; } catch (_) {} await this.cleanup();
  }
}

export class OneUiNativeTaskScope {
  private resources: OneUiAsyncResource[] = [];
  private tasks: Set<OneUiNativeTask> = new Set();
  private closed: boolean = false;
  private capacity: number;
  constructor(capacity: number = 16) {
    if (!Number.isInteger(capacity) || capacity < 1 || capacity > 16) { throw new Error('Task capacity'); }
    this.capacity = capacity;
  }
  own<T extends OneUiAsyncResource>(resource: T): T {
    if (this.closed) { throw new Error('Native task scope closed'); }
    this.resources.push(resource); return resource;
  }
  submit(work: (task: OneUiNativeTask) => Promise<Uint8Array>): Promise<Uint8Array> {
    if (this.closed || this.tasks.size >= this.capacity) { return Promise.reject(new Error('Native task capacity')); }
    const task: OneUiNativeTask = new OneUiNativeTask(); this.tasks.add(task);
    return task.begin(work, () => { this.tasks.delete(task); });
  }
  cancel(): void { this.tasks.forEach((task: OneUiNativeTask) => task.cancel()); }
  async retryCleanup(): Promise<void> { for (const task of this.tasks) { await task.cleanup(); } }
  async close(): Promise<void> {
    this.closed = true; this.cancel();
    // Wait all tasks even when one cleanup fails, then expose the failure for a
    // later retry while retaining each unreleased owner.
    let failed: Error | undefined = undefined;
    for (const task of Array.from(this.tasks)) { try { await task.close(); } catch (error) { failed = error as Error; } }
    if (failed !== undefined) { throw failed; }
    while (this.resources.length > 0) {
      await this.resources[this.resources.length - 1].close(); this.resources.pop();
    }
  }
}
