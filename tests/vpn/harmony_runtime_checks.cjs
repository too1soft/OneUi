'use strict';
// Actual ArkTS lifecycle, C++ session and native IPC. NetworkKit, AssetStore and
// packet engine are explicit OS mocks; this is not a Harmony SDK/device check.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os'),crypto=require('node:crypto');
const {TextDecoder}=require('node:util');
const root=path.resolve(__dirname,'../..'),native=require(path.resolve(process.argv[2]));
const ts=require(path.resolve(process.argv[3],'node_modules/typescript'));
class Decoder extends TextDecoder{decodeWithStream(bytes){return this.decode(bytes);}}
class Encoder{encodeInto(text){return new Uint8Array(Buffer.from(text));}}
const directory=path.join(os.homedir(),'.cache/oneui-vpn-runtime-fixture'),marker=path.join(directory,'.oneui-test');
if(fs.existsSync(directory)&&(!fs.existsSync(marker)||fs.readFileSync(marker,'utf8')!=='OneUI VPN runtime tests'))throw Error('Unknown runtime fixture');
fs.mkdirSync(directory,{recursive:true,mode:0o700});fs.writeFileSync(marker,'OneUI VPN runtime tests',{mode:0o600});
const blobs=new Map();let readGate;
class MockStore{
  constructor(_directory,namespace){this.namespace=namespace;}
  async read(key){if(readGate)await readGate.promise;const value=blobs.get(this.namespace+':'+key);return value?new Uint8Array(value):undefined;}
  async write(key,value){blobs.set(this.namespace+':'+key,new Uint8Array(value));}
}
let mode='allow',ability,pendingWant,stopCalls=0,tuns=0,engines=0,createGate,failStop=false;
const observers=new Set();
const vpnExtension={
  createVpnObserver:()=>{let callback;const observer={onAuthorizationResult:fn=>{callback=fn;observers.add(observer);},offAuthorizationResult:()=>observers.delete(observer),emit:allowed=>callback(allowed)};return observer;},
  startVpnExtensionAbility:async want=>{
    if(mode==='pending'){pendingWant=want;return;}
    if(mode==='deny'){for(const observer of [...observers])observer.emit(false);return;}
    ability=new Ability();ability.onCreate(want);for(const observer of [...observers])observer.emit(true);
  },
  stopVpnExtensionAbility:async()=>{++stopCalls;ability?.onDestroy();},
  createVpnConnection:()=>({
    create:async config=>{assert.equal(config.isBlocking,false);if(createGate)await createGate.promise;++tuns;return 400;},
    destroy:async()=>{assert.equal(tuns,1);--tuns;},protect:async()=>{},protectProcessNet:async()=>{}
  })
};
const loaded=new Map();
function load(file){
  const location=path.resolve(file);
  if(location.endsWith('/storage/OneUiSecureBlobStore.ets'))return{OneUiSecureBlobStore:MockStore};
  if(loaded.has(location))return loaded.get(location);
  const compiled=ts.transpileModule(fs.readFileSync(location,'utf8'),{fileName:location,reportDiagnostics:true,
    compilerOptions:{target:ts.ScriptTarget.ES2020,module:ts.ModuleKind.CommonJS}});
  assert.equal(compiled.diagnostics.length,0);const exports={};loaded.set(location,exports);
  new Function('require','exports',compiled.outputText)(name=>{
    if(name==='liboneui_native_services.so')return{__esModule:true,default:native};
    if(name==='@kit.ArkTS')return{util:{TextDecoder:Decoder,TextEncoder:Encoder}};
    if(name==='@kit.CryptoArchitectureKit')return{cryptoFramework:{createRandom:()=>({generateRandomSync:length=>({data:new Uint8Array(crypto.randomBytes(length))})})}};
    if(name==='@kit.NetworkKit')return{vpnExtension,VpnExtensionAbility:class{constructor(){this.context={filesDir:directory};}}};
    if(name.startsWith('.'))return load(path.resolve(path.dirname(location),name+'.ets'));
    throw Error('Unexpected OS dependency '+name);
  },exports);return exports;
}
const Base=load(path.join(root,'platform/harmonyos/vpn/OneUiVpnExtensionAbility.ets')).default;
class Ability extends Base{
  storageNamespace(){return'fixture.vpn';}
  async message(bytes){return new Uint8Array(bytes);}
  createEngine(){let active=false;return{
    start:async()=>{active=true;++engines;},
    stop:async()=>{if(failStop){throw Error('cleanup failed');}if(active){--engines;active=false;}}
  };}
}
const {OneUiVpnRuntime}=load(path.join(root,'platform/harmonyos/vpn/OneUiVpnRuntime.ets'));
const config=JSON.stringify({version:1,label:'OneUI fixture',mtu:1280,allowDefaultRoute:false,
  addresses:[{address:'100.80.0.2',prefix:32}],routes:[{address:'100.80.0.0',prefix:24}],dnsServers:[],dnsDomains:[]});
