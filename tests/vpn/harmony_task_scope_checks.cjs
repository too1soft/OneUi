const assert=require('node:assert/strict'),path=require('node:path');
const {OneUiNativeTaskScope}=require(path.resolve(process.argv[2]));
const turn=()=>new Promise(done=>setImmediate(done));
function gate(){let resolve;const promise=new Promise(done=>resolve=done);return{promise,resolve};}
(async()=>{
  const order=[],scope=new OneUiNativeTaskScope(1);
  const result=await scope.submit(async task=>{
    task.use({close:async()=>order.push('context')});task.use({close:async()=>order.push('core')});
    return new Uint8Array([0,255]);
  });assert.deepEqual([...result],[0,255]);assert.deepEqual(order,['core','context']);
  const release=gate(),started=gate();let cancelled=false,closed=false;
  const pending=assert.rejects(scope.submit(async task=>{
    task.use({close:async()=>{closed=true;}});task.onCancel(()=>{cancelled=true;});started.resolve();await release.promise;
    assert.equal(closed,false);return new Uint8Array([42]);
  }),/cancelled/);
  await started.promise;let scopeClosed=false;const closing=scope.close().then(()=>{scopeClosed=true;});await turn();
  assert.equal(cancelled,true);assert.equal(closed,false);assert.equal(scopeClosed,false);release.resolve();
  await closing;await pending;assert.equal(closed,true);assert.equal(scopeClosed,true);
  await assert.rejects(scope.submit(async()=>new Uint8Array()),/capacity/);
  const retained=new OneUiNativeTaskScope(1);let fail=true;const retryOrder=[];
  await assert.rejects(retained.submit(async task=>{
    task.use({close:async()=>retryOrder.push('context')});task.use({close:async()=>{if(fail)throw Error('close failed');retryOrder.push('core');}});
    return new Uint8Array([42]);
  }),/close failed/);assert.deepEqual(retryOrder,[]);
  await assert.rejects(retained.submit(async()=>new Uint8Array()),/capacity/);
  fail=false;await retained.retryCleanup();assert.deepEqual(retryOrder,['core','context']);
  await retained.submit(async()=>new Uint8Array([1]));await retained.close();
  const unstarted=new OneUiNativeTaskScope();let ran=false;
  const cancelledBeforeStart=assert.rejects(unstarted.submit(async()=>{ran=true;return new Uint8Array();}),/cancelled/);
  unstarted.cancel();await cancelledBeforeStart;await unstarted.close();assert.equal(ran,false);
  const persistent=new OneUiNativeTaskScope(1);let closes=0;
  persistent.own({close:async()=>{if(++closes===1)throw Error('persistent retry');}});
  await persistent.submit(async()=>new Uint8Array([1]));assert.equal(closes,0,'UI resource survives completed login requests');
  await assert.rejects(persistent.close(),/persistent retry/);assert.equal(closes,1);
  await persistent.close();assert.equal(closes,2,'Failed persistent owner retained for retry');
  assert.throws(()=>persistent.own({close:async()=>{}}),/closed/);
  console.log('PASS native task scope: reverse resource release, joined cancellation, failed cleanup ownership/capacity and explicit retry');
})().catch(error=>{console.error(error);process.exitCode=1;});
