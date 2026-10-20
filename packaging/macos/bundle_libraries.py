"""Relocate the x86-64 runtime libraries into a self-contained Mac application."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess


def resolve_library(name, loader, prefix):
    if name.startswith(('/usr/lib/', '/System/Library/')):
        return None
    prefix = Path(prefix).resolve()
    if name.startswith('@rpath/'):
        candidates = [prefix / 'lib' / name.removeprefix('@rpath/')]
    elif name.startswith('@loader_path/'):
        candidates = [Path(loader).parent / name.removeprefix('@loader_path/')]
    else:
        candidates = [Path(name)]
    for candidate in candidates:
        candidate = candidate.resolve()
        if candidate.is_relative_to(prefix) and candidate.is_file():
            return candidate
    raise ValueError(f'Missing or external dependency: {name} (loaded by {loader})')


def verify_contents(root):
    forbidden = {'eboot.bin', 'sce_module', 'sce_sys', 'savedata', 'user', 'cache',
                 'spartanport.ini', 'settings.json', 'mods.json', 'patches.json', 'fsr4_shaders'}
    for path in Path(root).rglob('*'):
        if path.name.lower() in forbidden or path.suffix.lower() in {'.log', '.spv', '.pkg', '.prx', '.sprx'}:
            raise ValueError(f'Package contains game or user data: {path}')


def output(*command):
    return subprocess.check_output(command, text=True)


def relocate(root, prefix):
    root = Path(root)
    lib = root / 'lib'
    lib.mkdir(exist_ok=True)
    queue = [root / 'spartanport-probe', root / 'spartanport-gpu-capabilities',
             root / 'gpu/libspartanportgpu.dylib', root / 'vulkan/libvulkan_kosmickrisp.dylib']
    fingerprints = {}
    originals = {}
    for file in queue:
        if 'x86_64' not in output('lipo', '-archs', str(file)).split():
            raise ValueError(f'Runtime library is not x86-64: {file}')
        own_id = [line.strip() for line in output('otool', '-D', str(file)).splitlines()[1:]]
        for line in output('otool', '-L', str(file)).splitlines()[1:]:
            name = line.strip().split(' (', 1)[0]
            if name in own_id:
                continue
            if name.endswith('/libspartanportgpu.dylib'):
                replacement = '@rpath/libspartanportgpu.dylib'
            else:
                source = resolve_library(name, originals.get(file, file), prefix)
                if source is None:
                    continue
                digest = hashlib.sha256(source.read_bytes()).digest()
                previous = fingerprints.get(source.name)
                if previous is not None and previous != digest:
                    raise ValueError(f'Conflicting libraries named {source.name}')
                if previous is None:
                    target = lib / source.name
                    shutil.copy2(source, target)
                    target.chmod(0o755)
                    fingerprints[source.name] = digest
                    originals[target] = source
                    queue.append(target)
                replacement = '@rpath/' + source.name
            subprocess.run(['install_name_tool', '-change', name, replacement, str(file)], check=True)
        if file.suffix == '.dylib':
            subprocess.run(['install_name_tool', '-id', '@rpath/' + file.name, str(file)], check=True)
        lines = output('otool', '-l', str(file)).splitlines()
        rpaths = []
        for index, line in enumerate(lines):
            if line.strip() == 'cmd LC_RPATH':
                rpaths.append(lines[index + 2].strip().removeprefix('path ').split(' (offset', 1)[0])
        for rpath in set(rpaths):
            subprocess.run(['install_name_tool', '-delete_rpath', rpath, str(file)], check=True)
        desired = (['@loader_path'] if file.parent == lib else ['@loader_path/../lib']) if file.suffix == '.dylib' else ['@executable_path/lib', '@executable_path/gpu']
        for rpath in desired:
            subprocess.run(['install_name_tool', '-add_rpath', rpath, str(file)], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('dependencies', type=Path)
    args = parser.parse_args()
    relocate(args.runtime, args.dependencies)
    verify_contents(args.runtime)
