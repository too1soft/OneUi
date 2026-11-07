import { OneUiLocalChannel } from './OneUiLocalChannel';
import { util } from '@kit.ArkTS';

export interface OneUiRpcReply { code: number; payload: Uint8Array; }
export type OneUiRpcHandler = (method: string, payload: Uint8Array) => Promise<OneUiRpcReply>;
interface Pending {
  resolve: (reply: OneUiRpcReply) => void;
  reject: (error: Error) => void;
  timer: number;
}

/** Bounded, versioned binary RPC over OneUI's private native channel. Products
 * own method schemas. No generic VPN or OS implementation belongs in products.
 * Close joins request handlers; do not await close from inside such a handler. */
export class OneUiLocalRpc {
  private channel: OneUiLocalChannel;
  private handler: OneUiRpcHandler;
  private event: (payload: Uint8Array) => void;
  private failure: (error: Error) => void;
  private pending: Map<number, Pending> = new Map();
  private inbound: Set<Promise<void>> = new Set();
  private inboundBytes: number = 0;
  private next: number = 0;
  private lastIncoming: number = 0;
  private closing: boolean = false;
  private cleanup: Promise<void> | undefined = undefined;
  private loop: Promise<void>;
  constructor(channel: OneUiLocalChannel, handler: OneUiRpcHandler,
    event: (payload: Uint8Array) => void, failure: (error: Error) => void) {
    this.channel = channel; this.handler = handler; this.event = event; this.failure = failure;
    this.loop = this.read().catch((error: Error) => {
      const expected: boolean = this.closing; this.closing = true;
      this.rejectAll(error); if (!expected) { this.failure(error); }
    });
  }
  private frame(type: number, id: number, code: number, payload: Uint8Array): Uint8Array {
    if (payload.length > 1024 * 1024 - 10 || !Number.isInteger(code) || code < -2147483648 || code > 2147483647) {
      throw new Error('RPC frame bounds');
    }
    const frame: Uint8Array = new Uint8Array(10 + payload.length);
    frame[0] = 1; frame[1] = type;
    const view: DataView = new DataView(frame.buffer); view.setUint32(2, id, true); view.setInt32(6, code, true);
    frame.set(payload, 10); return frame;
  }
  async call(method: string, payload: Uint8Array, timeoutMs: number = 10000): Promise<OneUiRpcReply> {
    if (this.closing || this.pending.size >= 16 || this.next >= 4294967295 ||
      !/^[A-Za-z0-9_.-]{1,128}$/.test(method) || !Number.isSafeInteger(timeoutMs) || timeoutMs < 1 || timeoutMs > 30000 ||
      payload.length > 1024 * 1024 - 140) { throw new Error('RPC request bounds'); }
    const name: Uint8Array = new util.TextEncoder().encodeInto(method);
    const body: Uint8Array = new Uint8Array(1 + name.length + payload.length);
    body[0] = name.length; body.set(name, 1); body.set(payload, 1 + name.length);
    const id: number = ++this.next;
    const frame: Uint8Array = this.frame(1, id, 0, body); body.fill(0);
    const result: Promise<OneUiRpcReply> = new Promise<OneUiRpcReply>((resolve, reject) => {
      const timer: number = setTimeout(() => {
        if (this.pending.delete(id)) { reject(new Error('RPC request timed out')); }
      }, timeoutMs);
      this.pending.set(id, { resolve: resolve, reject: reject, timer: timer });
    });
    // Keep a rejection observer while send is pending; the caller receives the
    // original result, including an early response or timeout.
    result.catch(() => {});
    this.channel.send(frame).catch((error: Error) => {
      const pending: Pending | undefined = this.pending.get(id);
      if (pending !== undefined) { this.pending.delete(id); clearTimeout(pending.timer); pending.reject(error); }
    }).finally(() => frame.fill(0));
    return result;
  }
  async notify(payload: Uint8Array): Promise<void> {
    if (this.closing) { throw new Error('RPC closed'); }
    const frame: Uint8Array = this.frame(3, 0, 0, payload);
    try { await this.channel.send(frame); } finally { frame.fill(0); }
  }
  private async respond(id: number, reply: OneUiRpcReply): Promise<void> {
    if (this.closing) { return; }
    const frame: Uint8Array = this.frame(2, id, reply.code, reply.payload);
    try { await this.channel.send(frame); } finally { frame.fill(0); }
  }
  private async request(id: number, frame: Uint8Array): Promise<void> {
    let reply: OneUiRpcReply = { code: -4, payload: new Uint8Array() };
    try {
      const size: number = frame[10];
      if (frame.length < 12 || size < 1 || size > 128 || frame.length < 11 + size) { throw new Error('RPC method'); }
      const method: string = new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(frame.subarray(11, 11 + size));
      if (!/^[A-Za-z0-9_.-]{1,128}$/.test(method)) { throw new Error('RPC method'); }
      reply = await this.handler(method, frame.subarray(11 + size));
      if (reply.payload.length > 1024 * 1024 - 10 || !Number.isInteger(reply.code) || reply.code > 0) { throw new Error('RPC handler reply'); }
    } catch (_) { reply.payload.fill(0); reply = { code: -4, payload: new Uint8Array() }; }
    try { await this.respond(id, reply); }
    catch (error) { if (!this.closing) { this.failure(error as Error); } }
    finally { this.inboundBytes -= frame.length; frame.fill(0); reply.payload.fill(0); }
  }
  private async read(): Promise<void> {
    while (!this.closing) {
      const frame: Uint8Array | undefined = await this.channel.receive();
      if (frame === undefined) { continue; }
      if (frame.length < 10 || frame[0] !== 1) { frame.fill(0); throw new Error('RPC version or frame'); }
      const view: DataView = new DataView(frame.buffer, frame.byteOffset, frame.byteLength);
      const type: number = frame[1], id: number = view.getUint32(2, true), code: number = view.getInt32(6, true);
      if (type === 1 && id !== 0 && code === 0) {
        if (id <= this.lastIncoming) { frame.fill(0); throw new Error('RPC request generation'); }
        this.lastIncoming = id;
        if (this.inbound.size >= 16 || this.inboundBytes + frame.length > 4 * 1024 * 1024) {
          try { await this.respond(id, { code: -2, payload: new Uint8Array() }); } finally { frame.fill(0); }
          continue;
        }
        this.inboundBytes += frame.length;
        const operation: Promise<void> = this.request(id, frame); this.inbound.add(operation);
        operation.finally(() => this.inbound.delete(operation));
      } else {
        try {
          if (type === 2 && id !== 0 && code <= 0) {
            const pending: Pending | undefined = this.pending.get(id);
            if (pending !== undefined) {
              this.pending.delete(id); clearTimeout(pending.timer);
              pending.resolve({ code: code, payload: new Uint8Array(frame.subarray(10)) });
            }
          } else if (type === 3 && id === 0 && code === 0) { this.event(frame.subarray(10)); }
          else { throw new Error('RPC envelope'); }
        } finally { frame.fill(0); }
      }
    }
  }
  private rejectAll(error: Error): void {
    this.pending.forEach((pending: Pending) => { clearTimeout(pending.timer); pending.reject(error); }); this.pending.clear();
  }
  async close(): Promise<void> {
    if (this.cleanup !== undefined) { return this.cleanup; }
    this.closing = true; this.rejectAll(new Error('RPC closed'));
    const pending: Promise<void> = this.release(); this.cleanup = pending;
    try { await pending; } catch (error) { this.cleanup = undefined; throw error as Error; }
  }
  private async release(): Promise<void> {
    await this.channel.close(); await this.loop; await Promise.allSettled(Array.from(this.inbound));
  }
}
