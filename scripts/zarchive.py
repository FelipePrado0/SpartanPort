# SPDX-License-Identifier: GPL-2.0-or-later
"""A packed .zar game image: the game reads it directly, the Python side gets a view.

A .zar (ZArchive, the format shadPS4 mounts as /app0) holds the game folder at its root:
eboot.bin, sce_module/, sce_sys/ and the data files. The native runtime reads it in place
(src/runtime_zarchive.c), so the game needs no extraction. The preparation scripts and the
launcher work on real files, so the entries they read are extracted once into a cache folder
through `spartanport-probe --zar-extract`; the game itself never uses that folder.

The cache belongs to one archive: its size and modification time identify it, and another
archive (or a repacked one) rebuilds it. Nothing is written beside the .zar.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

SUFFIX = '.zar'
# Entries, or folders with a trailing slash, that the preparation scripts and the launcher read.
VIEW = ('eboot.bin', 'sce_module/', 'sce_sys/')
# The WADs scripts/pause_menu.py and scripts/glyphs.py patch (copies are mounted over the archive):
# the button prompts, the QTE circle, and the lyre's discs (scripts/glyphs.py WADS).
PAUSE_MENU_WAD = 'EXEC/WAD/PS4_LE/R_UI.WAD'
GLYPH_WADS = ('EXEC/WAD/PS4_LE/R_UI.WAD', 'EXEC/WAD/PS4_LE/R_PERM.WAD', 'EXEC/WAD/PS4_LE/OLY150.WAD')
CREATE_NO_WINDOW = 0x08000000


class ArchiveError(ValueError):
    """The image cannot be read: not a ZArchive, damaged, or libzstd is missing."""


def is_archive(path):
    """Whether a game choice is a .zar file (a folder with one inside is not: pick the file)."""
    path = Path(path)
    return path.suffix.lower() == SUFFIX and path.is_file()


def probe_executable(root=None):
    """The probe beside the port (bin/, packaged) or in a source build (out/); SPARTAN_PROBE wins."""
    chosen = os.environ.get('SPARTAN_PROBE')
    if chosen and Path(chosen).is_file():
        return Path(chosen)
    root = Path(root or Path(__file__).resolve().parent.parent)
    name = 'spartanport-probe.exe' if sys.platform == 'win32' else 'spartanport-probe'
    for folder in ('bin', 'out'):
        if (root / folder / name).is_file():
            return root / folder / name
    return None


def _run(probe, *arguments, capture=False):
    if probe is None:
        raise ArchiveError('spartanport-probe was not found: build it (build.sh) or use a packaged port')
    flags = 0
    if sys.platform == 'win32':
        import ctypes
        if not ctypes.windll.kernel32.GetConsoleWindow():
            flags = CREATE_NO_WINDOW
    result = subprocess.run([str(probe), *map(str, arguments)], stdin=subprocess.DEVNULL,
                            text=True, encoding='utf-8', errors='replace',
                            stdout=subprocess.PIPE if capture else subprocess.DEVNULL,
                            stderr=subprocess.PIPE, creationflags=flags)
    if result.returncode:
        raise ArchiveError((result.stderr or '').strip() or f'{Path(probe).name} failed')
    return result.stdout or ''


class Listing:
    """Every folder and file of the archive with its size, from `--zar-list`."""

    def __init__(self, directories=(), files=None):
        self.directories = tuple(directories)
        self.files = dict(files or {})

    @classmethod
    def read(cls, archive, probe):
        directories, files = [], {}
        for line in _run(probe, '--zar-list', archive, capture=True).splitlines():
            fields = line.split('\t')
            if len(fields) == 2 and fields[0] == 'd':
                directories.append(fields[1])
            elif len(fields) == 3 and fields[0] == 'f':
                files[fields[1]] = int(fields[2])
        return cls(directories, files)

    def files_in(self, entry):
        """The files an entry names: itself, or everything under it when it ends with a slash."""
        if entry.endswith('/'):
            return [name for name in self.files if name.startswith(entry)]
        return [entry] if entry in self.files else []

    def write(self, path):
        """The listing as JSON, for prepare.py's resource census of the archive."""
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.files, ensure_ascii=False) + '\n', encoding='utf-8')
        return path


def listing(archive, probe=None):
    return Listing.read(Path(archive), probe or probe_executable())


def extract(archive, entry, destination, probe=None):
    """One file of the archive to destination, its folders made."""
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    _run(probe or probe_executable(), '--zar-extract', archive, entry, destination)
    return destination


def view(archive, cache, probe=None, entries=None, extra=()):
    """The folder holding the archive's eboot.bin, sce_module/ and sce_sys/, extracted once.

    `entries` is a Listing already read (run.py needs it for prepare.py's census); `extra`
    names files to add for this start (the WADs the pause menu and the glyphs read), whether the
    view was just built or already there. Raises ArchiveError when the archive cannot be read.
    """
    archive = Path(archive).resolve()
    cache = Path(cache)
    probe = probe or probe_executable()
    status = archive.stat()
    key = {'archive': str(archive), 'size': status.st_size, 'mtime_ns': status.st_mtime_ns}
    folder = cache / 'game'
    marker = cache / 'view.json'
    try:
        current = json.loads(marker.read_text(encoding='utf-8')) if marker.is_file() else None
    except (OSError, ValueError):
        current = None
    if current != key or not (folder / 'eboot.bin').is_file() or not (folder / 'sce_module').is_dir():
        entries = entries or Listing.read(archive, probe)
        shutil.rmtree(cache, ignore_errors=True)
        cache.mkdir(parents=True, exist_ok=True)
        _make(archive, entries, [name for entry in VIEW for name in entries.files_in(entry)], folder, probe)
        marker.write_text(json.dumps(key) + '\n', encoding='utf-8')
    elif extra:
        entries = entries or Listing.read(archive, probe)
    if extra:
        missing = [name for entry in extra for name in entries.files_in(entry)
                   if not (folder / name).is_file()]
        if missing:
            _make(archive, entries, missing, folder, probe)
    return folder


def _make(archive, entries, wanted, folder, probe):
    """Extracts wanted entries; the archive's folders are made so a dump's layout is visible."""
    for name in entries.directories:
        (folder / name).mkdir(parents=True, exist_ok=True)
    for name in wanted:
        extract(archive, name, folder / name, probe)
