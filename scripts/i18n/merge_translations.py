#!/usr/bin/env python3
"""Merge machine_translate.py outputs (<lang>.json) into i18n/catalog.json. Machine output replaces earlier machine output; run apply_hand_translations.py after this."""
import json, pathlib, sys
catalog = pathlib.Path(sys.argv[1]); parts = [pathlib.Path(p) for p in sys.argv[2:]]
data = json.loads(catalog.read_text(encoding='utf-8'))
for part in parts:
    lang = part.stem
    table = json.loads(part.read_text(encoding='utf-8'))
    n = 0
    for e in data['entries']:
        key = e['zh'] + '\u0004' + e.get('ctx', '')
        if key in table:
            e[lang] = table[key]; n += 1
    print(lang, n)
catalog.write_text(json.dumps(data, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')
