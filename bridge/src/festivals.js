import {readFileSync} from 'node:fs';
// Checked-in official HKO dates, independently cross-checked with PMO for 2026.
// No network requests at startup or during date lookup.
const calendar = JSON.parse(readFileSync(new URL('../data/calendar/calendar.json', import.meta.url), 'utf8'));
if (calendar.version !== 1 || calendar.timezone !== 'Asia/Shanghai') {
  throw new Error('Unsupported offline calendar cache');
}
const civil = new Intl.DateTimeFormat('en-CA', {
  timeZone:'Asia/Shanghai', year:'numeric', month:'2-digit', day:'2-digit',
});
// ICU is only used outside the cached date range.

const lunar = new Intl.DateTimeFormat('en-u-ca-chinese', {
  timeZone:'Asia/Shanghai', month:'numeric', day:'numeric',
});
if (lunar.resolvedOptions().calendar !== 'chinese') {
  throw new Error('Chinese calendar support requires a Node build with full ICU');
}
const newYear = ['lion_dance','receiving_a_red_envelope','setting_off_fireworks',
  'writing_the_fu_character','eating_dumplings','eating_rice_cake'];
const festivals = new Map([
  ['1/1',newYear],
  ['1/15',['eating_tangyuan','releasing_a_sky_lantern']],
  ['5/5',['eating_zongzi']],
  ['7/7',['qixi_needlework']],
  ['7/15',['releasing_a_river_lantern']],
  ['8/15',['moon_festival']],
  ['9/9',['chongyang_chrysanthemums','eating_chongyang_cake']],
  ['12/8',['eating_laba_congee']],
]);
export function lunarDate(date) {
  const civilParts=civil.formatToParts(date);
  const key=['year','month','day'].map(type=>civilParts.find(p=>p.type===type).value).join('-');
  const cached=calendar.days[key];
  if(cached) {
    const [month,day,leap]=cached;
    return {month,day,leap};
  }
  const parts=lunar.formatToParts(date);
  const month=parts.find(p=>p.type==='month').value;
  const day=Number(parts.find(p=>p.type==='day').value);
  // ICU marks intercalary months as e.g. "6bis". Never match them to
  // ordinary-month festivals (nor silently parseInt away the leap marker).
  return {month:Number(month),day,leap:!/^\d+$/.test(month)};
}
export function festivalClips(date) {
  const today=lunarDate(date);
  if(today.leap) return [];
  if(today.month===12 && today.day>=29) {
    const tomorrow=lunarDate(new Date(date.getTime()+86400000));
    if(!tomorrow.leap && tomorrow.month===1 && tomorrow.day===1) return newYear;
  }
  return festivals.get(`${today.month}/${today.day}`) ?? [];
}
