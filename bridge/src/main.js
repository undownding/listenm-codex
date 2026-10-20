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
const bridge = createBridge({token});
const stop = process.env.BRIDGE_WATCH === '0' ? () => {} : await watchSessions(
  process.env.CODEX_SESSIONS_DIR ?? path.join(process.env.CODEX_HOME ?? path.join(os.homedir(), '.codex'), 'sessions'),
  event => bridge.publish(event));
bridge.server.listen(port, host, () => console.error(`bridge listening on http://${host}:${port}/events`));
for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, async () => { stop(); await bridge.close(); process.exit(0); });
