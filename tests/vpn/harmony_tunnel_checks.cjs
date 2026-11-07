const assert=require('node:assert/strict');
const {OneUiPacketTunnel}=require(require('node:path').resolve(process.argv[2]));
async function main(){
  let current, next=0, failNew=false, failRollback=false, failDestroy=false;
  const operations=[];
  const tunnel=new OneUiPacketTunnel({
    create:async config=>{operations.push('create:'+config);if((config==='new'&&failNew)||(config==='old'&&failRollback))throw Error('OS create rejected');current=config;return next++;},
    destroy:async()=>{operations.push('destroy');if(failDestroy)throw Error('OS close rejected');current=undefined;}
  });
  let prepared=0;
  assert.deepEqual(await tunnel.change('old',()=>++prepared,()=>true),{accepted:true,fd:0,fatal:false});
  failNew=true;
  const rejected=await tunnel.change('new',()=>++prepared,()=>true);
  assert.equal(rejected.accepted,false);assert.equal(rejected.fatal,false);assert.equal(current,'old');assert.equal(prepared,2);
  failDestroy=true;await assert.rejects(tunnel.close());assert.equal(current,'old');
  failDestroy=false;await tunnel.close();assert.equal(current,undefined);await tunnel.close();
  failNew=false;await tunnel.change('old',()=>{},()=>true);failNew=true;failRollback=true;
  assert.equal((await tunnel.change('new',()=>{},()=>true)).fatal,true);assert.equal(current,undefined);
  await tunnel.close();
  let release, active=true, destroyed=0;
  const opening=new OneUiPacketTunnel({create:()=>new Promise(resolve=>{release=resolve;}),destroy:async()=>{++destroyed;}});
  const start=opening.change('late',()=>{},()=>active);
  await new Promise(resolve=>setImmediate(resolve));active=false;const close=opening.close();
  release(27);assert.equal((await start).accepted,false);await close;assert.equal(destroyed,1);
  let called=false;await opening.change('cancelled',()=>{called=true;},()=>false);assert.equal(called,false);
  console.log('Tunnel ownership: FD zero, replacement rollback, failed destroy retention/retry, fatal rollback, late create cancellation and joined close passed');
}
main().catch(e=>{console.error(e);process.exitCode=1;});
