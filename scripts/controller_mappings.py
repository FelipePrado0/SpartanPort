# SPDX-License-Identifier: GPL-2.0-or-later
"""Raw joystick capture and persistent SDL mappings, independent of the UI."""
from dataclasses import dataclass
import os
from pathlib import Path
import re
import tempfile

FILENAME = 'gamecontrollerdb.txt'
# Ask for the positive direction of each stick axis, so reversed axes can be recorded too.
STEPS = (
    ('a', 'Cross / A'), ('b', 'Circle / B'), ('x', 'Square / X'), ('y', 'Triangle / Y'),
    ('leftshoulder', 'L1 / LB'), ('rightshoulder', 'R1 / RB'),
    ('lefttrigger', 'L2 / LT'), ('righttrigger', 'R2 / RT'),
    ('back', 'Select / View / Share'), ('start', 'Start / Options'),
    ('leftstick', 'L3'), ('rightstick', 'R3'),
    ('dpup', 'D-pad up'), ('dpdown', 'D-pad down'),
    ('dpleft', 'D-pad left'), ('dpright', 'D-pad right'),
    ('leftx', 'Move the left stick to the right'), ('lefty', 'Move the left stick down'),
    ('rightx', 'Move the right stick to the right'), ('righty', 'Move the right stick down'),
)
STICK_AXES = {'leftx', 'lefty', 'rightx', 'righty'}
TRIGGERS = {'lefttrigger', 'righttrigger'}
GUID_RE = re.compile(r'[0-9a-fA-F]{32}')
INPUT_RE = re.compile(r'(?:b\d+|h\d+\.[1248]|[+-]?a\d+~?)')


@dataclass(frozen=True)
class State:
    buttons: tuple = ()
    axes: tuple = ()
    hats: tuple = ()


def mapping_line(guid, name, bindings, platform, *, require_cross=True):
    if not GUID_RE.fullmatch(guid) or (require_cross and not bindings.get('a')):
        raise ValueError('Map Cross / A before saving.')
    if platform not in ('Windows', 'Linux', 'Mac OS X'):
        raise ValueError('Unsupported controller platform.')
    if any(key not in dict(STEPS) or not INPUT_RE.fullmatch(value)
           for key, value in bindings.items()):
        raise ValueError('Invalid controller binding.')
    clean_name = re.sub(r'[,\r\n\x00-\x1f]', ' ', name).strip() or 'Custom controller'
    return ','.join([guid.lower(), clean_name] +
                    [f'{key}:{bindings[key]}' for key, _ in STEPS if key in bindings] +
                    [f'platform:{platform}', ''])


def mapping_key(line):
    parts = line.strip().split(',')
    if len(parts) < 3 or not GUID_RE.fullmatch(parts[0]):
        return None
    platform = next((part[9:] for part in parts[2:] if part.startswith('platform:')), '')
    return parts[0].lower(), platform


def load_mapping(path, guid, platform):
    """Prefer this platform's saved profile, falling back to a platform-neutral entry."""
    path = Path(path)
    if not path.is_file():
        return None
    lines = path.read_text(encoding='utf-8-sig').splitlines()
    for key in ((guid.lower(), platform), (guid.lower(), '')):
        for line in reversed(lines):
            if mapping_key(line) == key:
                return line.strip()
    return None


def mapping_bindings(line):
    if not line:
        return {}
    return {key: value for field in line.split(',')[2:] if ':' in field
            for key, value in (field.split(':', 1),)
            if key in dict(STEPS) and INPUT_RE.fullmatch(value)}


def edit_mapping(line, guid, name, platform, target, token):
    """Edit one input without losing other bindings or SDL metadata (including future fields)."""
    if target not in dict(STEPS) or (token is not None and not INPUT_RE.fullmatch(token)):
        raise ValueError('Invalid controller binding.')
    header = mapping_line(guid, name, {}, platform, require_cross=False).split(',')[:2]
    fields = []
    if line:
        if mapping_key(line) not in ((guid.lower(), platform), (guid.lower(), '')):
            raise ValueError('The saved mapping belongs to another controller.')
        fields = [part for part in line.split(',')[2:] if part and
                  not part.startswith((target + ':', 'platform:'))]
    if token is not None:
        fields.append(f'{target}:{token}')
    return ','.join(header + fields + [f'platform:{platform}', ''])


