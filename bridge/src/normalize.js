// Explicit allowlist: prompts, tool arguments, results and reasoning never go on the wire.
export function normalize(record, sessionId = 'unknown') {
  if (!record || typeof record !== 'object') return null;
  const p = record.payload ?? record.params ?? record;
  const type = record.method ?? (record.type === 'event_msg' || record.type === 'response_item' ? p.type : record.type);
  const states = {
    session_meta: 'idle', task_started: 'thinking', user_message: 'thinking',
    reasoning: 'thinking', function_call: 'working', custom_tool_call: 'working',
    web_search_call: 'working', function_call_output: 'thinking', custom_tool_call_output: 'thinking',
    task_complete: 'idle', turn_aborted: 'idle', context_compacted: 'thinking',
    'turn/started': 'thinking', 'turn/completed': 'idle',
    SessionStart: 'idle', UserPromptSubmit: 'thinking', PreToolUse: 'working',
    PostToolUse: 'thinking', PermissionRequest: 'attention', Stop: 'idle',
    Interrupt: 'idle', SessionEnd: 'sleeping', error: 'error',
  };
  const name = record.hook_event_name ?? type;
  let state = states[name];
  if (type === 'message' && p.role === 'assistant') state = 'speaking';
  if (type === 'item/started') state = p.item?.type === 'commandExecution' ? 'working' : 'thinking';
  if (type === 'turn/completed' && p.turn?.status === 'failed') state = 'error';
  if (!state) return null;
  const short = value => typeof value === 'string' ? value.slice(0, 128) : null;
  return {version: 1, source: 'codex', session_id: short(p.threadId ?? record.session_id ?? sessionId),
    type: short(name), state, timestamp: new Date().toISOString()};
}
