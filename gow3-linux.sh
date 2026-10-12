#!/usr/bin/env bash
# God of War III Remastered — Linux
# Doble clic este archivo para jugar. Hace todo automáticamente.
set -euo pipefail
cd -- "$(dirname -- "$0")"

# ---- Detectar gestor de paquetes ----
install_deps() {
    local missing=()
    local cmds=(cmake ninja gcc python3)
    for cmd in "${cmds[@]}"; do
        command -v "$cmd" &>/dev/null || missing+=("$cmd")
    done
    # Check libraries
    ldconfig -p 2>/dev/null | grep -q libvulkan || missing+=(vulkan-headers)
    ldconfig -p 2>/dev/null | grep -q libSDL3  || missing+=(sdl3)
    python3 -c "import tkinter" 2>/dev/null || missing+=(tk)

    if [[ ${#missing[@]} -eq 0 ]]; then
        return 0
    fi

    echo "Instalando dependencias: ${missing[*]}..."

    if command -v pacman &>/dev/null; then
        # Arch / CachyOS / Manjaro / SteamOS
        local pkgs=(cmake ninja gcc boost zydis robin-map vulkan-headers
                    vulkan-icd-loader spirv-headers sdl3 ffmpeg fmt xxhash
                    glslang python3 jsoncpp tk)
        pkexec pacman -S --needed --noconfirm "${pkgs[@]}" 2>/dev/null \
            || sudo pacman -S --needed --noconfirm "${pkgs[@]}"
    elif command -v dnf &>/dev/null; then
        # Fedora
        local pkgs=(gcc-c++ cmake ninja-build boost-devel vulkan-headers
                    vulkan-loader-devel spirv-headers-devel SDL3-devel
                    ffmpeg-free-devel fmt-devel xxhash-devel glslang
                    python3 jsoncpp-devel python3-tkinter)
        pkexec dnf install -y "${pkgs[@]}" 2>/dev/null \
            || sudo dnf install -y "${pkgs[@]}"
    elif command -v apt &>/dev/null; then
        # Ubuntu / Debian
        local pkgs=(build-essential cmake ninja-build libboost-dev
                    libvulkan-dev spirv-headers libsdl3-dev libavcodec-dev
                    libavformat-dev libavutil-dev libswscale-dev
                    libswresample-dev libfmt-dev libxxhash-dev glslang-tools
                    python3 libjsoncpp-dev python3-tk)
        pkexec apt install -y "${pkgs[@]}" 2>/dev/null \
            || sudo apt install -y "${pkgs[@]}"
    else
        echo "No se detectó un gestor de paquetes compatible."
        echo "Instala manualmente: cmake ninja gcc boost vulkan-headers sdl3 ffmpeg tk"
        exit 1
    fi
}

# ---- Compilar si no existe el binario ----
build_if_needed() {
    if [[ -f out/gow3-probe && -f out/gpu/libgow3gpu.so ]]; then
        return 0
    fi
    echo "Compilando God of War III (primera vez)..."
    bash build.sh
    echo "✓ Compilación exitosa."
}

# ---- Lanzar el launcher ----
launch() {
    exec python3 launcher/gow3_launcher_linux.py "$@"
}

# ---- Main ----
install_deps
build_if_needed
launch "$@"
