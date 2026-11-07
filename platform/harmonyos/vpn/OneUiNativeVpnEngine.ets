import { OneUiVpnEngine, OneUiVpnSystem } from './OneUiVpnExtensionAbility';
import { OneUiVpnConfiguration } from './VpnConfiguration';
import { OneUiNativePacketPort, OneUiNativeServices } from './OneUiNativePacketPort';
import { OneUiSecureBlobStore } from '../storage/OneUiSecureBlobStore';
import { OneUiNetworkState } from './OneUiNetworkState';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

export interface OneUiNativeVpnProtocol {
  start(host: string): Promise<void>;
  cancel(): void;
  close(): Promise<void>;
}

/** OS resources and native packet transport stay in OneUI. A product supplies
 * only protocol commands and its own library/storage namespace. */
export class OneUiNativeVpnEngine implements OneUiVpnEngine {
  private library: string;
  private protocol: OneUiNativeVpnProtocol;
  private storage: OneUiSecureBlobStore;
  private network: OneUiNetworkState = new OneUiNetworkState();
  private port: OneUiNativePacketPort | undefined = undefined;
  private opening: Promise<void> | undefined = undefined;
  private closing: Promise<void> | undefined = undefined;
  private networkRefresh: Promise<void> | undefined = undefined;
  private networkChanged: boolean = false;
  private stopRequested: boolean = false;
  private protocolClosed: boolean = false;
  private started: boolean = false;
  private stopped: boolean = false;
  constructor(filesDir: string, library: string, namespace: string, protocol: OneUiNativeVpnProtocol) {
    this.library = library; this.protocol = protocol; this.storage = new OneUiSecureBlobStore(filesDir, namespace);
  }
  async start(fd: number, config: OneUiVpnConfiguration, system: OneUiVpnSystem, failure: (code: number) => void): Promise<void> {
    if (this.started || this.stopRequested || this.stopped) { throw new Error('VPN engine start state'); }
    this.started = true;
    this.opening = this.begin(fd, config, system, failure);
    try { await this.opening; } finally { this.opening = undefined; }
  }
  private async begin(fd: number, config: OneUiVpnConfiguration, system: OneUiVpnSystem, failure: (code: number) => void): Promise<void> {
    if (!system.processProtected) { throw new Error('VPN process protection unavailable'); }
    const services: OneUiNativeServices = {
      applyNetwork: async (json: string): Promise<void> => {
        const current: OneUiNativePacketPort | undefined = this.port;
        if (current === undefined) { throw new Error('VPN packet port unavailable'); }
        let replacing: boolean = false;
        const result = await system.applyNetwork(json, () => { current.replace(-1); replacing = true; });
        if (replacing && result.fd >= 0) {
          try { current.replace(result.fd); } catch (error) { failure(1); throw error as Error; }
        }
        if (!result.accepted) { if (result.fatal) { failure(1); } throw new Error('VPN system update rejected'); }
      },
      protectSocket: (socketFd: number): Promise<void> => system.protect(socketFd),
      secureRead: (key: string): Promise<Uint8Array | undefined> => this.storage.read(key),
      secureWrite: (key: string, bytes: Uint8Array): Promise<void> => this.storage.write(key, bytes),
      networkState: (): Promise<Uint8Array> => this.network.snapshot(system.processProtected)
    };
    const port: OneUiNativePacketPort = new OneUiNativePacketPort(this.library, config.label, config.mtu, config.allowDefaultRoute, services, failure);
    this.port = port; port.replace(fd);
    await this.protocol.start(port.host);
    if (this.stopRequested) { throw new Error('VPN engine start cancelled'); }
    await this.network.watch(() => { this.refreshNetwork(port); });
    if (this.stopRequested) { throw new Error('VPN engine start cancelled'); }
  }
  private refreshNetwork(port: OneUiNativePacketPort): void {
    if (this.stopRequested) { return; }
    this.networkChanged = true;
    if (this.networkRefresh !== undefined) { return; }
    this.networkRefresh = this.refreshPendingNetwork(port);
  }
  private async refreshPendingNetwork(port: OneUiNativePacketPort): Promise<void> {
    try {
      while (this.networkChanged && !this.stopRequested) {
        this.networkChanged = false;
        if (!await this.tryNetworkRefresh(port) && !this.stopRequested) {
          await new Promise<void>((resolve) => { setTimeout(resolve, 250); });
          if (!this.stopRequested) { await this.tryNetworkRefresh(port); }
        }
      }
    } finally { this.networkRefresh = undefined; }
  }
  private async tryNetworkRefresh(port: OneUiNativePacketPort): Promise<boolean> {
    try { await port.networkChanged(); return true; }
    catch (error) {
      // NetworkKit can notify before the replacement underlay is queryable.
      // A refresh error leaves the current tunnel usable until the next event.
      hilog.warn(0x1000, 'OneUiVpn', 'Underlay refresh failed %{public}d', (error as BusinessError).code ?? 0);
      return false;
    }
  }
  async stop(): Promise<void> {
    if (this.stopped) { return; }
    if (this.closing !== undefined) { return this.closing; }
    this.stopRequested = true; this.protocol.cancel(); this.network.close();
    const closing: Promise<void> = this.closeResources(); this.closing = closing;
    try { await closing; } finally { if (this.closing === closing) { this.closing = undefined; } }
  }
  private async closeResources(): Promise<void> {
    if (this.opening !== undefined) { try { await this.opening; } catch (_) {} }
    await this.networkRefresh;
    if (!this.protocolClosed) { await this.protocol.close(); this.protocolClosed = true; }
    const port: OneUiNativePacketPort | undefined = this.port;
    if (port !== undefined) { await port.close(); this.port = undefined; }
    this.stopped = true;
  }
}
