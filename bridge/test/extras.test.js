import test from 'node:test';
import assert from 'node:assert/strict';
import {createBridge} from '../src/server.js';
test('extras catalog, authentication, selection and binary response',async()=>{
  const bridge=createBridge({token:'test-token'});
  await new Promise(resolve=>bridge.server.listen(0,'127.0.0.1',resolve));
  const base=`http://127.0.0.1:${bridge.server.address().port}`;
  const headers={Authorization:'Bearer test-token'};
  try {
    assert.equal((await fetch(base+'/pet/catalog')).status,401);
    const catalog=await (await fetch(base+'/pet/catalog',{headers})).json();
    assert.equal(catalog.length,81);
    assert.equal(catalog.filter(c=>c.category==='festivals').length,20);
    assert.equal(catalog.filter(c=>c.category==='games').length,9);
    assert(!catalog.some(c=>['drag','moves','turn'].includes(c.category)));
    assert.equal((await fetch(base+'/pet/play/not-real',{method:'POST',headers})).status,404);
    assert.equal((await fetch(base+'/pet/play/playing_gomoku',{method:'POST',headers})).status,202);
    const response=await fetch(base+'/pet/next',{headers});
    assert.equal(response.headers.get('x-animation'),'playing_gomoku');
    const data=Buffer.from(await response.arrayBuffer());
    assert.equal(data.subarray(0,4).toString(),'WHP2');
    assert.equal(data.length,Number(response.headers.get('content-length')));
    const next=await fetch(base+'/pet/next',{headers});
    assert.notEqual(next.headers.get('x-animation'),'playing_gomoku');
    await next.arrayBuffer();
  } finally { await bridge.close(); }
});
