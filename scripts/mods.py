#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Merge loose-file mods through links, preserving the original game and mod files.

A mod folder can also carry game patches: XML files in its patches/ folder, in the format of the built-in
patches (signatures, see patches.py). They are applied while the mod is enabled."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile

def child(folder, name):
    """`name` inside `folder`, matching an existing entry case-insensitively (mods made on
    Windows often differ from the game's lower-case names)."""
    folder = Path(folder)
    for entry in (folder.iterdir() if folder.is_dir() else ()):
        if entry.name.casefold() == name.casefold():
            return entry
    return folder / name


# Top-level folders a mod may shadow (casefolded). The game keeps its data in EXEC and one
# folder per language at the game root; dvdroot_ps4 is the shadPS4 layout many mods still use.
GAME_ROOTS = frozenset({'dvdroot_ps4', 'exec', 'dutch', 'english', 'french', 'german', 'italian',
                        'polish', 'portuguese', 'russian', 'spanish'})
# Never replaced through the overlay: executable patches go through the patch compiler.
PROTECTED = frozenset({'eboot.bin', 'sce_module', 'sce_sys'})


def data_roots(game=None):
    """Allowed data folders for this game only; never learn executable/system folders (#234)."""
    game = Path(game) if game else None
    if not game or not game.is_dir():
        return GAME_ROOTS
    return GAME_ROOTS | {e.name.casefold() for e in game.iterdir()
                         if e.is_dir() and e.name.casefold() not in PROTECTED}


def has_game_root(folder, roots=GAME_ROOTS):
    folder = Path(folder)
    return folder.is_dir() and any(e.is_dir() and e.name.casefold() in roots for e in folder.iterdir())


def game_path(relative, flat):
    """Where a mod file lands: in a game without dvdroot_ps4 (`flat`), a mod's dvdroot_ps4/
    prefix is dropped so dvdroot_ps4/EXEC/... replaces EXEC/... (#473)."""
    if flat and len(relative.parts) > 1 and relative.parts[0].casefold() == 'dvdroot_ps4':
        relative = Path(*relative.parts[1:])
    if relative.parts[0].casefold() in PROTECTED:
        raise ValueError(f'Executable/system replacement is unsupported: {relative}')
    return relative


def content_root(folder, roots=GAME_ROOTS):
    """(root, prefix): where a mod's files are and the game path they go to, or None.

    Accepted layouts: <mod>/<game folder>, <mod>/app0/<game folder> and one wrapper folder
    around either (an archive extracted into a folder of its name), where <game folder> is EXEC,
    a language folder or dvdroot_ps4 (GAME_ROOTS)."""
    folder = Path(folder)
    if not folder.is_dir():
        return None
    for wrapper in ('', 'app0'):
        base = child(folder, wrapper) if wrapper else folder
        if has_game_root(base, roots):
            return base, ''
    entries = [e for e in folder.iterdir() if not e.name.startswith('.')]
    folders = [e for e in entries if e.is_dir()]
    if len(folders) == 1 and not [e for e in entries if e.is_file() and
                                  e.suffix.casefold() not in METADATA]:
        return content_root(folders[0], roots)
    return None


# Files beside a mod's game folders that only describe it (a wrapper folder may hold them too).
METADATA = ('.txt', '.md', '.jpg', '.jpeg', '.png', '.ini', '.json')
IMAGES = ('.png', '.jpg', '.jpeg')
FIRST_IMAGES = ('preview', 'cover', 'screenshot')


def mod_info(folder, game=None):
    """What the launcher shows for a mod: name, author, version, description and url from an
    optional mod.json, else the folder name and the start of a readme; image is the mod's preview
    (preview, cover or screenshot first, else any picture) or None. Only reads inside the mod."""
    folder = Path(folder)
    layout = content_root(folder, data_roots(game))
    places = list(dict.fromkeys([folder, layout[0] if layout else folder]))
    info = {'name': folder.name, 'author': '', 'version': '', 'description': '', 'url': '', 'image': None}
    files = [e for place in places for e in sorted(place.iterdir(), key=lambda e: e.name.casefold())
             if e.is_file() and not e.is_symlink()]
    meta = next((e for e in files if e.name.casefold() == 'mod.json'), None)
    try:
        fields = json.loads(meta.read_text(encoding='utf-8')) if meta else {}
    except (OSError, ValueError):
        fields = {}
    for key in info.keys() - {'image'}:
        value = fields.get(key) if isinstance(fields, dict) else None
        if isinstance(value, str) and value.strip():
            info[key] = value.strip()
    if not info['url'].casefold().startswith(('http://', 'https://')):
        info['url'] = ''
    readme = next((e for e in files if e.stem.casefold() == 'readme' and e.suffix.casefold() in ('.txt', '.md')), None)
    if not info['description'] and readme:
        try:
            info['description'] = readme.read_text(encoding='utf-8', errors='replace').strip()[:400]
        except OSError:
            pass
    images = [e for e in files if e.suffix.casefold() in IMAGES]
    images.sort(key=lambda e: FIRST_IMAGES.index(e.stem.casefold()) if e.stem.casefold() in FIRST_IMAGES
                else len(FIRST_IMAGES))
    info['image'] = images[0] if images else None
    return info


