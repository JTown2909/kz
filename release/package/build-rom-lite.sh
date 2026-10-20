#!/bin/sh
# Patches a Majora's Mask rom with kz-lite.
# usage: ./build-rom-lite.sh [rom]
# Without a rom argument, the first .z64/.n64/.v64 in this folder is used.
# Uses the bundled Linux x86-64 gru, or a gru found in PATH (e.g. on macOS,
# build gru from https://github.com/glankk/n64).
cd "$(dirname "$0")" || exit 1

rom="$1"
if [ -z "$rom" ]; then
    for f in *.z64 *.n64 *.v64; do
        [ -f "$f" ] || continue
        case "$f" in kz-lite*) continue ;; esac
        rom="$f"; break
    done
fi
if [ -z "$rom" ]; then
    echo "No rom found. Run: ./build-rom-lite.sh /path/to/rom.z64"
    exit 1
fi
case "$rom" in /*) ;; *) [ -f "$rom" ] || rom="$OLDPWD/$rom" ;; esac

gru=gru
if [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ]; then
    gru=bin/gru
elif ! command -v gru >/dev/null 2>&1; then
    echo "gru not found. Build it from https://github.com/glankk/n64 and put it in your PATH."
    exit 1
fi

echo "Using rom: $rom"
"$gru" lua/patch.lua "$rom"
