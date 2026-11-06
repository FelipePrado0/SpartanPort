# Temporal upscaling and frame generation: historical research

> **Status, October 14, 2026:** this document preserves experiments and plans from earlier
> development. FSR 1 and temporal upscalers (FSR 3.1, FSR 4, TAA and DLSS) are disabled in the
> current launcher and game settings path, including release 2.2.0. RCAS sharpening remains
> available. Descriptions below of live selectors, experimental switches, assets and validation
> runs are historical, not instructions for enabling these features in the current release.
> The frame map remains useful for research. For current player instructions,
> see the [README](../README.md) and [release notes](https://github.com/FelipePrado0/SpartanPort/releases/latest).

## Native TAA and missing FSR 4 assets (2026-10-01)

`upscaler=taa` / `SPARTAN_UPSCALER=taa` is a separate native-resolution temporal AA
mode, available live in the launcher and overlay. It uses the existing jitter and
camera/object motion vectors, reprojection, depth rejection and 3×3 neighborhood
clipping in one compute pass with two ping-pong RGBA16F histories. History stores
depth in alpha; the scene's original alpha is preserved by the HDR merge. HUD is
composed afterwards and never enters history. Output changes and AA/provider
changes reset history. TAA uses Native AA dimensions while preserving the saved
FSR preset, and does not allocate an FSR context or execute an FSR model/RCAS.
It still adds GPU work compared with disabling temporal AA; native 4K raster cost
also remains. The explicit `SPARTAN_RENDER_RES` compatibility path must be unset.

FSR Native AA is not a fast pass-through: it renders at the output resolution and
also executes the temporal reconstruction (including the model for FSR 4) and
optional sharpening. It can therefore be slower than no AA at the same output.

FSR 4.1.1 assets have separate 1080p/2160p tiers and standard/Ultra models:
`t1080_m0`, `t1080_m1`, `t2160_m0`, `t2160_m1`. Outputs above 1080p need the 2160
tier, irrespective of the preset. A missing `t2160_m0/spd.spv` means missing
assets, not a GPU feature failure. These assets are not bundled: build the full
set with `tools/fsr4cap/build_assets.sh` and put it in `SPARTAN_FSR411_DIR` or the
packaged data directory's `fsr4_411`. Fatal asset failures now select FSR 3.1 in
the live settings, so the menu shows the active provider, retains the error, and
allows retrying FSR 4 after changing the output/installing assets.

`taa-shader-test` checks real SPIR-V output for accumulation, clipping, depth
disocclusion, invalid history and out-of-frame motion on Vulkan (Lavapipe works).
`out/taa-appimage-build.log` records these checks, settings round trips and 64
Python tests. `out/taa-live-validation.log` and `out/taa-appimage-validation.log`
check TAA at 1080p/720p/1440p/4K, TAA ↔ FSR 3, and a missing 2160-tier FSR 4.1.1
model in one gameplay process, including output captures and camera movement.
`out/taa-launcher-validation.log` checks actual GTK controls and saved settings.
TAA has not yet been tested on the tester's GTX 1060.

## FSR 3.1 with native HUD and text (2026-11-03)

Two FSR 3.1 choices. **FSR 3.1** is upstream's: with the Native AA preset the launcher patches the
game to 1918x1078, so the HUD, text and loading screens are drawn at that size and stretched to
1920x1080. **FSR 3.1 (native HUD and text)** (`upscaler=fsr3ui`) does not patch the render size: the
game keeps its own 1920x1080 targets (the live path, as the TAA does) and only the scene is upscaled.
Only this variant and the TAA use the port's own sharpening (the Sharpening (RCAS) switch and
strength, and the extra pass above FSR's maximum); FSR 3.1, FSR 4 and FSR 4.1 keep upstream's
(`sharpen`, `sharpness`). Switching between a patched and a live-path upscaler needs a restart.
The Insert menu (Image tab) has an "FSR 3.1 look" section for it: Texture detail (`fsr_lod`, a negative
mip bias in percent of a level, default 0) and Moving objects (`fsr_objects`, the strength of the
stencil reactive mask on characters and objects, default 100 = upstream's), both live.

Trade-off: upstream's FSR 3.1 normally runs on the scene colour before the game's composite compute
shader. At exactly 1920x1080 the game composites straight into the display buffer (that is why the
launcher patches Native AA to 1918x1078), so there is no such hook; the native variant therefore runs
after the composite pixel shader, on the finished 8-bit frame and ahead of the HUD
("FSR 3.1 running on the RGBA8 scene color ... (SpartanPort composite)" in `last_run.log`). That keeps the
HUD, text and loading screens out of FSR, at the cost of a slightly lower scene quality than the patched
variant (tonemapped 8-bit input instead of the earlier scene colour). The two are separate choices so
each can be kept, changed or merged by the maintainer; upstream's FSR 3.1 is not changed.

## Startup patch is the default again for outputs other than 1080p (2026-10-02)

The live path below made the Steam Deck and a GTX 1060 + 4-core Haswell drop to 7–8 FPS
(release 0.1 AppImage: 40–50 FPS on the Deck). With guest allocations at 1920×1080 the game's
post-processing stays at 1080p even for 720p output, guest compute passes resolve scene proxies
back to 1080p every frame (RX 7800 XT, 720p FSR 3 Performance on 4 cores: 826 vs 576
draws/frame, 185 vs 210 FPS), and on GPUs without shader stencil export every depth/stencil
copy took nine draws.

- `run.sh` again patches the game's render size for outputs other than 1080p (as in 0.1) and
  marks it with `SPARTAN_AUTO_RENDER_RES=1`, so an in-game restart recomputes it. Preset and output
  changes then need a restart (the menu offers it). `SPARTAN_LIVE_RES=1` selects the live path.
  1080p output and TAA keep the live path.
- Depth/stencil scene proxies need `VK_EXT_shader_stencil_export` again; without it these
  targets stay native (as in 0.1). `SPARTAN_SCENE_STENCIL_BITS=1` still forces the portable
  resampler for tests.

## Live resolution changes (2026-10-01)

The in-game output selector now resizes 720p/1080p/1440p/2160p host targets at a
display-pass boundary. Presets use `output / scale`, including raster sizes above
1080p (4K Quality: 2560×1440; Native AA: 3840×2160). Guest allocations stay at
1920×1080; `run.sh` no longer inserts a resolution patch for normal output choices.
The explicit `SPARTAN_RENDER_RES` compatibility override is retained.

`SceneTargets` resolves and retires old proxies before changing size. FSR resources
and history are rebuilt for the new input/output, with completion waits for both
FSR 3 and FSR 4. The 1080p path keeps HDR reconstruction before game post;
other outputs reconstruct tonemapped scene proxies at the first Scaleform pass,
then draw HUD/menu geometry into the output-size target. The display buffer is
resized too. Movie shader identification prevents a native-size fullscreen post
pass from being mistaken for UI.

Guest compute passes retain their native dispatch sizes and can resolve proxies
back to 1080p. This change does not scale every post-processing pass; performance
and intermediate detail can differ from the old startup-patched render sizes.

Verification: `out/dev/live-resolution-summary.log` switches output, preset and FSR
off/on in one PID. `SPARTAN_PRESENT_DUMP_TRIGGER` captures the completed host display
buffer (including HUD) into `SPARTAN_DUMP_DIR`. Captures confirm actual 720p/1440p/2160p
buffers. `scene-resolution-test` covers color/depth/stencil round trips through
proxies both smaller and larger than the guest allocation. `SPARTAN_PRESET_FILE` accepts
`preset [output-index [upscaler-index]]` for scripted menu-equivalent changes.

Frame analyzer: `SPARTAN_CAPTURE_TRIGGER=<file> SPARTAN_CAPTURE_DIR=<dir>`; creating the file records
the next frame (boundary: the pass writing a display buffer) — passes, targets, shaders,
sampled textures, and the first 1 KiB of bound constants for small passes and first draws.

## SpartanPort's frame (mapped 2026-10-10, 2560x1440 render)

Captured with the in-game menu (Insert > Diagnostics > Capture frame, files in
`user/captures/`). `tools/spartanport_frame_map.py <capture>` finds every piece below by structure,
not by address (addresses change with the render resolution patch), and exits 1 when one is
missing. There is no velocity buffer: motion vectors have to be computed.

| Stage | What |
|---|---|
| first pass of a frame | 2 draws copying the previous frame's final image into the display buffer |
| depth pre-pass | depth only, all opaque geometry (500–900 draws): **scene depth**, D32S8 at render size |
| linear depth | full-screen pass into R32F at render size |
| shadows + light pre-pass | 2048x2048 D32 cascades, lights accumulated into an RGBA8 sRGB target |
| material pass | the pre-pass geometry again (same draw count), sampling the light buffer |
| transparencies/effects | two RGBA8 sRGB targets with a second depth buffer |
| post | bloom pyramid 1280x720 down to 160x90, luminance in R16G16F, **1x1 R16G16F exposure** (read by the CPU every frame) |
| composite | full-size pass sampling the half-size bloom: **scene color before the HUD**, RGBA8 sRGB at render size |
| HUD | later passes drawing into that same image (some with the second depth); the game hides the HUD out of combat |

## Camera constants

| Where | What |
|---|---|
| depth pre-pass vertex shader (`aed40011eb5259f3`), slot 1, 144 bytes, floats 0–15 | **view-projection**, column-major; the same for every draw of a frame, changes with the camera and its field of view |
| same buffer, floats 16–31 | the object's world matrix (identity for static scenery) |
| 64-byte buffers (`*bbe4f109`, slot 0) | projection alone: x 1.96855, y 3.49965 at 16:9 (cinematic shots narrow it, e.g. 1.5073), near 0.5, very far plane, D3D depth |

The previous frame's matrices are not in the frame; the port has to keep them.

## Plan

1. Find the camera constants every frame (depth pre-pass, above), keep the previous view-projection.
2. Camera motion vectors: compute pass from depth and current/previous matrices into an
   RG16F target; debug view to check them. Object motion later (vertex shader replay with the
   previous frame's constants through the recompiler).
3. Jitter: sub-pixel offset of clip-space positions in the scene passes (recompiler).
4. Upscaler at the scene color stage (before post and UI): FSR 3.1 first (open, native Vulkan,
   also frame generation), then DLSS (native on Linux), XeSS/XeFG and OptiScaler through a
   loader for their Windows DLLs.

## Status

- Camera motion vectors: verified by reprojecting the previous frame (`SPARTAN_DEBUG_MOTION=1`,
  toggles 1<<22 reprojected frame, 1<<23 error map): static geometry matches.
- FSR 3.1 (`SPARTAN_UPSCALER=fsr3`, FireBurn/FSR-Vulkan submodule): scene color before the
  post-processing combine (compute shader 9a9cf8a9), 1:1, RGB written back (the game keeps data
  in the alpha). Toggle 1<<24 switches it off at run time.
- Jitter: viewport offset of scene geometry (drawn with the scene depth, not full-screen
  quads), Halton(2,3) 8 phases; sign checked by sharpness (correct 255, flipped 208, off 271 —
  1:1 jitter trades high-frequency aliasing for a slightly softer image). Toggle 1<<25 off.
- Frame-to-frame difference while standing still: ~33% lower with FSR.
- Missing: motion of animated objects (characters, cloth, foliage), reactive/transparency
  masks for particles and fog, render-resolution scaling, frame generation.

## 2026-10-01: FSR 4 при выводе 1440p/2160p не работал (мерцание и дрожание)

Видео `video_2026-10-01_03-58-59.mp4` (вывод 3840x2160, FSR 4 Performance): мерцание и дрожание
всех объектов. Причина — апскейлер в этом режиме вообще не выполнялся, а jitter оставался
включённым: на экран шёл растянутый кадр сцены, каждый кадр сдвинутый на свою фазу Halton.

1. Проход UI опознавался по точному размеру цели `SPARTAN_RENDER_RES` (1916x1078), а игра выделяет
   цели с выровненной высотой (1916x1080; в константах сцены тоже 1916x1080). `RunScaled` не
   вызывался ни разу (в логе не было `UI: native composition`). Теперь допускается выравнивание
   до 8 пикселей (`RenderTarget`), а размер сцены для FSR берётся из констант сцены
   (`CameraMotion::RenderSize`, `SceneSize`).
2. После этого FSR 4 падал с `external image registration failed (-1000069000)`: `RunScaled`
   создавал новые image view каждый кадр, а реестр FSR 4 вмещает восемь. Теперь те же
   `CachedView`, что и в пути Native AA.

Проверка дампом (`SPARTAN_DUMP_TRIGGER=<файл> SPARTAN_DUMP_DIR=<каталог>`, `SPARTAN_DUMP_FRAMES`, по умолчанию 8:
вход FSR, векторы движения и выход, raw): при неподвижной камере PSNR соседних кадров выхода
~45 дБ против ~28 дБ у входа с jitter; при повороте камеры и ходьбе шлейфов нет. FPS в этом
режиме ~107 вместо ~220 — раньше FSR 4 просто не выполнялся.

Осталось: спрайты (по 4 индекса) в цвет сцены со сценической глубиной — свечения, огоньки —
по-прежнему не сдвигаются jitter (правило «≤ 6 индексов = полноэкранный проход»).

## 2026-10-01: FSR 4 быстрее — проход post (2.9 → 0.8 мс в 4K)

Замеры: `SPARTAN_FSR4_PROFILE=1` (время каждого прохода FSR 4, патч провайдера в субмодуле,
см. `gpu/patches/fsr-vulkan`), бенчмарк вне игры `out/gpu/fsr4-bench` (`ninja -C out/gpu
fsr4-bench`; `--stats` — регистры и инструкции от RADV). 4K Balanced (2260x1272 → 3840x2160),
RX 7800 XT: весь FSR 4 — 5.8 мс, из них **post 2.5–2.9 мс** (не нейросеть: последние слои,
pixel shuffle 2x2 и смешивание с историей), 12 проходов модели — 2.4 мс, pre 0.55.

Причина: каждый поток считает блок 2x2 выходных пикселей и пишет их по одному в три образа
(рекуррентное состояние, история, выход) — каждая инструкция записи волны пишет пиксели через
один. Убрав любую из трёх записей, проход ускорялся в 1.3–4 раза при том же коде.

Решение: `tools/fsr4_optimize.sh` декомпилирует post (spirv-cross), `tools/fsr4_post_lds.pl`
собирает блок 16x16 рабочей группы в shared memory и пишет сплошными строками, glslang
компилирует обратно в `fsr4_shaders/opt/`; `vk_fsr4.cpp` берёт его оттуда (`SPARTAN_FSR4_OPT=0` —
оригинал). Две тонкости, найденные сравнением выходов:
- spirv-cross переводит знаковую распаковку int8 (`OpBitcast` в `i8vec4`) как `unpack8(uint)`
  (беззнаковую) — без исправления результат совсем другой (PSNR 17 дБ);
- значения в shared memory должны оставаться float: half из shared memory компилятор
  превращает в 16-битную запись, а она иначе округляет в unorm8.

`tools/fsr4_verify.sh` сравнивает оригинал и оптимизированный post для всех пресетов при выводе
1080p/1440p/4K на псевдослучайных входах: 1440p и 4K — побитово одинаково; post 4K 2.1–3.5 →
0.81–0.87 мс, 1440p 1.2–1.5 → 0.36–0.38 мс, 1080p 0.37–0.69 → 0.20–0.22 мс. В игре (4K
Balanced): FSR 4 5.8 → 4.0 мс, кадр GPU 13.1 → ~11.6 мс; дальше FPS упирается в CPU (поток
GPU-команд ждёт копии гостевой памяти, «host copies» ~20%).

Найдено попутно: **оригинальный FSR 4 на уровне 1080 (вывод 1920x1080) недетерминирован** — от
запуска к запуску на одинаковых входах меняется полоса у левого края (столбцы 0–132), то есть
где-то гонка или чтение неинициализированной памяти в модели/провайдере. При выводе 1080p это
может давать мерцание у левого края кадра. Не исследовано.

Маска реактивности при FSR 4 больше не считается (FSR 4 её не принимает).

### Что ещё проверено (там же, 4K Balanced)

- **A/B в одном процессе** (`ab.sh`, бит 24 — FSR выключен, UI-копия без апскейла): 86.5 FPS с
  FSR 4 против 87.7 без него. После оптимизации post кадр упирается в CPU (поток GPU-команд,
  ожидание копий гостевой памяти), а не в GPU. Async compute для FSR (перекрыть его с началом
  следующего кадра) дал бы не больше этого ~1% — отложено до ускорения CPU-части; к тому же он
  требует переставлять команды UI и вывода кадра N после работы кадра N+1.
- **WMMA (`VK_KHR_cooperative_matrix`, RADV на RDNA3 поддерживает)**: прототип прохода 1
  модели (остаточный блок 16 каналов: 3x3 16→16, 1x1 16→32 ReLU, 1x1 32→16) — 0.85 мс в первом
  варианте, 0.49 мс со словной раскладкой в shared memory, против 0.30 мс у исходного dot4.
  Арифметика на WMMA заняла бы ~0.1 мс, но при 16 каналах выкладка тайлов, эпилоги через shared
  memory и занятость (6 волн/SIMD, предел по LDS) съедают выигрыш. Потолок — около 1 мс на
  все 12 проходов при тонкой ручной переработке каждого; не начато.
- `RADV_PERFTEST=cswave32` (wave32 для compute): FSR 4 медленнее, 4.2 → 5.4 мс.
- Проход pre (0.54 мс) запись не ограничивает (0.50 мс без записи).

## 2026-10-01: мерцание FSR 4 у левого края при выводе 1080p — гонка в проходе 11 модели

Недетерминизм уровня 1080 (см. выше) — ошибка в шейдере модели v07, проход 11 (декодер,
1/4 → 1/2 разрешения). Каждый поток — пиксель входа 1/4 разрешения и пишет блок 2x2 выхода
1/2 разрешения. Диспатч округляет ширину до 64 потоков, а потоки за шириной входа не
останавливаются: на уровне 1080 (вход 480 в ширину) потоки 480..511 пишут пиксели выхода
960..1023, то есть первые пиксели следующей строки (строка любого тензора — 15360 байт), и
гоняются с их настоящими авторами. При выводе 4K ширина 960 делится на 64, ошибки нет.

`tools/fsr4_pass11_guard.pl` добавляет ранний выход для потоков за границей (размер берётся из
проверки соседей самого прохода); `tools/fsr4_optimize.sh` собирает его в `fsr4_shaders/opt`
для всех пресетов вместе с post; `vk_fsr4.cpp` и бенчмарк берут из `opt/` любой проход.
`tools/fsr4_verify.sh`: 1440p/4K — побитово как оригинал (защита там не срабатывает); 1080p —
одинаковый результат от запуска к запуску, отличия от оригинала после первого кадра только в
бывшей полосе у левого края.

Найдено при этом: spirv-cross переводит все знаковые распаковки int8 (`OpBitcast` в `v4char`)
как беззнаковый `unpack8(uint)`; в pass 11 их девять разных форм. Исправление общее для
скриптов — `tools/Fsr4SpirvCrossFixes.pm` (все `unpack8` через знаковый помощник).

## 2026-10-01: FSR 4.1.1 — что внутри и как его запустить в Vulkan (разведка)

Источник: `amd_fidelityfx_upscaler_dx12.dll` 4.1.1.2740 из OptiScaler (`FSR4_LATEST`).
- В DLL есть провайдер `ffxProvider_FSR4_Int8` / `Fsr4Int8UpscalerModel` (версия «FSR4-i8 4.1.1»).
  1028 DXIL-контейнеров (25.7 МБ): проходы модели `fsr4_model_v07_fp8_no_scale_pass1..12` (+ `_post`),
  270 вариантов `prepass`, 144 `postpass`, `fsr_rcas_pass` и запасной FSR 3.1/2. Сжатых данных нет.
- Несмотря на «fp8» в имени, проходы модели — обычный DXIL с `dot4AddPacked` (int8 dot4), без
  расширений AMD (WMMA). Архитектура та же v07 (12 проходов), но веса и смещения не зашиты в
  шейдер, а читаются из `InitializerBuffer` (t18, typed buffer), размеры тензоров — из cbuffer
  `CsTensorSizes`, scratch — `u11`. У каждого прохода 9 вариантов: сетка зашита (уровни 1080 / 4K /
  8K: 480x270, 960x540, 1920x1080 для прохода 5) и, видимо, пресеты.
- `amdxcffx64.dll` (загрузчик 2.3.0.2913, его подгружает Proton) — те же FP8-проходы FSR 4 и ML-
  генерация кадров (`mlfi_*`), отдельной INT8-модели нет. `FSR4_INT8` 4.0.2 — это `v07_i8`, то есть
  то, что порт уже использует.
- dxil-spirv (HansKristian-Work/dxil-spirv, собирается из исходников) переводит проходы в
  корректный SPIR-V: `dxil-spirv <pass> --enable-shader-i8-dot --ssbo-uav --use-reflection-names`.

Чего не хватает для своего провайдера 4.1.1: соответствие контейнеров (пресет, уровень, проход),
содержимое `InitializerBuffer`, значения `CsTensorSizes`, перестановки prepass/postpass, константы и
размеры диспатчей. Самый надёжный путь — не разбирать x86-код провайдера, а записать один кадр:
маленькая тестовая программа на FFX API (DX12, MinGW) под Wine/Proton с vkd3d-proton, явный выбор
провайдера «FSR4-i8» через override версии, и Vulkan-слой, который пишет пайплайны (SPIR-V от
dxil-spirv), содержимое буферов и размеры диспатчей. Затем — воспроизведение своим провайдером
через тот же бенчмарк (`fsr4-bench`), с проверкой против записанного выхода.

## 2026-10-01: FSR 4.1.1 в порту (upscaler=fsr411)

Модель AMD 4.1.1 (INT8) работает нативно в Vulkan и совпадает с DLL до бита.

**Как получено.** `tools/fsr4cap/fsr4cap.exe` (C, MinGW) вызывает FidelityFX API 2.3 из
`amd_fidelityfx_loader_dx12.dll` с явной версией 4.1.1 и через подмену таблиц методов D3D12
записывает всё, что делает DLL под vkd3d-proton: пайплайны (DXIL), корневые сигнатуры,
дескрипторы, загрузки, константы, диспатчи. Кадр — 29 диспатчей: SPD авто-экспозиции, prepass,
pass0_post, проходы модели 1..12 (за каждым `_post`, обнуление рамки тензора), postpass, RCAS.
- Моделей две: m0 (Native..Performance) и m1 (Ultra Performance, соотношение 3.0), веса — 128 КБ
  (`InitializerBuffer`). Уровни: t1080 (вывод до 1920x1080), t2160 (больше, вкл. 2560x1080).
- Тензоры считаются от вывода, выровненного до 8; таблица `CsTensorSizes` (17 размеров), число
  групп и константы (MLSR как у v07, но width/height не выровнены и `inv_scale = 1 / scale`;
  SPD; RCAS) проверяются `extract.py` на 20 записях.
- dxil-spirv переводит DXIL так же, как vkd3d-proton: `--mixed-float-dot-product`
  (`VK_VALVE_shader_mixed_float_dot_product`, dot2 по half с накоплением во float) — без него
  PSNR 40 дБ вместо точного совпадения. `--class-bindings` (патч) раскладывает регистры SRV/UAV/
  CBV/сэмплеров по биндингам +0/+32/+64/+96, рантайм привязывает ресурсы по именам.
- `fsr411/fsr411.cpp` — независимое ядро на Vulkan (игра и `fsr4-bench --fsr411`).
- `verify.sh`: DLL и воспроизведение на одинаковых входах, 8 кадров — побитово одинаково для
  1080p (обе модели), 1440p, 2560x1080, 4K (обе модели). 1600x900 расходится только у правого
  края, где сама DLL недетерминирована (гонка в её шейдерах при ширине тензора не кратной 64).

**Ускорение.** postpass переписан через workgroup memory на уровне SPIR-V-ассемблера
(`postpass_lds.py`; перевод через GLSL неточен: теряются `DenormPreserve` и точная расстановка
`RelaxedPrecision`, и ещё что-то меняет результат): 2.18 → 0.95 мс в 4K, побитово как DLL.
Весь FSR 4.1.1: 1.09 мс при 1080p (1280x720), 4.2 мс при 4K Balanced. В игре (4K Balanced)
апскейлер 4.12 мс против 4.32 мс у ускоренного v07.

**Сборка ассетов** (пользователем, из своих DLL; в репозитории ничего от AMD нет):

    bash tools/fsr4cap/build_assets.sh <amd_fidelityfx_upscaler_dx12.dll 4.1.x> <amd_fidelityfx_loader_dx12.dll 2.3.x>

Нужны nix-shell (или MinGW, cmake, ninja, python3, SPIRV-Tools, umu-run), сеть для dxil-spirv и
заголовков FidelityFX SDK и Proton (GE-Proton). DLL есть, например, у OptiScaler
(`FSR4_LATEST/`) и в играх с FSR 3.1/4. Сборка с нуля даёт те же файлы; `VERIFY=1` в конце
сравнивает с DLL.

**Поправка к A/B выше («кадр упирается в CPU»).** Те замеры шли, пока 12 зависших процессов
shadPS4 занимали по ядру. На свободной системе: «host copies» 1.6%, ожидание GPU ~41% — кадр
упирается в GPU, так что async compute для FSR снова имеет смысл.

## God of War III Remastered layout (the port's TAA)

The FSR 3.1 path (and its Native AA preset) is upstream's and is left untouched: the scene TAA below is
active only when `upscaler=taa`, every hook is gated by `TemporalUpscaler::SceneTaa()`.

The frame differs from the 5-target G-buffer layout above (found with `tools/spartanport_frame_map.py`
on F11 captures).

### Frame structure

- Scene: depth pre-pass (no colour target, screen-sized depth, more than 6 indices per draw; square
  shadow-map passes are skipped by aspect), then the material pass (colour target, the same depth,
  no depth write), bloom / depth of field, and the composite. The display passes repeat at 60 Hz while
  scenes come at 30 fps and are pipelined, so **a scene, not a display pass, is the frame**:
  it opens at its first pre-pass draw (`CameraMotion::OnScenePrepass`) and closes at the composite
  (`ResolveViewProjection`). A scene no composite closed within 24 display passes is dropped.
- Camera: one vertex constant buffer of the pre-pass holds a column-major view-projection in its
  first 16 floats. Which shader and slot differs, so it is recognised by content
  (`camera_view_projection.h`, unit test `tests/test_camera_vp.cpp`). Depth is `0.5 + 0.5 z / w`
  (the game's depth range scale/offset 0.5/0.5); the viewport is 960 x -540 around 960, 540.
- Motion vectors: `camera_motion_vp.comp`, from `previousVP * inverse(currentVP)` through the game's
  viewport; the push constants are 128 bytes (std430 `vec4` alignment, a static_assert guards it).
  Characters have no motion vectors of their own (see the moving-object handling below).
  `SPARTAN_VP_FLIP=x|y|xy` mirrors the screen axes for calibration.
- Trigger: the draw whose pixel shader is the post-process composite (`0c20ededc9e7c40c`, override
  `SPARTAN_UPSCALER_COMPOSITE_PS`). The upscale runs before the next draw or dispatch, on the 8-bit
  sRGB scene colour (sampled through an sRGB view), before the HUD. The result is written back with
  a blit; the game's alpha is kept (`ldr_alpha.comp`).
- Guest MSAA is turned off for the TAA (`vk_instance.cpp`), and the pipeline cache profile records that
  mode so a cache of the other mode is rebuilt. The game itself does not rely on MSAA; the half-resolution layer (instanced vegetation-like pass, own
  depth) is not jittered.

### Jitter

One Halton(2,3) sample per scene (`TemporalUpscaler::OnSceneStart`, `Motion::JitterPhases`), applied
as a viewport offset to the draws that use the scene depth or its same-size copy (transparents),
skipping screen-space passes (3 or 4 indices) so a shifted full-screen quad leaves no edge column.

### TAA (`taa.comp`, settings "TAA look (God of War III)" in the Image tab)

- Current colour: Catmull-Rom unjitter, history: Catmull-Rom resample at the reprojected position,
  depth rejection per bilinear tap, variance clipping in tonemapped YCoCg.
- Moving objects (no vectors): a 3x3 patch whose luma differs from the history loses history
  (`taa_objects`). The jitter makes textured pixels look changed in ~3% of a still picture, so a
  pixel needs a changed neighbour in the **previous** frame to lose the full amount (otherwise 30%).
  The flag lives in the sign of the history alpha (depth); every read of that alpha uses `abs()`.
- Pixels whose history was refused get an edge-directed 3-tap spatial smoothing on the current picture
  (`taa_spatial`; the history keeps the unfiltered result). Its strength is in `pad` bits 26..31.
- Texture detail: a negative LOD bias (`taa_lod / 100`) on the scene's material draws
  (`Rasterizer::BindResources`: scene depth address, no depth write, more than 4 indices, a colour
  target); the accumulation turns it into detail.
- Sharpening: RCAS-style pass `taa_sharpen.comp` after the accumulation (`taa_sharpen`, up to 200%);
  the menu's "Sharpening (RCAS)" switch turns it off (and FSR's own sharpening, `SharpenOn()`).
- `pad` bits: 0-7 diagnostic view (4 = history debug view: red depth-rejected, green history lost,
  blue weight used; the debug view uses whole colours only (the game's colour grading crushes small values). With history: even
  columns = how much of the bilinear footprint matched (blue 99%+, cyan 50%+, yellow 10%+, green less), odd columns =
  the weight used (blue 0.5+, yellow 0.1+, green less). Without history because the depth did not match: the bits
  of four numbers as black/white, column x mod 32 = bit (most significant on the left), row y mod 4 = number (expected
  previous depth, this frame's depth, depth stored at the nearest history texel, tolerance). Black = the pixel did
  not reproject, magenta = the motion vector leaves the picture, white = history reset; every other 16 px square of the whole picture is black, to show that the picture really is this shader's output), 8-15 A/B switches, 16-23 moving-object history loss in percent, 24 view-projection
  depth, 25 8-bit scene, 26-31 spatial smoothing.
- Defaults (tuned in game on a GTX 1070 Ti): history 99% still / 55% moving, tolerance 115%,
  moving objects 90%, edges 70%, texture detail 75%, sharpening 165%. They live in
  `shim/spartanport_settings.h` and in the menu hints.

### Simulator

The tuning used a software renderer of a procedural scene (static, slow and fast camera, moving box)
run through `taa.comp` on lavapipe with PSNR against a 9-sample ground truth, a Laplacian sharpness
ratio and the frame-to-frame flicker. It is not part of the repository; the numbers that mattered:
the change flag persistence halved the flicker of a still picture (0.0021 -> 0.0010) for 1.5 dB less
in the moving region.

### Log

`last_run.log` prints `Temporal AA status: ...` every 300 composites (`composite_seen`, `ldr_runs`,
`skip`, `scenes`, `done_skips`), `TAA scene: skipped (reason)` / `running again` when a scene is not
upscaled, and `TAA look: ...` once the sliders settle. `ran_last_frame` and the menu's "running"
indicator follow the dispatches, not the display passes.
