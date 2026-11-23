#!/usr/bin/env bash
# Puts the open FSR 4 v07 INT8 model files (shaders and weights, MIT; built by the Q2RTX project
# from the FSR 4 source AMD published) in out/fsr4_shaders, where package.sh and
# packaging/linux/build.sh take them from. The same files and revision IFreemz's shadPS4 Bloodborne
# FSR uses. A folder already there is kept.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
commit=ae8d628fae208813172446d1e49ed94150b04658
target=out/fsr4_shaders
if [[ -f $target/LICENSE-FSR4-v07.txt ]]; then
    echo "FSR 4 model: $target is ready"
    exit 0
fi
work=out/q2rtx-fsr4
rm -rf "$work"
git clone -q --filter=blob:none --no-checkout https://github.com/FireBurn/Q2RTX "$work"
git -C "$work" sparse-checkout set --no-cone baseq2/fsr4_shaders
git -C "$work" checkout -q "$commit"
rm -rf "$target"
cp -r "$work/baseq2/fsr4_shaders" "$target"
rm -rf "$work"
echo "FSR 4 model: $(ls "$target" | wc -l) files in $target"