def save_mapping(path, line):
    """Replace only this device/platform; commit atomically so Cancel/failure keeps old data."""
    path = Path(path)
    key = mapping_key(line)
    if key is None or '\n' in line or '\r' in line:
        raise ValueError('Invalid controller mapping.')
    old = path.read_text(encoding='utf-8-sig') if path.exists() else ''
    lines = [row for row in old.splitlines() if mapping_key(row) != key]
    lines.append(line)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', newline='\n',
                                         dir=path.parent, delete=False) as output:
            temporary = Path(output.name)
            output.write('\n'.join(lines) + '\n')
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def configure_environment(env, data_dir):
    """Profiles apply before explicit SDL overrides; harmless on repeated game restarts."""
    path = Path(data_dir) / FILENAME
    if not path.is_file():
        return
    def read(source):
        try:
            return Path(source).read_text(encoding='utf-8-sig')
        except (OSError, UnicodeError) as error:
            print(f'Controller mappings: could not read {source}: {error}', flush=True)
            return ''
    local = read(path)
    explicit_file = env.get('SDL_GAMECONTROLLERCONFIG_FILE')
    explicit = ''
    if explicit_file and Path(explicit_file).is_file():
        explicit = read(explicit_file)
    rows = [row.strip() for text in (local, explicit, env.get('SDL_GAMECONTROLLERCONFIG', ''))
            for row in text.splitlines() if row.strip() and not row.lstrip().startswith('#')]
    # Keep the last occurrence, retaining the priority of explicit mappings on the same GUID.
    rows = list(reversed(dict.fromkeys(reversed(rows))))
    if rows:
        env['SDL_GAMECONTROLLERCONFIG'] = '\n'.join(rows)


class Capture:
    """Require a stable input and release between prompts; ignore normal stick noise."""
    def __init__(self, baseline):
        if any(baseline.buttons) or any(baseline.hats):
            raise ValueError('Release all buttons and the D-pad before starting.')
        self.baseline = baseline
        self.bindings = {}
        self.index = 0
        self.waiting_release = False
        self.candidate = None
        self.frames = 0
        self.error = ''

    @property
    def done(self):
        return self.index == len(STEPS)

    def neutral(self, state):
        return (not any(state.buttons) and not any(state.hats) and
                len(state.axes) == len(self.baseline.axes) and
                all(abs(a-b) < 7000 for a, b in zip(state.axes, self.baseline.axes)))

    def detect(self, state):
        target = STEPS[self.index][0]
        if target not in STICK_AXES:
            for i, value in enumerate(state.buttons):
                if value:
                    return f'b{i}'
            for i, value in enumerate(state.hats):
                if value in (1, 2, 4, 8):
                    return f'h{i}.{value}'
        if len(state.axes) != len(self.baseline.axes):
            return None
        changes = [(abs(value-rest), i, value-rest) for i, (value, rest)
                   in enumerate(zip(state.axes, self.baseline.axes))]
        if not changes:
            return None
        magnitude, i, delta = max(changes)
        if magnitude < 18000:
            return None
        rest = self.baseline.axes[i]
        if target in STICK_AXES:
            if abs(rest) > 12000:  # A trigger resting at an endpoint is not a centered stick.
                return None
            return f'a{i}' + ('~' if delta < 0 else '')
        if target in TRIGGERS and abs(rest) > 24000:
            return f'a{i}' + ('~' if delta < 0 else '')
        return ('+' if delta > 0 else '-') + f'a{i}'

    def feed(self, state):
        if self.done:
            return None
        if self.waiting_release:
            self.frames = self.frames + 1 if self.neutral(state) else 0
            if self.frames >= 3:
                self.waiting_release, self.frames = False, 0
            return None
        token = self.detect(state)
        if token != self.candidate:
            self.candidate, self.frames = token, 0
        if not token:
            return None
        self.frames += 1
        if self.frames < 3:
            return None
        target = STEPS[self.index][0]
        # Full axes cannot also act as another control; opposite half axes can be shared.
        axis = re.search(r'a\d+', token)
        for key, value in self.bindings.items():
            used_axis = re.search(r'a\d+', value)
            if token == value or (axis and used_axis and axis.group() == used_axis.group() and
                                  (token[0] == 'a' or value[0] == 'a')):
                self.error = 'That input is already mapped. Use another input or skip this step.'
                return None
        self.bindings[target] = token
        self.index += 1
        self.waiting_release, self.frames, self.candidate = True, 0, None
        self.error = ''
        return token

    def skip(self):
        if not self.done:
            if self.index == 0:
                raise ValueError('Map Cross / A before saving.')
            self.index += 1
            self.waiting_release, self.frames, self.candidate = True, 0, None
            self.error = ''

    def back(self):
        if self.index:
            self.index -= 1
            self.bindings = {key: value for key, value in self.bindings.items()
                             if key in dict(STEPS[:self.index])}
            self.waiting_release, self.frames, self.candidate = True, 0, None
            self.error = ''
