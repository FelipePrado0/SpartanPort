#!/usr/bin/env bash
# Checks, and with --install installs, the system packages that building and packaging the Linux
# version need. The distribution is read from /etc/os-release (Arch, Debian/Ubuntu, Fedora/RHEL,
# openSUSE); each requirement is checked by what it provides (a command, pkg-config or a header),
# so a package from another source counts too.
#
#   packaging/linux/deps.sh [--build] [--packaging] [--install] [--yes] [--list]
#   packaging/linux/deps.sh --diagnose LOG...
#
#   --build, --packaging  the group to check (default: both)
#   --install             install what is missing with the package manager (sudo; asks first)
#   --yes                 do not ask
#   --list                only say which distribution and package manager were detected
#   --diagnose LOG...     read the logs of a failed build and say what is probably missing; every
#                         argument after it is a log file
#
# Without --install it only reports and exits 1 when something is missing. GOW3_SKIP_DEPS=1 makes
# it exit at once (use it when the tools come from somewhere the checks cannot see); GOW3_YES=1 is
# --yes; GOW3_FAMILY=arch|debian|fedora|suse overrides the detection. A variable set to 0 is off.
# Libraries in a prefix outside /usr are found through CPATH and CMAKE_PREFIX_PATH.
set -euo pipefail

truthy() { [[ -n ${1:-} && $1 != 0 ]]; }

groups=() install=0 assume_yes=0 list_only=0 diagnose=0 logs=()
for arg in "$@"; do
    ((diagnose)) && { logs+=("$arg"); continue; }
    case $arg in
    --build) groups+=(build) ;;
    --packaging) groups+=(packaging) ;;
    --install) install=1 ;;
    --yes) assume_yes=1 ;;
    --list) list_only=1 ;;
    --diagnose) diagnose=1 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done
if ((diagnose && ${#logs[@]} == 0)); then echo '--diagnose needs at least one log file.' >&2; exit 2; fi
truthy "${GOW3_SKIP_DEPS:-}" && ((!diagnose)) && exit 0
((${#groups[@]})) || groups=(build packaging)
truthy "${GOW3_YES:-}" && assume_yes=1

detect_family() {
    [[ -n ${GOW3_FAMILY:-} ]] && { echo "$GOW3_FAMILY"; return; }
    local id='' like=''
    if [[ -r /etc/os-release ]]; then
        # shellcheck disable=SC1091
        id=$(. /etc/os-release; echo "${ID:-}"); like=$(. /etc/os-release; echo "${ID_LIKE:-}")
    fi
    case " $id $like " in
    *" nixos "*) echo nix; return ;;
    *" arch "*) echo arch; return ;;
    *" debian "*|*" ubuntu "*) echo debian; return ;;
    *" fedora "*|*" rhel "*|*" centos "*) echo fedora; return ;;
    *suse*) echo suse; return ;;
    esac
    if command -v pacman >/dev/null; then echo arch
    elif command -v apt-get >/dev/null; then echo debian
    elif command -v dnf >/dev/null; then echo fedora
    elif command -v zypper >/dev/null; then echo suse
    else echo unknown; fi
}

family=$(detect_family)
pretty=$(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-unknown}")
case $family in
arch) manager=pacman ;;
debian) manager=apt ;;
fedora) manager=dnf ;;
suse) manager=zypper ;;
*) manager=none ;;
esac
if ((list_only)); then echo "$pretty: family $family, package manager $manager"; exit 0; fi
# Inside a nix-shell (or on NixOS) the tools come from shell.nix, which the checks cannot see.
if ((!diagnose)) && [[ $family == nix || -n ${IN_NIX_SHELL:-} || -n ${GOW3_IN_NIX_SHELL:-} ]]; then
    echo 'Nix: the build environment comes from shell.nix (nix-shell shell.nix).'
    exit 0
fi

