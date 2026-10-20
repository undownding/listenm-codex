#!/usr/bin/env python3
"""Build offline HKO 2026–2031 calendar; require a full-year PMO 2026 match.
Default: rebuild from checked-in sources. --refresh: download official sources first.
Requires Python 3 and pdftotext (poppler-utils); curl only for --refresh.
"""
import argparse, calendar, datetime as dt, hashlib, json, re, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'bridge/data/calendar'
SOURCES = DEST / 'sources'
URLS = {f'hko-{y}.txt': f'https://www.hko.gov.hk/tc/gts/time/calendar/text/files/T{y}c.txt' for y in range(2026, 2032)}
URLS['pmo-2026.pdf'] = 'https://pmo.cas.cn/xwdt2019/kpdt2019/202203/P020251230620718707826.pdf'
NUMBERS = dict(zip('一二三四五六七八九', range(1, 10)))
NUMBERS.update({'正': 1, '十': 10, '十一': 11, '十二': 12})
DAYS = ['初一','初二','初三','初四','初五','初六','初七','初八','初九','初十',
        '十一','十二','十三','十四','十五','十六','十七','十八','十九','二十',
        '廿一','廿二','廿三','廿四','廿五','廿六','廿七','廿八','廿九','三十']

def lunar_token(token, month, leap):
    if token.endswith('月'):
        leap = token.startswith(('闰', '閏'))
        name = token[:-1].lstrip('闰閏')
        return NUMBERS[name], 1, leap
    return month, DAYS.index(token) + 1, leap

def validate_year(days, year):
    expected = 366 if calendar.isleap(year) else 365
    if len(days) != expected: raise ValueError(f'{year}: missing days ({len(days)}/{expected})')
    for i in range(expected):
        key = (dt.date(year, 1, 1) + dt.timedelta(days=i)).isoformat()
        month, day, leap = days[key]
        if not 1 <= month <= 12 or not 1 <= day <= 30 or not isinstance(leap, bool):
            raise ValueError(f'Invalid lunar date: {key}')
    validate_sequence(days)

def validate_sequence(days):
    previous = None
    for key, value in sorted(days.items()):
        month, day, leap = value
        if previous:
            pm, pd, pl = previous
            if day == 1:
                if pd not in (29, 30) or not ((leap and not pl and month == pm) or
                                              (not leap and month == pm % 12 + 1)):
                    raise ValueError(f'Invalid lunar month transition: {key}')
            elif (month, day, leap) != (pm, pd+1, pl):
                raise ValueError(f'Invalid lunar day transition: {key}')
        previous = value

def parse_hko(text, year):
    rows = re.findall(r'^(\d{4})年(\d+)月(\d+)日\s+(\S+)\s+星期', text, re.M)
    first_month = next(token for _, _, _, token in rows if token.endswith('月'))
    # Each supported year starts in ordinary month 11 or 12, before its next
    # ordinary winter month. Reject changed input instead of guessing a seed.
    if first_month not in ('十二月', '正月'): raise ValueError('Unexpected HKO January month')
    month, leap = (11 if first_month == '十二月' else 12), False
    days = {}
    for y, m, d, token in rows:
        if int(y) != year: raise ValueError('Wrong HKO year')
        key = dt.date(int(y), int(m), int(d)).isoformat()
        if key in days: raise ValueError('Duplicate HKO date')
        month, day, leap = lunar_token(token, month, leap)
        days[key] = [month, day, leap]
    validate_year(days, year)
    return days

def parse_pmo(text):
    # pdftotext -raw emits vertical Chinese day labels across lines and fullwidth
    # digits. Normalize layout, then parse each Gregorian month independently.
    text = text.translate(str.maketrans('０１２３４５６７８９', '0123456789'))
    text = re.sub(r'\s+', '', text)
    blocks = re.split(r'二○二六年([一二三四五六七八九十]+)月[大小平]', text)
    if len(blocks) != 25: raise ValueError('Expected 12 PMO month tables')
    days = {}
    for name, block in zip(blocks[1::2], blocks[2::2]):
        greg_month = NUMBERS[name]
        header, body = block.split('星期六', 1)
        initial = re.search(r'(闰)?(十二|十一|正|[一二三四五六七八九十])月[大小]', header)
        if not initial: raise ValueError('Missing PMO initial month')
        month, leap = NUMBERS[initial[2]], bool(initial[1])
        body = body.split('【', 1)[0]
        rows = re.findall(r'(\d+)([^\d]+)', body)
        if len(rows) != calendar.monthrange(2026, greg_month)[1]: raise ValueError('Incomplete PMO month')
        for n, (day, token) in enumerate(rows, 1):
            if int(day) != n: raise ValueError('Out-of-order PMO day')
            month, ld, leap = lunar_token(token, month, leap)
            days[dt.date(2026, greg_month, n).isoformat()] = [month, ld, leap]
    validate_year(days, 2026)
    return days

def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--refresh', action='store_true')
    args = parser.parse_args(); SOURCES.mkdir(parents=True, exist_ok=True)
    if args.refresh:
        for name, url in URLS.items():
            target = SOURCES / name; tmp = target.with_suffix(target.suffix + '.tmp')
            subprocess.run(['curl','-fLsS','--retry','2','--max-time','30',url,'-o',str(tmp)], check=True)
            tmp.replace(target)
    days = {}
    for year in range(2026, 2032):
        days.update(parse_hko((SOURCES / f'hko-{year}.txt').read_text(encoding='utf-8-sig'), year))
    validate_sequence(days)
    text = subprocess.check_output(['pdftotext','-raw',str(SOURCES / 'pmo-2026.pdf'),'-'], text=True)
    pmo = parse_pmo(text)
    differences = [key for key, value in pmo.items() if days[key] != value]
    if differences: raise ValueError(f'HKO/PMO mismatch: {differences}')
    output = dict(version=1, timezone='Asia/Shanghai', start='2026-01-01', end='2031-12-31',
                  fields=['month','day','leap'], sources=[dict(file=name,url=url,
                  sha256=hashlib.sha256((SOURCES/name).read_bytes()).hexdigest()) for name,url in URLS.items()],
                  verification=dict(year=2026, compared_days=len(pmo), mismatches=0), days=days)
    target = DEST / 'calendar.json'; tmp = target.with_suffix('.json.tmp')
    tmp.write_text(json.dumps(output, ensure_ascii=False, separators=(',', ':'))+'\n'); tmp.replace(target)
    print(f'Cached {len(days)} days; PMO 2026: {len(pmo)} days checked, zero differences.')

if __name__ == '__main__': main()
