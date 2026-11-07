'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const crypto=require('node:crypto'),net=require('node:net'),{spawn}=require('node:child_process');
const library=path.resolve(process.argv[2]), native=require(library);
const root=path.join(os.homedir(),'.cache/oneui-local-channel-fixture'),marker=path.join(root,'.oneui-test');
if(fs.existsSync(root)&&(!fs.existsSync(marker)||fs.readFileSync(marker,'utf8')!=='OneUI local channel tests'))throw Error('Unknown channel fixture');
fs.mkdirSync(root,{recursive:true,mode:0o700});fs.writeFileSync(marker,'OneUI local channel tests',{mode:0o600});
const directory=path.join(root,'ipc'),token=crypto.randomBytes(32).toString('hex'),handles=new Set();
const id=bytes=>Buffer.from(bytes).toString('utf8');
const retain=bytes=>{const value=id(bytes);assert.match(value,/^[1-9][0-9]*$/);handles.add(value);return value;};
const close=async value=>{await native.channelClose(value);handles.delete(value);};
const delay=ms=>new Promise(done=>setTimeout(done,ms));
async function receive(value){for(let n=0;n<16;++n){const bytes=await native.channelReceive(value);if(bytes.length)return bytes;}throw Error('Message deadline');}
function child(code,args=[]){
  const process=spawn(global.process.execPath,['-e',code,library,...args],{stdio:['ignore','pipe','pipe']});
  let stdout='',stderr='';process.stdout.on('data',data=>stdout=(stdout+data).slice(-4000));process.stderr.on('data',data=>stderr=(stderr+data).slice(-4000));
  const done=new Promise((resolve,reject)=>{process.on('error',reject);process.on('exit',code=>code===0?resolve(stdout):reject(Error('Fixture child: '+stderr)));});
  return {process,done};
}
const deadline=setTimeout(()=>{console.error('Channel fixture deadline');process.exit(1);},20000);
async function main(){
  const listener=retain(await native.channelListen(directory,'vpn',token));
  assert.equal(fs.statSync(path.join(directory,'vpn.sock')).mode&0o777,0o600);
  await assert.rejects(native.channelListen(directory,'vpn',token));
  assert.equal((await native.channelAccept(listener)).length,0);
  const wrong=assert.rejects(native.channelConnect(directory,'vpn',(token[0]==='a'?'b':'a')+token.slice(1)));
  await assert.rejects(native.channelAccept(listener));await wrong;
  const exchange=child(`
    const assert=require('node:assert/strict'),n=require(process.argv[1]);
    (async()=>{
      const c=Buffer.from(await n.channelConnect(process.argv[2],'vpn',process.argv[3])).toString();
      const message=new Uint8Array(1024*1024);for(let i=0;i<message.length;++i)message[i]=i&255;
      const sending=n.channelSend(c,message);message.fill(0);await sending;
      let reply;for(let i=0;i<16;++i){reply=await n.channelReceive(c);if(reply.length)break;}
      assert.deepEqual([...reply],[0,255,1,0,128]);await n.channelClose(c);
    })().catch(e=>{console.error(e);process.exitCode=1;});`,[directory,token]);
  try {
    const connection=retain(await native.channelAccept(listener));
    const message=await receive(connection);assert.equal(message.length,1024*1024);
    assert(message.every((value,index)=>value===(index&255)));
    assert.throws(()=>native.channelSend(connection,new Uint8Array(1024*1024+1)));
    await native.channelSend(connection,new Uint8Array([0,255,1,0,128]));await exchange.done;await close(connection);
  } finally { if(exchange.process.exitCode===null)exchange.process.kill(); }
  // A real raw stream splits its frame header across poll timeouts.
  const raw=net.createConnection(path.join(directory,'vpn.sock'));
  const rawReady=new Promise((resolve,reject)=>{raw.once('error',reject);raw.once('data',bytes=>{assert.equal(bytes.toString(),token);resolve();});});
  raw.on('error',()=>{});raw.write(token);
  const stream=retain(await native.channelAccept(listener));await rawReady;
  try {
    const header=Buffer.alloc(4);header.writeUInt32BE(5);raw.write(header.subarray(0,2));
    assert.equal((await native.channelReceive(stream)).length,0);
    raw.write(Buffer.concat([header.subarray(2),Buffer.from([0,1,255,0,2])]));
    assert.deepEqual([...await receive(stream)],[0,1,255,0,2]);
    header.writeUInt32BE(1024*1024+1);raw.write(header);await assert.rejects(native.channelReceive(stream));
  } finally { raw.destroy();await close(stream); }
  // Closing joins a live receiver and invalidates its handle.
  const connecting=native.channelConnect(directory,'vpn',token);
  const receiver=retain(await native.channelAccept(listener)),sender=retain(await connecting);
  const pending=assert.rejects(native.channelReceive(receiver));await delay(20);
  const started=Date.now();await close(receiver);await pending;assert.ok(Date.now()-started<1000);
  assert.throws(()=>native.channelReceive(receiver));await close(sender);
  await close(listener);assert.equal(fs.existsSync(path.join(directory,'vpn.sock')),false);
  // A terminated process leaves only its own private socket; a new lifetime
  // lock permits its exact stale inode to be reclaimed on the next launch.
  const crashed=child(`const n=require(process.argv[1]);n.channelListen(process.argv[2],'crash',process.argv[3]).then(()=>process.exit(0),()=>process.exit(1));`,[directory,token]);
  await crashed.done;assert.equal(fs.lstatSync(path.join(directory,'crash.sock')).isSocket(),true);
  const recovered=retain(await native.channelListen(directory,'crash',token));await close(recovered);
  const target=path.join(root,'protected-file');fs.writeFileSync(target,'fixture data');
  fs.symlinkSync(target,path.join(directory,'symlink.sock'));
  await assert.rejects(native.channelListen(directory,'symlink',token));assert.equal(fs.readFileSync(target,'utf8'),'fixture data');
  fs.chmodSync(directory,0o755);await assert.rejects(native.channelListen(directory,'permissions',token));fs.chmodSync(directory,0o700);
  console.log('PASS actual separate-process IPC: UID-scoped private socket, mutual token check, 1 MiB binary transfer, split framing, malformed input, joined close and crash recovery');
}
main().catch(error=>{console.error(error);process.exitCode=1;}).finally(async()=>{
  for(const value of handles)await close(value);
  if(fs.realpathSync(root)!==root||fs.statSync(root).uid!==process.getuid()||fs.readFileSync(marker,'utf8')!=='OneUI local channel tests')throw Error('Channel cleanup guard');
  fs.rmSync(root,{recursive:true,force:true});clearTimeout(deadline);
});
