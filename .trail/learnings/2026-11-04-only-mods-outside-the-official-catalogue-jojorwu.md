---
title: "Only mods outside the official catalogue need their own lang folder, and Lua text must be wrapped in gettext to be translatable"
author: Jojorwu
date: 2026-11-04
branch: fix/order-dependent-test-failures
cost: { minutes: 19, tokens: 118036 }
id: l-926ba06d
scope: repo
status: active
src: agent
touches:
  - data/mods/sandevistan/README.md
  - tools/check_po_printf_format.py
  - src/language.cpp
  - lang/extract_json_strings.py
---

## Decided
In-tree mods under data/mods that upstream ships are translated through the main cataclysm-bn.mo (their strings are extracted into the main POT). A mod added only in this fork (sandevistan) is not in that catalogue, so it carries its own lang/<lang>.po + .mo, loaded from <mod>/lang/<id>.mo with fallback from ru_RU to ru.
Each catalogue keeps its own Plural-Forms, so a mod's 3-form ru does not clash with the core 4-form ru.

## Watch out
Lua strings passed straight to gapi.add_msg or sidebar widgets are never extracted; wrap them in locale.gettext(...) first, then re-extract.
tools/check_po_printf_format.py ignores arguments and only scans lang/po; point it at a mod's lang folder by editing the path in a scratch copy.

In the code: src/language.cpp add_mod_catalogue_if_exists (mod_path + "/lang/"), lang/extract_json_strings.py (needs pip polib + luaparser), data/mods/sandevistan/lang/.
