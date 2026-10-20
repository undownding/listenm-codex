import http from 'node:http';
import {createExtras} from './extras.js';
import {randomUUID, timingSafeEqual} from 'node:crypto';
import {normalize} from './normalize.js';

export function createBridge({token = '', heartbeatMs = 15000, historyLimit = 256} = {}) {
  const extras = createExtras();
  const epoch = randomUUID();
  let seq = 0;
  const history = [], clients = new Set();
  function send(res, frame) {
    if (!res.write(frame)) { clients.delete(res); res.destroy(); }
  }
  function publish(event) {
    const item = {id: `${epoch}:${++seq}`, event};
    history.push(item);
    if (history.length > historyLimit) history.shift();
    for (const res of clients) send(res, frame(item));
    return item;
  }
  const frame = item => `id: ${item.id}\nevent: codex\ndata: ${JSON.stringify(item.event)}\n\n`;
  const server = http.createServer(async (req, res) => {
    if (token) {
      const supplied = Buffer.from(req.headers.authorization ?? '');
      const expected = Buffer.from(`Bearer ${token}`);
      if (supplied.length !== expected.length || !timingSafeEqual(supplied, expected)) {
        res.writeHead(401); res.end(); return;
      }
    }
    if (await extras(req,res)) return;
    if (req.method === 'GET' && req.url === '/health') {
      res.setHeader('Content-Type', 'application/json');
      res.end(JSON.stringify({ok: true, clients: clients.size, sequence: seq})); return;
    }
    if (req.method === 'GET' && req.url === '/events') {
      if (clients.size >= 16) { res.writeHead(503); res.end(); return; }
      // No chunked framing: embedded client receives a close-delimited HTTP body.
      res.useChunkedEncodingByDefault = false;
      res.writeHead(200, {'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache',
        'Connection': 'close', 'X-Accel-Buffering': 'no'});
      res.flushHeaders();
      send(res, 'retry: 2000\n: connected\n\n');
      const last = req.headers['last-event-id'];
      const index = history.findIndex(item => item.id === last);
      if (last && index < 0) send(res, 'event: reset\ndata: {"reason":"history_unavailable"}\n\n');
      // New subscribers receive recent state, reconnects receive missed events.
      const replay = last && index >= 0 ? history.slice(index + 1) : history.slice(-1);
      for (const item of replay) send(res, frame(item));
      if (!res.destroyed) clients.add(res);
      res.on('close', () => clients.delete(res)); return;
    }
    if (req.method === 'POST' && req.url === '/ingest') {
      const chunks = []; let size = 0;
      try {
        for await (const chunk of req) {
          size += chunk.length;
          if (size > 65536) { res.writeHead(413); res.end(); return; }
          chunks.push(chunk);
        }
        const event = normalize(JSON.parse(Buffer.concat(chunks).toString('utf8')));
        if (event) publish(event);
        res.writeHead(202); res.end();
      } catch { if (!res.headersSent) res.writeHead(400); res.end(); }
      return;
    }
    res.writeHead(404); res.end();
  });
  server.requestTimeout = 10000;
  const heartbeat = setInterval(() => {
    for (const res of clients) send(res, ': heartbeat\n\n');
  }, heartbeatMs);
  heartbeat.unref();
  return {server, publish, async close() {
    clearInterval(heartbeat);
    for (const res of clients) res.destroy();
    await new Promise(resolve => server.close(resolve));
  }};
}
