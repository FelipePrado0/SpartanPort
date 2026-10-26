#!/usr/bin/env bash
set -euo pipefail
baseline=${1:?baseline commit required}
pr=${2:?pull request required}
[[ $baseline =~ ^[0-9a-f]{40}$ && $pr =~ ^[0-9]+$ ]]
mkdir -p out/validation
git rev-parse HEAD | tee out/validation/head.log

compilers=(clang)
extra=()
case $(uname -s) in
MINGW*|MSYS*)
    source ./msys2-env.sh
    extra=(src/compat_win.c -lbcrypt)
    ;;
*) compilers=(gcc clang) ;;
esac

# The standalone service test predates the runtime's GPU toast dependency.
printf '%s\n' '#include "gpu/spartanportgpu.h"' \
    'void spartanportgpu_toast_trophy(const char *a,const char *b,const unsigned char *c,uint64_t d) {(void)a;(void)b;(void)c;(void)d;}' \
    > out/validation/toast-stub.c
stub=()
if ! grep -q '^void spartanportgpu_toast_trophy(' tests/test_services.c; then
    stub=(out/validation/toast-stub.c)
fi

failed=0
for compiler in "${compilers[@]}"; do
    for name in savedata services; do
        log="out/validation/$compiler-$name.log"
        sources=("tests/test_$name.c")
        if [[ $name == services ]]; then sources+=("${stub[@]}"); fi
        if ! "$compiler" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc \
            "${sources[@]}" "${extra[@]}" -o "out/validation/$compiler-$name.exe" >"$log" 2>&1; then
            cat "$log"
            failed=1
            continue
        fi
        if timeout 60 "out/validation/$compiler-$name.exe" >>"$log" 2>&1; then
            printf 'PASS: %s %s\n' "$compiler" "$name"
        else
            cat "$log"
            failed=1
        fi
    done
done
exit "$failed"
