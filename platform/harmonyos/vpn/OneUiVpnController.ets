import { vpnExtension } from '@kit.NetworkKit';
import { Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

export interface OneUiVpnEvents { report(attempt: number, event: number, code: number): void; }

/** API 26+ UI-side authorization observer. A start request resolving does not
 * mean that the user consented or that the packet engine connected. Extension
 * events arrive through the product's process-safe native bridge separately. */
export class OneUiVpnController {
  private observer: vpnExtension.VpnObserver = vpnExtension.createVpnObserver();
  private want: Want;
  private attempt: number = 0;
  private waitingAuthorization: boolean = false;
  private cancelled: boolean = false;
  private events: OneUiVpnEvents;
  private disposed: boolean = false;
  private stopping: Promise<void> | undefined = undefined;

  constructor(bundleName: string, abilityName: string, events: OneUiVpnEvents) {
    this.want = { bundleName: bundleName, abilityName: abilityName };
    this.events = events;
    this.observer.onAuthorizationResult((allowed: boolean) => {
      if (!this.waitingAuthorization) { return; }
      hilog.info(0x1000, 'OneUiVpn', 'Authorization callback %{public}d', allowed ? 1 : 0);
      this.waitingAuthorization = false;
      if (!allowed) {
        if (!this.cancelled) { this.events.report(this.attempt, 2, 1); }
        this.events.report(this.attempt, 6, 0);
      } else if (this.cancelled) {
        // Late consent after cancel must not leave an extension running.
        this.stopExtension().catch(() => {
          this.events.report(this.attempt, 5, 1);
        });
      } else {
        this.events.report(this.attempt, 1, 0);
      }
      if (this.disposed) { this.observer.offAuthorizationResult(); }
    });
  }
  async start(attempt: number, configurationJson: string, channelToken: string = ''): Promise<void> {
    if (this.disposed || this.waitingAuthorization || !Number.isSafeInteger(attempt) || attempt <= 0) {
      throw new Error('VPN authorization pending or invalid attempt');
    }
    this.attempt = attempt;
    this.cancelled = false;
    this.waitingAuthorization = true;
    this.want.parameters = { oneuiVpnAttempt: attempt, oneuiVpnConfiguration: configurationJson };
    if (channelToken !== '') {
      if (!/^[0-9a-f]{64}$/.test(channelToken)) { this.waitingAuthorization = false; throw new Error('VPN channel token'); }
      this.want.parameters['oneuiVpnChannelToken'] = channelToken;
    }
    try {
      hilog.info(0x1000, 'OneUiVpn', 'Requesting VPN extension start');
      await vpnExtension.startVpnExtensionAbility(this.want);
      hilog.info(0x1000, 'OneUiVpn', 'System accepted VPN start request');
    }
    catch (error) { this.waitingAuthorization = false; throw error as Error; }
  }
  // Call when the extension's AUTHORIZED event is received. An already
  // authorized VPN need not show another permission dialog or observer event.
  authorized(attempt: number): void {
    if (attempt !== this.attempt) { return; }
    this.waitingAuthorization = false;
    if (this.cancelled) {
      this.stopExtension().catch(() => {
        this.events.report(attempt, 5, 1);
      });
    }
  }
  // Called only after a same-UID, authenticated extension snapshot is read.
  // No authorization request is issued when reconnecting an existing service.
  resume(attempt: number): void {
    if (this.disposed || !Number.isSafeInteger(attempt) || attempt <= 0) { throw new Error('VPN service attempt'); }
    this.attempt = attempt; this.waitingAuthorization = false; this.cancelled = false;
  }
  async stop(attempt: number): Promise<void> {
    if (attempt !== this.attempt) { return; }
    this.cancelled = true;
    if (!this.waitingAuthorization) { await this.stopExtension(); }
  }
  private async stopExtension(): Promise<void> {
    if (this.stopping !== undefined) { return this.stopping; }
    const pending: Promise<void> = vpnExtension.stopVpnExtensionAbility(this.want); this.stopping = pending;
    try { await pending; } finally { if (this.stopping === pending) { this.stopping = undefined; } }
  }
  dispose(): void {
    this.disposed = true;
    // Only a pending consent is cancelled by UI destruction. The extension
    // keeps ownership of an established or already-authorized connection.
    if (this.waitingAuthorization) { this.cancelled = true; }
    // Observe a pending dialog through its result, so late approval is stopped.
    if (!this.waitingAuthorization) { this.observer.offAuthorizationResult(); }
  }
}
