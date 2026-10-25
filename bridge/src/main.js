import {CodexClient} from './codex.js';
import {createVoice, createTranscriber} from './voice.js';
import os from 'node:os';
import path from 'node:path';
import {createBridge} from './server.js';
import {watchSessions} from './tail.js';
const host = process.env.BRIDGE_HOST ?? '127.0.0.1';
const port = Number(process.env.BRIDGE_PORT ?? 8787);
const token = process.env.BRIDGE_TOKEN ?? '';
if (!['127.0.0.1', '::1', 'localhost'].includes(host) && !token) {
  throw new Error('BRIDGE_TOKEN is required when listening beyond loopback');
}
let bridge, codex, voice;
if (process.env.BRIDGE_VOICE === '1') {
  if (!token || !process.env.VOICE_CWD || !path.isAbsolute(process.env.VOICE_CWD))
    throw new Error('Voice requires BRIDGE_TOKEN and absolute VOICE_CWD');
  const onState = (state, session) => bridge?.publish({version: 1, source: 'codex', state, session_id: session, type: 'voice', timestamp: new Date().toISOString()});
  codex = new CodexClient({command: process.env.CODEX_BIN ?? 'codex', cwd: process.env.VOICE_CWD, onState});
  voice = createVoice({codex, onState, transcribe: createTranscriber({key: process.env.OPENAI_API_KEY,
    baseUrl: process.env.VOICE_ASR_BASE_URL, model: process.env.VOICE_ASR_MODEL})});
}
bridge = createBridge({token, voice, log: process.env.BRIDGE_LOG === '1'
  ? message => console.error(`${new Date().toISOString()} [bridge] ${message}`) : undefined});
const stop = process.env.BRIDGE_WATCH === '0' ? () => {} : await watchSessions(
  process.env.CODEX_SESSIONS_DIR ?? path.join(process.env.CODEX_HOME ?? path.join(os.homedir(), '.codex'), 'sessions'),
  event => bridge.publish(event));
bridge.server.listen(port, host, () => console.error(`bridge listening on http://${host}:${port}/events`));
for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, async () => { stop(); codex?.close(); await bridge.close(); process.exit(0); });
