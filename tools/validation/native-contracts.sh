#!/usr/bin/env bash
set -euo pipefail
baseline=${1:?baseline commit required}
pr=${2:?pull request required}
[[ $baseline =~ ^[0-9a-f]{40}$ && $pr =~ ^[0-9]+$ ]]
mkdir -p out/validation
git rev-parse HEAD | tee out/validation/head.log
printf 'baseline=%s\npr=%s\n' "$baseline" "$pr" | tee out/validation/scope.log
ulimit -c 0 || true

compilers=(clang)
extra=()
case $(uname -s) in
MINGW*|MSYS*)
    source ./msys2-env.sh
    extra=(src/compat_win.c -lbcrypt -lws2_32)
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

if [[ $pr == 1070 ]]; then
    for compiler in "${compilers[@]}"; do
        log="out/validation/$compiler-trophy-process.log"
        # Keep this process fixture separate from the optimized contracts above: its fixed-size
        # mock directory is not the production runtime_file_user_dir implementation.
        if "$compiler" -std=c11 -O0 -g -Wall -Wextra -Werror -pthread -I. -Isrc \
            tools/validation/trophy-process.c "${extra[@]}" \
            -o "out/validation/$compiler-trophy-process.exe" >"$log" 2>&1 && \
            python3 tools/validation/trophy-process.py "out/validation/$compiler-trophy-process.exe" >>"$log" 2>&1; then
            cat "$log"
        else
            cat "$log"
            failed=1
        fi
    done
fi

if [[ $(uname -s) == Linux ]]; then
    for name in savedata services; do
        sources=("tests/test_$name.c")
        if [[ $name == services ]]; then sources+=("${stub[@]}"); fi
        log="out/validation/sanitized-$name.log"
        if clang -std=c11 -O1 -g -Wall -Wextra -Werror -pthread -I. -Isrc \
            -fsanitize=address,undefined -fno-omit-frame-pointer "${sources[@]}" \
            -o "out/validation/sanitized-$name" >"$log" 2>&1 && \
            timeout 60 "out/validation/sanitized-$name" >>"$log" 2>&1; then
            printf 'PASS: ASan/UBSan %s\n' "$name"
        else
            cat "$log"
            failed=1
        fi
    done

    if [[ $pr == 1009 ]]; then
        validation_root=$PWD
        base_dir="${RUNNER_TEMP:?}/pr1009-baseline"
        git worktree add --detach "$base_dir" "$baseline"
        log="$validation_root/out/validation/baseline-services.log"
        (
            cd "$base_dir"
            gcc -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc \
                tests/test_services.c "$validation_root/out/validation/toast-stub.c" \
                -o "$validation_root/out/validation/baseline-services"
            timeout 60 "$validation_root/out/validation/baseline-services"
        ) >"$log" 2>&1
        cp tests/test_services.c "$base_dir/tests/test_services.c"
        log="$validation_root/out/validation/regression-without-pr.log"
        (
            cd "$base_dir"
            gcc -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc \
                tests/test_services.c \
                -o "$validation_root/out/validation/regression-without-pr"
        ) >"$log" 2>&1
        if timeout 60 out/validation/regression-without-pr >>"$log" 2>&1; then
            echo 'FAIL: ordering regression unexpectedly passes without PR #1009'
            failed=1
        elif grep -q 'shown_items\[0\].*SAVE0002' "$log"; then
            echo 'PASS: original main passes; ordering regression fails without PR #1009'
        else
            cat "$log"
            failed=1
        fi
    fi
fi
if [[ $pr == 1007 ]]; then
    if ! bash tools/validation/menu-coherence.sh; then failed=1; fi
    original="${RUNNER_TEMP:?}/pr1007-original"
    git worktree add --detach "$original" 75078c918964a94b1d2ec1d37909e72ada388265
    mkdir -p "$original/tools/validation"
    cp tools/validation/menu-{runtime.c,coherence.cpp,coherence.sh} "$original/tools/validation/"
    if ! (cd "$original" && bash tools/validation/menu-coherence.sh); then failed=1; fi
    for log in "$original"/out/validation/*-menu-coherence.log; do
        cp "$log" "out/validation/original-$(basename "$log")"
    done
fi
exit "$failed"
