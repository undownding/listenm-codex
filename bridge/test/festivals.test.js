import test from 'node:test';
import assert from 'node:assert/strict';
import {festivalClips,lunarDate} from '../src/festivals.js';
import {createExtras} from '../src/extras.js';
import {readFileSync} from 'node:fs';
const at = day => new Date(`${day}T04:00:00Z`);
// Reference: https://www.hko.gov.hk/en/gts/time/calendar/pdf/files/2026e.pdf
const dates=[
  ['2026-01-26','eating_laba_congee'],['2026-02-16','setting_off_fireworks'],
  ['2026-02-17','receiving_a_red_envelope'],['2026-03-03','eating_tangyuan'],
  ['2026-06-19','eating_zongzi'],['2026-08-19','qixi_needlework'],
  ['2026-08-27','releasing_a_river_lantern'],['2026-09-25','moon_festival'],
  ['2026-10-18','eating_chongyang_cake'],
];
test('lunar festival dates map to available assets; ordinary days do not',()=>{
  const catalog=JSON.parse(readFileSync(new URL('../assets/catalog.json',import.meta.url)));
  for(const [date,id] of dates) {
    const clips=festivalClips(at(date)); assert(clips.includes(id),date);
    for(const clip of clips) assert(catalog.some(c=>c.id===clip),clip);
  }
  assert.deepEqual(festivalClips(at('2026-02-18')),[]);
  assert.deepEqual(festivalClips(at('2026-06-20')),[]);
});
test('Shanghai midnight, short and long New Year eve, and leap month',()=>{
  assert.deepEqual(festivalClips(new Date('2026-06-18T15:59:59Z')),[]);
  assert.deepEqual(festivalClips(new Date('2026-06-18T16:00:00Z')),['eating_zongzi']);
  assert.deepEqual(festivalClips(new Date('2026-06-19T16:00:00Z')),[]);
  assert.equal(lunarDate(at('2026-02-16')).day,29);
  assert.equal(lunarDate(at('2024-02-09')).day,30);
  assert(festivalClips(at('2024-02-09')).includes('lion_dance'));
  assert.deepEqual(festivalClips(at('2024-02-08')),[]);
  assert.equal(lunarDate(at('2025-07-25')).leap,true);
  assert.deepEqual(festivalClips(at('2025-07-25')),[]);
});
// Exercise the real HTTP route handler with a controlled clock, no OS clock changes.
function response() {
  return {headers:{},status:200,writeHead(status,headers={}) {this.status=status;this.headers=headers;},
    setHeader(k,v) {this.headers[k]=v;},end(body) {this.body=body;}};
}
test('festival routing, one-clip repeats, manual override, midnight refresh',async()=>{
  let date=at('2026-06-19');
  const handle=createExtras({now:()=>date,random:()=>0});
  async function request(url,method='GET') {
    const res=response(); assert(await handle({url,method},res)); return res;
  }
  for(let i=0;i<2;i++) {
    const res=await request('/pet/next');
    assert.equal(res.headers['X-Animation'],'eating_zongzi');
    assert.equal(res.body.subarray(0,4).toString(),'WHP2');
  }
  assert.equal((await request('/pet/play/playing_gomoku','POST')).status,202);
  assert.equal((await request('/pet/next')).headers['X-Animation'],'playing_gomoku');
  assert.equal((await request('/pet/next')).headers['X-Animation'],'eating_zongzi');
  date=at('2026-06-20');
  assert.notEqual((await request('/pet/next')).headers['X-Animation'],'eating_zongzi');
  date=at('2026-10-18');
  const first=(await request('/pet/next')).headers['X-Animation'];
  const second=(await request('/pet/next')).headers['X-Animation'];
  assert(festivalClips(date).includes(first));assert(festivalClips(date).includes(second));
  assert.notEqual(first,second);
});

test('all cached dates use HKO data without ICU lunar conversion, including Shanghai boundaries',t=>{
  const cache=JSON.parse(readFileSync(new URL('../data/calendar/calendar.json',import.meta.url)));
  assert.equal(Object.keys(cache.days).length,2191);
  assert.deepEqual(cache.verification,{year:2026,compared_days:365,mismatches:0});
  const original=Intl.DateTimeFormat.prototype.formatToParts;
  t.mock.method(Intl.DateTimeFormat.prototype,'formatToParts',function(date) {
    if(this.resolvedOptions().calendar==='chinese') throw new Error('ICU fallback invoked');
    return original.call(this,date);
  });
  for(const [key,[month,day,leap]] of Object.entries(cache.days)) {
    assert.deepEqual(lunarDate(at(key)),{month,day,leap},key);
  }
  assert.deepEqual(lunarDate(new Date('2025-12-31T16:00:00Z')),{month:11,day:13,leap:false});
  const [month,day,leap]=cache.days['2031-12-31'];
  assert.deepEqual(lunarDate(new Date('2031-12-31T15:59:59Z')),{month,day,leap});
  assert.throws(()=>lunarDate(new Date('2025-12-31T15:59:59Z')),/ICU fallback/);
  assert.throws(()=>lunarDate(new Date('2031-12-31T16:00:00Z')),/ICU fallback/);
});