# GOW3_SYSTEM_INCLUDE (colon separated) replaces the system header directories; only the tests set it.
have_header() {
    local dir prefix
    local -a dirs
    IFS=: read -r -a dirs <<< "${GOW3_SYSTEM_INCLUDE:-/usr/include:/usr/local/include:/usr/include/$(uname -m)-linux-gnu}"
    IFS=: read -r -a extra <<< "${CPATH:-}:${C_INCLUDE_PATH:-}:${CPLUS_INCLUDE_PATH:-}"
    dirs+=("${extra[@]}")
    IFS=: read -r -a prefixes <<< "${CMAKE_PREFIX_PATH:-}"
    for prefix in "${prefixes[@]}"; do [[ -n $prefix ]] && dirs+=("$prefix/include"); done
    for dir in "${dirs[@]}"; do
        [[ -n $dir && -e $dir/$1 ]] && return 0
    done
    return 1
}
# Status 2 (not 1) when pkg-config itself is missing: the answer is unknown, not "absent".
have_pc() { command -v pkg-config >/dev/null || return 2; pkg-config --exists "$@"; }
have_py() { python3 -c "import $1" 2>/dev/null; }
have_lib() {
    local dir
    for dir in /usr/lib /usr/lib64 /usr/lib/"$(uname -m)"-linux-gnu /lib/"$(uname -m)"-linux-gnu /usr/local/lib; do
        compgen -G "$dir/$1*" >/dev/null && return 0
    done
    return 1
}
# PyInstaller and iced-x86 are the distribution's, or come from pip in a venv; PyInstaller needs libpython as a shared library.
have_pyinstaller_env() { { have_py PyInstaller && have_py iced_x86; } || { have_py venv && have_py ensurepip && have_lib libpython3; }; }

# group ; name ; check ; arch ; debian ; fedora ; suse ; fetched
# An empty package list: the distribution has none. A last field "fetch": CMake downloads the item when the
# system lacks it, so it is only installed with --install and never makes the check fail.
requirements=(
'build;C and C++ compiler;(command -v "${CC:-cc}" || command -v gcc) && (command -v "${CXX:-c++}" || command -v g++);gcc;build-essential;gcc gcc-c++;gcc gcc-c++'
'build;cmake;command -v cmake;cmake;cmake;cmake;cmake'
'build;ninja;command -v ninja;ninja;ninja-build;ninja-build;ninja'
'build;pkg-config;command -v pkg-config;pkgconf;pkg-config;pkgconf-pkg-config;pkg-config'
'build;git;command -v git;git;git;git;git'
'build;python3;command -v python3;python;python3;python3;python3'
'build;binutils;command -v objdump;binutils;binutils;binutils;binutils'
'build;Vulkan headers and loader;have_pc vulkan && have_header vulkan/vulkan_core.h;vulkan-headers vulkan-icd-loader;libvulkan-dev;vulkan-headers vulkan-loader-devel;vulkan-headers vulkan-devel'
'build;SDL3;have_pc sdl3;sdl3;libsdl3-dev;SDL3-devel;SDL3-devel'
'build;FFmpeg;have_pc libavformat libavcodec libavutil libswscale libswresample;ffmpeg;libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev;ffmpeg-free-devel;ffmpeg-devel'
'build;Boost headers;have_header boost/version.hpp;boost;libboost-dev;boost-devel;libboost_headers-devel'
'build;fmt;have_pc fmt;fmt;libfmt-dev;fmt-devel;fmt-devel'
'build;robin-map;have_header tsl/robin_map.h;robin-map;robin-map-dev;robin-map-devel;;fetch'
'build;Vulkan Memory Allocator;have_header vk_mem_alloc.h || have_header vma/vk_mem_alloc.h;;libvulkan-memory-allocator-dev;VulkanMemoryAllocator-devel;;fetch'
'build;xxHash;have_pc libxxhash;xxhash;libxxhash-dev;xxhash-devel;xxhash-devel'
'build;Zydis;have_header Zydis/Zydis.h;zydis;libzydis-dev;zydis-devel;'
'build;glslang compiler;command -v glslang || command -v glslangValidator;glslang;glslang-tools;glslang;glslang-devel'
'build;X11;have_pc x11;libx11;libx11-dev;libX11-devel;libX11-devel'
'packaging;curl;command -v curl;curl;curl;curl;curl'
'packaging;file (used by appimagetool);command -v file;file;file;file;file'
'packaging;ldd;command -v ldd;glibc;libc-bin;glibc-common;glibc'
'packaging;Python Tk;have_py tkinter;tk;python3-tk;python3-tkinter;python3-tk'
'packaging;Python Pillow;have_py PIL.Image;python-pillow;python3-pil;python3-pillow;python3-Pillow'
'packaging;PyInstaller and iced-x86 (or Python venv with a shared libpython);have_pyinstaller_env;python-pip;python3-venv python3-pip python3-dev;python3-pip python3-libs;python3-pip python3-devel'
)

column=$(case $family in arch) echo 4 ;; debian) echo 5 ;; fedora) echo 6 ;; suse) echo 7 ;; *) echo 0 ;; esac)

