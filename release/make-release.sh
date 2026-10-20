#!/bin/bash
# Builds the release zip: patches Wii VC wads (kz-vc) and N64 roms (kz-lite)
# for NTSC-U 1.0, NTSC-J 1.0 and NTSC-J 1.1, without needing any toolchain.
#
# usage: release/make-release.sh <tools dir> [version]
#
# <tools dir> holds gru, gru.exe, gzinject and gzinject.exe (see
# build-gru.sh and build-gzinject.sh). Needs glankk's n64 toolchain and a
# powerpc-eabi toolchain in PATH to build kz and homeboy.
set -e
here=$(cd "$(dirname "$0")" && pwd)
tools=$(cd "$1" && pwd); version="${2:-4}"
for t in gru gru.exe gzinject gzinject.exe; do
    [ -f "$tools/$t" ] || { echo "missing $tools/$t"; exit 1; }
done

name="kz-savestates-v$version"
work=$(mktemp -d)
pkg="$work/$name"

# always from clean, so a stale object can't end up in a release
cd "$here/.."
export PACKAGE_VERSION="savestates-v$version"
rm -rf obj bin patch/dol/vc homeboy/obj homeboy/bin
for v in NZSE NZSJ NZSJ10; do
    for m in lite vc; do
        make "kz-$m-$v" "patch/gsc/kz-$m-$v/hooks.gsc"
    done
done
for n in NARE NARJ; do
    make "patch/dol/vc/kz-vc-$n/kz-vc.gzi"
done

cp -r "$here/package" "$pkg"
cp LICENSE "$pkg/LICENSE.txt"
mkdir -p "$pkg/bin" "$pkg/data/gzi"
cp "$tools/gru" "$tools/gru.exe" "$tools/gzinject" "$tools/gzinject.exe" "$pkg/bin/"
chmod +x "$pkg/bin/gru" "$pkg/bin/gzinject" "$pkg"/*.sh
for v in NZSE NZSJ NZSJ10; do
    for m in lite vc; do
        d="$pkg/data/kz-$m-$v"
        mkdir -p "$d"
        cp "bin/kz-$m-$v/kz.bin" "patch/gsc/kz-$m-$v/hooks.gsc" "patch/gsc/kz-$m-$v/ups_size_patch.gsc" "$d/"
    done
done
cp patch/gsc/zero_patch_lite.gsc patch/gsc/zero_patch_vc.gsc "$pkg/data/"
for n in NARE NARJ; do
    d="$pkg/data/vc/kz-vc-$n"
    mkdir -p "$d"
    cp "patch/dol/vc/kz-vc-$n/kz-vc.bin" "patch/dol/vc/kz-vc-$n/kz-vc.gzi" "patch/dol/vc/kz-vc-$n/homeboy.bin" "$d/"
done
cp patch/gzi/NKZE.gzi patch/gzi/NKZE-remap.gzi patch/gzi/NKZJ.gzi patch/gzi/NKZJ-remap.gzi patch/gzi/compress.gzi "$pkg/data/gzi/"

mkdir -p "$here/dist"
rm -f "$here/dist/$name.zip"
(cd "$work" && zip -qrX "$here/dist/$name.zip" "$name")
rm -rf "$work"
echo "built release/dist/$name.zip"
