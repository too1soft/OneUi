import native, { OneUiApplicationRequest, OneUiSystemBars } from 'liboneui_application.so';
import { util } from '@kit.ArkTS';

export interface OneUiApplicationReply { code: number; payload: Uint8Array; }
export interface OneUiApplicationServices {
  request(action: string, payload: Uint8Array): Promise<OneUiApplicationReply>;
}

/** Opaque business calls only; the OneUI host owns queue bounds and lifetimes. */
export class OneUiApplicationController {
  private static owners: Map<string, OneUiApplicationController> = new Map();
  readonly host: string;
  private services: OneUiApplicationServices;
  private pending: Set<string> = new Set();
  private closed: boolean = false;
  private openUrl: ((url: string) => Promise<void>) | undefined = undefined;
  private barStyle: ((bars: OneUiSystemBars) => Promise<void>) | undefined = undefined;
  setUrlHandler(handler: (url: string) => Promise<void>): void { this.openUrl = handler; }
  setSystemBarHandler(handler?: (bars: OneUiSystemBars) => Promise<void>): void { this.barStyle = handler; }
  async updateSystemBars(bars: OneUiSystemBars): Promise<void> {
    const apply = this.barStyle;
    if (this.closed || apply === undefined) { return; }
    await apply(bars);
    if (!this.closed && apply === this.barStyle) { native.acknowledgeSystemBars(this.host, bars.revision); }
  }
  constructor(library: string, services: OneUiApplicationServices) {
    this.services = services; this.host = native.create(library);
    OneUiApplicationController.owners.set(this.host, this);
  }
  // LocalStorage contains only this opaque string, preserving the identity of
  // native owners/services across the UI's observed state wrappers.
  static find(host: string): OneUiApplicationController | undefined { return OneUiApplicationController.owners.get(host); }
  drain(): void {
    if (this.closed) { return; }
    for (let count: number = 0; count < 16 && this.pending.size < 16; ++count) {
      const request: OneUiApplicationRequest | undefined = native.take(this.host);
      if (request === undefined) { return; }
      this.pending.add(request.id); this.handle(request);
    }
  }
  private async handle(request: OneUiApplicationRequest): Promise<void> {
    let result: OneUiApplicationReply = { code: -4, payload: new Uint8Array() };
    try {
      if (request.action === 'oneui.open-url') {
        const url: string = new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(request.payload);
        if (request.payload.length > 4096 || !/^https:\/\/[A-Za-z0-9.-]+(?::[0-9]{1,5})?(?:[/?#][^\u0000-\u0020]*)?$/.test(url) || this.openUrl === undefined) { throw new Error('HTTPS link required'); }
        await this.openUrl(url); result = { code: 0, payload: new Uint8Array() };
      } else { result = await this.services.request(request.action, request.payload); }
      if (result.payload.length > 1024 * 1024 || !Number.isInteger(result.code)) { throw new Error('Application response'); }
      if (!this.closed) { native.reply(this.host, request.id, result.code, result.payload); }
    } catch (_) {
      if (!this.closed) { native.reply(this.host, request.id, -4, new Uint8Array()); }
    } finally { request.payload.fill(0); result.payload.fill(0); this.pending.delete(request.id); }
  }
  event(payload: Uint8Array): void {
    if (!this.closed) { native.reply(this.host, '0', 0, payload); }
  }
  back(): boolean { return !this.closed && native.back(this.host); }
  isClosed(): boolean { return this.closed; }
  close(): void {
    if (!this.closed) { native.destroy(this.host); this.closed = true; this.openUrl = undefined; this.barStyle = undefined; OneUiApplicationController.owners.delete(this.host); }
  }
}
