// Real host N-API/worker boundary tests. OS services below are explicit mocks;
// this does not claim an OHOS toolchain, secure store or device test.
const assert = require('node:assert/strict');
const path = require('node:path');
const { Worker } = require('node:worker_threads');
const addon = require(path.resolve(process.argv[2]));
const encode = value => new Uint8Array(Buffer.from(value));
const decode = value => Buffer.from(value).toString('utf8');
async function main() {
  assert.throws(() => addon.moduleOpen('../bad', 'call'));
  assert.throws(() => addon.moduleOpen('oneui_harmony_fixture', 'bad/path'));
  const module = addon.moduleOpen('oneui_harmony_fixture', 'oneui_fixture_call');
  const native = addon.portCreate('oneui_harmony_fixture', 'native host fixture', 1280, 0);
  assert.deepEqual(addon.portStatus(native.port), {running:false,error:0});
  assert(addon.interfaceInfo('lo').index > 0); assert.equal(addon.interfaceInfo('lo').loopback, true);
  assert.throws(() => addon.interfaceInfo('no-iface'));
  const operations = [], store = new Map(); let failClose = false;
  const timer = setInterval(() => {
    let request;
    while ((request = addon.portPoll(native.port)) !== undefined) {
      let reply = new Uint8Array(), code = 0;
      operations.push(request.operation);
      if (request.operation === 1) {
        addon.portReplace(native.port, -1);
        if (failClose) code = -4;
      } else if (request.operation === 2) assert.equal(decode(request.payload), '17');
      else if (request.operation === 3) reply = new Uint8Array(store.get(decode(request.payload)) ?? []);
      else if (request.operation === 4) {
        const n = request.payload[0] | request.payload[1] << 8;
        store.set(decode(request.payload.slice(2, 2 + n)), request.payload.slice(2 + n));
      } else if (request.operation === 5) reply = encode('{"version":1,"interfaces":[]}');
      else assert.fail('Unknown OS operation');
      assert.equal(addon.portRespond(native.port, request.id, code, reply), true);
      assert.equal(addon.portRespond(native.port, request.id, code, reply), false);
      request.payload.fill(0); reply.fill(0);
    }
  }, 2);
  try {
    const result = await addon.moduleCall(module, 1, encode(native.host));
    assert.deepEqual([...result], [0, 255, 42, 0, 10]);
    assert.deepEqual(operations, [1, 2, 4, 5, 3]);
    await addon.portChanged(native.port);
    await assert.rejects(addon.moduleCall(module, 2, encode(native.host)));
    await assert.rejects(addon.moduleCall(module, 3, encode(native.host)));
    const session = addon.sessionCreate();
    const config = JSON.stringify({version:1,label:'native session',mtu:1280,allowDefaultRoute:false,
      addresses:[{address:'100.64.1.1',prefix:32}],routes:[{address:'100.64.1.2',prefix:32}],dnsServers:[],dnsDomains:[]});
    assert.equal(addon.sessionConnect(session, config), 0);
    const start = addon.sessionTake(session);
    assert.equal(start.operation, 1); assert.equal(addon.sessionDestroy(session), 2);
    for (const event of [1, 3, 4]) assert.equal(addon.sessionReport(session, start.attempt, event, 0), 0);
    assert.equal(addon.sessionSnapshot(session).state, 4);
    assert.equal(addon.sessionDisconnect(session), 0); assert.equal(addon.sessionTake(session).operation, 2);
    assert.equal(addon.sessionReport(session, start.attempt, 5, 9), 0);
    assert.equal(addon.sessionSnapshot(session).state, 6); assert.equal(addon.sessionConnect(session, config), 2);
    assert.equal(addon.sessionDestroy(session), 2);
    assert.equal(addon.sessionReport(session, start.attempt, 6, 0), 0); assert.equal(addon.sessionSnapshot(session).state, 0);
    await new Promise((resolve, reject) => {
      const worker = new Worker(`const {parentPort,workerData}=require('node:worker_threads');
        const a=require(workerData.addon);let rejected=0;
        for (const f of [()=>a.portPoll(workerData.port),()=>a.sessionSnapshot(workerData.session)]) {
          try { f(); } catch { ++rejected; }
        }parentPort.postMessage(rejected);`, {eval:true,workerData:{addon:path.resolve(process.argv[2]),port:native.port,session}});
      worker.once('message', count => { try { assert.equal(count, 2); resolve(); } catch(e) { reject(e); } }); worker.once('error', reject);
    });
    assert.equal(addon.sessionDestroy(session), 0); assert.throws(() => addon.sessionSnapshot(session));
    failClose = true; await assert.rejects(addon.portClose(native.port));
    failClose = false; await addon.portClose(native.port);
    assert.throws(() => addon.portPoll(native.port));
  } finally { clearInterval(timer); addon.moduleClose(module); }
  await assert.rejects(addon.moduleCall(module, 1, encode(native.host)));
  console.log('N-API host: callback copies/binary storage, bounded worker result, owner thread, stale handle, real C++ session and cleanup retry passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
