#!/usr/bin/env bash
# Checks, and with --install installs, the system packages that building and packaging the Linux
# version need. The distribution is read from /etc/os-release (Arch, Debian/Ubuntu, Fedora/RHEL,
# openSUSE); each requirement is checked by what it provides (a command, pkg-config or a header),
# so a package from another source counts too.
#
#   packaging/linux/deps.sh [--build] [--packaging] [--install] [--yes] [--list]
#
#   --build, --packaging  the group to check (default: both)
#   --install             install what is missing with the package manager (sudo; asks first)
#   --yes                 do not ask
#   --list                only say which distribution and package manager were detected
#   --diagnose LOG...     read the logs of a failed build and say what is probably missing
#
# Without --install it only reports and exits 1 when something is missing. GOW3_SKIP_DEPS=1 makes
# it exit at once; GOW3_YES=1 is --yes; GOW3_FAMILY=arch|debian|fedora|suse overrides the detection.
set -euo pipefail

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
[[ -n ${GOW3_SKIP_DEPS:-} ]] && ((!diagnose)) && exit 0
((${#groups[@]})) || groups=(build packaging)
[[ -n ${GOW3_YES:-} ]] && assume_yes=1

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
if [[ $family == nix ]]; then
    echo 'NixOS: the build environment comes from shell.nix (nix-shell shell.nix).'
    exit 0
fi

have_header() {
    local dir
    for dir in /usr/include /usr/local/include /usr/include/x86_64-linux-gnu; do
        [[ -e $dir/$1 ]] && return 0
    done
    return 1
}
have_pc() { command -v pkg-config >/dev/null && pkg-config --exists "$@"; }
have_py() { python3 -c "import $1" 2>/dev/null; }

# group ; name ; check ; arch ; debian ; fedora ; suse   (an empty package list: nothing to install)
requirements=(
'build;C and C++ compiler;command -v gcc && command -v g++;gcc;build-essential;gcc gcc-c++;gcc gcc-c++'
'build;cmake;command -v cmake;cmake;cmake;cmake;cmake'
'build;ninja;command -v ninja;ninja;ninja-build;ninja-build;ninja'
'build;pkg-config;command -v pkg-config;pkgconf;pkg-config;pkgconf-pkg-config;pkg-config'
'build;git;command -v git;git;git;git;git'
'build;python3;command -v python3;python;python3;python3;python3'
'build;binutils;command -v objdump;binutils;binutils;binutils;binutils'
'build;Vulkan headers and loader;have_pc vulkan;vulkan-headers vulkan-icd-loader;libvulkan-dev;vulkan-headers vulkan-loader-devel;vulkan-headers vulkan-devel'
'build;SDL3;have_pc sdl3;sdl3;libsdl3-dev;SDL3-devel;SDL3-devel'
'build;FFmpeg;have_pc libavformat libavcodec libavutil libswscale libswresample;ffmpeg;libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev;ffmpeg-free-devel;ffmpeg-devel'
'build;Boost headers;have_header boost/version.hpp;boost;libboost-dev;boost-devel;boost-devel'
'build;fmt;have_pc fmt;fmt;libfmt-dev;fmt-devel;fmt-devel'
# robin-map is not listed: CMake fetches it when the system has no package (Debian 13 has none).
'build;Vulkan Memory Allocator;have_header vk_mem_alloc.h || have_header vma/vk_mem_alloc.h;vulkan-memory-allocator;libvulkan-memory-allocator-dev;VulkanMemoryAllocator-devel;VulkanMemoryAllocator-devel'
'build;xxHash;have_pc libxxhash;xxhash;libxxhash-dev;xxhash-devel;xxhash-devel'
'build;Zydis;have_header Zydis/Zydis.h;zydis;libzydis-dev;zydis-devel;zydis-devel'
'build;glslang compiler;command -v glslang || command -v glslangValidator;glslang;glslang-tools;glslang;glslang-devel'
'build;X11;have_pc x11;libx11;libx11-dev;libX11-devel;libX11-devel'
'packaging;patchelf;command -v patchelf;patchelf;patchelf;patchelf;patchelf'
'packaging;curl;command -v curl;curl;curl;curl;curl'
'packaging;file (used by appimagetool);command -v file;file;file;file;file'
'packaging;ldd;command -v ldd;glibc;libc-bin;glibc-common;glibc'
'packaging;Python Tk;have_py tkinter;tk;python3-tk;python3-tkinter;python3-tk'
'packaging;Python Pillow;have_py PIL.Image;python-pillow;python3-pil;python3-pillow;python3-Pillow'
'packaging;Python venv (to install PyInstaller);have_py venv && have_py ensurepip;python-pip;python3-venv python3-pip;python3-pip;python3-pip'
)

column=$(case $family in arch) echo 4 ;; debian) echo 5 ;; fedora) echo 6 ;; suse) echo 7 ;; *) echo 0 ;; esac)

