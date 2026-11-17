#!/usr/bin/env python3
"""Fill one language of i18n/catalog.json from the English text with Argos Translate.

Offline, open-source neural translation (https://github.com/argosopentech/argos-translate,
MIT; models from the Argos index, OPUS-MT data). Runs in CI (.github/workflows/translate.yml),
never at app run time. Every entry is translated except the short interface labels, which are
hand-written in scripts/i18n/hand/<lang>.txt (apply_hand_translations.py puts them in afterwards).

Placeholders (%1, {}, {:.1f}), line breaks, tags and product names are swapped for numbered
tokens before translating and put back after; an entry whose tokens do not come back intact
keeps the English text, so a broken placeholder can never reach the interface.

usage: machine_translate.py <catalog.json> <lang> <out.json>
"""
import json, re, sys

ARGOS_CODE = {'nl': 'nl', 'de': 'de', 'es': 'es', 'fr': 'fr', 'it': 'it', 'pt-BR': 'pt',
              'tr': 'tr', 'ar': 'ar', 'pl': 'pl', 'ru': 'ru'}
TERMS = ['Glide', 'Veyra', 'Likely7', 'NVIDIA', 'GeForce', 'RTX', 'DLSS', 'DLAA', 'NGX', 'VFG', 'XeSS',
         'FidelityFX', 'FSR', 'AMD', 'Intel', 'Radeon', 'Arc', 'OBS Studio', 'OBS', 'Discord', 'Reddit',
         'GitHub', 'Patreon', 'GTA VI', 'PlayStation', 'PS5', 'Xbox', 'Nintendo', 'Switch', 'Moonlight',
         'Sunshine', 'Magewell', 'Blackmagic', 'DeckLink', 'Elgato', 'AVerMedia', 'HDMI', 'HDCP', 'HDR10',
         'HDR', 'SDR', 'VRR', 'G-SYNC', 'FreeSync', 'NIS', 'ANR', 'LUT', 'ICC', 'Windows', 'DirectX', 'D3D12',
         'Vulkan', 'CUDA', 'FFmpeg', 'H.264', 'HEVC', 'AV1', 'AAC', 'MP4', 'MKV', 'NV12', 'P010', 'YUY2',
         'MJPEG', 'RGB', 'YUV', 'Rec.709', 'Rec.2020', 'BT.709', 'BT.2020', 'PQ', 'HLG', 'nits', 'FPS', 'Hz',
         'ms', 'MB', 'GB', 'GPU', 'CPU', 'PC', 'USB', 'PCIe', 'RTSS', 'MSI Afterburner', 'Smooth Motion',
         'Reflex', 'VB-CABLE', 'WASAPI', 'Dolby', 'DTS', 'PSNTools', 'Remote Play', 'PUSHFPS1:', 'F8', 'F9',
         'F11', 'Ctrl', 'Alt', 'Shift', 'Enter', 'Esc', 'Webhook', 'webhook', 'NR', 'SR', 'FG', 'AI', '1% low', 'p95']
TERMS.sort(key=len, reverse=True)
TOKEN = re.compile(r'%\d+|\{[^{}]*\}|\n|<[^<>]+>|&[a-z]+;|https?://\S+|\b[A-Za-z]:\\\S*')
TERM_RE = re.compile('|'.join(r'(?<![A-Za-z])' + re.escape(t) + r'(?![A-Za-z])' for t in TERMS))
# Words the general-purpose models translate in the wrong sense ("driver" as a car driver).
# Swapping terms for tokens before translating broke the grammar around them, so the
# wrong words are corrected after translating instead.
FIXES = {
    'nl': {'bestuurder': 'driver', 'bestuurders': 'drivers'},
    'de': {'Fahrer': 'Treiber', 'Fahrers': 'Treibers', 'Fahrern': 'Treibern'},
    'es': {'conductor': 'controlador', 'conductores': 'controladores'},
    'fr': {'conducteur': 'pilote', 'conducteurs': 'pilotes'},
    'it': {'conducente': 'driver', 'conducenti': 'driver', 'autista': 'driver'},
    'pt-BR': {'motorista': 'driver', 'motoristas': 'drivers'},
    'pl': {'kierowca': 'sterownik', 'kierowcy': 'sterowniki', 'kierowcę': 'sterownik'},
    'ru': {'водитель': 'драйвер', 'водителя': 'драйвера', 'водители': 'драйверы'},
    'ar': {'السائق': 'برنامج التعريف', 'سائق': 'برنامج تعريف'},
}

