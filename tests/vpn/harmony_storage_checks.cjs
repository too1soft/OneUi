const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const native = require(path.resolve(process.argv[2]));
const {AuthenticatedBlobStore} = require(path.resolve(process.argv[3]));
const root = path.join(os.homedir(), '.cache/oneui-mobile-native-fixture');
const marker = path.join(root, '.oneui-native-test');
if (fs.existsSync(root) && (!fs.existsSync(marker) || fs.readFileSync(marker,'utf8') !== 'OneUI native storage tests')) throw Error('Unknown fixture directory');
fs.mkdirSync(root,{recursive:true,mode:0o700});fs.writeFileSync(marker,'OneUI native storage tests',{mode:0o600});
const directory = path.join(root, 'blobs');
const hex = text => crypto.createHash('sha256').update(text).digest('hex');
const encode = text => new Uint8Array(Buffer.from(text));
const keys = new Map();
const platform = {
  encode, digest:async bytes=>new Uint8Array(crypto.createHash('sha256').update(bytes).digest()),
  random:n=>new Uint8Array(crypto.randomBytes(n)),
  key:async (alias,create)=>{if(!keys.has(alias)&&create)keys.set(alias,crypto.randomBytes(32));return keys.has(alias)?new Uint8Array(keys.get(alias)):undefined;},
  encrypt:async(key,iv,aad,plain)=>{const c=crypto.createCipheriv('aes-256-gcm',key,iv);c.setAAD(aad);return new Uint8Array(Buffer.concat([c.update(plain),c.final(),c.getAuthTag()]));},
  decrypt:async(key,iv,aad,encrypted)=>{const d=crypto.createDecipheriv('aes-256-gcm',key,iv);d.setAAD(aad);d.setAuthTag(encrypted.slice(-16));return new Uint8Array(Buffer.concat([d.update(encrypted.slice(0,-16)),d.final()]));},
  read:hash=>native.blobRead(directory,hash),write:async(hash,bytes)=>{await native.blobWrite(directory,hash,bytes);}
};
async function main() {
  const store = new AuthenticatedBlobStore('oneui-fixture',platform), key='device-record';
  const hash=hex('oneui-fixture\0'+key), file=path.join(directory,hash+'.blob');
  assert.equal(await store.read(key),undefined);
  const plain=encode('explicit fixture credential, never a production secret');
  await store.write(key,plain);assert.deepEqual(await store.read(key),plain);
  const first=fs.readFileSync(file);assert.equal(first.includes(Buffer.from(plain)),false);
  await store.write(key,plain);const second=fs.readFileSync(file);assert.notDeepEqual(first,second);
  second[second.length-1]^=1;fs.writeFileSync(file,second);await assert.rejects(store.read(key));
  fs.writeFileSync(file,first);
  const movedHash=hex('oneui-fixture\0other');fs.copyFileSync(file,path.join(directory,movedHash+'.blob'));
  await assert.rejects(store.read('other'));
  const savedKeys=new Map(keys);keys.clear();await assert.rejects(store.read(key));await assert.rejects(store.write(key,plain));
  assert.equal(keys.size,0);for(const [alias,value] of savedKeys)keys.set(alias,value);
  await assert.rejects(store.write(key,new Uint8Array()));await assert.rejects(store.write(key,new Uint8Array(1024*1024+1)));
  assert.throws(()=>new AuthenticatedBlobStore('../namespace',platform));
  await assert.rejects(store.read('../key'));
  const big=new Uint8Array(1024*1024).fill(73);await store.write('maximum',big);assert.deepEqual(await store.read('maximum'),big);
  const writes=Array.from({length:8},(_,i)=>new Uint8Array(8192).fill(i));
  await Promise.all(writes.map(value=>native.blobWrite(directory,hex('atomic'),value)));
  const atomic=await native.blobRead(directory,hex('atomic'));
  assert.equal(atomic.length,8192);assert(atomic.every(value=>value===atomic[0]));
  const symlink=hex('symlink');fs.symlinkSync(file,path.join(directory,symlink+'.blob'));await assert.rejects(native.blobRead(directory,symlink));
  await assert.rejects(native.blobRead(directory,'../escape'));await assert.rejects(native.blobWrite(directory,hex('empty'),new Uint8Array()));
  assert.equal(fs.readdirSync(directory).some(name=>name.endsWith('.pending')),false);
  console.log('Authenticated blobs: actual AES-GCM, tamper and key/name binding, nonce change, missing-key fail-closed, 1 MiB limit, private atomic files/concurrent writers/symlink rejection passed; OS key provider mocked');
}
main().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>{
  if(fs.realpathSync(root)!==root||fs.statSync(root).uid!==process.getuid()||fs.readFileSync(marker,'utf8')!=='OneUI native storage tests')throw Error('Fixture cleanup guard');
  fs.rmSync(root,{recursive:true,force:true});
});
