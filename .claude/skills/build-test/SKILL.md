---
name: build-test
description: Build the game and/or Catch2 tests with the fast Ninja+ccache+PCH path and run selected tests on this Windows machine.
disable-model-invocation: true
argument-hint: "[test names or tags, comma-separated]"
---

Build and test the local Cataclysm-BN tree. Arguments: `$ARGUMENTS` (test filter; empty = build only).

## Build

Run from the project root in **PowerShell** (Git Bash mangles `/m` and VsDevCmd paths):

```powershell
.\build-fast.bat cata_test-tiles
.\build-fast.bat              # game binary (cataclysm-bn-tiles); always build it too when validating code changes
```

- Output: `out\build\windows-ninja-msvc\{src,tests}\RelWithDebInfo\`.
- First build after a clean tree is ~14 min; ccache rebuild ~2 min; one .cpp edit ~30 s. Use `run_in_background` for cold builds.
- On compile errors: fix and rebuild; do not switch to the VS build dir.

## Test

```powershell
$env:CATA_TEST_COMPUTE_ACCELERATION='cpu'; .\out\build\windows-ninja-msvc\tests\RelWithDebInfo\cata_test-tiles.exe "<filter>"
```

- Run from the project root (tests read `data/`).
- `CATA_TEST_COMPUTE_ACCELERATION=cpu` is required: the default gpu_software backend fails here.
- Several test names go as ONE comma-separated argument: `"test_a,test_b"`. Tags: `"[vehicle]"`.
- Do not run the whole suite unprompted: it exhausts RAM on this machine. Run the tests related to the change.
- Known noise: `ranged_balance` unskilled_shooter_accuracy is `[!mayfail]`; explosion_balance EMP / start_location_fire fail only in full-suite order. Re-run a failing test alone before calling it a regression.

## Report

State: build result, tests run, pass/fail counts, and the failing assertion output verbatim if any.
