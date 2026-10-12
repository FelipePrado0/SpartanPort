"""Read a PS4 PKG through the read-only LibOrbisPkg named-pipe bridge.

Executable preparation produces normal out/ build artifacts; game assets stay in
the original package. The helper must be built with tools/build_pkg_bridge.ps1.
"""
import os
from pathlib import Path, PurePosixPath
import struct
import subprocess
import threading
from types import SimpleNamespace
import uuid

PORT = Path(__file__).resolve().parent.parent
MAX_READ = 8 * 1024 * 1024


def pkg_metadata(path):
    """Read the unencrypted PARAM_SFO entry without starting the bridge."""
    from prepare import sfo
    with Path(path).open('rb') as stream:
        header = stream.read(32)
        if len(header) != 32 or header[:4] != b'\x7fCNT':
            raise ValueError('Expected a PS4 PKG file')
        count, = struct.unpack_from('>I', header, 16)
        table, = struct.unpack_from('>I', header, 24)
        if count > 65536:
            raise ValueError('Invalid PKG entry count')
        stream.seek(table)
        entries = stream.read(count * 32)
        if len(entries) != count * 32:
            raise ValueError('Truncated PKG entry table')
        for i in range(count):
            ident, _, flags, _, offset, size = struct.unpack_from('>6I', entries, i * 32)
            if ident == 0x1000:
                if flags & 0x80000000 or size > 1024 * 1024:
                    raise ValueError('Unsupported encrypted or oversized PKG metadata')
                stream.seek(offset)
                payload = stream.read(size)
                if len(payload) != size or len(payload) < 20:
                    raise ValueError('Truncated PKG metadata')
                try:
                    return sfo(payload)
                except (struct.error, UnicodeError, IndexError) as error:
                    raise ValueError('Invalid PKG metadata') from error
    raise ValueError('PKG has no PARAM_SFO entry')


class PkgClient:
    def __init__(self, pipe):
        self.pipe = pipe
        self.stream = None
        self.lock = threading.Lock()

    def _exact(self, size):
        data = bytearray()
        while len(data) < size:
            chunk = self.stream.read(size - len(data))
            if not chunk:
                raise OSError('PKG reader disconnected')
            data.extend(chunk)
        return bytes(data)

    def request(self, op, path, offset=0, count=0):
        encoded = path.encode('utf-8')
        with self.lock:
            if self.stream is None:
                self.stream = open('\\\\.\\pipe\\' + self.pipe, 'r+b', buffering=0)
            request = struct.pack('<IIqI', op, len(encoded), offset, count) + encoded
            view = memoryview(request)
            while view:
                written = self.stream.write(view)
                if not written:
                    raise OSError('PKG reader disconnected')
                view = view[written:]
            error, length = struct.unpack('<iI', self._exact(8))
            if length > 16 * 1024 * 1024:
                raise OSError('Invalid PKG response size')
            reply = self._exact(length)
            if error:
                raise OSError(error, 'PKG filesystem request failed', path)
            return reply

    def close(self):
        if self.stream:
            self.stream.close()
            self.stream = None


class PkgPath:
    def __init__(self, client, path=''):
        self.client, self.path = client, str(PurePosixPath(path)) if path else ''

    def __truediv__(self, child):
        return PkgPath(self.client, str(PurePosixPath(self.path) / child))

    def __str__(self):
        return 'pkg:/' + self.path

    @property
    def name(self):
        return PurePosixPath(self.path).name

    def stat(self):
        kind, size = struct.unpack('<Iq', self.client.request(1, self.path))
        return SimpleNamespace(st_size=size, kind=kind)

    def is_file(self):
        try:
            return self.stat().kind == 8
        except FileNotFoundError:
            return False

    def is_dir(self):
        try:
            return self.stat().kind == 4
        except FileNotFoundError:
            return False

    def exists(self):
        return self.is_file() or self.is_dir()

    def read_bytes(self):
        size = self.stat().st_size
        if size > 512 * 1024 * 1024:
            raise ValueError('PKG member exceeds preparation limit')
        data = bytearray()
        while len(data) < size:
            count = min(MAX_READ, size - len(data))
            chunk = self.client.request(2, self.path, len(data), count)
            if not chunk:
                raise OSError('Unexpected EOF in PKG member')
            if len(chunk) > count:
                raise OSError('Invalid PKG read size')
            data.extend(chunk)
        return bytes(data)

    def iterdir(self):
        data = self.client.request(3, self.path)
        count, = struct.unpack_from('<I', data)
        offset = 4
        for _ in range(count):
            _, length = struct.unpack_from('<II', data, offset)
            offset += 8
            name = data[offset:offset + length].decode('utf-8')
            offset += length
            yield self / name

    def rglob(self, pattern):
        if pattern != '*':
            raise ValueError('Only recursive wildcard listing is supported')
        for child in self.iterdir():
            yield child
            if child.is_dir():
                yield from child.rglob(pattern)

    def relative_to(self, parent):
        return PurePosixPath(self.path).relative_to(parent.path)


def game_source(path):
    if isinstance(path, PkgPath):
        return path
    path = Path(path)
    if path.suffix.lower() == '.pkg':
        pipe = os.environ.get('GOW3_PKG_PIPE')
        if not pipe:
            raise ValueError('Start PKG files with run.py or play-pkg.cmd')
        return PkgPath(PkgClient(pipe))
    return path


class PkgSession:
    def __init__(self, package):
        self.package = Path(package).resolve()
        self.process = None
        self.pipe = 'gow3-pkg-' + uuid.uuid4().hex
        self.client = PkgClient(self.pipe)

    def __enter__(self):
        helper = next((p for p in (PORT / 'bin/pkg-tools/gow3-pkg.exe', PORT / 'out/pkg-tools/gow3-pkg.exe') if p.is_file()), PORT / 'out/pkg-tools/gow3-pkg.exe')
        if not helper.is_file():
            raise ValueError('PKG helper missing: run tools/build_pkg_bridge.ps1')
        self.process = subprocess.Popen([str(helper), str(self.package), self.pipe], stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, text=True,
                                        creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        if self.process.stdout.readline().strip() != 'READY':
            error = self.process.stderr.read()
            self.close()
            raise ValueError(error.strip() or 'PKG reader did not start')
        self.previous_pipe = os.environ.get('GOW3_PKG_PIPE')
        os.environ['GOW3_PKG_PIPE'] = self.pipe
        return PkgPath(self.client)

    def close(self):
        self.client.close()
        if self.process:
            self.process.terminate()
            self.process.wait(timeout=10)
            self.process.stdout.close()
            self.process.stderr.close()
            self.process = None

    def __exit__(self, *args):
        self.close()
        if self.previous_pipe is None:
            os.environ.pop('GOW3_PKG_PIPE', None)
        else:
            os.environ['GOW3_PKG_PIPE'] = self.previous_pipe
