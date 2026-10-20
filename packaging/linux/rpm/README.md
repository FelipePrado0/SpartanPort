# Linux RPM (Fedora)

Builds the `spartanport` RPM from the source at the checked-out commit, on Fedora 44, in the
GitHub Actions runner. The workflow regenerates the source tarball (six pinned
submodules + pre-applied FSR-Vulkan patches + pinned VMA header) and then calls the
existing spec, `packaging/linux/rpm/spartanport.spec`.

## Running it from Windows (no Fedora install needed)

The workflow must first be committed and present on the repository's default branch
for **Run workflow** to appear. GitHub Actions must be enabled; manual runs require
repository write access. No local Linux tools are needed when using the web interface.

1. Push the branch or open a pull request. The **Linux RPM** workflow runs automatically
   and uploads the artifact `spartanport-rpm` (binary RPM, SRPM and `SHA256SUMS`).
2. Or open **Actions → Linux RPM → Run workflow**, choose a branch and, optionally, type
   the tag of an existing release. When given, that exact tag is checked out and
   built (not the selected branch). An empty tag only builds and tests.
3. Download the artifact from the workflow run page (Artifacts → `spartanport-rpm`), or with the
   GitHub CLI (`gh run download -n spartanport-rpm`). The RPM is `spartanport-<version>.x86_64.rpm`;
   no Fedora machine is required.

Publishing:

- On **release published**, the RPM and checksums are attached to that release.
- On a manual run with a tag, they are uploaded to that release (`--clobber`).
- Branch/PR runs never publish; they only produce the workflow artifact.

The tarball generated in CI is specific to the commit being built. The spec's default
`commit`/`Source0` values describe the earlier verified local snapshot; the workflow
overrides them with `rpmbuild --define` (`commit`, `source_version`, `snapshot_version`,
`source_sha256`, `vma_sha256`).

The defaults now refer to SpartanPort 2.6.0 at upstream commit
`9749bd5b7e497e25ef6ea325f1b7f695fea07d24`, with source version `2.6.0-git9749bd5`.
Two offline tarball generations compared byte-for-byte; their SHA-256 is
`7d8ac8becadb4d6cce0dd64b627ae348ab51c202497809a0ad9a5100bec19219`.
CI derives the release version from `VERSION` in the launcher and adds the date and
short commit. The package, command and desktop entry are now named `spartanport`;
the old experimental `gow3` RPM is not silently removed or replaced.

## Local reproduction

On any Linux host with Podman or Docker:

```sh
podman run --rm -it -v "$PWD:/src:ro" -v "$PWD/out-rpm:/rpm" fedora:44 bash -lc '
  dnf -y install git tar gzip coreutils rpm-build dnf5-plugins curl ca-certificates patch findutils shadow-utils util-linux
  git -c safe.directory=/src clone /src /work && cd /work && git submodule update --init
  dnf -y builddep packaging/linux/rpm/spartanport.spec
  short=$(git rev-parse --short=7 HEAD)
  release_version=2.6.0 # Must match VERSION in launcher/spartanport_launcher_win.py.
  source_version=$release_version-git$short
  snapshot_version="$release_version^$(git show -s --format=%cd --date=format:%Y%m%d)git$short"
  topdir=/rpm/rpmbuild; mkdir -p "$topdir/SOURCES"
  cp packaging/linux/rpm/*.patch "$topdir/SOURCES/"
  curl -fsSL -o /tmp/vk_mem_alloc.h \
    https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/v3.1.0/include/vk_mem_alloc.h
  vma_sha=f063056705f2cd0ddd8d9da034f673b09814ca96aa45a958e9e687bf9f3fed15
  echo "$vma_sha  /tmp/vk_mem_alloc.h" | sha256sum --check -
  # Fedora 44 ships Vulkan-Headers 1.4.341; FSR 4.1.1 needs the extension added in
  # 1.4.344. Build against the headers the AppImage CI pins (build-time only).
  vk_headers=vulkan-headers-1.4.357.0.tar.gz
  vk_headers_sha=e87dce08116151f6b6d7de6b6faf41498e87e6cf848ff16fa3bd5402190ad4a3
  curl -fsSL -o "$topdir/SOURCES/$vk_headers" \
    https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz
  echo "$vk_headers_sha  $topdir/SOURCES/$vk_headers" | sha256sum --check -
  VMA_HEADER=/tmp/vk_mem_alloc.h VMA_SHA256=$vma_sha \
    bash packaging/linux/rpm/make-tarball.sh "$source_version" HEAD "$topdir/SOURCES/spartanport-$source_version.tar.gz"
  source_sha=$(sha256sum "$topdir/SOURCES/spartanport-$source_version.tar.gz" | cut -d" " -f1)
  useradd --create-home rpmbuilder
  chown -R rpmbuilder:rpmbuilder "$topdir"
  runuser -u rpmbuilder -- rpmbuild -ba packaging/linux/rpm/spartanport.spec \
    --define "_topdir $topdir" \
    --define "commit $(git rev-parse HEAD)" \
    --define "source_version $source_version" \
    --define "snapshot_version $snapshot_version" \
    --define "source_sha256 $source_sha" \
    --define "vma_sha256 $vma_sha"
  cp "$topdir"/RPMS/x86_64/*.rpm "$topdir"/SRPMS/*.rpm /rpm/
'
```

