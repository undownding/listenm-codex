import {test} from 'node:test';
import assert from 'node:assert/strict';
import {once} from 'node:events';
import {mkdtemp, writeFile, appendFile, rm} from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {createBridge} from '../src/server.js';
import {normalize} from '../src/normalize.js';
import {watchSessions} from '../src/tail.js';

test('normalizes actual rollout and app-server records without content', () => {
  const event = normalize({type:'response_item',payload:{type:'function_call',arguments:'secret'}},'abc');
  assert.equal(event.state,'working'); assert.equal(event.session_id,'abc');
  assert.ok(!JSON.stringify(event).includes('secret'));
  assert.equal(normalize({type:'event_msg',payload:{type:'task_complete'}}).state,'idle');
  assert.equal(normalize({method:'turn/completed',params:{turn:{status:'failed'}}}).state,'error');
  assert.equal(normalize({type:'event_msg',payload:{type:'token_count'}}),null);
});

test('SSE authorization, replay, restart/reset, ingest and bounds', async t => {
  const b = createBridge({token:'test',historyLimit:2,heartbeatMs:50});
  b.server.listen(0,'127.0.0.1'); await once(b.server,'listening');
  t.after(()=>b.close());
  const base = `http://127.0.0.1:${b.server.address().port}`;
  assert.equal((await fetch(base+'/events')).status,401);
  const headers = {Authorization:'Bearer test'};
  assert.equal((await fetch(base+'/ingest',{method:'POST',headers,body:'bad'})).status,400);
  assert.equal((await fetch(base+'/ingest',{method:'POST',headers,body:'x'.repeat(65537)})).status,413);
  const first = b.publish(normalize({type:'task_started'}));
  const second = b.publish(normalize({type:'task_complete'}));
  const controller = new AbortController(); t.after(()=>controller.abort());
  const response = await fetch(base+'/events',{headers:{...headers,'Last-Event-ID':first.id},signal:controller.signal});
  assert.equal(response.headers.get('transfer-encoding'),null);
  const reader = response.body.getReader();
  let received = '';
  while (!received.includes(second.id)) received += new TextDecoder().decode((await reader.read()).value);
  assert.ok(!received.includes(`id: ${first.id}`));
  controller.abort();
  b.publish(normalize({type:'reasoning'}));
  const resetController = new AbortController(); t.after(()=>resetController.abort());
  const reset = await fetch(base+'/events',{headers:{...headers,'Last-Event-ID':'old:1'},signal:resetController.signal});
  const text = new TextDecoder().decode((await reset.body.getReader().read()).value);
  assert.ok(text.includes('event: reset'));
});

test('tail skips history, handles partial UTF-8 lines, new sessions and truncation', async t => {
  const root = await mkdtemp(path.join(os.tmpdir(),'listenm-'));
  t.after(()=>rm(root,{recursive:true,force:true}));
  const file = path.join(root,'rollout-a.jsonl');
  await writeFile(file,JSON.stringify({type:'session_meta',payload:{id:'session-a'}})+'\n');
  const events = [];
  const stop = await watchSessions(root,e=>events.push(e),{intervalMs:10}); t.after(stop);
  assert.equal(events.length,0);
  const record = Buffer.from(JSON.stringify({type:'event_msg',payload:{type:'task_started',text:'你好'}})+'\n');
  await appendFile(file,record.subarray(0,record.length-4)); await delay(40);
  assert.equal(events.length,0);
  await appendFile(file,record.subarray(record.length-4));
  for(let i=0;i<100 && events.length<1;i++) await delay(10);
  assert.equal(events[0].session_id,'session-a'); assert.equal(events[0].state,'thinking');
  await writeFile(file,JSON.stringify({type:'event_msg',payload:{type:'task_complete'}})+'\n');
  for(let i=0;i<100 && events.length<2;i++) await delay(10);
  assert.equal(events[1].state,'idle');
  await writeFile(path.join(root,'rollout-b.jsonl'),JSON.stringify({type:'session_meta',payload:{id:'session-b'}})+'\n');
  for(let i=0;i<100 && events.length<3;i++) await delay(10);
  assert.equal(events[2].session_id,'session-b');
});
