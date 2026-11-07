import { asset } from '@kit.AssetStoreKit';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { util } from '@kit.ArkTS';
import { BusinessError } from '@kit.BasicServicesKit';
import native from 'liboneui_native_services.so';
import { AuthenticatedBlobStore, BlobPlatform } from './AuthenticatedBlobStore';

/** Device-local AES key in AssetStore, AES-256-GCM ciphertext in private files.
 * Usable after first unlock; never synchronized or retained after uninstall.
 * Files are copied, locked and atomically replaced by OneUI's native adapter. */
class HarmonyBlobPlatform implements BlobPlatform {
  private directory: string;
  constructor(filesDir: string) { this.directory = filesDir + '/oneui-secure-blobs'; }
  encode(text: string): Uint8Array { return new util.TextEncoder().encodeInto(text); }
  async digest(bytes: Uint8Array): Promise<Uint8Array> {
    const md: cryptoFramework.Md = cryptoFramework.createMd('SHA256');
    await md.update({ data: bytes }); return (await md.digest()).data;
  }
  random(length: number): Uint8Array { return cryptoFramework.createRandom().generateRandomSync(length).data; }
  private async query(alias: Uint8Array): Promise<Uint8Array | undefined> {
    const query: asset.AssetMap = new Map();
    query.set(asset.Tag.ALIAS, alias); query.set(asset.Tag.RETURN_TYPE, asset.ReturnType.ALL);
    let values: asset.AssetMap[];
    try { values = await asset.query(query); }
    catch (error) { if ((error as BusinessError).code === 24000002) { return undefined; } throw error as Error; }
    if (values.length !== 1) { throw new Error('Secure storage key result'); }
    const secret: Uint8Array = values[0].get(asset.Tag.SECRET) as Uint8Array;
    if (!(secret instanceof Uint8Array) || secret.length !== 32) { throw new Error('Secure storage key format'); }
    const result: Uint8Array = new Uint8Array(secret); secret.fill(0); return result;
  }
  async key(name: string, create: boolean): Promise<Uint8Array | undefined> {
    const alias: Uint8Array = this.encode(name);
    const existing: Uint8Array | undefined = await this.query(alias);
    if (existing !== undefined || !create) { return existing; }
    const generated: Uint8Array = this.random(32);
    const attributes: asset.AssetMap = new Map();
    attributes.set(asset.Tag.ALIAS, alias); attributes.set(asset.Tag.SECRET, generated);
    attributes.set(asset.Tag.ACCESSIBILITY, asset.Accessibility.DEVICE_FIRST_UNLOCKED);
    attributes.set(asset.Tag.AUTH_TYPE, asset.AuthType.NONE); attributes.set(asset.Tag.SYNC_TYPE, asset.SyncType.NEVER);
    try {
      try { await asset.add(attributes); }
      catch (error) { if ((error as BusinessError).code !== 24000003) { throw error as Error; } }
      return await this.query(alias);
    } finally { generated.fill(0); }
  }
  private params(iv: Uint8Array, aad: Uint8Array, tag: Uint8Array): cryptoFramework.GcmParamsSpec {
    return { algName: 'GcmParamsSpec', iv: { data: iv }, aad: { data: aad }, authTag: { data: tag } };
  }
  async encrypt(raw: Uint8Array, iv: Uint8Array, aad: Uint8Array, plain: Uint8Array): Promise<Uint8Array> {
    const key: cryptoFramework.SymKey = await cryptoFramework.createSymKeyGenerator('AES256').convertKey({ data: raw });
    try {
      const cipher: cryptoFramework.Cipher = cryptoFramework.createCipher('AES256|GCM|NoPadding');
      await cipher.init(cryptoFramework.CryptoMode.ENCRYPT_MODE, key, this.params(iv, aad, new Uint8Array(16)));
      // The official GCM contract appends the 16-byte tag to doFinal output.
      return (await cipher.doFinal({ data: plain })).data;
    } finally { key.clearMem(); }
  }
  async decrypt(raw: Uint8Array, iv: Uint8Array, aad: Uint8Array, encrypted: Uint8Array): Promise<Uint8Array> {
    const key: cryptoFramework.SymKey = await cryptoFramework.createSymKeyGenerator('AES256').convertKey({ data: raw });
    try {
      const cipher: cryptoFramework.Cipher = cryptoFramework.createCipher('AES256|GCM|NoPadding');
      await cipher.init(cryptoFramework.CryptoMode.DECRYPT_MODE, key, this.params(iv, aad, encrypted.slice(-16)));
      return (await cipher.doFinal({ data: encrypted.slice(0, -16) })).data;
    } finally { key.clearMem(); }
  }
  read(hash: string): Promise<Uint8Array> { return native.blobRead(this.directory, hash); }
  async write(hash: string, ciphertext: Uint8Array): Promise<void> { await native.blobWrite(this.directory, hash, ciphertext); }
}

export class OneUiSecureBlobStore {
  private store: AuthenticatedBlobStore;
  constructor(filesDir: string, namespace: string) { this.store = new AuthenticatedBlobStore(namespace, new HarmonyBlobPlatform(filesDir)); }
  read(key: string): Promise<Uint8Array | undefined> { return this.store.read(key); }
  write(key: string, bytes: Uint8Array): Promise<void> { return this.store.write(key, bytes); }
}
