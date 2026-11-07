import { VpnExtensionAbility, vpnExtension } from '@kit.NetworkKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { Want } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { OneUiVpnConfiguration, parseVpnConfiguration, systemVpnConfiguration } from './VpnConfiguration';
import { OneUiPacketTunnel, OneUiTunnelDriver, OneUiTunnelUpdate } from './OneUiPacketTunnel';
import { OneUiVpnSession } from './OneUiVpnSession';
import { OneUiVpnChannelServer, OneUiVpnRemoteState } from './OneUiVpnChannelServer';

export interface OneUiVpnEngine {
  start(fd: number, config: OneUiVpnConfiguration, system: OneUiVpnSystem,
    failure: (code: number) => void): Promise<void>;
  stop(): Promise<void>;
}
export interface OneUiVpnSystem {
  readonly processProtected: boolean;
  protect(socketFd: number): Promise<void>;
  applyNetwork(json: string, beforeReplace: () => void): Promise<OneUiTunnelUpdate>;
}

/** Stage VPN lifetime. The generic engine, platform bridge and event channel
 * belong to OneUI; products provide only their protocol implementation. */
export default class OneUiVpnExtensionAbility extends VpnExtensionAbility {
  private tunnel: OneUiPacketTunnel | undefined = undefined;
  private engine: OneUiVpnEngine | undefined = undefined;
  private generation: number = 0;
  private activeAttempt: number = 0;
  private cleanup: Promise<void> = Promise.resolve();
  private opening: Promise<void> | undefined = undefined;
  private activeWant: Want = {};
  private session: OneUiVpnSession | undefined = undefined;
  private nativeAttempt: number = 0;
  private channel: OneUiVpnChannelServer | undefined = undefined;
  private lastState: OneUiVpnRemoteState = { attempt: 0, state: 0, errorCode: 0, revision: '0' };
  protected createEngine(): OneUiVpnEngine { throw new Error('Packet engine is required'); }
  protected storageNamespace(): string { return 'oneui.vpn'; }
  protected message(_payload: Uint8Array): Promise<Uint8Array> { return Promise.reject(new Error('Application message unavailable')); }
  private state(): OneUiVpnRemoteState {
    if (this.session !== undefined) {
      const snapshot = this.session.snapshot();
      this.lastState = { attempt: this.activeAttempt, state: snapshot.state, errorCode: snapshot.errorCode, revision: snapshot.revision };
    }
    return this.lastState;
  }
  private report(attempt: number, event: number, code: number): void {
    if (attempt !== this.activeAttempt || this.session === undefined) { return; }
    if (this.session.report(this.nativeAttempt, event, code) !== 0) { return; }
    hilog.info(0x1000, 'OneUiVpn', 'Extension event %{public}d code %{public}d', event, code);
    this.state(); this.channel?.publish().catch(() => {});
  }
  private requestStop(): OneUiVpnRemoteState {
    if (this.session !== undefined) {
      if (this.session.disconnect() !== 0) { throw new Error('VPN stop state'); }
      this.session.take(); // The UI calls the actual OS stop after this acknowledgement.
    }
    return this.state();
  }
  onCreate(want: Want): void {
    hilog.info(0x1000, 'OneUiVpn', 'VPN extension created');
    const generation: number = ++this.generation;
    const attempt: number = want.parameters?.['oneuiVpnAttempt'] as number;
    const json: string = want.parameters?.['oneuiVpnConfiguration'] as string;
    this.activeAttempt = attempt; this.activeWant = want;
    const opening: Promise<void> = this.start(generation, attempt, json); this.opening = opening;
    opening.catch((error: BusinessError) => {
      hilog.error(0x1000, 'OneUiVpn', 'VPN extension start failed %{public}d', error.code ?? 0);
      if (generation === this.generation) { this.fail(generation, attempt, error.code ?? 1); }
    }).finally(() => { if (this.opening === opening) { this.opening = undefined; } });
  }
  private async start(generation: number, attempt: number, json: string): Promise<void> {
    if (!Number.isSafeInteger(attempt) || attempt <= 0 || typeof json !== 'string') { throw new Error('VPN start parameters'); }
    const config: OneUiVpnConfiguration = parseVpnConfiguration(json);
    systemVpnConfiguration(config); // Reject unsupported settings before touching OS resources.
    await this.cleanup;
    if (generation !== this.generation) { return; }
    this.session = new OneUiVpnSession();
    if (this.session.connect(json) !== 0) { throw new Error('VPN session configuration'); }
    const start = this.session.take();
    if (start === undefined || start.operation !== 1) { throw new Error('VPN session start'); }
    this.nativeAttempt = Number(start.attempt);
    const token: string = this.activeWant.parameters?.['oneuiVpnChannelToken'] as string;
    if (token !== undefined && token !== '') {
      this.channel = await OneUiVpnChannelServer.open(this.context.filesDir, this.storageNamespace(),
        { token: token, attempt: attempt, configuration: json }, () => this.state(), () => this.requestStop(),
        (payload: Uint8Array): Promise<Uint8Array> => this.message(payload));
    }
    if (generation !== this.generation) { return; }
    const conn: vpnExtension.VpnConnection = vpnExtension.createVpnConnection(this.context);
    // Protect the extension's control, DNS and UDP sockets before adding routes.
    await conn.protectProcessNet();
    if (generation !== this.generation) { return; }
    const driver: OneUiTunnelDriver = {
      create: (settings: string): Promise<number> => conn.create(systemVpnConfiguration(parseVpnConfiguration(settings, true))),
      destroy: (): Promise<void> => conn.destroy()
    };
    const tunnel: OneUiPacketTunnel = new OneUiPacketTunnel(driver); this.tunnel = tunnel;
    this.report(attempt, 1, 0);
    const opened: OneUiTunnelUpdate = await tunnel.change(json, () => {}, () => generation === this.generation);
    if (generation !== this.generation) { return; }
    if (!opened.accepted) { throw new Error('VPN interface creation failed'); }
    this.report(attempt, 3, 0);
    const engine: OneUiVpnEngine = this.createEngine(); this.engine = engine;
    const system: OneUiVpnSystem = {
      processProtected: true,
      protect: (socketFd: number): Promise<void> => conn.protect(socketFd),
      applyNetwork: (settings: string, beforeReplace: () => void): Promise<OneUiTunnelUpdate> => {
        const update: OneUiVpnConfiguration = parseVpnConfiguration(settings, true);
        if (update.addresses.length > 0) { systemVpnConfiguration(update); }
        return tunnel.change(update.addresses.length === 0 ? undefined : settings, beforeReplace, () => generation === this.generation);
      }
    };
    await engine.start(opened.fd, config, system, (code: number): void => this.fail(generation, attempt, code));
    if (generation === this.generation) { this.report(attempt, 4, 0); }
  }
  private fail(generation: number, attempt: number, code: number): void {
    if (generation !== this.generation) { return; }
    ++this.generation; this.report(attempt, 5, code);
    this.cleanup = this.close(attempt, false).then(() => {
      // The actual onDestroy callback acknowledges STOPPED.
      vpnExtension.stopVpnExtensionAbility(this.activeWant).catch((error: BusinessError) => this.report(attempt, 5, error.code ?? 1));
    }).catch((error: BusinessError) => this.report(attempt, 5, error.code ?? 1));
  }
  private async close(attempt: number, acknowledge: boolean = true): Promise<void> {
    const engine: OneUiVpnEngine | undefined = this.engine;
    if (engine !== undefined) { await engine.stop(); this.engine = undefined; }
    const tunnel: OneUiPacketTunnel | undefined = this.tunnel;
    if (tunnel !== undefined) { await tunnel.close(); this.tunnel = undefined; }
    if (acknowledge) { this.report(attempt, 6, 0); }
    const channel: OneUiVpnChannelServer | undefined = this.channel;
    if (channel !== undefined) {
      await channel.publish(); await channel.close(); this.channel = undefined;
    }
    const session: OneUiVpnSession | undefined = this.session;
    if (session !== undefined && acknowledge) { session.close(); this.session = undefined; }
  }
  onDestroy(): void {
    ++this.generation;
    const attempt: number = this.activeAttempt;
    const opening: Promise<void> | undefined = this.opening;
    // Interrupt a protocol start before joining its Promise. Late listener,
    // storage or TUN creation remains owned until start actually unwinds.
    this.engine?.stop().catch(() => {});
    this.cleanup = this.cleanup.catch(() => {}).then(async () => {
      if (opening !== undefined) { try { await opening; } catch (_) {} }
      await this.close(attempt);
    }).catch((error: BusinessError) => {
      this.report(attempt, 5, error.code ?? 1);
    });
  }
}
