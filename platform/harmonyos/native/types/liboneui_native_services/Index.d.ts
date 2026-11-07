export interface NativePortIdentity { port: string; host: string; }
export interface NativeServiceRequest { id: string; operation: number; payload: Uint8Array; }
export interface NativeSessionSnapshot { state: number; errorCode: number; attempt: string; revision: string; }
export interface NativeSessionCommand { attempt: string; operation: number; configuration: string; }
export interface NativeInterfaceInfo { index: number; up: boolean; loopback: boolean; pointToPoint: boolean; multicast: boolean; }
export interface NativePacketStatus { running: boolean; error: number; }
declare const native: {
  moduleOpen(library: string, symbol: string): string;
  moduleCall(module: string, command: number, request: Uint8Array): Promise<Uint8Array>;
  moduleInterrupt(module: string, command: number, request: Uint8Array): Uint8Array;
  moduleClose(module: string): void;
  portCreate(library: string, label: string, mtu: number, flags: number): NativePortIdentity;
  portPoll(port: string): NativeServiceRequest | undefined;
  portRespond(port: string, request: string, code: number, bytes: Uint8Array): boolean;
  portReplace(port: string, borrowedNonblockingFd: number): void;
  portChanged(port: string): Promise<Uint8Array>;
  portStatus(port: string): NativePacketStatus;
  portClose(port: string): Promise<Uint8Array>;
  sessionCreate(): string;
  sessionConnect(session: string, json: string): number;
  sessionDisconnect(session: string): number;
  sessionReport(session: string, attempt: string, event: number, code: number): number;
  sessionSnapshot(session: string): NativeSessionSnapshot;
  sessionTake(session: string): NativeSessionCommand | undefined;
  sessionDestroy(session: string): number;
  blobRead(privateDirectory: string, keyHash: string): Promise<Uint8Array>;
  blobWrite(privateDirectory: string, keyHash: string, ciphertext: Uint8Array): Promise<Uint8Array>;
  interfaceInfo(name: string): NativeInterfaceInfo;
  channelListen(privateDirectory: string, name: string, tokenHex: string): Promise<Uint8Array>;
  channelConnect(privateDirectory: string, name: string, tokenHex: string): Promise<Uint8Array>;
  channelAccept(listener: string): Promise<Uint8Array>;
  channelSend(channel: string, payload: Uint8Array): Promise<Uint8Array>;
  channelReceive(channel: string): Promise<Uint8Array>;
  channelClose(channel: string): Promise<Uint8Array>;
};
export default native;
