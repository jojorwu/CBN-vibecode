#!/usr/bin/env bash
# PreToolUse(Edit|Write): block edits to build output and release-copied translations.
cd "$(dirname "$0")/../.." || exit 0
f="$(python -I -c 'import json,os,sys;p=json.load(sys.stdin).get("tool_input",{}).get("file_path","");print(os.path.relpath(p).replace(os.sep,"/") if p else "")' 2>/dev/null)"
case "$f" in
    out/*|lang/mo/*)
        echo "Blocked: $f is generated (out/ = build output, lang/mo/ = .mo copied from release). Edit sources instead." >&2
        exit 2 ;;
esac
exit 0
