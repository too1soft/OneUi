/** Portable authenticated-file policy. Platform keys never enter the file. */
export interface BlobPlatform {
  encode(text: string): Uint8Array;
  digest(bytes: Uint8Array): Promise<Uint8Array>;
  random(length: number): Uint8Array;
  key(alias: string, create: boolean): Promise<Uint8Array | undefined>;
  encrypt(key: Uint8Array, iv: Uint8Array, aad: Uint8Array, plain: Uint8Array): Promise<Uint8Array>;
  decrypt(key: Uint8Array, iv: Uint8Array, aad: Uint8Array, encrypted: Uint8Array): Promise<Uint8Array>;
  read(hash: string): Promise<Uint8Array>;
  write(hash: string, ciphertext: Uint8Array): Promise<void>;
}

export class AuthenticatedBlobStore {
  private platform: BlobPlatform;
  private namespace: string;
  constructor(namespace: string, platform: BlobPlatform) {
    if (!/^[a-zA-Z0-9._-]{1,128}$/.test(namespace)) { throw new Error('Secure storage namespace'); }
    this.namespace = namespace; this.platform = platform;
  }
  private validate(key: string): void {
    if (!/^[a-zA-Z0-9._-]{1,128}$/.test(key)) { throw new Error('Secure storage key'); }
  }
  private async hash(text: string): Promise<string> {
    const bytes: Uint8Array = await this.platform.digest(this.platform.encode(text));
    if (bytes.length !== 32) { throw new Error('Secure storage digest'); }
    let result: string = '';
    bytes.forEach((value: number) => { result += value.toString(16).padStart(2, '0'); });
    return result;
  }
  private async asset(create: boolean): Promise<Uint8Array> {
    const alias: string = 'oneui_blob_' + await this.hash(this.namespace);
    const key: Uint8Array | undefined = await this.platform.key(alias, create);
    if (key === undefined || key.length !== 32) {
      if (key !== undefined) { key.fill(0); }
      throw new Error('Secure storage key unavailable');
    }
    return key;
  }
  private aad(key: string): Uint8Array {
    return this.platform.encode('OUIS\x01' + this.namespace + '\x00' + key);
  }
  async read(key: string): Promise<Uint8Array | undefined> {
    this.validate(key);
    const encrypted: Uint8Array = await this.platform.read(await this.hash(this.namespace + '\x00' + key));
    if (encrypted.length === 0) { return undefined; }
    if (encrypted.length < 34 || encrypted.length > 1024 * 1024 + 33 ||
        encrypted[0] !== 79 || encrypted[1] !== 85 || encrypted[2] !== 73 || encrypted[3] !== 83 || encrypted[4] !== 1) {
      encrypted.fill(0); throw new Error('Secure storage record invalid');
    }
    const raw: Uint8Array = await this.asset(false);
    try {
      const plain: Uint8Array = await this.platform.decrypt(raw, encrypted.slice(5, 17), this.aad(key), encrypted.slice(17));
      if (plain.length < 1 || plain.length > 1024 * 1024) { plain.fill(0); throw new Error('Secure storage record size'); }
      return plain;
    } finally { raw.fill(0); encrypted.fill(0); }
  }
  async write(key: string, bytes: Uint8Array): Promise<void> {
    this.validate(key);
    if (bytes.length < 1 || bytes.length > 1024 * 1024) { throw new Error('Secure storage record size'); }
    const plain: Uint8Array = new Uint8Array(bytes);
    try {
      const hash: string = await this.hash(this.namespace + '\x00' + key);
      // Missing OS keys must not silently replace an encrypted identity.
      const existing: Uint8Array = await this.platform.read(hash);
      let raw: Uint8Array;
      try { raw = await this.asset(existing.length === 0); }
      finally { existing.fill(0); }
      try {
        const iv: Uint8Array = this.platform.random(12);
        if (iv.length !== 12) { throw new Error('Secure storage nonce'); }
        const cipher: Uint8Array = await this.platform.encrypt(raw, iv, this.aad(key), plain);
        if (cipher.length !== plain.length + 16) { cipher.fill(0); throw new Error('Secure storage authentication tag'); }
        const record: Uint8Array = new Uint8Array(17 + cipher.length);
        record.set(new Uint8Array([79, 85, 73, 83, 1])); record.set(iv, 5); record.set(cipher, 17);
        try { await this.platform.write(hash, record); }
        finally { cipher.fill(0); record.fill(0); iv.fill(0); }
      } finally { raw.fill(0); }
    } finally { plain.fill(0); }
  }
}
