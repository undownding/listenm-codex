import {readFileSync} from 'node:fs';
import {festivalClips} from './festivals.js';
const assets = new URL('../assets/', import.meta.url);
export function createExtras({now = () => new Date(), random = Math.random} = {}) {
  let catalog=[];
  try { catalog=JSON.parse(readFileSync(new URL('catalog.json',assets))); } catch {}
  let queued=null, previous='';
  return async function(req,res) {
    if (!req.url.startsWith('/pet/')) return false;
    if (req.method==='GET' && req.url==='/pet/catalog') {
      res.setHeader('Content-Type','application/json'); res.end(JSON.stringify(catalog)); return true;
    }
    if (req.method==='POST' && req.url.startsWith('/pet/play/')) {
      const id=req.url.slice('/pet/play/'.length);
      if (!catalog.some(c=>c.id===id)) { res.writeHead(404); res.end(); return true; }
      queued=id; res.writeHead(202); res.end(); return true;
    }
    if (req.method==='GET' && (req.url==='/pet/next' || req.url==='/pet/next-id')) {
      if (!catalog.length) { res.writeHead(204); res.end(); return true; }
      const date=now(), month=date.getMonth()+1, day=date.getDate();
      // Seasonal/festival weights; every category remains reachable year-round.
      let pool=catalog.filter(c=>c.id!==previous);
      const favorites=pool.filter(c=>
        (month===10 && day>=24 && /halloween|ghost/.test(c.id)) ||
        (month===12 && day>=20 && /christmas|gift/.test(c.id)) ||
        (month<=2 && c.id==='building_a_snowman') ||
        (month>=3 && month<=5 && c.id==='flying_a_kite') ||
        (month>=6 && month<=8 && c.id==='cooling_with_a_hand_fan') ||
        (month>=9 && month<=11 && c.id==='buried_in_autumn_leaves'));
      if (favorites.length && random()<0.5) pool=favorites;
      // Festival priority precedes repeat avoidance: a one-clip holiday must
      // not fall back to unrelated animations on its second request.
      const ids=festivalClips(date);
      const festive=catalog.filter(c=>ids.includes(c.id));
      if (festive.length) {
        const fresh=festive.filter(c=>c.id!==previous);
        pool=fresh.length ? fresh : festive;
      }
      const clip=catalog.find(c=>c.id===queued) ?? pool[Math.floor(random()*pool.length)] ?? catalog[0];
      if (req.url==='/pet/next-id') {
        res.writeHead(200,{'Content-Type':'text/plain','Content-Length':Buffer.byteLength(clip.id),'Connection':'close'});
        res.end(clip.id); queued=null; previous=clip.id; return true;
      }
      try {
        const data=readFileSync(new URL(`${clip.id}.bin`,assets));
        res.writeHead(200,{'Content-Type':'application/octet-stream','Content-Length':data.length,'X-Animation':clip.id,'Connection':'close'});
        res.end(data); queued=null; previous=clip.id;
      } catch { res.writeHead(503); res.end(); }
      return true;
    }
    res.writeHead(404); res.end(); return true;
  };
}
