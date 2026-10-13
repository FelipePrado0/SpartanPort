# God of War III Remastered — Linux Port

> Fork de [FelipePrado0/gow3_pc](https://github.com/FelipePrado0/gow3_pc) con soporte nativo para Linux.

Este fork compila y ejecuta el port de God of War III Remastered directamente en Linux,
sin Wine ni Proton. El motor gráfico traduce los comandos de GPU de PS4 a **Vulkan nativo**.

## Cambios respecto al upstream

Solo 4 archivos modificados (12 líneas) para compilar con GCC 16 + Vulkan 1.4:

| Archivo | Fix |
|---|---|
| `gpu/CMakeLists.txt` | xbyak include path para Linux; fallback VMA header |
| `gpu/.../vk_instance.cpp` | Disambiguación `operator=` con Vulkan-Hpp 1.4 |
| `src/actor_hook.h` | `_Static_assert` → macro C/C++ compatible |
| `build.sh` | Include paths y `-lm` para tests |

## Requisitos

- **Linux x86-64** (probado en CachyOS/Arch, compatible con cualquier distro moderna)
- **GPU con Vulkan 1.3+** y driver actualizado
- **Tu dump propio** de God of War III Remastered: **CUSA01623 v01.02** (EE. UU.). La versión
  europea **CUSA01715** también arranca, en v01.00 o con el update 01.02; los patches de
  resolución y FPS son solo para CUSA01623.

### Updates

Si el update está en una carpeta aparte (`CUSAxxxxx-UPDATE`, como en shadPS4), elígela en el
launcher en **Game → Update folder**, debajo de la carpeta del juego. En cada inicio se aplica
encima del juego base mediante enlaces, sin modificar ninguna de las dos carpetas. Desde la
terminal:

```bash
GOW3_GAME_DIR=/ruta/CUSA01715 GOW3_UPDATE_DIR=/ruta/CUSA01715-UPDATE bash run.sh
```

También sirve una carpeta con el update ya copiado encima del juego base. El log dice qué
versión se detectó (`Patches: ...`).

### Dependencias (Arch/CachyOS)

```bash
sudo pacman -S --needed gcc cmake ninja boost zydis robin-map \
    vulkan-headers vulkan-icd-loader spirv-headers \
    sdl3 ffmpeg fmt xxhash glslang python3 jsoncpp
```

<details>
<summary>Fedora / Ubuntu / Debian</summary>

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake ninja-build boost-devel zydis-devel \
    tsl-robin-map-devel vulkan-headers vulkan-loader-devel spirv-headers-devel \
    SDL3-devel ffmpeg-free-devel fmt-devel xxhash-devel glslang python3 jsoncpp-devel
```

**Ubuntu/Debian** (24.04+):
```bash
sudo apt install build-essential cmake ninja-build libboost-dev \
    libvulkan-dev vulkan-validationlayers spirv-headers \
    libsdl3-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
    libfmt-dev libxxhash-dev glslang-tools python3 libjsoncpp-dev
```
> Nota: `zydis`, `robin-map` y `magic_enum` se usan desde los submódulos si no están empaquetados.
</details>

### Steam Deck

```bash
# Modo escritorio con terminal
sudo steamos-readonly disable
sudo pacman -S --needed gcc cmake ninja boost zydis robin-map \
    vulkan-headers spirv-headers sdl3 ffmpeg fmt xxhash glslang python3 jsoncpp
```

## Compilar

```bash
git clone --recursive https://github.com/TU_USUARIO/gow3_pc.git
cd gow3_pc
bash build.sh
```

Esto genera:
- `out/gow3-probe` — Ejecutable principal (1.2 MB)
- `out/gpu/libgow3gpu.so` — Motor gráfico Vulkan (130 MB)
- `out/gow3-gpu-capabilities` — Verificador de GPU

## Ejecutar

```bash
GOW3_GAME_DIR=/ruta/a/tu/dump/CUSA01623 bash run.sh
```

### Case sensitivity

Si encuentras errores de archivos no encontrados, los nombres del dump PS4 pueden
tener mayúsculas/minúsculas mezcladas. Solución rápida con `ciopfs`:

```bash
sudo pacman -S --needed ciopfs  # o apt install ciopfs
mkdir -p /tmp/gow3mount
ciopfs /ruta/al/dump/CUSA01623 /tmp/gow3mount
GOW3_GAME_DIR=/tmp/gow3mount bash run.sh
```

### Renderizado por software (sin GPU)

```bash
GOW3_GAME_DIR=/ruta/al/dump bash run.sh --software
```

## Tests

```bash
bash build.sh --test
```

## Probado en

| Componente | Versión |
|---|---|
| OS | CachyOS (Arch Linux) |
| Kernel | 6.x |
| GCC | 16.2.1 |
| Vulkan | 1.4.357 |
| SDL3 | 3.4.14 |
| GPU | NVIDIA RTX 5070 Laptop GPU |

## Créditos

- **Felipe Prado** ([FelipePrado0](https://github.com/FelipePrado0)) — Port a PC y las 45+ APIs de GoW3
- **Supermedo** — Port Windows base
- **deadinside28** — bbport original (Linux/Steam Deck)
- **shadPS4** — Video core (GPU PS4 → Vulkan)
- **cuesta4** — Fixes de renderizado para GoW3

> **No se incluyen archivos del juego.** Necesitas un dump de tu propia PS4.
> Este proyecto no está afiliado a Sony Interactive Entertainment ni a Santa Monica Studio.
> God of War es una marca registrada de Sony Interactive Entertainment.
