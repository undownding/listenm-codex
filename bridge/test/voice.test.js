import test from 'node:test';
import assert from 'node:assert/strict';
import {once} from 'node:events';
import {createBridge} from '../src/server.js';
import {createVoice, createTranscriber, pcmWav} from '../src/voice.js';

async function setup(t, {transcribe = async () => '请继续', now = Date.now} = {}) {
  const sent = []; let latest = 'first';
  const codex = {latest: async () => latest, submit: async (id, text) => { sent.push({id, text}); return {turnId: 'turn'}; }};
  const bridge = createBridge({token: 'secret', voice: createVoice({codex, transcribe, now})});
  bridge.server.listen(0, '127.0.0.1'); await once(bridge.server, 'listening');
  t.after(() => bridge.close());
  const post = (path, body, token = 'secret') => fetch(`http://127.0.0.1:${bridge.server.address().port}${path}`, {
    method: 'POST', headers: {Authorization: `Bearer ${token}`}, body});
  return {post, sent, latest: id => { latest = id; }};
}
test('wake locks destination, authenticated upload submits exactly once', async t => {
  const s = await setup(t);
  assert.equal((await s.post('/voice/wake', undefined, 'wrong')).status, 401);
  const wake = await s.post('/voice/wake'); const {id} = await wake.json();
  assert.equal((await s.post('/voice/wake')).status, 409);
  s.latest('second');
  const result = await s.post(`/voice/${id}/audio`, Buffer.alloc(6400));
  assert.equal(result.status, 200); assert.equal((await result.json()).threadId, 'first');
  assert.deepEqual(s.sent, [{id: 'first', text: '请继续'}]);
  assert.equal((await s.post(`/voice/${id}/audio`, Buffer.alloc(6400))).status, 409);
  assert.equal(s.sent.length, 1);
});
test('expired captures and invalid PCM never submit', async t => {
  let now = 0;
  const s = await setup(t, {now: () => now});
  let {id} = await (await s.post('/voice/wake')).json();
  now = 61000;
  assert.equal((await s.post(`/voice/${id}/audio`, Buffer.alloc(6400))).status, 404);
  ({id} = await (await s.post('/voice/wake')).json());
  assert.equal((await s.post(`/voice/${id}/audio`, Buffer.alloc(3201))).status, 400);
  assert.equal(s.sent.length, 0);
});
test('transcription failure consumes capture without retries', async t => {
  const s = await setup(t, {transcribe: async () => { throw new Error('offline'); }});
  const {id} = await (await s.post('/voice/wake')).json();
  assert.equal((await s.post(`/voice/${id}/audio`, Buffer.alloc(6400))).status, 502);
  assert.equal((await s.post(`/voice/${id}/audio`, Buffer.alloc(6400))).status, 409);
  assert.equal(s.sent.length, 0);
});
test('WAV and transcription request preserve PCM format and reject blank speech', async () => {
  const pcm = Buffer.alloc(6400, 5), wav = pcmWav(pcm);
  assert.equal(wav.readUInt32LE(24), 16000); assert.equal(wav.readUInt16LE(22), 1);
  assert.equal(wav.readUInt32LE(40), pcm.length); assert.deepEqual(wav.subarray(44), pcm);
  assert.throws(() => pcmWav(Buffer.alloc(3)));
  let calls = 0;
  const transcribe = createTranscriber({key: 'test', fetchImpl: async (url, options) => {
    calls++; assert.equal(url, 'https://api.openai.com/v1/audio/transcriptions');
    assert.equal(options.headers.Authorization, 'Bearer test');
    assert.equal(options.body.get('model'), 'gpt-4o-mini-transcribe');
    assert.deepEqual(Buffer.from(await options.body.get('file').arrayBuffer()), wav);
    return {ok: true, json: async () => ({text: ' '})};
  }});
  await assert.rejects(transcribe(pcm), /Empty/); assert.equal(calls, 1);
});
test('cancel frees a silent capture immediately and oversized upload never transcribes', async t => {
  let calls = 0;
  const s = await setup(t, {transcribe: async () => { calls++; return 'test'; }});
  let {id} = await (await s.post('/voice/wake')).json();
  assert.equal((await s.post(`/voice/${id}/cancel`)).status, 200);
  ({id} = await (await s.post('/voice/wake')).json());
  assert.equal((await s.post(`/voice/${id}/audio`, Buffer.alloc(960002))).status, 413);
  assert.equal(calls, 0); assert.equal(s.sent.length, 0);
});
