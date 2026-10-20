#!/bin/sh
# Patches a Majora's Mask Wii VC wad with kz.
# usage: ./build-wad.sh [wad]
# Without a wad argument, the first .wad in this folder is used.
# Uses the bundled Linux x86-64 gru and gzinject, or ones found in PATH
# (e.g. on macOS, build gru from https://github.com/glankk/n64 and gzinject
# from https://github.com/krimtonz/gzinject).
cd "$(dirname "$0")" || exit 1

wad="$1"
if [ -z "$wad" ]; then
    for f in *.wad; do
        [ -f "$f" ] || continue
        case "$f" in kz-*) continue ;; esac
        wad="$f"; break
    done
fi
if [ -z "$wad" ]; then
    echo "No wad found. Run: ./build-wad.sh /path/to/majoras-mask.wad"
    exit 1
fi
case "$wad" in /*) ;; *) [ -f "$wad" ] || wad="$OLDPWD/$wad" ;; esac

gru=gru; gzinject=gzinject
if [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ]; then
    gru=bin/gru; gzinject=bin/gzinject
else
    for t in gru gzinject; do
        command -v $t >/dev/null 2>&1 || { echo "$t not found, put it in your PATH."; exit 1; }
    done
fi

echo "Using wad: $wad"
rm -rf wadwork && mkdir wadwork
echo 45e | "$gzinject" -a genkey -k wadwork/common-key.bin >/dev/null
"$gru" lua/patchwad.lua "$gzinject" "$wad"
res=$?
rm -rf wadwork
exit $res