# --diagnose: the build failed; say why in terms of packages. The requirement checks run first (a missing
# dependency is the usual cause), then the logs are searched for what the compiler, CMake or the linker
# could not find.
if ((diagnose)); then
    export LC_ALL=C
    echo >&2
    echo "The build failed. Looking for the cause ($pretty):" >&2
    env -u GOW3_SKIP_DEPS -u IN_NIX_SHELL -u GOW3_IN_NIX_SHELL bash "${BASH_SOURCE[0]}" --build >&2 || true
    # The compiler colors its messages on a terminal; the color codes would split the phrases searched for.
    log_text=$(cat "${logs[@]}" 2>/dev/null | sed -E 's/\x1b\[[0-9;]*[A-Za-z]//g' || true)
    # Which requirement a missing item belongs to: regex ; requirement name (first match wins).
    keywords=(
        'vk_mem_alloc|VulkanMemoryAllocator;Vulkan Memory Allocator'
        'robin_map|robin-map|tsl;robin-map'
        'vulkan|Vulkan;Vulkan headers and loader'
        'SDL3|sdl3;SDL3'
        'libav|libsw|ffmpeg|FFMPEG;FFmpeg'
        'xxhash|XXHASH;xxHash'
        'Zydis|zydis;Zydis'
        'glslang;glslang compiler'
        'boost|Boost;Boost headers'
        '(^|[^a-z])(lib)?fmt($|[^a-z])|fmt/;fmt'
        'X11|x11;X11'
    )
    package_of() {  # package(s) of the requirement a missing item belongs to, "" when unknown
        local item=$1 pair name row
        local -a field
        for pair in "${keywords[@]}"; do
            if [[ $item =~ ${pair%;*} ]]; then
                name=${pair##*;}
                for row in "${requirements[@]}"; do
                    IFS=';' read -r -a field <<< "$row"
                    [[ ${field[1]} == "$name" ]] && ((column)) && { echo "${field[column - 1]:-}"; return 0; }
                done
            fi
        done
        return 0
    }
    search_command() {
        case $manager in
        apt) echo "apt-file search $1 (after apt-file update)" ;;
        dnf) echo "dnf provides '*/$1'" ;;
        pacman) echo "pacman -F $1 (after pacman -Fy)" ;;
        zypper) echo "zypper search --provides $1" ;;
        *) echo "your package manager's file search for $1" ;;
        esac
    }
    found=0
    report() {  # what, item
        local package
        package=$(package_of "$2")
        found=1
        if [[ -n $package ]]; then
            echo "  - $1 '$2': install $package" >&2
        else
            echo "  - $1 '$2': find the package with: $(search_command "$2")" >&2
        fi
    }
    while read -r item; do [[ -n $item ]] && report "CMake package" "$item"
    done < <(grep -oE 'Could not find a package configuration file provided by "[^"]+"' <<< "$log_text" | sed -E 's/.*"([^"]+)"/\1/' | sort -u || true)
    while read -r item; do [[ -n $item ]] && report "CMake could not find" "$item"
    done < <(grep -oE 'Could NOT find [A-Za-z0-9_+.-]+' <<< "$log_text" | awk '{print $4}' | sort -u || true)
    while read -r item; do [[ -n $item ]] && report "file" "$item"
    done < <(grep -oE 'Could not find [A-Za-z0-9_]+ using the following (files|names): .*' <<< "$log_text" | sed -E 's/.*following (files|names): //; s/,.*//' | sort -u || true)
    while read -r item; do [[ -n $item ]] && report "pkg-config module" "$item"
    done < <(grep -oE "(No package '[^']+' found|Package '?[A-Za-z0-9_.+-]+'?,? required by[^,]*)" <<< "$log_text" | sed -E "s/No package '([^']+)' found/\1/; s/Package '?([A-Za-z0-9_.+-]+)'?,? required by.*/\1/" | sort -u || true)
    while read -r item; do [[ -n $item ]] && report "header" "$item"
    done < <(grep -oE 'fatal error: [^ :]+: No such file or directory' <<< "$log_text" | awk '{print $3}' | tr -d ':' | sort -u || true)
    while read -r item; do [[ -n $item ]] && report "library" "lib$item"
    done < <(grep -oE 'cannot find -l[A-Za-z0-9_+.-]+' <<< "$log_text" | sed 's/cannot find -l//' | sort -u || true)
    while read -r item; do [[ -n $item ]] && report "program" "$item"
    done < <(grep -oE '[A-Za-z0-9_.+-]+: command not found' <<< "$log_text" | sed 's/: command not found//' | sort -u || true)
    if grep -qE 'No space left on device' <<< "$log_text"; then
        echo '  - the disk is full: free some space (the build needs a few GB in out/).' >&2; found=1
    fi
    if grep -qE 'internal compiler error: Killed|Killed signal terminated program|c\+\+: fatal error: Killed' <<< "$log_text"; then
        echo '  - the compiler was killed, usually for lack of memory: build with fewer jobs (ninja -C out/gpu -j4) or add swap.' >&2; found=1
    fi
    if grep -qE 'Could not resolve host|Failed to connect|Could not resolve hostname|TLS connect error' <<< "$log_text"; then
        echo '  - a download failed (CMake fetches Vulkan-Headers, robin-map and Vulkan Memory Allocator from GitHub when the system has none): check the network.' >&2; found=1
    fi
    if ((!found)); then
        echo '  - no known cause in the logs; the last error lines:' >&2
        { grep -iE 'error|failed' <<< "$log_text" | grep -v CMAKE_ERROR_DEPRECATED | tail -8 | cut -c1-200 | sed 's/^/      /' >&2; } || true
    fi
    echo "Logs: ${logs[*]}" >&2
    exit 0