The tarball build refuses a dirty checkout and uninitialized/divergent submodules. It
never applies a patch twice and makes no network access. Provisioning downloads the
Fedora build/runtime packages, VMA header, Vulkan-Headers archive and submodules;
these downloads occur outside `rpmbuild` and the tarball generator.

## Vulkan headers

Fedora 44 provides `vulkan-headers` 1.4.341, which does not define
`VK_VALVE_shader_mixed_float_dot_product`; the extension was added in Vulkan 1.4.344 and
is required by FSR 4.1.1 (`gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp`).
Instead of disabling FSR or patching the source, the RPM builds against the same pinned
Vulkan-Headers tag the AppImage CI uses (`vulkan-sdk-1.4.357.0`, `Source1`, SHA-256
verified in `%prep` and in this workflow). The headers are build-time only: the binaries
still load the system Vulkan loader and drivers at runtime, and a driver without the
extension keeps FSR 4.1.1 unavailable through the existing feature check. Windows and
AppImage builds are untouched.

## Dependencies and `iced-x86`

`iced-x86` is an **optional runtime** dependency used only by `scripts/isa_rewrite.py`
on CPUs missing MOVBE/BMI1/LZCNT/F16C. No Fedora 44 package provides
`python3dist(iced-x86)` (see the iced-x86 evaluation reports), so the spec declares it
as `Recommends:`, never as an invented `Provides` or a hard `Requires`.

Consequences:

- CPUs with all required instruction sets do not need this module. Successful game
  execution still requires runtime testing; the CI smoke does not prove it.
- On CPUs that need rewrite, the loader still refuses safely (`spartanport-probe` exits 20 with
  a `STOP` message) when the module is missing. The safety check is not weakened.
- The CI smoke test installs with `install_weak_deps=False`, so it does not require an
  `iced-x86` provider that does not exist.
- Until a real Fedora provider exists, this RPM is suitable for GitHub Releases/COPR,
  not for submission to the official Fedora repositories (official packages must have
  all regular and weak dependencies satisfiable in Fedora).

## Limitations

- The RPM applies `0001-size-proc-task-path-for-dirent.patch` to size the Linux
  CPU-report path buffer for `dirent.d_name`. This fixes the GCC 16
  `-Werror=format-truncation` build failure without disabling warnings or changing
  AppImage/Windows sources.

- Built against Fedora 44 libraries; install on older Fedora/openSUSE is not guaranteed
  because ELF `Requires` are generated from the build host.
- The RPM requires the user's own game dump; no game files are shipped.
- `rpmlint` and a full desktop run are not part of the workflow; the test only checks
  that the package installs and `spartanport-probe` starts without game data.
