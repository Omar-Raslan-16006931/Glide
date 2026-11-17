#!/usr/bin/env python3
"""Apply hand-written translations of the short interface labels to i18n/catalog.json.

scripts/i18n/hand/<lang>.txt has one line per English label, "index|translation", in the
order of scripts/i18n/hand/en.txt ("=" keeps the English text). Leading and trailing spaces
of the English source are restored around the translation, so concatenated fragments
(" · strength ") keep their spacing. Hand translations replace machine translations.
A line "zh:<Chinese source>|translation" overrides one source whose English word has
several meanings (轻 "Light" = weak, 亮 "Light" = the brightness section).

Machine translations that are left are checked: a repeated word ("Start Start Start"),
or an output far longer than the English, falls back to the English text.

Usage: python3 -B scripts/i18n/apply_hand_translations.py
"""
import json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
HAND = ROOT / 'scripts/i18n/hand'
LANGS = ['nl', 'de', 'es', 'fr', 'it', 'pt-BR', 'tr', 'ar', 'pl', 'ru']

def read_table(path, overrides=None):
    out = {}
    for n, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        if not line.strip():
            continue
        idx, sep, text = line.partition('|')
        if idx.startswith('zh:') and overrides is not None:
            overrides[idx[3:]] = text  # one English word, several meanings: by Chinese source
            continue
        if not sep or not idx.isdigit():
            sys.exit(f'{path.name}:{n}: expected "index|text"')
        out[int(idx)] = text
    return out

def degenerate(en, text):
    words = text.split()
    if any(words[i] == words[i + 1] == words[i + 2] for i in range(len(words) - 2)):
        return True
    return len(text) > max(3 * len(en), len(en) + 40)

def main():
    english = read_table(HAND / 'en.txt')
    catalog_path = ROOT / 'i18n/catalog.json'
    data = json.loads(catalog_path.read_text(encoding='utf-8'))
    by_en = {}
    for e in data['entries']:
        by_en.setdefault(e.get('en', ''), []).append(e)
    for lang in LANGS:
        path = HAND / f'{lang}.txt'
        if not path.exists():
            print(f'{lang}: no hand file'); continue
        overrides = {}
        table = read_table(path, overrides)
        missing = sorted(set(english) - set(table))
        if missing:
            sys.exit(f'{lang}: {len(missing)} labels missing, first index {missing[0]}')
        applied = 0
        for idx, en in english.items():
            text = table[idx].strip()
            if text == '=' or not text:
                text = en.strip()
            lead = en[:len(en) - len(en.lstrip())]
            trail = en[len(en.rstrip()):]
            for e in by_en.get(en, []):
                e[lang] = lead + text + trail
                applied += 1
        for e in data['entries']:
            if e['zh'] in overrides:
                e[lang] = overrides[e['zh']]; applied += 1
        fallback = 0
        for e in data['entries']:
            en = e.get('en', '')
            if en not in english.values() and e.get(lang) and degenerate(en, e[lang]):
                e[lang] = en
                fallback += 1
        print(f'{lang}: {applied} hand entries, {fallback} machine entries back to English')
    catalog_path.write_text(json.dumps(data, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')

if __name__ == '__main__':
    main()
