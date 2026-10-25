// node --test .github/scripts/issue-triage.test.js
const test = require('node:test');
const assert = require('node:assert/strict');
const t = require('./issue-triage.js');

const BODY = `### Category

Crash or freeze

### Port version

v2.5.9

### Operating system

Windows

### CPU

_No response_
`;

test('field reads issue form sections', () => {
  assert.equal(t.field(BODY, 'Category'), 'Crash or freeze');
  assert.equal(t.field(BODY, 'Port version'), 'v2.5.9');
  assert.equal(t.field(BODY, 'CPU'), '');
  assert.equal(t.field(BODY, 'Missing'), '');
  assert.equal(t.field('', 'Category'), '');
  assert.equal(t.field('### Port version\r\n\r\nv2.7.0\r\n', 'Port version'), 'v2.7.0');
});

test('category and platform map to labels', () => {
  for (const [option, label] of Object.entries({
    'Crash or freeze': 'crash',
    'Infinite loading': 'infinite-loading',
    'Graphics or visual glitch': 'area:graphics',
    'Graphics or display options': 'area:graphics',
    'Performance (FPS, stutter, CPU/GPU/RAM)': 'performance',
    'Audio': 'area:audio',
    'Audio or language': 'area:audio',
    'Gameplay, collision or progression': 'gameplay',
    'Gameplay options or cheats': 'gameplay',
    'Controls or input': 'area:input',
    'Launcher or settings': 'area:launcher',
    'Launcher or UI': 'area:launcher',
    'Save system': 'saves',
    'Platform or hardware support': 'platform-support',
  })) assert.equal(t.CATEGORY_LABELS[option], label, option);
  assert.equal(t.CATEGORY_LABELS.Other, undefined);
  assert.equal(t.PLATFORM_LABELS.Windows, 'platform:windows');
  assert.equal(t.PLATFORM_LABELS.Linux, 'platform:linux');
  assert.equal(t.PLATFORM_LABELS.macOS, 'platform:macos');
  assert.equal(t.PLATFORM_LABELS.All, undefined);
  assert.equal(t.PLATFORM_LABELS.Other, undefined);
});

test('openedLabels picks category and platform', () => {
  assert.deepEqual(t.openedLabels(BODY), ['crash', 'platform:windows']);
  assert.deepEqual(t.openedLabels('### Category\n\nPlatform or hardware support\n\n### Platform\n\nmacOS\n'),
    ['platform-support', 'platform:macos']);
  assert.deepEqual(t.openedLabels('### Category\n\nOther\n\n### Operating system\n\nOther\n'), []);
  assert.deepEqual(t.openedLabels(''), []);
});

// Every Category / Operating system / Platform option in every form must map to a label, except "Other" and "All".
test('every form option is mapped', () => {
  const fs = require('node:fs');
  const path = require('node:path');
  const dir = path.join(__dirname, '..', 'ISSUE_TEMPLATE');
  const unmapped = new Set(['Other', 'All']);
  let checked = 0;
  for (const file of fs.readdirSync(dir).filter((f) => f.endsWith('.yml') && f !== 'config.yml')) {
    for (const block of fs.readFileSync(path.join(dir, file), 'utf8').replace(/\r/g, '').split(/^  - type: /m)) {
      const label = /^      label: (.+)$/m.exec(block)?.[1];
      if (!block.startsWith('dropdown') || !['Category', 'Operating system', 'Platform'].includes(label)) continue;
      const options = [...block.matchAll(/^        - "?(.+?)"?$/gm)].map((m) => m[1]);
      const map = label === 'Category' ? t.CATEGORY_LABELS : t.PLATFORM_LABELS;
      for (const option of options) {
        assert.ok(map[option] || unmapped.has(option), `${file}: "${option}" has no label`);
        checked++;
      }
    }
  }
  assert.ok(checked >= 25, `only ${checked} options checked`);
});

test('parseVersion', () => {
  assert.deepEqual(t.parseVersion('v2.5.9'), [2, 5, 9]);
  assert.deepEqual(t.parseVersion('2.6.0-issue209-test1'), [2, 6, 0]);
  assert.deepEqual(t.parseVersion('windows-v2.7.0'), [2, 7, 0]);
  assert.deepEqual(t.parseVersion('windows-v2.1'), [2, 1, 0]);
  assert.equal(t.parseVersion('latest'), null);
  assert.equal(t.parseVersion(''), null);
});

