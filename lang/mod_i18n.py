#!/usr/bin/env python3
"""Translate a mod in one command: extract strings, update PO files, compile MO files.

Usage:
    python lang/mod_i18n.py <mod_dir> [extract|update|compile|check|all] [--langs ru,de,...]

Files are kept in <mod_dir>/lang/: extracted_strings.pot, <lang>.po and <lang>.mo.
Without --langs, update/compile work on the PO files that already exist.
Needs the `polib` and `luaparser` Python modules; GNU gettext is not required.
"""
import argparse
import os
import re
import subprocess
import sys

try:
    import polib
except ImportError:
    sys.exit("You need the 'polib' module: pip install polib luaparser")

LANG_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(LANG_DIR)
POT_NAME = "extracted_strings.pot"

# gettext plural rules, keyed by language code without the dialect.
PLURAL_FORMS = {
    "en": "nplurals=2; plural=(n != 1);",
    "de": "nplurals=2; plural=(n != 1);",
    "es": "nplurals=2; plural=(n != 1);",
    "it": "nplurals=2; plural=(n != 1);",
    "nl": "nplurals=2; plural=(n != 1);",
    "hu": "nplurals=2; plural=(n != 1);",
    "da": "nplurals=2; plural=(n != 1);",
    "nb": "nplurals=2; plural=(n != 1);",
    "el": "nplurals=2; plural=(n != 1);",
    "fr": "nplurals=2; plural=(n > 1);",
    "pt": "nplurals=2; plural=(n > 1);",
    "tr": "nplurals=2; plural=(n != 1);",
    "is": "nplurals=2; plural=(n%10!=1 || n%100==11);",
    "ru": "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);",
    "uk": "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);",
    "sr": "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);",
    "pl": "nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);",
    "sk": "nplurals=3; plural=(n==1) ? 0 : (n>=2 && n<=4) ? 1 : 2;",
    "ar": "nplurals=6; plural=(n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n%100>=3 && n%100<=10 ? 3 : n%100>=11 ? 4 : 5);",
    "ja": "nplurals=1; plural=0;",
    "ko": "nplurals=1; plural=0;",
    "zh": "nplurals=1; plural=0;",
    "id": "nplurals=1; plural=0;",
}

# shortcut: regex, not a Lua parser; catches the common add_msg(type, "text") case only.
UNWRAPPED_LUA_MSG = re.compile(r'add_msg\s*\(\s*[^,()]+,\s*["\']')


def plural_forms(lang):
    forms = PLURAL_FORMS.get(lang.split("_")[0])
    if forms is None:
        sys.exit(f"No plural rule known for '{lang}'; add it to PLURAL_FORMS in {__file__}")
    return forms


def nplurals(forms):
    return int(re.search(r"nplurals\s*=\s*(\d+)", forms).group(1))


def lang_dir(mod):
    return os.path.join(mod, "lang")


def existing_langs(mod):
    d = lang_dir(mod)
    if not os.path.isdir(d):
        return []
    return sorted(f[:-3] for f in os.listdir(d) if f.endswith(".po"))


def warn_unwrapped_lua(mod):
    for root, _, files in os.walk(mod):
        for name in files:
            if not name.endswith(".lua"):
                continue
            path = os.path.join(root, name)
            with open(path, encoding="utf-8") as f:
                for n, line in enumerate(f, 1):
                    if UNWRAPPED_LUA_MSG.search(line) and not line.lstrip().startswith("--"):
                        print(f"WARNING: {os.path.relpath(path, mod)}:{n}: message is not wrapped "
                              "in locale.gettext(), it will not be translated")


def extract(mod):
    os.makedirs(lang_dir(mod), exist_ok=True)
    pot = os.path.join(lang_dir(mod), POT_NAME)
    subprocess.run([sys.executable, os.path.join(LANG_DIR, "extract_json_strings.py"),
                    "-i", mod, "-E", lang_dir(mod), "-o", pot], cwd=LANG_DIR, check=True)
    subprocess.run([sys.executable, os.path.join(LANG_DIR, "dedup_pot_file.py"), pot],
                   cwd=LANG_DIR, check=True)
    # Source references relative to the mod, so the POT doesn't leak local paths.
    po = polib.pofile(pot)
    for e in po:
        e.occurrences = [(os.path.relpath(f, mod).replace("\\", "/"), l) for f, l in e.occurrences]
    po.save(pot)
    warn_unwrapped_lua(mod)
    print(f"{len(po)} strings -> {pot}")


def new_po(pot, lang):
    forms = plural_forms(lang)
    po = polib.POFile()
    po.metadata = {
        "Project-Id-Version": pot.metadata.get("Project-Id-Version", ""),
        "Language": lang,
        "MIME-Version": "1.0",
        "Content-Type": "text/plain; charset=UTF-8",
        "Content-Transfer-Encoding": "8bit",
        "Plural-Forms": forms,
    }
    return po


def update(mod, langs):
    pot_path = os.path.join(lang_dir(mod), POT_NAME)
    if not os.path.exists(pot_path):
        sys.exit(f"{pot_path} not found, run 'extract' first")
    pot = polib.pofile(pot_path)
    for lang in langs:
        path = os.path.join(lang_dir(mod), f"{lang}.po")
        po = polib.pofile(path) if os.path.exists(path) else new_po(pot, lang)
        po.merge(pot)
        n = nplurals(po.metadata.get("Plural-Forms") or plural_forms(lang))
        for e in po:
            if e.msgid_plural and len(e.msgstr_plural) != n:
                e.msgstr_plural = {i: e.msgstr_plural.get(i, "") for i in range(n)}
        po.save(path)
        print(f"{lang}: {po.percent_translated()}% translated, "
              f"{len(po.untranslated_entries())} untranslated -> {path}")


def compile_mo(mod, langs):
    for lang in langs:
        po = polib.pofile(os.path.join(lang_dir(mod), f"{lang}.po"))
        mo = os.path.join(lang_dir(mod), f"{lang}.mo")
        po.save_as_mofile(mo)
        print(f"{lang}: {len(po.translated_entries())} strings -> {mo}")


def check(mod, langs):
    paths = [os.path.join(lang_dir(mod), f"{lang}.po") for lang in langs]
    return subprocess.run([sys.executable, os.path.join(REPO_DIR, "tools", "check_po_printf_format.py"),
                           *paths]).returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mod", help="path to the mod folder (the one with modinfo.json)")
    ap.add_argument("step", nargs="?", default="all", choices=["extract", "update", "compile", "check", "all"])
    ap.add_argument("--langs", default="", help="comma-separated language ids, e.g. ru,de,zh_CN")
    args = ap.parse_args()

    mod = os.path.abspath(args.mod)
    if not os.path.exists(os.path.join(mod, "modinfo.json")):
        sys.exit(f"{mod} has no modinfo.json")
    langs = [l for l in args.langs.split(",") if l] or existing_langs(mod)

    if args.step in ("extract", "all"):
        extract(mod)
    if args.step in ("update", "all"):
        update(mod, langs)
    if args.step in ("compile", "all"):
        compile_mo(mod, langs)
    if args.step in ("check", "all") and langs:
        sys.exit(check(mod, langs))


if __name__ == "__main__":
    main()