fi

missing_names=() missing_packages=() optional_names=() optional_packages=() no_package_names=() deferred_names=()
for row in "${requirements[@]}"; do
    IFS=';' read -r -a field <<< "$row"
    in_group=0
    for g in "${groups[@]}"; do [[ ${field[0]} == "$g" ]] && in_group=1; done
    ((in_group)) || continue
    status=0
    (eval "${field[2]}") >/dev/null 2>&1 || status=$?
    ((status == 0)) && continue
    if ((status == 2)); then deferred_names+=("${field[1]}"); continue; fi
    packages=''
    ((column)) && packages=${field[column - 1]:-}
    if [[ ${field[7]:-} == fetch ]]; then
        optional_names+=("${field[1]}")
        # shellcheck disable=SC2206
        [[ -n $packages ]] && optional_packages+=($packages)
        continue
    fi
    missing_names+=("${field[1]}")
    if ((column == 0)); then continue; fi
    # shellcheck disable=SC2206
    if [[ -n $packages ]]; then missing_packages+=($packages); else no_package_names+=("${field[1]}"); fi
done

unique() { printf '%s\n' "$@" | awk 'NF && !seen[$0]++'; }
mapfile -t missing_packages < <(unique "${missing_packages[@]}")
mapfile -t optional_packages < <(unique "${optional_packages[@]}")

