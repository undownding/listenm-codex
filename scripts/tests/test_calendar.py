import importlib.util
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('cache_calendar', ROOT/'scripts/cache-calendar.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)

class CalendarTests(unittest.TestCase):
    def test_official_sources_and_cross_check(self):
        all_days = {}
        for year in range(2026,2032):
            days = module.parse_hko((module.SOURCES/f'hko-{year}.txt').read_text(encoding='utf-8-sig'), year)
            all_days.update(days)
        self.assertEqual(len(all_days),2191)
        module.validate_sequence(all_days)
        text = subprocess.check_output(['pdftotext','-raw',str(module.SOURCES/'pmo-2026.pdf'),'-'],text=True)
        pmo = module.parse_pmo(text)
        self.assertEqual(len(pmo),365)
        self.assertEqual(pmo,{k:v for k,v in all_days.items() if k.startswith('2026-')})
        self.assertEqual(all_days['2028-02-29'][1],5)
        self.assertTrue(any(value[2] for value in all_days.values()))

    def test_rejects_missing_and_duplicate_days(self):
        text = (module.SOURCES/'hko-2026.txt').read_text(encoding='utf-8-sig')
        line = next(line for line in text.splitlines() if line.startswith('2026年6月19日'))
        with self.assertRaises(ValueError): module.parse_hko(text.replace(line,''),2026)
        with self.assertRaises(ValueError): module.parse_hko(text+'\n'+line,2026)
        with self.assertRaises(ValueError): module.parse_hko(text,2027)
        with self.assertRaises(ValueError): module.parse_pmo('unexpected PDF layout')

if __name__=='__main__': unittest.main()
