'use strict';
// The actual ArkTS scheduling/port source runs with explicit OS/N-API mocks.
// This checks late-operation ownership; it is not an ArkTS SDK compilation.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const ts = require(path.resolve(process.argv[2], 'node_modules/typescript'));
const {TextDecoder} = require('node:util');
class OsTextDecoder extends TextDecoder { decodeWithStream(bytes) { return this.decode(bytes); } }
const deadline = setTimeout(() => { console.error('Host check deadline'); process.exit(1); },10000);
const root = path.resolve(__dirname, '../..');
function load(source, dependencies = {}) {
  const result = ts.transpileModule(fs.readFileSync(path.join(root, source), 'utf8'), {
    fileName: source, reportDiagnostics: true,
    compilerOptions: {target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS}
  });
  assert.equal(result.diagnostics.length, 0);
  const exports = {};
  new Function('require', 'exports', result.outputText)(name => {
    if (!(name in dependencies)) throw Error('Unexpected dependency ' + name);
    return dependencies[name];
  }, exports);
  return exports;
}
const work = load('platform/harmonyos/nativebridge/OneUiNativeWork.ets');
function gate() { let resolve; const promise = new Promise(done => { resolve = done; }); return {promise, resolve}; }
const turn = () => new Promise(done => setImmediate(done));

async function scheduler() {
  const a = new work.OneUiNativeWorkOwner(), b = new work.OneUiNativeWorkOwner();
  const release = gate(); let running = 0, peak = 0, cancelledRan = false;
  const action = async () => { peak = Math.max(peak, ++running); await release.promise; --running; return new Uint8Array(); };
  const first = work.OneUiNativeWork.submit(a, action), second = work.OneUiNativeWork.submit(a, action);
  const cancelled = work.OneUiNativeWork.submit(b, async () => { cancelledRan = true; return new Uint8Array(); });
  const rejected = assert.rejects(cancelled, /cancelled/);
  work.OneUiNativeWork.cancel(b);
  const queued = Array.from({length:16}, () => work.OneUiNativeWork.submit(a, action));
  await assert.rejects(work.OneUiNativeWork.submit(a, action), /capacity/);
  await turn(); assert.equal(running, 2); release.resolve();
  await Promise.all([first, second, ...queued, rejected]); await turn();
  assert.equal(peak, 2); assert.equal(running, 0); assert.equal(cancelledRan, false);
}

async function portOwnership() {
  const entered = gate(), release = gate(); let responded = 0, nativeClosed = false, failClose = false;
  const payload = new Uint8Array([1,0,107,42]);
  const requests = [{id:'1', operation:4, payload}];
  const native = {
    portCreate: () => ({port:'1', host:'2'}), portStatus: () => ({running:true,error:0}),
    portPoll: () => requests.shift(), portRespond: () => { ++responded; return true; },
    portClose: async () => { if (failClose) throw Error('close failed'); nativeClosed = true; return new Uint8Array(); }
  };
  const {OneUiNativePacketPort} = load('platform/harmonyos/vpn/OneUiNativePacketPort.ets', {
    'liboneui_native_services.so': {__esModule:true, default:native}, '@kit.ArkTS': {util:{TextDecoder:OsTextDecoder}},
    '../nativebridge/OneUiNativeWork': work
  });
  const services = {secureWrite: async (key, bytes) => {
    assert.equal(key, 'k'); assert.equal(bytes[0], 42); entered.resolve(); await release.promise;
    assert.equal(bytes[0], 42); // A running OS operation still owns its input.
  }};
  const failures = [];
  const port = new OneUiNativePacketPort('fixture', 'test', 1280, false, services, code => failures.push(code));
  try {
    await entered.promise;
    let closed = false;
    const closing = port.close().then(() => { closed = true; });
    await turn(); await turn();
    assert.equal(nativeClosed, true); assert.equal(closed, false);
    assert.equal(payload[3], 42);
    release.resolve(); await closing;
    assert.equal(closed, true); assert.equal(responded, 0); assert.deepEqual([...payload], [0,0,0,0]);
    assert.deepEqual(failures, []); await port.close();
  } finally { release.resolve(); await port.close(); }
  failClose = true;
  const retry = new OneUiNativePacketPort('fixture', 'test', 1280, false, {}, code => failures.push(code));
  try { await assert.rejects(retry.close(), /close failed/); }
  finally { failClose = false; await retry.close(); }
}
(async () => {
  await scheduler(); await portOwnership();
  console.log('PASS bounded Go scheduling, cancellation, late OS ownership and cleanup retry (explicit OS mocks)');
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(()=>clearTimeout(deadline));