ZIP_STAMP = '.from-zip'
ZIP_LIMIT = 16 << 30  # bytes a mod zip may unpack to (a zip bomb is refused)


def zip_members(archive):
    """(info, relative path) of the files in a mod zip. Anything that could write outside the mod's folder
    (absolute paths, "..", drive letters) or is a link is refused with a ValueError."""
    total, members = 0, []
    for info in archive.infolist():
        parts = Path(info.filename.replace('\\', '/')).parts
        if info.is_dir() and not parts:
            continue
        if not parts or Path(info.filename).is_absolute() or info.filename.startswith(('/', '\\')) \
                or '..' in parts or any(':' in part or '\0' in part or part.rstrip(' .') != part for part in parts):
            raise ValueError(f'unsafe path in the zip: {info.filename}')
        if (info.external_attr >> 16) & 0o170000 == 0o120000:
            raise ValueError(f'links are not allowed in a mod zip: {info.filename}')
        total += info.file_size
        if total > ZIP_LIMIT:
            raise ValueError('the zip unpacks to more than 16 GiB')
        members.append((info, Path(*parts)))
    return members


def unpack_zips(root, game=None):
    """Unpacks every <root>/<name>.zip into <root>/<name>/ (a mod dropped into the mods folder as a zip).
    Done again when the zip changes; a folder with that name that did not come from the zip is left alone.
    Returns the names unpacked now; a zip that cannot be unpacked is reported and skipped."""
    root = Path(root)
    done = []
    if not root.is_dir():
        return done
    for archive_path in sorted(root.glob('*.zip'), key=lambda p: p.name.casefold()):
        target = root / archive_path.stem
        try:
            resolved_target = target.resolve()
            if resolved_target.parent != root.resolve():
                print(f'Mods: {archive_path.name}: unsafe extraction folder', file=sys.stderr)
                continue
        except (OSError, RuntimeError) as error:
            print(f'Mods: {archive_path.name}: {error}', file=sys.stderr)
            continue
        stat = archive_path.stat()
        stamp = f'{stat.st_size} {stat.st_mtime_ns}'
        marker = target / ZIP_STAMP
        if target.exists() and not (marker.is_file() and marker.read_text() != stamp):
            if not marker.is_file():
                print(f'Mods: {archive_path.name}: a folder called {target.name} is already there; the zip is not unpacked',
                      file=sys.stderr)
            continue
        try:
            with zipfile.ZipFile(archive_path) as archive:
                members = zip_members(archive)
                destinations = [(info, target / relative) for info, relative in members]
                if any(not destination.resolve().is_relative_to(resolved_target)
                       for _, destination in destinations):
                    raise ValueError('unsafe path in the zip: destination outside the mod folder')
                if target.exists():
                    shutil.rmtree(target)
                for info, destination in destinations:
                    if info.is_dir():
                        destination.mkdir(parents=True, exist_ok=True)
                        continue
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with archive.open(info) as source, open(destination, 'wb') as out:
                        shutil.copyfileobj(source, out)
            target.mkdir(exist_ok=True)
            marker.write_text(stamp)
            done.append(archive_path.stem)
            kind = 'a mod' if (content_root(target, data_roots(game)) or patch_files(target)) else 'not a mod (no EXEC, language folder, dvdroot_ps4 or patches)'
            print(f'Mods: {archive_path.name} unpacked: {kind}', file=sys.stderr)
        except (OSError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
            shutil.rmtree(target, ignore_errors=True)
            print(f'Mods: {archive_path.name}: {error}', file=sys.stderr)
    return done


def patch_files(folder):
    """The game patch files (XML) of a mod: <mod>/patches/*.xml, with the same one wrapper folder
    around it that content_root accepts. A mod with only these has no files to replace."""
    folder = Path(folder)
    if not folder.is_dir():
        return []
    found = child(folder, 'patches')
    if found.is_dir():
        files = sorted((f for f in found.iterdir() if f.is_file() and f.suffix.casefold() == '.xml'),
                       key=lambda f: f.name.casefold())
        if files:
            return files
    entries = [e for e in folder.iterdir() if not e.name.startswith('.')]
    folders = [e for e in entries if e.is_dir()]
    if len(folders) == 1 and not [e for e in entries if e.is_file() and
                                  e.suffix.casefold() not in METADATA]:
        return patch_files(folders[0])
    return []


def discover(root, game=None):
    """Named mods: folders of `root` with an accepted layout (content_root) or with patch files. A mod
    dropped in as <name>.zip is unpacked first (unpack_zips)."""
    root = Path(root)
    if not root.is_dir():
        return []
    unpack_zips(root, game)
    roots = data_roots(game)
    return sorted((p.name for p in root.iterdir() if p.is_dir() and (content_root(p, roots) or patch_files(p))),
                  key=lambda name: (name.casefold(), name))


def selected(root, config, game=None):
    available = discover(root, game)
    if not config or not Path(config).is_file():
        return available
    settings = json.loads(Path(config).read_text())
    disabled_names = settings.get('disabled', [])
    if not isinstance(disabled_names, list) or not all(isinstance(n, str) for n in disabled_names):
        raise ValueError('Disabled mods must be a list of folder names')
    disabled = set(disabled_names)
    order = settings.get('order', [])
    if not isinstance(order, list) or not all(isinstance(n, str) for n in order):
        raise ValueError('Mod order must be a list of folder names')
    # New folders are enabled automatically and appended in alphabetical order.
    return list(dict.fromkeys(n for n in [*order, *available]
                             if n in available and n not in disabled))


def enabled_patch_files(root, config, enabled=True, game=None):
    """The patch files of the enabled mods, in load order (a later file wins where patches share a name)."""
    if not enabled:
        return []
    root = Path(root)
    return [f for name in selected(root, config, game) for f in patch_files(root / name)]


def mod_files(folder, game=None):
    """(mod path, source) of every file a mod replaces or adds under a GAME_ROOTS folder; the
    game path is game_path(mod path)."""
    roots = data_roots(game)
    layout = content_root(folder, roots)
    if not layout:
        if patch_files(folder):
            return  # patches only
        raise ValueError(f'{folder}: expected EXEC, a language folder or dvdroot_ps4 inside the mod folder')
    root, prefix = layout
    root = root.resolve()
    for directory, folders, files in os.walk(root, followlinks=False):
        directory = Path(directory)
        for name in [*folders, *files]:
            entry = directory / name
            # os.walk(followlinks=False) still descends into Windows junctions (they are not
            # symlinks), which would let a mod reach outside its folder; reject them too.
            if entry.is_symlink() or (hasattr(entry, 'is_junction') and entry.is_junction()):
                raise ValueError(f'Mod links are unsupported: {entry}')
        for name in sorted(files):
            source = directory / name
            relative = Path(prefix) / source.relative_to(root) if prefix else source.relative_to(root)
            # Readme/metadata stay outside the mounted game. This loader handles assets;
            # executable patches use the existing patch compiler, with address validation.
            if relative.parts[0].casefold() not in roots:
                if relative.parts[0].casefold() in PROTECTED:
                    raise ValueError(f'Executable/system replacement is unsupported: {source}')
                continue
            if not source.is_file():
                raise ValueError(f'Not a regular mod file: {source}')
            yield relative, source


def conflicts(root, names, game=None):
    """Files more than one of `names` (in load order) replaces: [(game path, [mods])], the last
    mod of each list wins. Paths match case-insensitively, as in build_overlay; unreadable mods
    are skipped."""
    owners = {}
    for name in names:
        try:
            for relative, _source in mod_files(Path(root) / name, game):
                relative = game_path(relative, flat=True)  # the game has no dvdroot_ps4
                key = relative.as_posix().casefold()
                owners.setdefault(key, (relative.as_posix(), []))[1].append(name)
        except (OSError, ValueError):
            continue
    return sorted((path, mods) for path, mods in owners.values() if len(mods) > 1)


def apply_preset(preset, available):
    """(order, disabled) of a saved preset for the mods present now: missing ones are dropped,
    new ones go last and enabled."""
    order = preset.get('order') if isinstance(preset.get('order'), list) else []
    disabled = preset.get('disabled') if isinstance(preset.get('disabled'), list) else []
    order = list(dict.fromkeys(n for n in [*order, *available] if n in available))
    return order, [n for n in disabled if n in available]


# Windows without the symlink privilege (Developer Mode off): directories become junctions,
# files hard links (copies across volumes). Those are not symlinks, so the overlay remembers
# what it linked and where to. A network share (SMB) refuses junctions too: a folder is then
# mirrored instead (real directories, files hard-linked), which is not a link and so is not
# recorded here - expand()/remove() treat it as the ordinary directory it has become.
LINKED = {}


def make_link(link, target):
    try:
        link.symlink_to(target, target_is_directory=target.is_dir())
        return
    except OSError:
        if os.name != 'nt':
            raise
    if target.is_dir():
        try:
            import _winapi
            _winapi.CreateJunction(str(target), str(link))
        except OSError:
            # A network share supports neither symlinks nor junctions. Mirror the folder:
            # real directories whose files are hard-linked (or copied across volumes) by
            # make_link, so the game reads real files through the view without copying the
            # data. CreateJunction leaves the empty directory behind when it refuses the
            # reparse point, so mkdir must tolerate it. Unlike a junction this is a plain
            # directory, so it is not recorded in LINKED and needs no special handling when
            # the view is expanded or removed.
            link.mkdir(exist_ok=True)
            for entry in target.iterdir():
                make_link(link / entry.name, entry)
            return
    else:
        try:
            os.link(target, link)
        except OSError:
            shutil.copy2(target, link)
    LINKED[str(link)] = target


def symlinks_work(directory):
    probe = Path(tempfile.mkdtemp(prefix='link-test-', dir=directory))
    try:
        (probe / 'link').symlink_to(probe, target_is_directory=True)
        return True
    except OSError:
        return False
    finally:
        shutil.rmtree(probe, ignore_errors=True)


def is_link(path):
    return path.is_symlink() or str(path) in LINKED or (hasattr(path, 'is_junction') and path.is_junction())


def link_target(path):
    return LINKED.get(str(path)) or path.resolve(strict=True)


def remove_link(path):
    if hasattr(path, 'is_junction') and path.is_junction():
        os.rmdir(path)
    else:
        path.unlink()
    LINKED.pop(str(path), None)


def expand(directory):
    """Materialize one directory level; never write through a directory link."""
    if is_link(directory):
        target = link_target(directory)
        if not target.is_dir():
            raise ValueError(f'File/directory conflict at {directory.name}')
        remove_link(directory)
        directory.mkdir()
        for entry in target.iterdir():
            make_link(directory / entry.name, entry)
    elif directory.exists() and not directory.is_dir():
        raise ValueError(f'File/directory conflict at {directory.name}')
    else:
        directory.mkdir(exist_ok=True)


def build_overlay(game, out, mods):
    game = Path(game).resolve(strict=True)
    flat = not child(game, 'dvdroot_ps4').is_dir()
    replacements = []
    owners = {}
    for name, root in mods:
        count = 0
        for relative, source in mod_files(root, game):
            relative = game_path(relative, flat)
            key = relative.as_posix().casefold()
            if key in owners:
                print(f'Mods: {relative}: {owners[key]} -> {name}', file=sys.stderr)
            owners[key] = name
            replacements.append((relative, source))
            count += 1
        print(f'Mods: {name}: {count} files', file=sys.stderr)
    if not replacements:
        return game
    out = Path(out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if os.name == 'nt' and not symlinks_work(out) and out.drive.casefold() != game.drive.casefold():
        # Hard links only reach files on the same volume: the overlay goes beside the game.
        out = game.parent
    overlay = Path(tempfile.mkdtemp(prefix='mod-game-', dir=out))
    try:
        for entry in game.iterdir():
            make_link(overlay / entry.name, entry)
        replaced = added = 0
        for relative, source in replacements:
            # Each component takes the game's spelling when it exists in another case.
            parent = overlay
            for part in relative.parts[:-1]:
                parent = child(parent, part)
                expand(parent)
            destination = child(parent, relative.parts[-1])
            if is_link(destination):
                replaced += link_target(destination).is_relative_to(game)
                remove_link(destination)
            elif destination.exists():
                raise ValueError(f'File/directory conflict: {relative}')
            else:
                added += 1
            make_link(destination, source)
        print(f'Mods: {replaced} game files replaced, {added} added', file=sys.stderr)
        return overlay
    except BaseException:
        shutil.rmtree(overlay)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--mods-dir', required=True, type=Path)
    parser.add_argument('--config', type=Path)
    parser.add_argument('--enabled', choices=('0', '1'), default='1')
    args = parser.parse_args()
    layers = []
    if args.enabled == '1':
        # shadPS4's loose overlay convention, beside the original game.
        legacy = Path(str(args.game.resolve()) + '-mods')
        if legacy.is_dir():
            layers.append((legacy.name, legacy))
        # A directly selected loose overlay is also accepted.
        if has_game_root(args.mods_dir, data_roots(args.game)):
            layers.append((args.mods_dir.name, args.mods_dir))
        else:
            for name in selected(args.mods_dir, args.config, args.game):
                layers.append((name, args.mods_dir / name))
    print(build_overlay(args.game, args.out, layers))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, TypeError) as error:
        print(f'Mods: {error}', file=sys.stderr)
        sys.exit(1)
