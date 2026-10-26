#!/usr/bin/env bash
set -euo pipefail
mkdir -p out/validation
test -s out/gpu/libspartanportgpu.so
python3 - <<'PY'
from pathlib import Path

source = Path('build.sh').read_text()
marker = '# $ORIGIN/gpu: packaged copies keep the library next to the binary without patching it.\n'
assert source.count(marker) == 1
prefix = '''#!/usr/bin/env bash
set -euo pipefail
CC=clang
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3 libavcodec libavutil libswscale)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3 libavcodec libavutil libswscale)"
'''
Path('out/validation/production-runtime-build.sh').write_text(prefix + source.split(marker, 1)[1])
PY
bash out/validation/production-runtime-build.sh
g++ -std=c++20 -O2 -UNDEBUG -Igpu/shim tests/test_graphics_settings.cpp \
    gpu/shim/spartanport_settings.cpp -o out/validation/graphics-settings-test
timeout --kill-after=5s 60 out/validation/graphics-settings-test
export XDG_DATA_HOME="$RUNNER_TEMP/spartanport-xdg"
mkdir -p "$XDG_DATA_HOME"
timeout --kill-after=5s 30 out/spartanport-probe --cpu-features