if ((${#missing_names[@]} == 0 && ${#optional_names[@]} == 0 && ${#deferred_names[@]} == 0)); then
    echo "Dependencies for the Linux build: all present ($pretty)."
    exit 0
fi

echo "Missing on $pretty (family $family):"
((${#missing_names[@]})) && printf '  - %s\n' "${missing_names[@]}"
if ((${#deferred_names[@]})); then
    echo "Not checked until pkg-config is installed: ${deferred_names[*]}"
fi
if ((${#optional_names[@]})); then
    echo "Downloaded by CMake when absent (so not required): ${optional_names[*]}"
fi
if ((${#no_package_names[@]})); then
    echo "No package for this distribution: ${no_package_names[*]}. Build them from source (on Arch, the AUR may have them)."
fi
# An immutable system does not take the install command below.
if [[ -e /run/ostree-booted ]] || command -v steamos-readonly >/dev/null || command -v transactional-update >/dev/null; then
    echo 'This system looks immutable (ostree, SteamOS or transactional): the install command may not work. Build inside a container instead: bash packaging/linux/container.sh'
fi

skip_hint='To skip this check (the tools come from somewhere it cannot see): GOW3_SKIP_DEPS=1.'
if [[ $manager == none ]]; then
    echo 'This distribution is not recognized: install the items above with its package manager.'
    echo "$skip_hint"
    exit 1
fi
if ((${#missing_names[@]} == 0 && !install)); then exit 0; fi

case $manager in
pacman) install_command=(pacman -S --needed --noconfirm); install_note='' ;;
apt) install_command=(apt-get install -y); install_note='' ;;
dnf) install_command=(dnf install -y); install_note='' ;;
zypper) install_command=(zypper --non-interactive install); install_note='' ;;
esac

as_root() {
    if ((EUID == 0)); then "$@"; elif command -v sudo >/dev/null; then sudo "$@"; else echo "Run as root: $*" >&2; return 1; fi
}
as_root_text=$( ((EUID == 0)) && echo '' || echo 'sudo ')

apt_lists=${GOW3_APT_LISTS:-/var/lib/apt/lists}
apt_index_present() { compgen -G "$apt_lists/*_Packages*" >/dev/null || compgen -G "$apt_lists/*_Packages.*" >/dev/null; }
pacman_index_present() { compgen -G "${GOW3_PACMAN_SYNC:-/var/lib/pacman/sync}/*.db" >/dev/null; }

# Splits the packages into the ones the repositories of this release offer and the ones they lack. A package
# manager without a usable index (apt before apt-get update, pacman on a fresh image) cannot tell: everything
# is treated as available and the install command reports what is really missing.
available=() unavailable=()
classify_packages() {
    local package ok index=1
    available=() unavailable=()
    case $manager in
    apt) apt_index_present && command -v apt-cache >/dev/null || index=0 ;;
    pacman) pacman_index_present && command -v pacman >/dev/null || index=0 ;;
    dnf) command -v dnf >/dev/null || index=0 ;;
    zypper) command -v zypper >/dev/null || index=0 ;;
    esac
    for package in "$@"; do
        ok=1
        if ((index)); then
            case $manager in
            apt) apt-cache show "$package" >/dev/null 2>&1 || ok=0 ;;
            pacman) pacman -Si "$package" >/dev/null 2>&1 || ok=0 ;;
            dnf) dnf -q info "$package" >/dev/null 2>&1 || ok=0 ;;
            zypper) zypper --non-interactive info "$package" 2>&1 | grep -q '^Name' || ok=0 ;;
            esac
        fi
        if ((ok)); then available+=("$package"); else unavailable+=("$package"); fi
    done
}

to_install=("${missing_packages[@]}" "${optional_packages[@]}")
to_install=($(unique "${to_install[@]}"))
if ((!install)); then
    classify_packages "${to_install[@]}"
    if ((${#unavailable[@]})); then
        echo "Not in this release's repositories: ${unavailable[*]}"
        echo 'Build them from source, or use a newer release of the distribution.'
    fi
    if ((${#available[@]})); then echo "Install with: ${as_root_text}${install_command[*]} ${available[*]}"; fi
    echo 'Run again with --install (or set up the packages yourself).'
    echo "$skip_hint"
    exit 1
fi

if ((${#to_install[@]} == 0)); then
    echo 'There is nothing to install for this distribution.'
    echo "$skip_hint"
    exit 1
fi
echo "Install with: ${as_root_text}${install_command[*]} ${to_install[*]}"
[[ -n $install_note ]] && echo "$install_note"
if ((!assume_yes)); then
    if [[ -t 0 ]]; then
        read -r -p 'Install them now? [y/N] ' answer
        [[ $answer == [yYsS]* ]] || { echo 'Not installed.'; exit 1; }
    else
        echo 'Not installing without confirmation: pass --yes.' >&2
        exit 1
    fi
fi
if [[ $manager == apt ]]; then
    as_root apt-get update || echo 'apt-get update failed (a broken repository?); going on with the index it has.' >&2
fi
classify_packages "${to_install[@]}"
if ((${#unavailable[@]})); then
    echo "Not in this release's repositories: ${unavailable[*]}"
    echo 'Build them from source, or use a newer release of the distribution.'
fi
if ((${#available[@]})); then
    if ! as_root env DEBIAN_FRONTEND=noninteractive "${install_command[@]}" "${available[@]}"; then
        if [[ $manager != pacman ]]; then exit 1; fi
        # A stale or missing package database makes pacman -S fail (404). Syncing it alone would leave the system
        # half-upgraded, which Arch does not support, so the retry upgrades everything (pacman -Syu).
        echo 'pacman -S failed (probably an out-of-date package database). Retrying with pacman -Syu, which also updates the whole system.' >&2
        if ((!assume_yes)); then
            read -r -p 'Update the whole system now? [y/N] ' answer
            [[ $answer == [yYsS]* ]] || { echo 'Not installed.'; exit 1; }
        fi
        as_root pacman -Syu --needed --noconfirm "${available[@]}"
    fi
    echo 'Packages installed.'
fi
# Check again. pkg-config may have been installed just now, so what was "not checked" is asked for once more.
recheck=()
for g in "${groups[@]}"; do recheck+=("--$g"); done
if ((${#deferred_names[@]})) && [[ -z ${GOW3_DEPS_SECOND_PASS:-} ]]; then
    exec env GOW3_DEPS_SECOND_PASS=1 bash "${BASH_SOURCE[0]}" "${recheck[@]}" --install --yes
fi
exec bash "${BASH_SOURCE[0]}" "${recheck[@]}"
