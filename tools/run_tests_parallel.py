#!/usr/bin/env python3
"""Run cata_test in N parallel shards (Catch2 v2 has no --shard-count).

Each shard is a separate cata_test process with its own --user-dir, so worlds
and saves never collide. Test names are split across shards using timings
from the previous run (longest-processing-time-first), so shards finish at
roughly the same moment. The first run falls back to round-robin.

Usage:
  python tools/run_tests_parallel.py [--exe PATH] [-j N] [filter ...] [-- extra cata_test args]

Examples:
  python tools/run_tests_parallel.py                       # all tests, j = CPU count
  python tools/run_tests_parallel.py "[bionics]" -j 6      # only [bionics]
  python tools/run_tests_parallel.py -- --rng-seed 42      # pass args through

Run it from the repository root (cata_test needs data/ next to the cwd).
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

TIMINGS_FILE = Path(".cata_test_times.json")
DURATION_RE = re.compile(r"^\s*([0-9.]+) s: (.+?)\s*$")
CANDIDATES = [
    "out/build/windows-ninja-msvc/tests/RelWithDebInfo/cata_test-tiles.exe",
    "out/build/windows-ninja-msvc/tests/Release/cata_test-tiles.exe",
    "out/build/windows-tiles-sounds-x64-msvc/tests/RelWithDebInfo/cata_test-tiles.exe",
    "out/build/linux-dev/tests/cata_test-tiles",
    "out/build/linux-full/tests/cata_test-tiles",
    "out/build/linux-slim/tests/cata_test-tiles",
    "out/build/linux-curses/tests/cata_test",
    "cata_test-tiles",
    "cata_test",
]


def find_exe() -> str:
    found = [p for c in CANDIDATES for p in glob.glob(c) if os.path.isfile(p)]
    if not found:
        sys.exit("cata_test executable not found; pass --exe PATH")
    return max(found, key=os.path.getmtime)  # newest build wins


def escape_name(name: str) -> str:
    """Quote a test name for a Catch2 v2 -f file so it matches literally.

    Inside a quoted name only backslash, comma and double quote are special
    (TestSpecParser::isControlChar / visitChar); quoting ourselves also stops
    Catch from treating a name that starts with '#' as a comment.
    """
    return '"' + re.sub(r'([\\,"])', r"\\\1", name) + '"'


def list_tests(exe: str, filters: list[str]) -> list[str]:
    # An exclusion-only spec such as "~[slow]" also matches hidden [.] tests in
    # Catch2 v2; keep hidden tests out unless the caller explicitly asks for them.
    if filters and not any("[." in f for f in filters):
        filters = [*filters, "~[.]"] if all(f.lstrip().startswith("~") for f in filters) else filters
    out = subprocess.run(
        [exe, "--list-test-names-only", *filters],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
    ).stdout
    names = []
    for line in out.splitlines():
        line = line.rstrip("\r")
        if not line.strip():
            continue
        if line.startswith('"#') and line.endswith('"'):  # Catch quotes names starting with '#'
            line = line[1:-1]
        names.append(line)
    return names


def make_shards(names: list[str], jobs: int, timings: dict[str, float]) -> list[list[str]]:
    default = (sum(timings.values()) / len(timings)) if timings else 1.0
    ordered = sorted(names, key=lambda n: timings.get(n, default), reverse=True)
    shards: list[list[str]] = [[] for _ in range(jobs)]
    loads = [0.0] * jobs
    for name in ordered:  # LPT: give the next-longest test to the least-loaded shard
        i = loads.index(min(loads))
        shards[i].append(name)
        loads[i] += timings.get(name, default)
    return [s for s in shards if s]


def run_shard(idx: int, exe: str, names: list[str], workdir: Path, extra: list[str]):
    spec = workdir / f"shard{idx}.txt"
    spec.write_text("\n".join(escape_name(n) for n in names) + "\n", encoding="utf-8")
    user_dir = workdir / f"user{idx}"
    cmd = [exe, "-f", str(spec), f"--user-dir={user_dir.as_posix()}", "-D",
           "--durations", "yes", "--rng-seed", "time", *extra]
    # Same as CI (build-scripts/run-linux-test-shards.ts): N shards each spinning up a
    # software GPU device is slow; the CPU compute path is enough for tests on Linux.
    # Windows keeps the default gpu_software backend: CPU compute gives wrong
    # lighting/visibility in the vision tests there.
    env = {**os.environ}
    if os.name != "nt":
        env.setdefault("CATA_TEST_COMPUTE_ACCELERATION", "cpu")
    start = time.monotonic()
    proc = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", env=env)
    return idx, proc.returncode, proc.stdout + proc.stderr, time.monotonic() - start


def main() -> int:
    # Test output is UTF-8; a cp1251 console would raise UnicodeEncodeError while
    # printing failure details, losing the shard result.
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8", errors="replace")
    argv = sys.argv[1:]
    extra: list[str] = []
    if "--" in argv:
        cut = argv.index("--")
        argv, extra = argv[:cut], argv[cut + 1:]
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe")
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--keep", action="store_true", help="keep shard user dirs for debugging")
    ap.add_argument("filters", nargs="*", help="Catch2 test spec, e.g. \"[bionics]\"")
    args = ap.parse_args(argv)

    exe = args.exe or find_exe()
    names = list_tests(exe, args.filters)
    if not names:
        sys.exit("no tests matched")
    timings = json.loads(TIMINGS_FILE.read_text()) if TIMINGS_FILE.exists() else {}
    shards = make_shards(names, max(1, args.jobs), timings)
    print(f"{exe}\n{len(names)} test cases -> {len(shards)} shards", flush=True)

    workdir = Path(tempfile.mkdtemp(prefix="cata_test_"))
    failed = []
    wall = time.monotonic()
    try:
        with ThreadPoolExecutor(len(shards)) as pool:
            futures = [pool.submit(run_shard, i, exe, s, workdir, extra) for i, s in enumerate(shards)]
            for fut in futures:
                idx, code, output, secs = fut.result()
                for line in output.splitlines():
                    m = DURATION_RE.match(line)
                    if m and m.group(2) in names:  # only whole test cases, not sections
                        timings[m.group(2)] = float(m.group(1))
                status = "ok" if code == 0 else f"FAILED ({code})"
                print(f"[shard {idx}] {len(shards[idx])} tests, {secs:.1f}s: {status}", flush=True)
                if code != 0:
                    failed.append(idx)
                    tail = [l for l in output.splitlines() if not DURATION_RE.match(l)]
                    print("\n".join(tail[-200:]))
    finally:
        if not args.keep:
            shutil.rmtree(workdir, ignore_errors=True)
    TIMINGS_FILE.write_text(json.dumps(timings, indent=0, sort_keys=True))
    print(f"wall time {time.monotonic() - wall:.1f}s; " + ("all shards passed" if not failed else f"failed shards: {failed}"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
