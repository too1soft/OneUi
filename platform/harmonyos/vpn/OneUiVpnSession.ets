import native, { NativeSessionCommand, NativeSessionSnapshot } from 'liboneui_native_services.so';

export class OneUiVpnSession {
  private id: string = native.sessionCreate();
  private closed: boolean = false;
  private check(): void { if (this.closed) { throw new Error('VPN session closed'); } }
  connect(configuration: string): number { this.check(); return native.sessionConnect(this.id, configuration); }
  disconnect(): number { this.check(); return native.sessionDisconnect(this.id); }
  report(attempt: number, event: number, code: number): number {
    this.check();
    if (!Number.isSafeInteger(attempt) || attempt <= 0) { throw new Error('VPN attempt'); }
    return native.sessionReport(this.id, attempt.toString(), event, code);
  }
  snapshot(): NativeSessionSnapshot { this.check(); return native.sessionSnapshot(this.id); }
  take(): NativeSessionCommand | undefined { this.check(); return native.sessionTake(this.id); }
  close(): void {
    if (this.closed) { return; }
    if (native.sessionDestroy(this.id) !== 0) { throw new Error('VPN cleanup not acknowledged'); }
    this.closed = true;
  }
}
