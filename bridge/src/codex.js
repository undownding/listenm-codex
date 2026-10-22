import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';

// One persistent app-server owns the turns submitted by this bridge. Never edit rollouts.
export class CodexClient {
  constructor({command = 'codex', cwd, timeoutMs = 15000, onState = () => {}} = {}) {
    this.command = command; this.cwd = cwd; this.timeoutMs = timeoutMs;
    this.onState = onState; this.pending = new Map(); this.active = new Map(); this.seq = 0;
  }
  async start() {
    if (!this.ready) this.ready = this.initialize();
    return this.ready;
  }
  async initialize() {
    this.child = spawn(this.command, ['app-server'], {cwd: this.cwd, stdio: ['pipe', 'pipe', 'pipe']});
    this.child.stderr.resume(); // Do not leak prompts, paths, or credentials into bridge logs.
    this.child.stdin.on('error', () => this.fail(new Error('Codex transport closed')));
    this.child.on('error', () => this.fail(new Error('Cannot start Codex app-server')));
    this.child.on('exit', () => { this.dead = true; this.fail(new Error('Codex app-server exited')); });
    this.lines = createInterface({input: this.child.stdout});
    this.lines.on('line', line => {
      let msg; try { msg = JSON.parse(line); } catch { return; }
      if (msg.method && msg.id !== undefined) {
        // This headless input bridge cannot approve commands or answer interactive questions.
        this.write({id: msg.id, error: {code: -32601, message: 'Interactive requests require a user-facing client'}});
      } else if (msg.id !== undefined) {
        const p = this.pending.get(msg.id);
        if (p) { clearTimeout(p.timer); this.pending.delete(msg.id);
          msg.error ? p.reject(new Error(`Codex RPC failed: ${msg.error.code}`)) : p.resolve(msg.result); }
      } else if (msg.method === 'turn/started') {
        this.active.set(msg.params.threadId, msg.params.turn.id);
        this.onState('thinking', msg.params.threadId);
      } else if (msg.method === 'turn/completed') {
        this.active.delete(msg.params.threadId);
        this.onState(msg.params.turn.status === 'completed' ? 'idle' : 'error', msg.params.threadId);
      }
    });
    await this.rpc('initialize', {clientInfo: {name: 'listenm_voice', title: 'ListenM Voice', version: '0.1.0'}});
    this.write({method: 'initialized'});
  }
  write(msg) { if (!this.dead && this.child?.stdin.writable) this.child.stdin.write(JSON.stringify(msg) + '\n'); }
  rpc(method, params) {
    if (this.dead) return Promise.reject(new Error('Codex app-server unavailable'));
    return new Promise((resolve, reject) => {
      const id = ++this.seq;
      const timer = setTimeout(() => { this.pending.delete(id); reject(new Error('Codex RPC timeout; delivery may be unknown')); }, this.timeoutMs);
      this.pending.set(id, {resolve, reject, timer}); this.write({id, method, params});
    });
  }
  fail(error) { for (const p of this.pending.values()) { clearTimeout(p.timer); p.reject(error); } this.pending.clear(); }
  async latest() {
    await this.start();
    const result = await this.rpc('thread/list', {cwd: this.cwd, archived: false, sortKey: 'updated_at',
      sortDirection: 'desc', sourceKinds: ['cli', 'vscode', 'appServer', 'exec'], limit: 100});
    const thread = result.data?.find(t => !t.parentThreadId && !t.ephemeral);
    if (!thread) throw new Error('No eligible conversation in VOICE_CWD');
    return thread.id;
  }
  async submit(threadId, text) {
    await this.start();
    const input = [{type: 'text', text}];
    const active = this.active.get(threadId);
    if (active) return this.rpc('turn/steer', {threadId, expectedTurnId: active, input});
    const resumed = await this.rpc('thread/resume', {threadId});
    if (resumed.thread?.status?.type === 'active') throw new Error('Conversation is busy');
    const result = await this.rpc('turn/start', {threadId, input});
    if (result.turn?.id) this.active.set(threadId, result.turn.id);
    return {turnId: result.turn?.id};
  }
  close() { this.dead = true; this.fail(new Error('Bridge stopped')); this.lines?.close(); this.child?.kill(); }
}
