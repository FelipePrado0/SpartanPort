"""Change only Ubuntu mirror URLs in the temporary CI runner's public APT sources."""
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('/etc/apt')
paths = [root / 'sources.list']
paths += sorted((root / 'sources.list.d').glob('*.list'))
paths += sorted((root / 'sources.list.d').glob('*.sources'))
paths += sorted(root.glob('*mirror*.txt'))
paths += sorted((root / 'mirrors').glob('*.list'))
official = False
for path in paths:
    if not path.is_file():
        continue
    text = path.read_text()
    updated = text.replace('http://azure.archive.ubuntu.com/ubuntu', 'https://archive.ubuntu.com/ubuntu')
    updated = updated.replace('https://azure.archive.ubuntu.com/ubuntu', 'https://archive.ubuntu.com/ubuntu')
    updated = updated.replace('http://security.ubuntu.com/ubuntu', 'https://security.ubuntu.com/ubuntu')
    if text != updated:
        path.write_text(updated)
        print(f'Updated official Ubuntu mirror: {path}')
    assert 'azure.archive.ubuntu.com' not in updated, f'Azure mirror remains in {path}'
    official |= 'https://archive.ubuntu.com/ubuntu' in updated
assert official, 'No official Ubuntu HTTPS archive found in the effective source files'
