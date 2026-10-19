# Linux RPM (Fedora)

Builds the `gow3` RPM from the source at the checked-out commit, on Fedora 44, in the
GitHub Actions runner. The workflow regenerates the source tarball (six pinned
submodules + pre-applied FSR-Vulkan patches + pinned VMA header) and then calls the
existing spec, `packaging/linux/rpm/gow3.spec`.

## Running it from Windows (no Fedora install needed)

The workflow must first be committed and present on the repository's default branch
for **Run workflow** to appear. GitHub Actions must be enabled; manual runs require
repository write access. No local Linux tools are needed when using the web interface.

1. Push the branch or open a pull request. The **Linux RPM** workflow runs automatically
   and uploads the artifact `gow3-rpm` (binary RPM, SRPM and `SHA256SUMS`).
2. Or open **Actions → Linux RPM → Run workflow**, choose a branch and, optionally, type
   the tag of an existing release. When given, that exact tag is checked out and
   built (not the selected branch). An empty tag only builds and tests.
3. Download the artifact from the workflow run page (Artifacts → `gow3-rpm`), or with the
   GitHub CLI (`gh run download -n gow3-rpm`). The RPM is `gow3-<version>.x86_64.rpm`;
   no Fedora machine is required.

Publishing:

- On **release published**, the RPM and checksums are attached to that release.
- On a manual run with a tag, they are uploaded to that release (`--clobber`).
- Branch/PR runs never publish; they only produce the workflow artifact.

The tarball generated in CI is specific to the commit being built. The spec's default
`commit`/`Source0` values describe the earlier verified local snapshot; the workflow
overrides them with `rpmbuild --define` (`commit`, `source_version`, `snapshot_version`,
`source_sha256`, `vma_sha256`).

## Local reproduction

On any Linux host with Podman or Docker:

```sh
podman run --rm -it -v "$PWD:/src:ro" -v "$PWD/out-rpm:/rpm" fedora:44 bash -lc '
  dnf -y install git tar gzip coreutils rpm-build dnf5-plugins curl ca-certificates patch findutils shadow-utils util-linux
  git -c safe.directory=/src clone /src /work && cd /work && git submodule update --init
  dnf -y builddep packaging/linux/rpm/gow3.spec
  short=$(git rev-parse --short=7 HEAD)
  source_version=0.0.0-git$short
  snapshot_version="0^$(git show -s --format=%cd --date=format:%Y%m%d)git$short"
  topdir=/rpm/rpmbuild; mkdir -p "$topdir/SOURCES"
  curl -fsSL -o /tmp/vk_mem_alloc.h \
    https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/v3.1.0/include/vk_mem_alloc.h
  vma_sha=f063056705f2cd0ddd8d9da034f673b09814ca96aa45a958e9e687bf9f3fed15
  echo "$vma_sha  /tmp/vk_mem_alloc.h" | sha256sum --check -
  VMA_HEADER=/tmp/vk_mem_alloc.h VMA_SHA256=$vma_sha \
    bash packaging/linux/rpm/make-tarball.sh "$source_version" HEAD "$topdir/SOURCES/gow3-$source_version.tar.gz"
  source_sha=$(sha256sum "$topdir/SOURCES/gow3-$source_version.tar.gz" | cut -d" " -f1)
  useradd --create-home rpmbuilder
  chown -R rpmbuilder:rpmbuilder "$topdir"
  runuser -u rpmbuilder -- rpmbuild -ba packaging/linux/rpm/gow3.spec \
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
never applies a patch twice and makes no network access; the only downloads are the VMA
header and the submodules fetched by `git`.

## Dependencies and `iced-x86`

`iced-x86` is an **optional runtime** dependency used only by `scripts/isa_rewrite.py`
on CPUs missing MOVBE/BMI1/LZCNT/F16C. No Fedora 44 package provides
`python3dist(iced-x86)` (see the iced-x86 evaluation reports), so the spec declares it
as `Recommends:`, never as an invented `Provides` or a hard `Requires`.

Consequences:

- CPUs with all required instruction sets do not need this module. Successful game
  execution still requires runtime testing; the CI smoke does not prove it.
- On CPUs that need rewrite, the loader still refuses safely (`gow3-probe` exits 20 with
  a `STOP` message) when the module is missing. The safety check is not weakened.
- The CI smoke test installs with `install_weak_deps=False`, so it does not require an
  `iced-x86` provider that does not exist.
- Until a real Fedora provider exists, this RPM is suitable for GitHub Releases/COPR,
  not for submission to the official Fedora repositories (official packages must have
  all regular and weak dependencies satisfiable in Fedora).

## Limitations

- Built against Fedora 44 libraries; install on older Fedora/openSUSE is not guaranteed
  because ELF `Requires` are generated from the build host.
- The RPM requires the user's own game dump; no game files are shipped.
- `rpmlint` and a full desktop run are not part of the workflow; the test only checks
  that the package installs and `gow3-probe` starts without game data.
