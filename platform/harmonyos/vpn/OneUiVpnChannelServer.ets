import { OneUiLocalChannel } from '../nativebridge/OneUiLocalChannel';
import { OneUiLocalRpc, OneUiRpcReply } from '../nativebridge/OneUiLocalRpc';
import { OneUiSecureBlobStore } from '../storage/OneUiSecureBlobStore';
import { util } from '@kit.ArkTS';

export interface OneUiVpnRemoteState { attempt: number; state: number; errorCode: number; revision: string; }
export interface OneUiVpnChannelRecord { token: string; attempt: number; configuration: string; }
export const ONEUI_VPN_CHANNEL_RECORD: string = 'oneui.vpn-channel';

/** The extension owns its listener independently of any UI process. A UI
 * reconnects using encrypted local metadata; losing a UI never stops the VPN. */
export class OneUiVpnChannelServer {
  private listener: OneUiLocalChannel;
  private store: OneUiSecureBlobStore;
  private loop: Promise<void> = Promise.resolve();
  private rpc: OneUiLocalRpc | undefined = undefined;
  private disconnect: (() => void) | undefined = undefined;
  private closing: boolean = false;
  private cleanup: Promise<void> | undefined = undefined;
  private state: () => OneUiVpnRemoteState;
  private stop: () => OneUiVpnRemoteState;
  private message: (payload: Uint8Array) => Promise<Uint8Array>;
  private constructor(listener: OneUiLocalChannel, store: OneUiSecureBlobStore,
    state: () => OneUiVpnRemoteState, stop: () => OneUiVpnRemoteState, message: (payload: Uint8Array) => Promise<Uint8Array>) {
    this.listener = listener; this.store = store; this.state = state; this.stop = stop; this.message = message;
  }
  static async open(filesDir: string, namespace: string, record: OneUiVpnChannelRecord,
    state: () => OneUiVpnRemoteState, stop: () => OneUiVpnRemoteState,
    message: (payload: Uint8Array) => Promise<Uint8Array>): Promise<OneUiVpnChannelServer> {
    if (!/^[0-9a-f]{64}$/.test(record.token) || !Number.isSafeInteger(record.attempt) || record.attempt <= 0) { throw new Error('VPN channel record'); }
    const store: OneUiSecureBlobStore = new OneUiSecureBlobStore(filesDir, namespace);
    const listener: OneUiLocalChannel = await OneUiLocalChannel.listen(filesDir + '/oneui-vpn-ipc', 'vpn', record.token);
    try {
      const encoded: Uint8Array = new util.TextEncoder().encodeInto(JSON.stringify(record));
      try { await store.write(ONEUI_VPN_CHANNEL_RECORD, encoded); } finally { encoded.fill(0); }
    } catch (error) { await listener.close(); throw error as Error; }
    const server: OneUiVpnChannelServer = new OneUiVpnChannelServer(listener, store, state, stop, message);
    server.loop = server.serve(); server.loop.catch(() => {}); return server;
  }
  private encoded(state: OneUiVpnRemoteState): Uint8Array { return new util.TextEncoder().encodeInto(JSON.stringify(state)); }
  private async serve(): Promise<void> {
    while (!this.closing) {
      let channel: OneUiLocalChannel | undefined = undefined;
      try { channel = await this.listener.accept(); }
      catch (_) {
        if (this.closing) { return; }
        await new Promise<void>((resolve) => { setTimeout(resolve, 50); }); continue;
      } // Reject a bad local handshake, retain the listener.
      if (channel === undefined) { continue; }
      const disconnected: Promise<void> = new Promise<void>((resolve) => { this.disconnect = resolve; });
      const rpc: OneUiLocalRpc = new OneUiLocalRpc(channel, async (method: string, payload: Uint8Array): Promise<OneUiRpcReply> => {
        if (method === 'oneui.vpn.state' && payload.length === 0) { return { code: 0, payload: this.encoded(this.state()) }; }
        if (method === 'oneui.vpn.stop' && payload.length === 0) { return { code: 0, payload: this.encoded(this.stop()) }; }
        if (method === 'application') { return { code: 0, payload: await this.message(payload) }; }
        return { code: -4, payload: new Uint8Array() };
      }, (_payload: Uint8Array): void => {}, (_error: Error): void => { this.disconnect?.(); });
      this.rpc = rpc;
      await disconnected;
      await rpc.close(); if (this.rpc === rpc) { this.rpc = undefined; }
      this.disconnect = undefined;
    }
  }
  async publish(): Promise<void> {
    const rpc: OneUiLocalRpc | undefined = this.rpc;
    if (rpc !== undefined && !this.closing) {
      const bytes: Uint8Array = this.encoded(this.state());
      try { await rpc.notify(bytes); } catch (_) { this.disconnect?.(); } finally { bytes.fill(0); }
    }
  }
  async close(): Promise<void> {
    if (this.cleanup !== undefined) { return this.cleanup; }
    this.closing = true;
    const pending: Promise<void> = this.release(); this.cleanup = pending;
    try { await pending; } catch (error) { this.cleanup = undefined; throw error as Error; }
  }
  private async release(): Promise<void> {
    this.disconnect?.(); await this.listener.close();
    if (this.rpc !== undefined) { await this.rpc.close(); }
    try { await this.loop; } catch (_) {} // A retained RPC owner above has now completed its cleanup retry.
    const empty: Uint8Array = new util.TextEncoder().encodeInto('{}');
    try { await this.store.write(ONEUI_VPN_CHANNEL_RECORD, empty); } finally { empty.fill(0); }
  }
}
