import {readdir, stat, open} from 'node:fs/promises';
import path from 'node:path';
import {StringDecoder} from 'node:string_decoder';
import {normalize} from './normalize.js';

// Polling works on Linux/macOS and across atomic file creation. Only appended data
// is consumed on startup; no historical conversation is replayed as live activity.
export async function watchSessions(root, publish, {intervalMs = 500, onError = console.error} = {}) {
  const files = new Map();
  let stopped = false, timer;
  async function discover(dir, depth = 0) {
    if (depth > 4) return [];
    let entries;
    try { entries = await readdir(dir, {withFileTypes: true}); }
    catch (e) { if (e.code === 'ENOENT') return []; throw e; }
    const result = [];
    for (const entry of entries) {
      const file = path.join(dir, entry.name);
      if (entry.isDirectory()) result.push(...await discover(file, depth + 1));
      else if (entry.isFile() && entry.name.endsWith('.jsonl')) result.push(file);
    }
    return result;
  }
  async function scan(initial = false) {
    const candidates = await discover(root);
    const current = new Set(candidates);
    for (const file of files.keys()) if (!current.has(file)) files.delete(file);
    for (const file of candidates) {
      const s = await stat(file);
      let f = files.get(file);
      if (!f || f.ino !== s.ino || s.size < f.offset) {
        f = {ino: s.ino, offset: initial ? s.size : 0, pending: '', decoder: new StringDecoder('utf8'),
          session: path.basename(file).match(/([0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12})\.jsonl$/i)?.[1] ?? path.basename(file, '.jsonl'), dropping: false};
        const handle = await open(file, 'r');
        try {
          const b = Buffer.alloc(8192); const {bytesRead} = await handle.read(b, 0, b.length, 0);
          const first = b.subarray(0, bytesRead).toString().split('\n')[0];
          try { const meta = JSON.parse(first); if (meta.type === 'session_meta') f.session = meta.payload.id; } catch {}
          // Ignore an already partial record at startup.
          if (initial && s.size) {
            const last = Buffer.alloc(1); await handle.read(last, 0, 1, s.size - 1);
            f.dropping = last[0] !== 10;
          }
        } finally { await handle.close(); }
        files.set(file, f);
      }
      if (s.size === f.offset) continue;
      const handle = await open(file, 'r');
      try {
        // Bound per-file work so a busy session cannot starve other sessions.
        const b = Buffer.alloc(Math.min(s.size - f.offset, 262144));
        const {bytesRead} = await handle.read(b, 0, b.length, f.offset);
        f.offset += bytesRead;
        f.pending += f.decoder.write(b.subarray(0, bytesRead));
        let end;
        while ((end = f.pending.indexOf('\n')) >= 0) {
          const line = f.pending.slice(0, end); f.pending = f.pending.slice(end + 1);
          if (f.dropping) { f.dropping = false; continue; }
          try {
            const record = JSON.parse(line);
            if (record.type === 'session_meta') f.session = record.payload.id;
            const event = normalize(record, f.session); if (event) publish(event);
          } catch { /* incomplete/invalid records are not state transitions */ }
        }
        if (f.pending.length > 1048576) { f.pending = ''; f.dropping = true; }
      } finally { await handle.close(); }
    }
  }
  await scan(true);
  async function tick() {
    if (stopped) return;
    try { await scan(); } catch (e) { onError(`session watcher: ${e.code ?? e.message}`); }
    if (!stopped) timer = setTimeout(tick, intervalMs);
  }
  timer = setTimeout(tick, intervalMs);
  return () => { stopped = true; clearTimeout(timer); };
}
