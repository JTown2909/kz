#!/bin/bash
# Builds standalone gru binaries (Lua and zlib linked in statically) for the
# release package: a Windows gru.exe (needs mingw-w64) and a Linux gru.
#
# usage: release/build-gru.sh <outdir>
# result: <outdir>/win/gru.exe and <outdir>/linux/gru
set -e
out=$(mkdir -p "$1" && cd "$1" && pwd)
src="$out/src"
mkdir -p "$src"

[ -d "$src/n64" ]  || git clone -q --depth 1 https://github.com/glankk/n64.git "$src/n64"
[ -d "$src/lua" ]  || git -c advice.detachedHead=false clone -q --depth 1 -b v5.3.6 https://github.com/lua/lua.git "$src/lua"
[ -d "$src/zlib" ] || git -c advice.detachedHead=false clone -q --depth 1 -b v1.3.1 https://github.com/madler/zlib.git "$src/zlib"

build() { # <cc> <dir> <exe> [ldflags]
    local cc="$1" dir="$out/$2" exe="$3"; shift 3
    mkdir -p "$dir/cfg" "$dir/obj"
    cat > "$dir/cfg/config.h" <<CFG
#define HAVE_LUA_H 1
#define HAVE_ZLIB_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_DECL_ASPRINTF 1
CFG
    local f
    for f in $(ls "$src"/lua/*.c | grep -vE '/(lua|luac|ltests|onelua)\.c$') \
             "$src"/zlib/{adler32,compress,crc32,deflate,infback,inffast,inflate,inftrees,trees,uncompr,zutil}.c; do
        "$cc" -O2 -w -c "$f" -o "$dir/obj/$(basename "${f%.c}").o"
    done
    local g="$src/n64/src/libgru"
    "$cc" -std=gnu11 -O2 -w -I"$dir/cfg" -I"$g" -I"$src/n64/src" -I"$src/n64/include" -I"$src/lua" -I"$src/zlib" \
        "$g"/src/lib/*.c "$g"/src/exe/*.c "$src/n64/include/vector/vector.c" "$dir"/obj/*.o \
        -o "$dir/$exe" -lm "$@"
    echo "built $dir/$exe"
}

build x86_64-w64-mingw32-gcc win gru.exe -static
build gcc linux gru -static
