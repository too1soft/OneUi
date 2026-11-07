import native from 'liboneui_native_services.so';
import { util } from '@kit.ArkTS';

/** Same-UID native IPC. Endpoint names and 32-byte random tokens are supplied
 * by the owner. The token never goes in logs, file names or business payloads. */
export class OneUiLocalChannel {
  private id: string;
  private stopping: boolean = false;
  private closed: boolean = false;
  private cleanup: Promise<void> | undefined = undefined;
  private sending: Promise<void> = Promise.resolve();
  private receiving: Promise<Uint8Array | undefined> | undefined = undefined;
  private accepting: Promise<OneUiLocalChannel | undefined> | undefined = undefined;
  private queued: number = 0;
  private bytes: number = 0;
  private constructor(id: string) { this.id = id; }
  private static identity(bytes: Uint8Array): string {
    const id: string = new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(bytes);
    if (!/^[1-9][0-9]{0,19}$/.test(id)) { throw new Error('Channel identity'); }
    return id;
  }
  static async listen(directory: string, name: string, tokenHex: string): Promise<OneUiLocalChannel> {
    return new OneUiLocalChannel(OneUiLocalChannel.identity(await native.channelListen(directory, name, tokenHex)));
  }
  static async connect(directory: string, name: string, tokenHex: string): Promise<OneUiLocalChannel> {
    return new OneUiLocalChannel(OneUiLocalChannel.identity(await native.channelConnect(directory, name, tokenHex)));
  }
  async accept(): Promise<OneUiLocalChannel | undefined> {
    if (this.closed || this.stopping || this.accepting !== undefined) { throw new Error('Channel accept state'); }
    const operation: Promise<OneUiLocalChannel | undefined> = native.channelAccept(this.id).then(async (bytes: Uint8Array) => {
      if (bytes.length === 0) { return undefined; }
      const channel: OneUiLocalChannel = new OneUiLocalChannel(OneUiLocalChannel.identity(bytes));
      if (this.stopping || this.closed) { await channel.close(); return undefined; }
      return channel;
    });
    this.accepting = operation;
    try { return await operation; } finally { this.accepting = undefined; }
  }
  async receive(): Promise<Uint8Array | undefined> {
    if (this.closed || this.stopping || this.receiving !== undefined) { throw new Error('Channel receive state'); }
    const operation: Promise<Uint8Array | undefined> = native.channelReceive(this.id).then((bytes: Uint8Array) => {
      if (this.stopping || this.closed) { bytes.fill(0); return undefined; }
      return bytes.length > 0 ? bytes : undefined;
    });
    this.receiving = operation;
    try { return await operation; } finally { this.receiving = undefined; }
  }
  async send(payload: Uint8Array): Promise<void> {
    if (this.closed || this.stopping || payload.length === 0 || payload.length > 1024 * 1024 ||
      this.queued >= 16 || this.bytes + payload.length > 4 * 1024 * 1024) { throw new Error('Channel send capacity'); }
    const owned: Uint8Array = new Uint8Array(payload); ++this.queued; this.bytes += owned.length;
    const operation: Promise<void> = this.sending.catch(() => {}).then(async () => {
      if (this.stopping || this.closed) { throw new Error('Channel closed'); }
      await native.channelSend(this.id, owned);
    });
    this.sending = operation;
    try { await operation; }
    finally { --this.queued; this.bytes -= owned.length; owned.fill(0); }
  }
  async close(): Promise<void> {
    if (this.closed) { return; }
    if (this.cleanup !== undefined) { return this.cleanup; }
    this.stopping = true;
    const pending: Promise<void> = this.release(); this.cleanup = pending;
    try { await pending; } finally { this.cleanup = undefined; this.stopping = false; }
  }
  private async release(): Promise<void> {
    await native.channelClose(this.id);
    // Native close interrupts/joins I/O. Join its JS continuations too, including
    // an accepted child which arrived after the listener began closing.
    await Promise.allSettled([this.sending, this.receiving, this.accepting]);
    this.closed = true;
  }
}
