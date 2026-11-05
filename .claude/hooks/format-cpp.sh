#!/usr/bin/env bash
# PostToolUse(Edit|Write): format the touched C++ file the way build-scripts/format-cpp.sh does.
# astyle 3.1 matches upstream output; 3.6 does not.
ASTYLE=/c/Users/maksi/tools/astyle/v31/AStyle/bin/AStyle.exe
CLANG_FORMAT=/c/Users/maksi/AppData/Roaming/Python/Python310/Scripts/clang-format.exe

cd "$(dirname "$0")/../.." || exit 0
f="$(python -I -c 'import json,os,sys;p=json.load(sys.stdin).get("tool_input",{}).get("file_path","");print(os.path.relpath(p).replace(os.sep,"/") if p else "")' 2>/dev/null)"
[[ -f "$f" ]] || exit 0

case "$f" in
    src/lua/*|src/sol/*|src/third-party/*|tests/catch/*|tools/clang-tidy-plugin/test/*) exit 0 ;;
esac
case "$f" in
    *.cpp|*.h|*.hpp) ;;
    *) exit 0 ;;
esac

if [[ "$f" =~ ^src/[^/]+\.(cpp|h)$ ]]; then
    "$ASTYLE" --options=.astylerc -n -q "$f"
elif [[ "$f" == src/*/* || "$f" == tests/* || "$f" == tools/format/* || "$f" == tools/clang-tidy-plugin/* ]]; then
    "$CLANG_FORMAT" -i "$f"
fi
