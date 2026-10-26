#!/usr/bin/env bash
set -euo pipefail
mkdir -p out/validation
extra=()
compilers=(gcc clang)
case $(uname -s) in
MINGW*|MSYS*)
    source ./msys2-env.sh
    compilers=(clang)
    clang -std=c11 -O2 -ffunction-sections -fdata-sections -Isrc -c src/compat_win.c -o out/validation/menu-compat.o
    extra=(out/validation/menu-compat.o -lbcrypt -lws2_32)
    ;;
esac
failed=0
for compiler in "${compilers[@]}"; do
    case $compiler in gcc) cxx=g++ ;; clang) cxx=clang++ ;; esac
    log="out/validation/$compiler-menu-coherence.log"
    flags=(-O2 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -pthread -UNDEBUG)
    if "$compiler" -std=c11 "${flags[@]}" -I. -Isrc -c tools/validation/menu-runtime.c \
        -o "out/validation/$compiler-menu-runtime.o" >"$log" 2>&1 && \
        "$cxx" -std=gnu++23 "${flags[@]}" -I. -Igpu/shim \
        tools/validation/menu-coherence.cpp gpu/shim/spartanport_settings.cpp \
        "out/validation/$compiler-menu-runtime.o" "${extra[@]}" -Wl,--gc-sections \
        -o "out/validation/$compiler-menu-coherence.exe" >>"$log" 2>&1 && \
        timeout 30 "out/validation/$compiler-menu-coherence.exe" \
        "$PWD/out/validation/missing-menu-config.ini" >>"$log" 2>&1; then
        cat "$log"
    else
        cat "$log"
        failed=1
    fi
done
exit "$failed"
