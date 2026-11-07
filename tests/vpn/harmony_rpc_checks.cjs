'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),crypto=require('node:crypto');
const {TextDecoder}=require('node:util');
const ts=require(path.resolve(process.argv[3],'node_modules/typescript')),native=require(path.resolve(process.argv[2]));
class OsDecoder extends TextDecoder{decodeWithStream(bytes){return this.decode(bytes);}}
class OsEncoder{encodeInto(text){return new Uint8Array(Buffer.from(text));}}
const util={TextDecoder:OsDecoder,TextEncoder:OsEncoder};
function load(file,dependencies){
  const result=ts.transpileModule(fs.readFileSync(path.resolve(__dirname,'../../platform/harmonyos/nativebridge',file),'utf8'),{
    fileName:file,reportDiagnostics:true,compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}
  });assert.equal(result.diagnostics.length,0);const exports={};
  new Function('require','exports',result.outputText)(name=>{if(!(name in dependencies))throw Error('Unexpected dependency '+name);return dependencies[name];},exports);return exports;
}
const channelModule=load('OneUiLocalChannel.ets',{'liboneui_native_services.so':{__esModule:true,default:native},'@kit.ArkTS':{util}});
const {OneUiLocalRpc}=load('OneUiLocalRpc.ets',{'./OneUiLocalChannel':channelModule,'@kit.ArkTS':{util}});
const {OneUiLocalChannel}=channelModule;
const root=path.join(os.homedir(),'.cache/oneui-local-rpc-fixture'),marker=path.join(root,'.oneui-test');
if(fs.existsSync(root)&&(!fs.existsSync(marker)||fs.readFileSync(marker,'utf8')!=='OneUI local RPC tests'))throw Error('Unknown RPC fixture');
fs.mkdirSync(root,{recursive:true,mode:0o700});fs.writeFileSync(marker,'OneUI local RPC tests',{mode:0o600});
const directory=path.join(root,'ipc'),token=crypto.randomBytes(32).toString('hex');
const delay=ms=>new Promise(done=>setTimeout(done,ms));
function gate(){let resolve;const promise=new Promise(done=>resolve=done);return{promise,resolve};}
const deadline=setTimeout(()=>{console.error('RPC fixture deadline');process.exit(1);},20000);
let listener,left,right,server,client;
async function main(){
  listener=await OneUiLocalChannel.listen(directory,'rpc',token);
  const connecting=OneUiLocalChannel.connect(directory,'rpc',token);
  right=await listener.accept();left=await connecting;assert.ok(right);
  const slow=gate(),entered=gate(),errors=[],events=[];
  server=new OneUiLocalRpc(right,async(method,payload)=>{
    if(method==='slow'){entered.resolve();await slow.promise;}
    if(method==='error')throw Error('handler failure with private details');
    return{code:0,payload:new Uint8Array(payload)};
  },payload=>events.push([...payload]),error=>errors.push(error));
  client=new OneUiLocalRpc(left,async()=>({code:-4,payload:new Uint8Array()}),payload=>events.push([...payload]),error=>errors.push(error));
  const payload=new Uint8Array([0,255,128,1,0]);
  assert.deepEqual(await client.call('echo',payload),{code:0,payload});assert.deepEqual([...payload],[0,255,128,1,0]);
  assert.equal((await client.call('error',new Uint8Array())).code,-4);
  await assert.rejects(client.call('../method',payload));
  await assert.rejects(client.call('echo',new Uint8Array(1024*1024)));
  await client.notify(payload);for(let i=0;i<20&&events.length===0;++i)await delay(10);
  assert.deepEqual(events,[[0,255,128,1,0]]);
  const waiting=Array.from({length:16},()=>client.call('slow',payload));
  await entered.promise;await assert.rejects(client.call('overflow',payload));
  slow.resolve();const replies=await Promise.all(waiting);assert(replies.every(reply=>reply.code===0));
  const late=gate(),lateEntered=gate();
  // Replace the service for the lifetime test using a fresh channel/owner.
  await client.close();await server.close();client=server=undefined;await listener.close();
  listener=await OneUiLocalChannel.listen(directory,'rpc',token);
  const second=OneUiLocalChannel.connect(directory,'rpc',token);right=await listener.accept();left=await second;
  let owned;
  server=new OneUiLocalRpc(right,async(_method,payload)=>{owned=payload;lateEntered.resolve();await late.promise;assert.equal(payload[1],255);return{code:0,payload:new Uint8Array(payload)};},()=>{},()=>{});
  client=new OneUiLocalRpc(left,async()=>({code:-4,payload:new Uint8Array()}),()=>{},()=>{});
  const timed=assert.rejects(client.call('late',payload,20),/timed out/);await lateEntered.promise;await timed;
  let closed=false;const closing=server.close().then(()=>{closed=true;});await delay(40);
  assert.equal(closed,false);assert.equal(owned[1],255);
  late.resolve();await closing;assert.equal(closed,true);assert(owned.every(value=>value===0));
  await client.close();await server.close();await listener.close();
  console.log('PASS actual ArkTS channel/RPC on native sockets: binary/event round trip, handler errors, 16-request bound, timeout, late-reply rejection and joining live handlers on close');
}
main().catch(error=>{console.error(error);process.exitCode=1;}).finally(async()=>{
  if(client)await client.close();if(server)await server.close();if(left)await left.close();if(right)await right.close();if(listener)await listener.close();
  if(fs.realpathSync(root)!==root||fs.statSync(root).uid!==process.getuid()||fs.readFileSync(marker,'utf8')!=='OneUI local RPC tests')throw Error('RPC cleanup guard');
  fs.rmSync(root,{recursive:true,force:true});clearTimeout(deadline);
});
