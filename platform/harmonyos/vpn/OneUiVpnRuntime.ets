import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { util } from '@kit.ArkTS';
import { OneUiLocalChannel } from '../nativebridge/OneUiLocalChannel';
import { OneUiLocalRpc, OneUiRpcReply } from '../nativebridge/OneUiLocalRpc';
import { OneUiSecureBlobStore } from '../storage/OneUiSecureBlobStore';
import { OneUiVpnController } from './OneUiVpnController';
import { OneUiVpnRemoteState, OneUiVpnChannelRecord, ONEUI_VPN_CHANNEL_RECORD } from './OneUiVpnChannelServer';
import { parseVpnConfiguration, systemVpnConfiguration } from './VpnConfiguration';

/** App-scoped native VPN client. The extension's C++ session is authoritative.
 * Local authorization/stop requests never acknowledge encrypted connectivity
 * or cleanup. Reopening a UI reads a live authenticated service snapshot. */
export class OneUiVpnRuntime {
  private filesDir: string;
  private store: OneUiSecureBlobStore;
  private controller: OneUiVpnController;
  private rpc: OneUiLocalRpc | undefined = undefined;
  private opening: Promise<void> | undefined = undefined;
  private closed: boolean = false;
  private released: boolean = false;
  private cleanup: Promise<void> | undefined = undefined;
  private generation: number = 0;
  private attempt: number = 0;
  private remoteRevision: string = '0';
  private changed: (state: OneUiVpnRemoteState) => void;
  private current: OneUiVpnRemoteState = { attempt: 0, state: 0, errorCode: 0, revision: '0' };
  constructor(filesDir: string, namespace: string, bundleName: string, abilityName: string,
    changed: (state: OneUiVpnRemoteState) => void) {
    this.filesDir = filesDir; this.store = new OneUiSecureBlobStore(filesDir, namespace); this.changed = changed;
    this.controller = new OneUiVpnController(bundleName, abilityName, { report: (attempt: number, event: number, code: number): void => {
      if (this.closed || attempt !== this.attempt) { return; }
      if (event === 1) { this.update({ attempt: attempt, state: 2, errorCode: 0, revision: '0' }); }
      else if (event === 2 || event === 5) { ++this.generation; this.update({ attempt: attempt, state: 6, errorCode: code, revision: '0' }); }
      else if (event === 6 && this.current.state !== 6) { this.update({ attempt: attempt, state: 0, errorCode: 0, revision: '0' }); }
    } });
  }
  snapshot(): OneUiVpnRemoteState { return { attempt: this.current.attempt, state: this.current.state, errorCode: this.current.errorCode, revision: this.current.revision }; }
  private update(state: OneUiVpnRemoteState): void { this.current = state; this.changed(this.snapshot()); }
  private decode(bytes: Uint8Array): OneUiVpnRemoteState {
    const state: OneUiVpnRemoteState = JSON.parse(new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(bytes)) as OneUiVpnRemoteState;
    if (!Number.isSafeInteger(state.attempt) || state.attempt <= 0 || !Number.isInteger(state.state) || state.state < 0 || state.state > 6 ||
      !Number.isInteger(state.errorCode) || typeof state.revision !== 'string' || !/^[0-9]{1,20}$/.test(state.revision)) { throw new Error('VPN service snapshot'); }
    return state;
  }
  private async attach(record: OneUiVpnChannelRecord, generation: number): Promise<boolean> {
    const channel: OneUiLocalChannel = await OneUiLocalChannel.connect(this.filesDir + '/oneui-vpn-ipc', 'vpn', record.token);
    if (this.closed || generation !== this.generation) { await channel.close(); return false; }
    const rpc: OneUiLocalRpc = new OneUiLocalRpc(channel, async (): Promise<OneUiRpcReply> => ({ code: -4, payload: new Uint8Array() }),
      (bytes: Uint8Array): void => { if (!this.closed && generation === this.generation) { this.accept(this.decode(bytes), record.attempt); } },
      (_error: Error): void => {
        if (!this.closed && generation === this.generation && !(this.current.revision !== '0' && (this.current.state === 0 || this.current.state === 6))) {
          this.update({ attempt: record.attempt, state: 6, errorCode: 1, revision: '0' });
        }
        rpc.close().then(() => { if (this.rpc === rpc) { this.rpc = undefined; } }).catch(() => {});
      });
    this.rpc = rpc;
    this.remoteRevision = '0';
    try {
      const reply: OneUiRpcReply = await rpc.call('oneui.vpn.state', new Uint8Array());
      try {
        if (reply.code !== 0) { throw new Error('VPN service response'); }
        if (this.closed || generation !== this.generation) { await rpc.close(); return false; }
        this.accept(this.decode(reply.payload), record.attempt); return true;
      } finally { reply.payload.fill(0); }
    } catch (error) { await rpc.close(); if (this.rpc === rpc) { this.rpc = undefined; } throw error as Error; }
  }
  private accept(state: OneUiVpnRemoteState, expected: number): void {
    if (state.attempt !== expected) { throw new Error('VPN service attempt'); }
    if (state.revision.length < this.remoteRevision.length || state.revision.length === this.remoteRevision.length && state.revision < this.remoteRevision) { return; }
    this.remoteRevision = state.revision;
    this.attempt = expected; this.controller.resume(expected); this.controller.authorized(expected); this.update(state);
  }
  async restore(): Promise<boolean> {
    if (this.closed || this.opening !== undefined || this.rpc !== undefined) { throw new Error('VPN client state'); }
    const operation: Promise<boolean> = this.restoreState();
    const barrier: Promise<void> = operation.then(() => {}); barrier.catch(() => {});
    this.opening = barrier;
    try { return await operation; } finally { if (this.opening === barrier) { this.opening = undefined; } }
  }
  private async restoreState(): Promise<boolean> {
    const data: Uint8Array | undefined = await this.store.read(ONEUI_VPN_CHANNEL_RECORD);
    if (data === undefined) { return false; }
    if (this.closed) { data.fill(0); return false; }
    let record: OneUiVpnChannelRecord;
    try { record = JSON.parse(new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(data)) as OneUiVpnChannelRecord; }
    finally { data.fill(0); }
    if (typeof record.token !== 'string' || !/^[0-9a-f]{64}$/.test(record.token) || !Number.isSafeInteger(record.attempt) || record.attempt <= 0) { return false; }
    try { return await this.attach(record, this.generation); } catch (_) { return false; }
  }
  async connect(configuration: string): Promise<void> {
    if (this.closed || this.opening !== undefined || this.rpc !== undefined || this.current.state >= 1 && this.current.state <= 5) { throw new Error('VPN client already active'); }
    systemVpnConfiguration(parseVpnConfiguration(configuration));
    const random: Uint8Array = cryptoFramework.createRandom().generateRandomSync(32).data;
    let token: string = ''; random.forEach((value: number) => { token += value.toString(16).padStart(2, '0'); }); random.fill(0);
    const attempt: number = ++this.attempt; if (!Number.isSafeInteger(attempt)) { throw new Error('VPN attempt capacity'); }
    const generation: number = ++this.generation;
    this.update({ attempt: attempt, state: 1, errorCode: 0, revision: '0' });
    const record: OneUiVpnChannelRecord = { token: token, attempt: attempt, configuration: configuration };
    const operation: Promise<void> = this.start(record, generation); this.opening = operation;
    try { await operation; } finally { if (this.opening === operation) { this.opening = undefined; } }
  }
  private async start(record: OneUiVpnChannelRecord, generation: number): Promise<void> {
    try {
      await this.controller.start(record.attempt, record.configuration, record.token);
      const deadline: number = Date.now() + 60000;
      while (!this.closed && generation === this.generation && Date.now() < deadline) {
        try { if (await this.attach(record, generation)) { return; } } catch (_) {}
        await new Promise<void>((resolve) => { setTimeout(resolve, 100); });
      }
      if (!this.closed && generation === this.generation) { throw new Error('VPN service start timed out'); }
    } catch (error) {
      if (!this.closed && generation === this.generation) {
        await this.controller.stop(record.attempt);
        this.update({ attempt: record.attempt, state: 6, errorCode: 1, revision: '0' });
      }
      throw error as Error;
    }
  }
  async message(payload: Uint8Array): Promise<Uint8Array> {
    const rpc: OneUiLocalRpc | undefined = this.rpc;
    if (rpc === undefined || this.closed) { throw new Error('VPN service unavailable'); }
    const reply: OneUiRpcReply = await rpc.call('application', payload, 30000);
    if (reply.code !== 0) { reply.payload.fill(0); throw new Error('VPN application response'); }
    return reply.payload;
  }
  async disconnect(): Promise<void> {
    if (this.closed) { return; }
    const rpc: OneUiLocalRpc | undefined = this.rpc;
    if (rpc === undefined) { ++this.generation; }
    this.update({ attempt: this.attempt, state: 5, errorCode: 0, revision: '0' });
    try {
      if (rpc !== undefined) {
        const reply: OneUiRpcReply = await rpc.call('oneui.vpn.stop', new Uint8Array());
        try {
          if (reply.code !== 0) { throw new Error('VPN stop response'); }
          this.accept(this.decode(reply.payload), this.attempt);
        } finally { reply.payload.fill(0); }
      }
    } finally { await this.controller.stop(this.attempt); }
  }
  async close(): Promise<void> {
    if (this.released) { return; }
    if (this.cleanup !== undefined) { return this.cleanup; }
    if (!this.closed) { this.closed = true; ++this.generation; this.controller.dispose(); }
    const pending: Promise<void> = this.release(); this.cleanup = pending;
    try { await pending; this.released = true; } finally { this.cleanup = undefined; }
  }
  private async release(): Promise<void> {
    if (this.rpc !== undefined) { await this.rpc.close(); this.rpc = undefined; }
    if (this.opening !== undefined) { try { await this.opening; } catch (_) {} }
  }
}
