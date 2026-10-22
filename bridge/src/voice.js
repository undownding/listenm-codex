import {randomUUID} from 'node:crypto';

export function pcmWav(pcm) {
  if (!pcm.length || pcm.length % 2 || pcm.length > 16000 * 2 * 30) throw new Error('Invalid PCM length');
  const h = Buffer.alloc(44);
  h.write('RIFF'); h.writeUInt32LE(36 + pcm.length, 4); h.write('WAVEfmt ', 8);
  h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20); h.writeUInt16LE(1, 22);
  h.writeUInt32LE(16000, 24); h.writeUInt32LE(32000, 28); h.writeUInt16LE(2, 32);
  h.writeUInt16LE(16, 34); h.write('data', 36); h.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([h, pcm]);
}

export function createTranscriber({key, baseUrl = 'https://api.openai.com/v1', model = 'gpt-4o-mini-transcribe', fetchImpl = fetch}) {
  if (!key) throw new Error('OPENAI_API_KEY is required for voice transcription');
  return async pcm => {
    const form = new FormData();
    form.append('file', new Blob([pcmWav(pcm)], {type: 'audio/wav'}), 'speech.wav');
    form.append('model', model); form.append('response_format', 'json');
    const response = await fetchImpl(`${baseUrl.replace(/\/$/, '')}/audio/transcriptions`, {
      method: 'POST', headers: {Authorization: `Bearer ${key}`}, body: form, signal: AbortSignal.timeout(45000)});
    if (!response.ok) throw new Error(`Transcription HTTP ${response.status}`);
    const result = await response.json();
    if (typeof result.text !== 'string' || !result.text.trim() || result.text.length > 16000) throw new Error('Empty or invalid transcription');
    return result.text.trim();
  };
}

export function createVoice({codex, transcribe, now = Date.now, onState = () => {}}) {
  const captures = new Map();
  let busy = false;
  const reply = (res, status, value) => {
    const body = JSON.stringify(value);
    res.writeHead(status, {'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body)}); res.end(body);
  };
  return async (req, res) => {
    if (!req.url.startsWith('/voice/')) return false;
    if (req.method !== 'POST') { reply(res, 405, {error: 'POST required'}); return true; }
    for (const [id, capture] of captures) if (capture.expires < now()) captures.delete(id);
    if (req.url === '/voice/wake') {
      if (busy || [...captures.values()].some(c => !c.used)) { reply(res, 409, {error: 'Voice input busy'}); return true; }
      busy = true;
      try {
        const threadId = await codex.latest(), id = randomUUID();
        captures.set(id, {threadId, expires: now() + 60000, used: false});
        reply(res, 200, {id, threadId}); onState('attention', threadId);
      } catch { reply(res, 503, {error: 'Cannot select latest conversation'}); }
      finally { busy = false; }
      return true;
    }
    const match = /^\/voice\/([0-9a-f-]{36})\/(audio|cancel)$/.exec(req.url);
    const capture = match && captures.get(match[1]);
    if (!capture) { reply(res, 404, {error: 'Unknown or expired capture'}); return true; }
    if (capture.used || busy) { reply(res, 409, {error: 'Capture already used or busy; do not retry'}); return true; }
    if (match[2] === 'cancel') { captures.delete(match[1]); reply(res, 200, {status: 'cancelled'}); onState('idle', capture.threadId); return true; }
    capture.used = true; busy = true;
    try {
      const chunks = []; let bytes = 0;
      for await (const chunk of req) {
        bytes += chunk.length;
        if (bytes > 960000) { reply(res, 413, {error: 'Maximum speech duration is 30 seconds'}); return true; }
        chunks.push(chunk);
      }
      const pcm = Buffer.concat(chunks);
      if (pcm.length < 3200 || pcm.length % 2) { reply(res, 400, {error: 'Expected 16 kHz mono PCM16LE'}); return true; }
      const text = await transcribe(pcm);
      if (typeof text !== 'string' || !text.trim()) throw new Error('Empty speech');
      const result = await codex.submit(capture.threadId, text);
      reply(res, 200, {threadId: capture.threadId, turnId: result.turnId, status: 'submitted'});
      onState('thinking', capture.threadId);
    } catch {
      if (!res.headersSent) reply(res, 502, {error: 'Transcription or submission failed; delivery may be unknown. Do not automatically retry.'});
      onState('error', capture.threadId);
    } finally { busy = false; }
    return true;
  };
}
