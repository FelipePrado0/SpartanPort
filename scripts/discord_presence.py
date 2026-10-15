import json
import os
import socket
import struct
import sys
import threading
import time
import uuid

CLIENT_ID = '1560048868677132431'
RETRY_SECONDS = 15
JOIN_SECONDS = 3
POLL_SECONDS = 0.1
READ_SECONDS = 5
WINDOWS = sys.platform == 'win32'
HEADER = struct.Struct('<II')
HANDSHAKE, FRAME, CLOSE, PING, PONG = range(5)


def pack(opcode, payload):
    body = json.dumps(payload, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    return HEADER.pack(opcode, len(body)) + body


def read_exact(transport, size, stop=None, timeout=READ_SECONDS):
    deadline = time.monotonic() + timeout if timeout is not None else None
    data = bytearray()
    while len(data) < size:
        if stop is not None and stop.is_set():
            raise ConnectionAbortedError('Discord presence stopped')
        if deadline is not None and time.monotonic() >= deadline:
            raise TimeoutError('Discord response timed out')
        try:
            chunk = transport.read(size - len(data))
        except TimeoutError:
            continue
        if not chunk:
            raise ConnectionError('Discord closed the connection')
        data.extend(chunk)
    return bytes(data)


def read_frame(transport, stop=None, timeout=READ_SECONDS):
    while True:
        opcode, length = HEADER.unpack(read_exact(transport, HEADER.size, stop, timeout))
        if length > 1024 * 1024:
            raise ValueError('Discord frame is too large')
        body = read_exact(transport, length, stop) if length else b''
        if opcode == PING:
            transport.write(HEADER.pack(PONG, length) + body)
            continue
        if opcode == PONG:
            continue
        if opcode == CLOSE:
            raise ConnectionError('Discord closed the connection')
        if opcode != FRAME:
            raise ValueError('Unexpected Discord opcode')
        data = json.loads(body) if body else {}
        if not isinstance(data, dict):
            raise ValueError('Discord frame must contain an object')
        return opcode, data


def activity(started):
    return {'timestamps': {'start': int(started)}}


def set_activity(pid, value=None):
    args = {'pid': pid}
    if value is not None:
        args['activity'] = value
    return pack(FRAME, {'cmd': 'SET_ACTIVITY', 'args': args, 'nonce': str(uuid.uuid4())})


class PipeTransport:
    def __init__(self, path):
        import ctypes
        import msvcrt

        self.handle = open(path, 'r+b', buffering=0)
        self.kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
        self.pipe = ctypes.c_void_p(msvcrt.get_osfhandle(self.handle.fileno()))
        # Nonblocking writes also prevent a stalled Discord client from holding up shutdown.
        mode = ctypes.c_uint32(1)  # PIPE_NOWAIT | PIPE_READMODE_BYTE
        if not self.kernel32.SetNamedPipeHandleState(self.pipe, ctypes.byref(mode), None, None):
            error = ctypes.WinError(ctypes.get_last_error())
            self.handle.close()
            raise error

    def write(self, data):
        if self.handle.write(data) != len(data):
            raise ConnectionError('Discord pipe could not accept the frame')

    def read(self, size):
        import ctypes

        deadline = time.monotonic() + POLL_SECONDS
        available = ctypes.c_uint32()
        while True:
            if not self.kernel32.PeekNamedPipe(self.pipe, None, 0, None, ctypes.byref(available), None):
                raise ctypes.WinError(ctypes.get_last_error())
            if available.value:
                return self.handle.read(min(size, available.value))
            if time.monotonic() >= deadline:
                raise TimeoutError
            time.sleep(0.01)

    def close(self):
        self.handle.close()


class SocketTransport:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(POLL_SECONDS)
        try:
            self.sock.connect(path)
        except OSError:
            self.sock.close()
            raise

    def write(self, data):
        self.sock.sendall(data)

    def read(self, size):
        return self.sock.recv(size)

    def close(self):
        self.sock.close()


def candidates():
    if WINDOWS:
        return [rf'\\?\pipe\discord-ipc-{index}' for index in range(10)]
    bases = [os.environ.get(name) for name in ('XDG_RUNTIME_DIR', 'TMPDIR', 'TMP', 'TEMP')] + ['/tmp']
    paths = []
    for base in dict.fromkeys(base for base in bases if base):
        for sub in ('', 'app/com.discordapp.Discord', 'snap.discord'):
            for index in range(10):
                paths.append(os.path.join(base, sub, f'discord-ipc-{index}'))
    return paths


def open_transport(stop=None):
    kind = PipeTransport if WINDOWS else SocketTransport
    for path in candidates():
        if stop is not None and stop.is_set():
            break
        try:
            return kind(path)
        except OSError:
            continue
    return None


def handshake(transport, client_id, stop=None):
    transport.write(pack(HANDSHAKE, {'v': 1, 'client_id': str(client_id)}))
    opcode, data = read_frame(transport, stop)
    if opcode != FRAME or data.get('evt') != 'READY':
        raise ConnectionError('Discord rejected the handshake')


def send_activity(transport, value, pid, stop=None):
    transport.write(set_activity(pid, value))
    _, data = read_frame(transport, stop)
    if data.get('evt') == 'ERROR':
        raise ConnectionError('Discord rejected the activity')


class Presence:
    def __init__(self, client_id=None, pid=None):
        self.transport = None
        self.thread = None
        self.stop = threading.Event()
        self.started = time.time()
        self.pid = os.getpid() if pid is None else pid
        self.client_id = client_id or os.environ.get('GOW3_DISCORD_ID') or CLIENT_ID
        if not self.client_id or os.environ.get('GOW3_DISCORD') == '0':
            return
        self.thread = threading.Thread(target=self.run, name='discord-presence', daemon=True)
        self.thread.start()

    def run(self):
        while not self.stop.is_set():
            self.connect()
            self.stop.wait(RETRY_SECONDS)

    def connect(self):
        transport = open_transport(self.stop)
        if transport is None:
            return
        self.transport = transport
        published = False
        try:
            handshake(transport, self.client_id, self.stop)
            send_activity(transport, activity(self.started), self.pid, self.stop)
            published = True
            while not self.stop.is_set():
                _, data = read_frame(transport, self.stop, timeout=None)
                if data.get('evt') == 'ERROR':
                    raise ConnectionError('Discord rejected the activity')
        except (OSError, ValueError):
            pass
        finally:
            try:
                if published and self.stop.is_set():
                    transport.write(set_activity(self.pid))
                    transport.write(pack(CLOSE, {}))
            except OSError:
                pass
            finally:
                self.transport = None
                try:
                    transport.close()
                except OSError:
                    pass

    def close(self):
        self.stop.set()
        if self.thread:
            self.thread.join(JOIN_SECONDS)
