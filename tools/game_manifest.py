#!/usr/bin/env python3
"""Inventory of an extracted game folder, and comparison of two inventories.

Read-only: it never copies, moves, changes or uploads game files; it only reads the tree and
writes the manifest you ask for. The manifest keeps only the folder's name, not its full path.

  python -I tools/game_manifest.py create  <game folder> [-o manifest.json] [--format json|text]
                                            [--quick] [--exclude PATTERN]...
  python -I tools/game_manifest.py compare <A> <B> [--format text|json]

A and B are manifests (JSON or text written by this tool) or folders (inventoried on the spot).
compare reports files missing from B, extra in B, different (size or SHA-256), differing only in
letter case (Windows vs Linux extractions) and compared by size only (one side made with --quick).
Exit status: 0 no difference, 1 differences (or unreadable files in create), 2 usage/input error.
Spec: docs/specs/2026-10-29-load-game-manifest.md.
"""
import argparse
import fnmatch
import hashlib
import json
import os
import sys

FORMAT = 'spartanport-game-manifest'
VERSION = 1
BLOCK = 1 << 20


class InputError(Exception):
    pass


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(BLOCK), b''):
            digest.update(block)
    return digest.hexdigest()


def create(root, quick=False, exclude=(), progress=None):
    """Manifest dict for the folder `root`. Unreadable entries go to 'errors'."""
    if not os.path.isdir(root):
        raise InputError(f'{root}: not a folder')
    files, errors = [], []

    def relative(path):
        return os.path.relpath(path, root).replace(os.sep, '/')

    def walk_error(error):
        errors.append({'path': relative(error.filename or root), 'error': error.strerror or str(error)})

    for folder, dirs, names in os.walk(root, onerror=walk_error, followlinks=False):
        dirs.sort()
        for name in sorted(names):
            path = os.path.join(folder, name)
            rel = relative(path)
            if any(fnmatch.fnmatchcase(rel, pattern) for pattern in exclude):
                continue
            try:
                if not os.path.isfile(path):
                    continue  # sockets, fifos, broken links: not game data
                size = os.path.getsize(path)
                digest = None if quick else file_sha256(path)
            except OSError as error:
                errors.append({'path': rel, 'error': error.strerror or str(error)})
                continue
            files.append({'path': rel, 'size': size, 'sha256': digest})
            if progress:
                progress(len(files), rel)
    files.sort(key=lambda f: f['path'])
    errors.sort(key=lambda e: e['path'])
    return {'format': FORMAT, 'version': VERSION,
            'root': os.path.basename(os.path.normpath(os.path.abspath(root))),
            'hash': None if quick else 'sha256', 'files': files, 'errors': errors,
            'totals': {'files': len(files), 'bytes': sum(f['size'] for f in files), 'errors': len(errors)}}


def to_text(manifest):
    lines = [f"# {FORMAT} {VERSION} root={manifest['root']} files={manifest['totals']['files']} "
             f"bytes={manifest['totals']['bytes']} errors={manifest['totals']['errors']}"]
    lines += [f"{f['sha256'] or '-'} {f['size']} {f['path']}" for f in manifest['files']]
    lines += [f"# error {e['path']}: {e['error']}" for e in manifest['errors']]
    return '\n'.join(lines) + '\n'


def parse(text, source):
    """Manifest dict from JSON or text written by this tool."""
    stripped = text.lstrip('﻿ \t\r\n')
    if stripped.startswith('{'):
        try:
            manifest = json.loads(stripped)
        except ValueError as error:
            raise InputError(f'{source}: not a game manifest ({error})')
        if not isinstance(manifest, dict) or manifest.get('format') != FORMAT or not isinstance(manifest.get('files'), list):
            raise InputError(f'{source}: not a game manifest')
        return manifest
    files, errors, header = [], [], False
    for number, line in enumerate(stripped.splitlines(), 1):
        if line.startswith(f'# {FORMAT} '):
            header = True
        elif line.startswith('# error '):
            path, _, reason = line[8:].partition(': ')
            errors.append({'path': path, 'error': reason})
        elif line.strip() and not line.startswith('#'):
            parts = line.split(' ', 2)
            if len(parts) != 3 or not parts[1].isdigit():
                raise InputError(f'{source}:{number}: not a game manifest line')
            files.append({'path': parts[2], 'size': int(parts[1]), 'sha256': None if parts[0] == '-' else parts[0]})
    if not header:
        raise InputError(f'{source}: not a game manifest')
    return {'format': FORMAT, 'version': VERSION, 'files': files, 'errors': errors}


