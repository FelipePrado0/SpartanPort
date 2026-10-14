import json
import os
import socket
import struct
import sys
import threading
import time
import uuid

# Do not change this, its the Client ID to show the RPC in discord please :)
CLIENT_ID = '1559731151793561680'
RETRY_SECONDS = 15
JOIN_SECONDS = 3
WINDOWS = sys.platform == 'win32'
HEADER = struct.Struct('<II')
HANDSHAKE, FRAME, CLOSE = 0, 1, 2


def pack(opcode, payload):
    body = json.dumps(payload, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    return HEADER.pack(opcode, len(body)) + body


def read_exact(transport, size):
    data = b''
    while len(data) < size:
        chunk = transport.read(size - len(data))
        if not chunk:
            raise ConnectionError('Discord closed the connection')
        data += chunk
    return data


def read_frame(transport):
    opcode, length = HEADER.unpack(read_exact(transport, HEADER.size))
    body = read_exact(transport, length) if length else b''
    return opcode, json.loads(body) if body else {}


def activity(started):
    return {'timestamps': {'start': int(started)}}


def set_activity(pid, value=None):
    args = {'pid': pid}
    if value is not None:
        args['activity'] = value
    return pack(FRAME, {'cmd': 'SET_ACTIVITY', 'args': args, 'nonce': str(uuid.uuid4())})


class PipeTransport:
    def __init__(self, path):
        self.handle = open(path, 'r+b', buffering=0)

    def write(self, data):
        self.handle.write(data)

    def read(self, size):
        return self.handle.read(size)

    def close(self):
        self.handle.close()


class SocketTransport:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(5)
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


def open_transport():
    kind = PipeTransport if WINDOWS else SocketTransport
    for path in candidates():
        try:
            return kind(path)
        except OSError:
            continue
    return None


def handshake(transport, client_id):
    transport.write(pack(HANDSHAKE, {'v': 1, 'client_id': str(client_id)}))
    opcode, data = read_frame(transport)
    if opcode != FRAME or data.get('evt') != 'READY':
        raise ConnectionError('Discord rejected the handshake')


def send_activity(transport, value):
    transport.write(set_activity(os.getpid(), value))
    _, data = read_frame(transport)
    if data.get('evt') == 'ERROR':
        raise ConnectionError('Discord rejected the activity')


class Presence:
    def __init__(self, client_id=None):
        self.transport = None
        self.thread = None
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.started = time.time()
        self.client_id = client_id or os.environ.get('GOW3_DISCORD_ID') or CLIENT_ID
        if not self.client_id or os.environ.get('GOW3_DISCORD') == '0':
            return
        self.thread = threading.Thread(target=self.run, name='discord-presence', daemon=True)
        self.thread.start()

    def run(self):
        while not self.stop.is_set():
            if self.connect():
                return
            self.stop.wait(RETRY_SECONDS)

    def connect(self):
        transport = open_transport()
        if transport is None:
            return False
        try:
            handshake(transport, self.client_id)
            send_activity(transport, activity(self.started))
        except (OSError, ValueError):
            transport.close()
            return False
        with self.lock:
            if self.stop.is_set():
                transport.close()
            else:
                self.transport = transport
        return True

    def close(self):
        self.stop.set()
        if self.thread:
            self.thread.join(JOIN_SECONDS)
        with self.lock:
            transport, self.transport = self.transport, None
        if not transport:
            return
        try:
            transport.write(set_activity(os.getpid()))
            transport.write(pack(CLOSE, {}))
        except OSError:
            pass
        finally:
            try:
                transport.close()
            except OSError:
                pass
