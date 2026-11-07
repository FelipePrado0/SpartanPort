#!/usr/bin/env bash
# Puts Intel's XeSS runtime (libxess.dll, the Intel Simplified Software License lets it be
# redistributed unmodified with its license) in out/xess, where package.sh takes it from. The
# headers the code is built with are in gpu/third_party/xess (MIT). A folder already there is kept.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
commit=de0fb9c1c510661c571164e1418ceca8101dab69
sha256=251659dd84a3e84de67c886a4186e01f3eca49b00641906fe38bb6b807e5d5b7
target=out/xess
if [[ -f $target/libxess.dll && -f $target/LICENSE.txt ]]; then
    echo "XeSS: $target is ready"
    exit 0
fi
work=out/intel-xess
rm -rf "$work"
git clone -q --filter=blob:none --no-checkout https://github.com/intel/xess "$work"
git -C "$work" sparse-checkout set --no-cone bin/libxess.dll LICENSE.txt third-party-programs.txt
git -C "$work" checkout -q "$commit"
[[ $(sha256sum "$work/bin/libxess.dll" | cut -d' ' -f1) == "$sha256" ]] || { echo 'XeSS: libxess.dll does not match the pinned release' >&2; exit 1; }
rm -rf "$target"
mkdir -p "$target"
cp "$work/bin/libxess.dll" "$work/LICENSE.txt" "$work/third-party-programs.txt" "$target/"
rm -rf "$work"
echo "XeSS: libxess.dll and its license in $target"
