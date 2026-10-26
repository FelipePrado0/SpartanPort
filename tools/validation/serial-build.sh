#!/usr/bin/env bash
# Direct Ninja calls do not honor CMAKE_BUILD_PARALLEL_LEVEL.
ninja() { command ninja -j1 "$@"; }
make() { command make -j1 "$@"; }
export -f ninja make
export CMAKE_BUILD_PARALLEL_LEVEL=1 CARGO_BUILD_JOBS=1 MAKEFLAGS=-j1 RAYON_NUM_THREADS=1
