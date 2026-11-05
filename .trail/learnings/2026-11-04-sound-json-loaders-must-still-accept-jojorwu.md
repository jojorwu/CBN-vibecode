---
title: "Sound JSON loaders must still accept their fields when audio is off, or mod loading fails CI"
author: Jojorwu
date: 2026-11-04
branch: fix/order-dependent-test-failures
cost: { minutes: 2, tokens: 1115 }
id: l-9f475e29
scope: repo
status: active
src: agent
touches:
  - src/sdlsound.cpp
  - src/sounds.cpp
---

## Decided
When sound is unavailable (no audio device, or a build without sound), the sound_effect / sound_effect_preload / playlist loaders return early but first mark the JSON fields as accepted. Skipping loading is required: otherwise preload calls the audio loader with no mixer and pops one error dialog per preloaded sound.

## Tried and ruled out
- Removing the "sound initialised" early return entirely: brings back the preload error dialogs when audio init fails.
- Keeping a bare early return (upstream style): any mod that ships sounds.json reports "Invalid or misplaced field" for every field. CI's all-mods init check ('~*' with every mod loaded) treats that as failure, so every PR goes red on the curses job even though all test shards pass.

## Watch out
- A red "clang, curses" job with all shards at status 0 means the final '~*' all-mods run logged an error. Look for "Treating result as failure due to error logged during initialization" and the JSON errors just before it.
- Mod sound effects are never cleared when the world changes, so a disabled mod's sounds keep playing until the game restarts. This was already true before this work.

In the code: src/sdlsound.cpp sfx::load_sound_effects/load_sound_effect_preload/load_playlist (jsobj.allow_omitted_members() before return); src/sounds.cpp no-sound stubs do the same. Local repro: cata_test-tiles.exe '~*' --user-dir=<tmp> --mods=sandevistan.
