"""Read-only combined view of a dumped base game and its separate update folder.

The update (eboot.bin, sce_sys, ...) is laid over the base game in a temporary folder, so
neither dump is modified. Whatever the update does not touch is linked as a whole (a symlink,
or a junction / hard link on Windows without the symlink privilege, see mods.make_link), so
a 47 GB game costs a handful of links. Names are matched without regard to case.
"""
from pathlib import Path
import shutil
import tempfile

from mods import is_link, make_link, remove_link


def overlay(source, dest):
    """Lays source over dest: its files replace dest's, folders are merged, names are
    case-insensitive. dest holds links to the base; a folder both have is made a real one."""
    entries = {entry.name.casefold(): entry for entry in dest.iterdir()}
    for item in source.iterdir():
        match = entries.get(item.name.casefold(), dest / item.name)
        if item.is_dir() and match.exists() and match.is_dir():
            if is_link(match):  # a linked folder of the base: replace the link by a merged copy
                inner = match.resolve(strict=True)
                remove(match)
                match.mkdir()
                for child in inner.iterdir():
                    make_link(match / child.name, child)
            overlay(item, match)
        else:
            if match.exists() or is_link(match):
                remove(match)
            make_link(match, item)


def remove(path):
    """Removes a file, link or folder of a view without following links into the dumps."""
    path = Path(path)
    if is_link(path):
        # remove_link also drops the entry the Windows junction/hard-link fallback recorded in
        # mods.LINKED; without it is_link() keeps reporting a recreated real folder as a link.
        remove_link(path)
    elif path.is_dir():
        for child in list(path.iterdir()):
            remove(child)
        path.rmdir()
    else:
        path.unlink()


def create_view(base, update, out):
    """The folder holding base with update over it, made in out; the caller removes it."""
    from patches import game_title_id
    base, update = Path(base).resolve(), Path(update).resolve()
    base_id, update_id = game_title_id(base), game_title_id(update)
    if not base_id or base_id != update_id:
        raise ValueError(f'Game and update are not the same title: {base_id} / {update_id}')
    if base == update or base in update.parents or update in base.parents:
        raise ValueError('Choose two separate folders for the game and its update')
    if not (update / 'eboot.bin').is_file():
        raise ValueError('The update folder needs eboot.bin and sce_sys/param.sfo')
    Path(out).mkdir(parents=True, exist_ok=True)
    target = Path(tempfile.mkdtemp(prefix='game-update-', dir=out))
    try:
        for entry in base.iterdir():
            make_link(target / entry.name, entry)
        overlay(update, target)
    except BaseException:
        remove_view(target)
        raise
    return target


def remove_view(path):
    """Deletes a view made by create_view (the dumps stay untouched)."""
    if Path(path).exists():
        remove(path)