# --diagnose: the build failed; say why in terms of packages. The requirement checks run first (a missing
# dependency is the usual cause), then the logs are searched for what the compiler, CMake or the linker
# could not find.
if ((diagnose)); then
    echo >&2
    echo "The build failed. Looking for the cause ($pretty):" >&2
    env -u GOW3_SKIP_DEPS bash "${BASH_SOURCE[0]}" --build >&2 || true
    log_text=$(cat "${logs[@]}" 2>/dev/null || true)
    # Which requirement a missing item belongs to: regex ; requirement name (first match wins).
    keywords=(
        'vk_mem_alloc|VulkanMemoryAllocator;Vulkan Memory Allocator'
        'vulkan|Vulkan;Vulkan headers and loader'
        'SDL3|sdl3;SDL3'
        'libav|libsw|ffmpeg|FFMPEG;FFmpeg'
        'xxhash|XXHASH;xxHash'
        'Zydis|zydis;Zydis'
        'glslang;glslang compiler'
        'boost|Boost;Boost headers'
        '(^|[^a-z])fmt($|[^a-z])|fmt/;fmt'
        'X11|x11;X11'
    )
    package_of() {  # package(s) of the requirement a missing item belongs to, "" when unknown
        local item=$1 pair name row
        for pair in "${keywords[@]}"; do
            if [[ $item =~ ${pair%;*} ]]; then
                name=${pair##*;}
                for row in "${requirements[@]}"; do
                    IFS=';' read -r -a field <<< "$row"
                    [[ ${field[1]} == "$name" ]] && ((column)) && { echo "${field[column - 1]}"; return; }
                done
            fi
        done
    }
    search_command() {
        case $manager in
        apt) echo "apt-file search $1" ;;
        dnf) echo "dnf provides '*/$1'" ;;
        pacman) echo "pacman -F $1" ;;
        zypper) echo "zypper search --provides $1" ;;
        esac
    }
    found=0
    report() {  # what, item, kind
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
    done < <(grep -oE 'Could not find a package configuration file provided by "[^"]+"' <<< "$log_text" | sed -E 's/.*"([^"]+)"/\1/' | sort -u)
    while read -r item; do [[ -n $item ]] && report "CMake could not find" "$item"
    done < <(grep -oE 'Could NOT find [A-Za-z0-9_+.-]+' <<< "$log_text" | awk '{print $4}' | sort -u)
    while read -r item; do [[ -n $item ]] && report "pkg-config module" "$item"
    done < <(grep -oE "(No package '[^']+' found|Package '?[A-Za-z0-9_.+-]+'?,? required by[^,]*)" <<< "$log_text" | sed -E "s/No package '([^']+)' found/\1/; s/Package '?([A-Za-z0-9_.+-]+)'?,? required by.*/\1/" | sort -u)
    while read -r item; do [[ -n $item ]] && report "header" "$item"
    done < <(grep -oE 'fatal error: [^ :]+: No such file or directory' <<< "$log_text" | awk '{print $3}' | tr -d ':' | sort -u)
    while read -r item; do [[ -n $item ]] && report "library" "lib$item"
    done < <(grep -oE 'cannot find -l[A-Za-z0-9_+.-]+' <<< "$log_text" | sed 's/cannot find -l//' | sort -u)
    while read -r item; do [[ -n $item ]] && report "program" "$item"
    done < <(grep -oE '[A-Za-z0-9_.+-]+: command not found' <<< "$log_text" | sed 's/: command not found//' | sort -u)
    if grep -qE 'No space left on device' <<< "$log_text"; then
        echo '  - the disk is full: free some space (the build needs a few GB in out/).' >&2; found=1
    fi
    if grep -qE 'internal compiler error: Killed|Killed signal terminated program|c\+\+: fatal error: Killed' <<< "$log_text"; then
        echo '  - the compiler was killed, usually for lack of memory: build with fewer jobs (ninja -C out/gpu -j4) or add swap.' >&2; found=1
    fi
    if ((!found)); then
        echo '  - no known cause in the logs; the last error lines:' >&2
        grep -iE 'error|failed' <<< "$log_text" | tail -8 | cut -c1-200 | sed 's/^/      /' >&2
    fi
    echo "Logs: ${logs[*]}" >&2
    exit 0
fi

missing_names=() missing_packages=()
for row in "${requirements[@]}"; do
    IFS=';' read -r -a field <<< "$row"
    in_group=0
    for g in "${groups[@]}"; do [[ ${field[0]} == "$g" ]] && in_group=1; done
    ((in_group)) || continue
    if (eval "${field[2]}") >/dev/null 2>&1; then continue; fi
    missing_names+=("${field[1]}")
    ((column)) && [[ -n ${field[column - 1]} ]] && missing_packages+=(${field[column - 1]})
done

if ((${#missing_names[@]} == 0)); then
    echo "Dependencies for the Linux build: all present ($pretty)."
    exit 0
fi

mapfile -t missing_packages < <(printf '%s\n' "${missing_packages[@]}" | awk 'NF && !seen[$0]++')
echo "Missing on $pretty (family $family):"
printf '  - %s\n' "${missing_names[@]}"

if [[ $manager == none ]]; then
    echo 'This distribution is not recognized: install the items above with its package manager.'
    exit 1
fi

# apt can say which packages the configured repositories lack.
unavailable=()
if [[ $manager == apt ]] && command -v apt-cache >/dev/null; then
    available=()
    for package in "${missing_packages[@]}"; do
        if apt-cache show "$package" >/dev/null 2>&1; then available+=("$package"); else unavailable+=("$package"); fi
    done
    # A stale package index hides packages: do not report them as missing before updating.
    if ((${#unavailable[@]})) && [[ ! -d /var/lib/apt/lists || -z $(ls -A /var/lib/apt/lists 2>/dev/null) ]]; then
        unavailable=()
    else
        missing_packages=("${available[@]}")
    fi
fi

case $manager in
pacman) install_command=(pacman -S --needed --noconfirm) ;;
apt) install_command=(apt-get install -y) ;;
dnf) install_command=(dnf install -y) ;;
zypper) install_command=(zypper --non-interactive install) ;;
esac

as_root() {
    if ((EUID == 0)); then "$@"; elif command -v sudo >/dev/null; then sudo "$@"; else echo "Run as root: $*" >&2; return 1; fi
}
as_root_text=$( ((EUID == 0)) && echo '' || echo 'sudo ')

if ((${#unavailable[@]})); then
    echo "Not in this release's repositories: ${unavailable[*]}"
    echo 'Build them from source, or use a newer release of the distribution.'
fi
if ((${#missing_packages[@]} == 0)); then exit 1; fi
echo "Install with: ${as_root_text}${install_command[*]} ${missing_packages[*]}"

if ((!install)); then
    echo 'Run again with --install (or set up the packages yourself).'
    exit 1
fi
if ((!assume_yes)); then
    if [[ -t 0 ]]; then
        read -r -p 'Install them now? [y/N] ' answer
        [[ $answer == [yYsS]* ]] || { echo 'Not installed.'; exit 1; }
    else
        echo 'Not installing without confirmation: pass --yes.' >&2
        exit 1
    fi
fi
[[ $manager == apt ]] && as_root apt-get update
as_root env DEBIAN_FRONTEND=noninteractive "${install_command[@]}" "${missing_packages[@]}"
echo 'Packages installed.'
recheck=()
for g in "${groups[@]}"; do recheck+=("--$g"); done
exec bash "${BASH_SOURCE[0]}" "${recheck[@]}"