def load(source, progress=None):
    if os.path.isdir(source):
        return create(source, progress=progress)
    try:
        with open(source, encoding='utf-8') as stream:
            return parse(stream.read(), source)
    except OSError as error:
        raise InputError(f'{source}: {error.strerror or error}')
    except UnicodeDecodeError:
        raise InputError(f'{source}: not a game manifest')


def compare(a, b):
    files_a = {f['path']: f for f in a['files']}
    files_b = {f['path']: f for f in b['files']}
    missing = sorted(set(files_a) - set(files_b))
    extra = sorted(set(files_b) - set(files_a))
    lower_extra = {}
    for path in extra:
        lower_extra.setdefault(path.lower(), []).append(path)
    case_only = []
    for path in list(missing):
        match = lower_extra.get(path.lower())
        if match:
            other = match.pop(0)
            case_only.append({'a': path, 'b': other})
            missing.remove(path)
            extra.remove(other)
    different, unhashed = [], []
    for path in sorted(set(files_a) & set(files_b)):
        fa, fb = files_a[path], files_b[path]
        hashed = fa.get('sha256') and fb.get('sha256')
        if fa['size'] != fb['size'] or (hashed and fa['sha256'] != fb['sha256']):
            different.append({'path': path, 'a': {'size': fa['size'], 'sha256': fa.get('sha256')},
                              'b': {'size': fb['size'], 'sha256': fb.get('sha256')}})
        elif not hashed:
            unhashed.append(path)
    return {'missing': missing, 'extra': extra, 'different': different, 'case_only': case_only,
            'unhashed': unhashed, 'errors': {'a': a.get('errors', []), 'b': b.get('errors', [])},
            'totals': {'a': len(files_a), 'b': len(files_b)}}


def report_text(report):
    lines = [f"compared {report['totals']['a']} file(s) in A with {report['totals']['b']} in B"]
    lines += [f'missing {p}' for p in report['missing']]
    lines += [f'extra {p}' for p in report['extra']]
    lines += [f"different {d['path']} (A {d['a']['size']} bytes {d['a']['sha256'] or '-'}, "
              f"B {d['b']['size']} bytes {d['b']['sha256'] or '-'})" for d in report['different']]
    lines += [f"case-only A {c['a']} B {c['b']}" for c in report['case_only']]
    if report['unhashed']:
        lines.append(f"size-only {len(report['unhashed'])} file(s) (one side made with --quick)")
    for side in ('a', 'b'):
        lines += [f"unreadable in {side.upper()} {e['path']}: {e['error']}" for e in report['errors'][side]]
    if not (report['missing'] or report['extra'] or report['different'] or report['case_only']):
        lines.append('identical' + (' (sizes only for some files)' if report['unhashed'] else ''))
    return '\n'.join(lines) + '\n'


def has_differences(report):
    return bool(report['missing'] or report['extra'] or report['different'] or report['case_only'])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    commands = parser.add_subparsers(dest='command', required=True)
    make = commands.add_parser('create', help='inventory a game folder')
    make.add_argument('folder')
    make.add_argument('-o', '--output', help='write here instead of standard output')
    make.add_argument('--format', choices=('json', 'text'), default='json')
    make.add_argument('--quick', action='store_true', help='sizes only, no SHA-256 (fast triage)')
    make.add_argument('--exclude', action='append', default=[], metavar='PATTERN',
                      help='skip relative paths matching this glob (repeatable)')
    diff = commands.add_parser('compare', help='compare two manifests or folders')
    diff.add_argument('a')
    diff.add_argument('b')
    diff.add_argument('--format', choices=('text', 'json'), default='text')
    args = parser.parse_args(argv)

    def progress(count, path):
        if count % 100 == 0:
            print(f'\r{count} files...', end='', file=sys.stderr, flush=True)
    show = progress if sys.stderr.isatty() else None
    try:
        if args.command == 'create':
            manifest = create(args.folder, args.quick, args.exclude, show)
            text = json.dumps(manifest, indent=1) + '\n' if args.format == 'json' else to_text(manifest)
            if args.output:
                with open(args.output, 'w', encoding='utf-8', newline='\n') as stream:
                    stream.write(text)
            else:
                sys.stdout.write(text)
            if show:
                print(file=sys.stderr)
            totals = manifest['totals']
            print(f"{totals['files']} files, {totals['bytes']} bytes, {totals['errors']} unreadable", file=sys.stderr)
            return 1 if manifest['errors'] else 0
        report = compare(load(args.a, show), load(args.b, show))
        sys.stdout.write(json.dumps(report, indent=1) + '\n' if args.format == 'json' else report_text(report))
        return 1 if has_differences(report) else 0
    except InputError as error:
        print(f'game_manifest: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
