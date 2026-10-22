import test from 'node:test';
import assert from 'node:assert/strict';
import {CodexClient} from '../src/codex.js';

test('latest uses project, updated time, and non-subagent sources; resume then start', async () => {
  const c = new CodexClient({cwd: '/project'}), calls = [];
  c.start = async () => {};
  c.rpc = async (method, params) => {
    calls.push({method, params});
    if (method === 'thread/list') return {data: [{id: 'latest'}]};
    if (method === 'thread/resume') return {thread: {status: {type: 'idle'}}};
    return {turn: {id: 'running'}};
  };
  assert.equal(await c.latest(), 'latest');
  assert.equal(calls[0].params.sortKey, 'updated_at');
  assert.equal(calls[0].params.cwd, '/project');
  assert.ok(!calls[0].params.sourceKinds.includes('subAgent'));
  assert.deepEqual(await c.submit('latest', 'hello'), {turnId: 'running'});
  assert.deepEqual(calls.slice(1).map(c => c.method), ['thread/resume', 'turn/start']);
  await c.submit('latest', 'more');
  assert.equal(calls.at(-1).method, 'turn/steer');
  assert.equal(calls.at(-1).params.expectedTurnId, 'running');
});
test('no eligible thread and foreign busy thread fail instead of starting another turn', async () => {
  const c = new CodexClient(); c.start = async () => {};
  c.rpc = async () => ({data: []});
  await assert.rejects(c.latest(), /No eligible/);
  c.rpc = async method => { assert.equal(method, 'thread/resume'); return {thread: {status: {type: 'active'}}}; };
  await assert.rejects(c.submit('busy', 'hi'), /busy/);
});

test('JSONL transport initializes, rejects interactive requests, and fails pending RPC on exit', async t => {
  const {mkdtemp, writeFile, rm} = await import('node:fs/promises');
  const {tmpdir} = await import('node:os');
  const {join} = await import('node:path');
  const dir = await mkdtemp(join(tmpdir(), 'listenm-codex-'));
  t.after(() => rm(dir, {recursive: true, force: true}));
  const bin = join(dir, 'fake-codex');
  await writeFile(bin, `#!/usr/bin/env node
const readline = require('node:readline');
const send = x => process.stdout.write(JSON.stringify(x)+'\\n');
readline.createInterface({input: process.stdin}).on('line', line => {
 const m = JSON.parse(line);
 if (m.method === 'initialize') send({id:m.id,result:{}});
 else if (m.method === 'thread/list') send({id:m.id,result:{data:[{id:'one'}]}});
 else if (m.method === 'ask') {global.request=m.id;send({id:'approval',method:'item/commandExecution/requestApproval',params:{}});}
 else if (m.id === 'approval') send({id:global.request,result:{rejected:!!m.error}});
 else if (m.method === 'exit') process.exit(0);
});
`, {mode: 0o700});
  const c = new CodexClient({command: bin, cwd: dir, timeoutMs: 1000});
  t.after(() => c.close());
  assert.equal(await c.latest(), 'one');
  assert.deepEqual(await c.rpc('ask', {}), {rejected: true});
  await assert.rejects(c.rpc('exit', {}), /exited/);
});
