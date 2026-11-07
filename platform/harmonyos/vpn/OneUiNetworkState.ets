import { connection } from '@kit.NetworkKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { util } from '@kit.ArkTS';
import native, { NativeInterfaceInfo } from 'liboneui_native_services.so';

interface PhysicalInterface {
  name: string; index: number; mtu: number; up: boolean; loopback: boolean;
  pointToPoint: boolean; multicast: boolean; addresses: string[];
}
interface PhysicalDnsState { servers: string[]; searchDomains: string[]; }
interface PhysicalNetworkState {
  version: number; defaultInterface: string; underlayId: string; underlayType: string;
  vpnProcessProtected: boolean; interfaces: PhysicalInterface[]; baseDNS?: PhysicalDnsState;
}

function underlayType(bearers: connection.NetBearType[]): string {
  if (bearers.includes(connection.NetBearType.BEARER_WIFI)) { return 'wifi'; }
  if (bearers.includes(connection.NetBearType.BEARER_CELLULAR)) { return 'cellular'; }
  if (bearers.includes(connection.NetBearType.BEARER_ETHERNET)) { return 'ethernet'; }
  return '';
}

/** Public NetworkKit addresses/default network, real native interface IDs and
 * flags. VPN interfaces are excluded; errors do not fabricate interface data. */
export class OneUiNetworkState {
  private observer: connection.NetConnection | undefined = undefined;
  private generation: number = 0;
  private notification: number = -1;
  async snapshot(processProtected: boolean = false): Promise<Uint8Array> {
    const defaultNet: connection.NetHandle | undefined = await connection.hasDefaultNet() ?
      await connection.getDefaultNet() : undefined;
    const networks: connection.NetHandle[] = await connection.getAllNets();
    if (networks.length > 256) { throw new Error('Physical network count'); }
    const result: PhysicalNetworkState = { version: 1, defaultInterface: '', underlayId: '', underlayType: '',
      vpnProcessProtected: processProtected, interfaces: [] };
    const names: Set<string> = new Set();
    for (const net of networks) {
      const capabilities: connection.NetCapabilities = await connection.getNetCapabilities(net);
      if (capabilities.bearerTypes.includes(connection.NetBearType.BEARER_VPN)) { continue; }
      const properties: connection.ConnectionProperties = await connection.getConnectionProperties(net);
      const name: string = properties.interfaceName;
      if (net.netId === defaultNet?.netId) {
        result.defaultInterface = name;
        result.underlayId = net.netId.toString();
        result.underlayType = underlayType(capabilities.bearerTypes);
        // The Go resolver needs the physical network's DNS to forward public
        // names while the VPN handles its private zones.
        const servers: string[] = properties.dnses.map((item: connection.NetAddress): string => item.address)
          .filter((address: string): boolean => address.length > 0);
        if (servers.length > 0) {
          const domains: string[] = properties.domains.split(/[\s,]+/).filter((domain: string): boolean => domain.length > 0);
          result.baseDNS = { servers: Array.from(new Set<string>(servers)).slice(0, 8),
            searchDomains: Array.from(new Set<string>(domains)).slice(0, 16) };
        }
      }
      if (names.has(name)) { continue; }
      if (name.length < 1 || name.length > 128 || properties.linkAddresses.length > 64 || properties.mtu < 1) {
        throw new Error('Physical network properties');
      }
      const info: NativeInterfaceInfo = native.interfaceInfo(name);
      const addresses: string[] = properties.linkAddresses.map((p: connection.LinkAddress): string => {
        const address: string = p.address.address.split('%')[0];
        return address + '/' + p.prefixLength.toString();
      });
      result.interfaces.push({ name: name, index: info.index, mtu: properties.mtu,
        up: info.up, loopback: info.loopback, pointToPoint: info.pointToPoint, multicast: info.multicast, addresses: addresses });
      names.add(name);
    }
    const bytes: Uint8Array = new util.TextEncoder().encodeInto(JSON.stringify(result));
    if (bytes.length > 65536) { throw new Error('Physical network snapshot size'); }
    return bytes;
  }
  async watch(changed: () => void): Promise<void> {
    if (this.observer !== undefined) { throw new Error('Network observation already active'); }
    const generation: number = ++this.generation;
    const observer: connection.NetConnection = connection.createNetConnection(); this.observer = observer;
    const notify = (): void => {
      if (generation !== this.generation || this.notification >= 0) { return; }
      this.notification = setTimeout(() => {
        this.notification = -1;
        if (generation === this.generation) { changed(); }
      }, 100);
    };
    observer.on('netAvailable', notify); observer.on('netLost', notify);
    observer.on('netCapabilitiesChange', notify); observer.on('netConnectionPropertiesChange', notify);
    await new Promise<void>((resolve, reject) => observer.register((error: BusinessError): void => {
      if (error) { reject(error); } else { resolve(); }
    }));
    if (generation !== this.generation) { observer.unregister(() => {}); }
  }
  close(): void {
    ++this.generation;
    if (this.notification >= 0) { clearTimeout(this.notification); this.notification = -1; }
    const previous: connection.NetConnection | undefined = this.observer; this.observer = undefined;
    if (previous !== undefined) { previous.unregister(() => {}); }
  }
}
