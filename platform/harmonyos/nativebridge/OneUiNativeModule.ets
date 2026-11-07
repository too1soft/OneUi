import native from 'liboneui_native_services.so';
import { OneUiNativeWork, OneUiNativeWorkOwner } from './OneUiNativeWork';

/** Business bytes only. OS services, scheduling and N-API stay in OneUI. */
export class OneUiNativeModule {
  private id: string;
  private closed: boolean = false;
  private owner: OneUiNativeWorkOwner = new OneUiNativeWorkOwner();
  constructor(library: string, symbol: string) { this.id = native.moduleOpen(library, symbol); }
  async call(command: number, request: Uint8Array): Promise<Uint8Array> {
    if (this.closed || !Number.isInteger(command) || request.length > 1024 * 1024) { throw new Error('Native module call'); }
    const owned: Uint8Array = new Uint8Array(request);
    try {
      return await OneUiNativeWork.submit(this.owner, () => {
        if (this.closed) { return Promise.reject(new Error('Native module closed')); }
        return native.moduleCall(this.id, command, owned);
      });
    } finally { owned.fill(0); }
  }
  // Only for a documented nonblocking native cancellation command. It must not
  // perform OS work or wait for this ArkTS event loop; normal calls use workers.
  interrupt(command: number, request: Uint8Array): Uint8Array {
    if (this.closed || request.length > 4096) { throw new Error('Native interrupt'); }
    return native.moduleInterrupt(this.id, command, request);
  }
  close(): void {
    if (!this.closed) { this.closed = true; OneUiNativeWork.cancel(this.owner); native.moduleClose(this.id); }
  }
}