const delay=ms=>new Promise(done=>setTimeout(done,ms));
async function until(predicate){for(let n=0;n<250;++n){if(predicate())return;await delay(20);}throw Error('Runtime state deadline');}
function gate(){let resolve;const promise=new Promise(done=>resolve=done);return{promise,resolve};}
const runtimes=[];function runtime(){const value=new OneUiVpnRuntime(directory,'fixture.vpn','oneui.fixture','Vpn',()=>{});runtimes.push(value);return value;}
const deadline=setTimeout(()=>{console.error('Runtime check deadline');process.exit(1);},25000);
async function main(){
  let ui=runtime();mode='deny';await ui.connect(config);assert.equal(ui.snapshot().state,6);assert.equal(tuns,0);
  mode='allow';await ui.connect(config);await until(()=>ui.snapshot().state===4);assert.equal(tuns,1);assert.equal(engines,1);
  assert.deepEqual([...await ui.message(new Uint8Array([0,255,1]))],[0,255,1]);
  const before=stopCalls;await ui.close();assert.equal(stopCalls,before);assert.equal(engines,1,'UI destruction retains established VPN');
  ui=runtime();assert.equal(await ui.restore(),true);assert.equal(ui.snapshot().state,4);
  // A cleanup failure retains both the engine and OS interface for retry.
  failStop=true;await ui.disconnect();await until(()=>ui.snapshot().state===6);
  assert.equal(tuns,1);assert.equal(engines,1);
  failStop=false;await ui.disconnect();await until(()=>ui.snapshot().state===0&&!fs.existsSync(path.join(directory,'oneui-vpn-ipc/vpn.sock')));
  assert.equal(engines,0);assert.equal(tuns,0);await ui.close();
  // Stop during late OS creation waits for and destroys the created interface.
  ui=runtime();createGate=gate();await ui.connect(config);assert.notEqual(ui.snapshot().state,4);
  await ui.disconnect();assert.equal(tuns,0);createGate.resolve();
  await until(()=>ui.snapshot().state===0&&!fs.existsSync(path.join(directory,'oneui-vpn-ipc/vpn.sock')));
  assert.equal(tuns,0);assert.equal(engines,0);createGate=undefined;await ui.close();
  // A late consent after the UI was closed is cancelled by its observer.
  ui=runtime();mode='pending';const starting=ui.connect(config);await delay(30);await ui.close();await starting;
  const oldStops=stopCalls;ability=new Ability();ability.onCreate(pendingWant);
  for(const observer of [...observers])observer.emit(true);
  await until(()=>stopCalls>oldStops&&!fs.existsSync(path.join(directory,'oneui-vpn-ipc/vpn.sock')));
  assert.equal(tuns,0);assert.equal(engines,0);
  ui=runtime();readGate=gate();const restoring=ui.restore();await delay(20);
  let released=false;const closing=ui.close().then(()=>released=true);await delay(20);assert.equal(released,false);
  readGate.resolve();await closing;assert.equal(await restoring,false);readGate=undefined;
  console.log('PASS actual ArkTS VPN runtime + C++ state/native IPC: denial/retry, live service restore, UI exit persistence, cleanup-failure retention/retry, late TUN creation and late consent (OS/engine mocked)');
}
main().catch(error=>{console.error(error);process.exitCode=1;}).finally(async()=>{
  failStop=false;if(readGate)readGate.resolve();if(createGate)createGate.resolve();for(const ui of runtimes)await ui.close();ability?.onDestroy();
  await until(()=>!fs.existsSync(path.join(directory,'oneui-vpn-ipc/vpn.sock'))&&tuns===0&&engines===0);
  if(fs.realpathSync(directory)!==directory||fs.statSync(directory).uid!==process.getuid()||fs.readFileSync(marker,'utf8')!=='OneUI VPN runtime tests')throw Error('Runtime cleanup guard');
  fs.rmSync(directory,{recursive:true,force:true});clearTimeout(deadline);
});
