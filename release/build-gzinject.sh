#!/bin/bash
# Builds standalone gzinject binaries for the release package: a static Linux
# x86-64 build and a Windows build (needs mingw-w64).
#
# usage: release/build-gzinject.sh <outdir>
set -e
out=$(mkdir -p "$1" && cd "$1" && pwd)
work=$(mktemp -d)
git clone -q https://github.com/krimtonz/gzinject "$work/gzinject"
cd "$work/gzinject"
gcc -O2 -s -Wno-unused-result -static -o "$out/gzinject" src/*.c
x86_64-w64-mingw32-gcc -O2 -s -Wno-unused-result -static -o "$out/gzinject.exe" src/*.c
rm -rf "$work"
echo "built $out/gzinject and $out/gzinject.exe"
