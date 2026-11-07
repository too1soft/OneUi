import native, { NativeServiceRequest } from 'liboneui_native_services.so';
import { util } from '@kit.ArkTS';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { OneUiNativeWork, OneUiNativeWorkOwner } from '../nativebridge/OneUiNativeWork';

export interface OneUiNativeServices {
  applyNetwork(json: string): Promise<void>;
  protectSocket(fd: number): Promise<void>;
  secureRead(key: string): Promise<Uint8Array | undefined>;
  secureWrite(key: string, bytes: Uint8Array): Promise<void>;
  networkState(): Promise<Uint8Array>;
}

/** Go threads wait in a bounded native queue; only this ArkTS event loop calls
 * platform services. Byte arrays are copied at N-API boundaries and cleared.
 * Keep this port alive until close succeeds; failed cleanup can be retried. */
export class OneUiNativePacketPort {
  readonly host: string;
  private id: string;
  private services: OneUiNativeServices;
  private timer: number = -1;
  private closing: boolean = false;
  private closed: boolean = false;
  private nativeClosed: boolean = false;
  private inFlight: Set<string> = new Set();
  private failure: (code: number) => void;
  private failureReported: boolean = false;
  private work: OneUiNativeWorkOwner = new OneUiNativeWorkOwner();
  private drained: (() => void) | undefined = undefined;
  constructor(library: string, label: string, mtu: number, allowDefaultRoute: boolean,
    services: OneUiNativeServices, failure: (code: number) => void) {
    const identity = native.portCreate(library, label, mtu, allowDefaultRoute ? 1 : 0);
    this.host = identity.host; this.id = identity.port; this.services = services; this.failure = failure;
    this.timer = setInterval(() => this.drain(), 10);
  }
  private text(bytes: Uint8Array): string {
    return new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(bytes);
  }
  private drain(): void {
    if (this.closed || this.nativeClosed) { return; }
    try {
      const status = native.portStatus(this.id);
      if (!this.closing && status.error !== 0 && !this.failureReported) {
        hilog.error(0x1000, 'OneUiVpn', 'Packet pump stopped %{public}d', status.error);
        this.failureReported = true; this.failure(Math.abs(status.error));
      }
      for (let count: number = 0; count < 16; ++count) {
        const request: NativeServiceRequest | undefined = native.portPoll(this.id);
        if (request === undefined) { return; }
        this.inFlight.add(request.id); this.handle(request);
      }
    } catch (error) {
      // A successful native close erases the handle before its Promise callback
      // is delivered. No OS callbacks remain at that point.
      if (!this.closing) {
        hilog.error(0x1000, 'OneUiVpn', 'Native packet service polling failed %{public}d', (error as BusinessError).code ?? 0);
        this.failure(1);
      }
    }
  }
  private async handle(request: NativeServiceRequest): Promise<void> {
    let code: number = 0;
    let reply: Uint8Array = new Uint8Array();
    try {
      if (request.operation === 1) { await this.services.applyNetwork(this.text(request.payload)); }
      else if (request.operation === 2) {
        const text: string = this.text(request.payload);
        if (!/^[0-9]{1,10}$/.test(text) || !Number.isSafeInteger(Number(text))) { throw new Error('Socket descriptor'); }
        await this.services.protectSocket(Number(text));
      } else if (request.operation === 3) {
        const value: Uint8Array | undefined = await this.services.secureRead(this.text(request.payload));
        if (value !== undefined) { reply = value; }
      } else if (request.operation === 4) {
        if (request.payload.length < 4) { throw new Error('Secure storage request'); }
        const length: number = request.payload[0] | request.payload[1] << 8;
        if (length < 1 || length > 128 || request.payload.length <= length + 2) { throw new Error('Secure storage key'); }
        const key: string = this.text(request.payload.subarray(2, 2 + length));
        await this.services.secureWrite(key, request.payload.subarray(2 + length));
      } else if (request.operation === 5) { reply = await this.services.networkState(); }
      else { throw new Error('Native service request'); }
      if (reply.length > 1024 * 1024) { throw new Error('Native service response'); }
    } catch (error) {
      hilog.warn(0x1000, 'OneUiVpn', 'Native OS service %{public}d failed %{public}d',
        request.operation, (error as BusinessError).code ?? 0);
      code = -4; reply.fill(0); reply = new Uint8Array();
    }
    try { if (!this.nativeClosed) { native.portRespond(this.id, request.id, code, reply); } }
    catch (error) {
      if (!this.closing) {
        hilog.error(0x1000, 'OneUiVpn', 'Native packet service reply failed %{public}d', (error as BusinessError).code ?? 0);
        this.failure(1);
      }
    }
    finally {
      request.payload.fill(0); reply.fill(0); this.inFlight.delete(request.id);
      if (this.inFlight.size === 0 && this.drained !== undefined) { const done = this.drained; this.drained = undefined; done(); }
    }
  }
  // The OS must supply a nonblocking packet descriptor. OneUI owns a duplicate.
  replace(fd: number): void { native.portReplace(this.id, fd); if (fd >= 0) { this.failureReported = false; } }
  async networkChanged(): Promise<void> {
    if (!this.closing && !this.closed) { await OneUiNativeWork.submit(this.work, () => native.portChanged(this.id)); }
  }
  async close(): Promise<void> {
    if (this.closed) { return; }
    if (this.closing) { throw new Error('Native cleanup pending'); }
    this.closing = true;
    OneUiNativeWork.cancel(this.work);
    try {
      await OneUiNativeWork.submit(this.work, () => native.portClose(this.id)); this.nativeClosed = true;
      if (this.timer >= 0) { clearInterval(this.timer); this.timer = -1; }
      // A timed-out Go callback may still have an actual OS operation in flight.
      // Join it before acknowledging cleanup or allowing a new identity scope.
      if (this.inFlight.size > 0) { await new Promise<void>((resolve) => { this.drained = resolve; }); }
      this.closed = true;
    } finally { this.closing = false; }
  }
}