test('minSupported is the previous minor line of the newest stable', () => {
  assert.deepEqual(t.minSupported(['windows-v2.7.0', 'windows-v2.6.0', 'windows-v2.5.0']), [2, 6, 0]);
  assert.deepEqual(t.minSupported(['windows-v2.4.1', 'windows-v2.4.0', 'windows-v2.3.0']), [2, 3, 0]);
  assert.deepEqual(t.minSupported(['v3.0.0', 'v2.9.1', 'v2.9.0']), [2, 9, 0]);
  assert.equal(t.minSupported(['windows-v2.0']), null);
  assert.equal(t.minSupported([]), null);
});

test('compareVersions is numeric', () => {
  assert.ok(t.compareVersions([2, 5, 9], [2, 6, 0]) < 0);
  assert.equal(t.compareVersions([2, 6, 0], [2, 6, 0]), 0);
  assert.ok(t.compareVersions([2, 10, 0], [2, 6, 0]) > 0);
});

test('versionComment is bilingual with one separator', () => {
  const text = t.versionComment('v2.5.9', [2, 6, 0], 'https://example/r', 'https://example/all');
  assert.match(text, /^\*\*EN:\*\*/);
  assert.match(text, /\*\*PT-BR:\*\*/);
  assert.equal(text.split('\n---\n').length, 2);
  assert.ok(text.includes('2.6.0') && text.includes('https://example/r') && text.includes('https://example/all'));
});

test('onOpened labels and comments through the API', async () => {
  const run = async (body) => {
    const calls = [];
    const github = {
      paginate: async () => [
        { tag_name: 'windows-v2.8.0-test1', prerelease: true, draft: false },
        { tag_name: 'windows-v2.7.0', prerelease: false, draft: false, html_url: 'https://example/2.7.0' },
        { tag_name: 'windows-v2.6.0', prerelease: false, draft: false },
      ],
      rest: { repos: {}, issues: {
        createComment: async (p) => calls.push(['comment', p.body]),
        addLabels: async (p) => calls.push(['add', p.labels]),
      } },
    };
    const context = { repo: { owner: 'o', repo: 'r' }, serverUrl: 'https://github.com',
      payload: { issue: { number: 1, body, labels: [{ name: 'bug' }] } } };
    await t.onOpened({ github, context, core: { info: () => 1, warning: () => 1 } });
    return calls;
  };
  assert.deepEqual(await run(BODY.replace('v2.5.9', 'v2.6.0')), [['add', ['crash', 'platform:windows']]]);
  const old = await run(BODY);
  assert.match(old[0][1], /older than 2\.6\.0.*https:\/\/example\/2\.7\.0/s);
  assert.deepEqual(old[1], ['add', ['crash', 'platform:windows', 'question']]);
});

const DAY = 24 * 3600 * 1000;
const NOW = Date.parse('2026-11-10T00:00:00Z');
const CUTOFF = Date.parse('2026-11-01T00:00:00Z');
const ago = (days) => new Date(NOW - days * DAY).toISOString();
const issue = (over = {}) => ({ created_at: ago(3), author_association: 'NONE', labels: [], ...over });
const comment = (assoc, days, type = 'User') => ({ author_association: assoc, created_at: ago(days), user: { type } });
const awaiting = (i, c = []) => t.awaitingResponse(i, c, NOW, CUTOFF);

test('awaitingResponse', () => {
  assert.equal(awaiting(issue()), true, '3 days without reply');
  assert.equal(awaiting(issue({ created_at: ago(1) })), false, '1 day');
  assert.equal(awaiting(issue(), [comment('OWNER', 2.5), comment('NONE', 2.1)]), true, 'reporter replied after collaborator');
  assert.equal(awaiting(issue(), [comment('NONE', 2.5), comment('COLLABORATOR', 2.1)]), false, 'collaborator answered last');
  assert.equal(awaiting(issue(), [comment('NONE', 1)]), false, 'reporter replied 1 day ago');
  assert.equal(awaiting(issue(), [comment('NONE', 0.1, 'Bot')]), true, 'bot comment ignored');
  assert.equal(awaiting(issue({ labels: [{ name: 'question' }] })), false);
  assert.equal(awaiting(issue({ labels: [{ name: 'awaiting-test' }] })), false);
  assert.equal(awaiting(issue({ labels: [{ name: 'awaiting-confirmation' }] })), false);
  assert.equal(awaiting(issue({ created_at: ago(12) })), false, 'before cutoff');
  assert.equal(awaiting(issue({ author_association: 'MEMBER' })), false, 'opened by collaborator');
  assert.equal(awaiting(issue({ pull_request: {} })), false, 'pull request');
});
