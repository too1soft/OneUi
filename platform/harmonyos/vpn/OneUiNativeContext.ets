import { OneUiNativePacketPort, OneUiNativeServices } from './OneUiNativePacketPort';
import { OneUiSecureBlobStore } from '../storage/OneUiSecureBlobStore';
import { OneUiNetworkState } from './OneUiNetworkState';
import { parseVpnConfiguration } from './VpnConfiguration';

/** Non-tunnel context for business identity/enrollment work in the main app.
 * An engine must run in the VPN extension; the UI cannot claim OS protection. */
export class OneUiNativeContext {
  readonly host: string;
  private port: OneUiNativePacketPort;
  constructor(filesDir: string, library: string, namespace: string, label: string, mtu: number) {
    const storage: OneUiSecureBlobStore = new OneUiSecureBlobStore(filesDir, namespace);
    const network: OneUiNetworkState = new OneUiNetworkState();
    const services: OneUiNativeServices = {
      applyNetwork: async (json: string): Promise<void> => {
        if (parseVpnConfiguration(json, true).addresses.length > 0) { throw new Error('Main application cannot own a VPN interface'); }
      },
      protectSocket: async (_fd: number): Promise<void> => { throw new Error('Socket protection requires the VPN extension'); },
      secureRead: (key: string): Promise<Uint8Array | undefined> => storage.read(key),
      secureWrite: (key: string, data: Uint8Array): Promise<void> => storage.write(key, data),
      networkState: (): Promise<Uint8Array> => network.snapshot()
    };
    this.port = new OneUiNativePacketPort(library, label, mtu, false, services, (_code: number): void => {});
    this.host = this.port.host;
  }
  close(): Promise<void> { return this.port.close(); }
}