def postprocess(lang, en, text):
    """Correct known wrong-sense words; None when the output is not usable (English is kept)."""
    for wrong, right in FIXES.get(lang, {}).items():
        text = re.sub(r'(?<!\w)' + re.escape(wrong) + r'(?!\w)', right, text)
    # Untranslated stretches (three or more English words of the source left as they were)
    # make a mixed-language sentence: keep the English instead.
    src = set(w.lower() for w in re.findall(r"[A-Za-z']+", TERM_RE.sub(' ', en)))
    run = 0
    for w in re.findall(r"[^\s]+", TERM_RE.sub(' ', text)):
        core = re.sub(r"[^A-Za-z']", '', w).lower()
        run = run + 1 if core and core == re.sub(r"[^\w']", '', w).lower() and core in src else 0
        if run >= 3:
            return None
    words = text.split()
    if any(words[i] == words[i + 1] == words[i + 2] for i in range(len(words) - 2)):
        return None
    return text

def protect(text):
    kept = []
    def keep(m):
        kept.append(m.group(0)); return f' [{len(kept) - 1}] '
    out = TOKEN.sub(keep, text)
    out = TERM_RE.sub(keep, out)
    return re.sub(r'\s{2,}', ' ', out).strip(), kept

def restore(text, kept):
    found = re.findall(r'\[\s*(\d+)\s*\]', text)
    if sorted(int(n) for n in found) != list(range(len(kept))):
        return None
    out = re.sub(r'\[\s*(\d+)\s*\]', lambda m: '\0' + m.group(1) + '\0', text)
    for i, k in enumerate(kept):
        out = out.replace('\0' + str(i) + '\0', k)
    out = re.sub(r' +\n', '\n', re.sub(r'\n +', '\n', out))
    out = re.sub(r' +([.,;:!?)\]、。，])', r'\1', out)
    out = re.sub(r'([(\[]) +', r'\1', out)
    return re.sub(r'  +', ' ', out).strip()

def placeholders(s):
    return sorted(re.findall(r'%\d+|\{[^{}]*\}', s))

def main():
    catalog, lang, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    import argostranslate.package, argostranslate.translate
    target = ARGOS_CODE[lang]
    argostranslate.package.update_package_index()
    pkg = next(p for p in argostranslate.package.get_available_packages() if p.from_code == 'en' and p.to_code == target)
    argostranslate.package.install_from_path(pkg.download())
    translator = argostranslate.translate.get_translation_from_codes('en', target)
    entries = json.load(open(catalog, encoding='utf-8'))['entries']
    import pathlib
    hand_file = pathlib.Path(__file__).resolve().parent / 'hand' / 'en.txt'
    hand = {l.partition('|')[2] for l in hand_file.read_text(encoding='utf-8').splitlines() if l}
    result, fallback = {}, 0
    for e in entries:
        en = e.get('en') or ''
        if not en or en in hand:
            continue  # hand-written (scripts/i18n/hand/<lang>.txt): applied afterwards
        key = e['zh'] + '\u0004' + e.get('ctx', '')
        text, kept = protect(en)
        if not re.search(r'[A-Za-z]{2,}', text):
            result[key] = en; continue
        try:
            done = restore(translator.translate(text), kept)
        except Exception:
            done = None
        if done:
            done = postprocess(lang, en, done)
        if not done or placeholders(done) != placeholders(en):
            done, fallback = en, fallback + 1
        result[key] = done
    json.dump(result, open(out_path, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(f'{lang}: {len(result)} entries, {fallback} kept in English (tokens did not survive)')

if __name__ == '__main__':
    main()
