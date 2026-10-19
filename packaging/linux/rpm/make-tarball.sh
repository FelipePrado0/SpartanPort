#!/usr/bin/env bash
# Build a source archive from a commit and its already initialized submodules.
# No network access is performed. VMA_HEADER and VMA_SHA256 pin the external VMA header.
set -euo pipefail

usage() {
    echo "Usage: VMA_HEADER=/path/vk_mem_alloc.h VMA_SHA256=<sha256> $0 VERSION REF OUTPUT.tar.gz" >&2
    exit 2
}

[[ $# == 3 ]] || usage
version=$1
ref=$2
output=$3
[[ $version =~ ^[A-Za-z0-9.+_~-]+$ ]] || usage
[[ -n ${VMA_HEADER:-} && -f $VMA_HEADER && -n ${VMA_SHA256:-} ]] || usage
[[ $VMA_SHA256 =~ ^[[:xdigit:]]{64}$ ]] || usage

root=$(git rev-parse --show-toplevel) || { echo 'Run from a Git checkout.' >&2; exit 1; }
cd "$root"
if ! commit=$(git rev-parse --verify --quiet --end-of-options "$ref^{commit}"); then
    echo "Invalid source ref (must resolve to a commit): $ref" >&2
    exit 1
fi
dirty=$(git status --porcelain=v1 --untracked-files=all --ignore-submodules=all)
[[ -z $dirty ]] || { echo 'Refusing to archive a dirty checkout; commit or remove local changes first.' >&2; exit 1; }
source_date=$(git show -s --format=%ct "$commit")
header_hash=$(sha256sum "$VMA_HEADER" | awk '{print $1}')
[[ ${header_hash,,} == ${VMA_SHA256,,} ]] || { echo 'VMA_HEADER checksum does not match VMA_SHA256.' >&2; exit 1; }

mapfile -t submodule_lines < <(git config --blob "$commit:.gitmodules" --get-regexp '^submodule\..*\.path$' || true)
[[ ${#submodule_lines[@]} -gt 0 ]] || {
    echo 'No submodules are declared in the source ref .gitmodules.' >&2
    exit 1
}
gitlink_count=$(git ls-tree -r "$commit" | awk '$1 == "160000" { count++ } END { print count+0 }')
[[ $gitlink_count == "${#submodule_lines[@]}" ]] || {
    echo "Submodule declarations and gitlinks differ in source ref (${#submodule_lines[@]} declared, $gitlink_count gitlinks)." >&2
    exit 1
}

declare -A seen_paths=()
for entry in "${submodule_lines[@]}"; do
    read -r _ path <<<"$entry"
    [[ -n $path && ! ${seen_paths[$path]:-} ]] || {
        echo "Invalid or duplicate submodule path in .gitmodules: $path" >&2
        exit 1
    }
    seen_paths[$path]=1
    [[ -e $path/.git ]] || { echo "Submodule is not initialized: $path" >&2; exit 1; }
    expected=$(git ls-tree "$commit" -- "$path" | awk '$1 == "160000" { print $3 }')
    actual=$(git -C "$path" rev-parse --verify HEAD 2>/dev/null || true)
    [[ -n $expected && $actual == "$expected" ]] || {
        echo "Submodule is not at the commit recorded by $commit: $path" >&2
        exit 1
    }
    submodule_dirty=$(git -C "$path" status --porcelain=v1 --untracked-files=all --ignore-submodules=none)
    [[ -z $submodule_dirty ]] || { echo "Refusing to archive a dirty submodule: $path" >&2; exit 1; }
    nested=$(git -C "$path" ls-tree -r "$expected" | awk '$1 == "160000" { print $4 }')
    [[ -z $nested ]] || {
        echo "Nested submodules are not supported in this source archive ($path: $nested)." >&2
        exit 1
    }
done

output_dir=$(dirname -- "$output")
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd)
output="$output_dir/$(basename -- "$output")"
tmp=$(mktemp -d)
tmp_archive="$output.tmp.$$"
trap 'rm -rf -- "$tmp"; rm -f -- "$tmp_archive"' EXIT
prefix="gow3-$version"
mkdir -p "$tmp/tree/$prefix"

git archive --format=tar "$commit" | tar -xf - -C "$tmp/tree/$prefix"
for entry in "${submodule_lines[@]}"; do
    read -r _ path <<<"$entry"
    subcommit=$(git ls-tree "$commit" -- "$path" | awk '$1 == "160000" { print $3 }')
    mkdir -p "$tmp/tree/$prefix/$path"
    git -C "$path" archive --format=tar "$subcommit" | tar -xf - -C "$tmp/tree/$prefix/$path"
done

patch_dir="$tmp/tree/$prefix/gpu/patches/fsr-vulkan"
fsr_dir="$tmp/tree/$prefix/gpu/third_party/fsr-vulkan"
for patch_file in "$patch_dir"/*.patch; do
    [[ -f $patch_file ]] || { echo 'No FSR-Vulkan patches found in source.' >&2; exit 1; }
    # --force keeps each probe in the requested direction; --batch may reverse it.
    if patch --dry-run --force --fuzz=0 -p1 -d "$fsr_dir" -i "$patch_file" >/dev/null 2>&1; then
        if patch --reverse --dry-run --force --fuzz=0 -p1 -d "$fsr_dir" -i "$patch_file" >/dev/null 2>&1; then
            echo "FSR-Vulkan patch state is ambiguous: $(basename -- "$patch_file")" >&2
            exit 1
        fi
        if ! patch --force --no-backup-if-mismatch --reject-file=- --fuzz=0 -p1 -d "$fsr_dir" -i "$patch_file" >/dev/null; then
            echo "Failed to apply FSR-Vulkan patch: $(basename -- "$patch_file")" >&2
            exit 1
        fi
    elif patch --reverse --dry-run --force --fuzz=0 -p1 -d "$fsr_dir" -i "$patch_file" >/dev/null 2>&1; then
        echo "Patch is already applied in the pinned FSR-Vulkan source: $(basename -- "$patch_file")" >&2
        exit 1
    else
        echo "FSR-Vulkan patch is partial or does not match the pinned submodule: $(basename -- "$patch_file")" >&2
        exit 1
    fi
done

install -m 0644 "$VMA_HEADER" "$tmp/tree/$prefix/gpu/third_party/vk_mem_alloc.h"
tar --sort=name --mtime="@$source_date" --owner=0 --group=0 --numeric-owner \
    -C "$tmp/tree" -cf - "$prefix" | gzip -n >"$tmp_archive"
mv -f -- "$tmp_archive" "$output"
echo "Created $output"
