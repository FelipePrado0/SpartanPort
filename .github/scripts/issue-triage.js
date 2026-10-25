// Issue triage automation, called from actions/github-script in issue-opened.yml and
// issue-awaiting-response.yml. Pure helpers are exported for issue-triage.test.js.

const CATEGORY_LABELS = {
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
};

const PLATFORM_LABELS = {
  Windows: 'platform:windows',
  Linux: 'platform:linux',
  macOS: 'platform:macos',
};

const COLLABORATORS = new Set(['OWNER', 'MEMBER', 'COLLABORATOR']);
const WAITING_ON_REPORTER = new Set(['question', 'awaiting-test', 'awaiting-confirmation']);
const AWAITING_LABEL = 'awaiting-response';
const AWAITING_AFTER_MS = 48 * 3600 * 1000;

// Issue forms render each field as "### <label>" followed by the answer.
function field(body, heading) {
  for (const section of (body || '').replace(/\r/g, '').split(/^### /m)) {
    const newline = section.indexOf('\n');
    if (newline < 0 || section.slice(0, newline).trim() !== heading) continue;
    const value = section.slice(newline + 1).trim();
    return value === '_No response_' ? '' : value;
  }
  return '';
}

function openedLabels(body) {
  const labels = [];
  const category = CATEGORY_LABELS[field(body, 'Category')];
  if (category) labels.push(category);
  const platform = PLATFORM_LABELS[field(body, 'Operating system') || field(body, 'Platform')];
  if (platform) labels.push(platform);
  return labels;
}

function parseVersion(text) {
  const m = /(\d+)\.(\d+)(?:\.(\d+))?/.exec(text || '');
  return m ? [Number(m[1]), Number(m[2]), Number(m[3] || 0)] : null;
}

function compareVersions(a, b) {
  return a[0] - b[0] || a[1] - b[1] || a[2] - b[2];
}

// Oldest supported version: the start of the minor line before the newest stable release.
function minSupported(tags) {
  const versions = tags.map(parseVersion).filter(Boolean).sort(compareVersions);
  const latest = versions.at(-1);
  if (!latest) return null;
  if (latest[1] > 0) return [latest[0], latest[1] - 1, 0];
  const previous = versions.filter((v) => v[0] < latest[0]).at(-1);
  return previous ? [previous[0], previous[1], 0] : null;
}

function versionComment(reported, min, latestUrl, allUrl) {
  const minText = min.join('.');
  return `**EN:**

This report is for version \`${reported}\`, which is older than ${minText}. Many problems from older versions are already fixed, so please try again on the [latest release](${latestUrl}) (Linux and macOS builds are listed on the [releases page](${allUrl})).

If the problem still happens there, reply here with the version you tested and the new \`user/last_run.log\`. This issue stays open.

---

**PT-BR:**

Este relato é da versão \`${reported}\`, anterior à ${minText}. Muitos problemas de versões antigas já foram corrigidos, então teste novamente na [última release](${latestUrl}) (as builds de Linux e macOS estão na [página de releases](${allUrl})).

Se o problema continuar, responda aqui com a versão testada e o novo \`user/last_run.log\`. Esta issue continua aberta.`;
}

function awaitingResponse(issue, comments, now, cutoff) {
  if (issue.pull_request) return false;
  if (Date.parse(issue.created_at) < cutoff) return false;
  if (issue.labels.some((l) => WAITING_ON_REPORTER.has(l.name))) return false;
  const last = comments.filter((c) => c.user?.type !== 'Bot').at(-1) || issue;
  if (COLLABORATORS.has(last.author_association)) return false;
  return now - Date.parse(last.created_at) > AWAITING_AFTER_MS;
}

async function checkVersion({ github, context, core }, issue) {
  const reported = field(issue.body, 'Port version');
  const version = parseVersion(reported);
  if (!version) {
    core.info(`Port version not readable: "${reported}"`);
    return null;
  }

  const releases = await github.paginate(github.rest.repos.listReleases, { ...context.repo, per_page: 100 });
  const stable = releases.filter((r) => !r.draft && !r.prerelease && parseVersion(r.tag_name));
  const min = minSupported(stable.map((r) => r.tag_name));
  if (!min) {
    core.info('No stable release to compare against');
    return null;
  }
  if (compareVersions(version, min) >= 0) {
    core.info(`${reported} is supported (minimum ${min.join('.')})`);
    return null;
  }

  const latest = stable.reduce((a, b) => (compareVersions(parseVersion(a.tag_name), parseVersion(b.tag_name)) >= 0 ? a : b));
  const allUrl = `${context.serverUrl}/${context.repo.owner}/${context.repo.repo}/releases`;
  await github.rest.issues.createComment({
    ...context.repo,
    issue_number: issue.number,
    body: versionComment(reported, min, latest.html_url, allUrl),
  });
  return 'question';
}

async function onOpened({ github, context, core }) {
  const issue = context.payload.issue;
  const labels = openedLabels(issue.body);
  if (issue.labels.some((l) => l.name === 'bug')) {
    try {
      const extra = await checkVersion({ github, context, core }, issue);
      if (extra) labels.push(extra);
    } catch (err) {
      core.warning(`Version check skipped: ${err.message}`);
    }
  }
  if (labels.length) await github.rest.issues.addLabels({ ...context.repo, issue_number: issue.number, labels });
  core.info(`Labels added: ${labels.join(', ') || 'none'}`);
}

async function onComment({ github, context, core }) {
  const { issue, comment } = context.payload;
  if (issue.pull_request || !COLLABORATORS.has(comment.author_association)) return;
  if (!issue.labels.some((l) => l.name === AWAITING_LABEL)) return;
  await github.rest.issues.removeLabel({ ...context.repo, issue_number: issue.number, name: AWAITING_LABEL });
  core.info(`Removed ${AWAITING_LABEL} from #${issue.number}`);
}

async function onSweep({ github, context, core }, cutoff) {
  const now = Date.now();
  const issues = await github.paginate(github.rest.issues.listForRepo, { ...context.repo, state: 'open', per_page: 100 });
  for (const issue of issues) {
    if (issue.pull_request) continue;
    const comments = issue.comments
      ? await github.paginate(github.rest.issues.listComments, { ...context.repo, issue_number: issue.number, per_page: 100 })
      : [];
    const want = awaitingResponse(issue, comments, now, cutoff);
    const has = issue.labels.some((l) => l.name === AWAITING_LABEL);
    if (want && !has) {
      await github.rest.issues.addLabels({ ...context.repo, issue_number: issue.number, labels: [AWAITING_LABEL] });
      core.info(`Added ${AWAITING_LABEL} to #${issue.number}`);
    } else if (!want && has) {
      await github.rest.issues.removeLabel({ ...context.repo, issue_number: issue.number, name: AWAITING_LABEL });
      core.info(`Removed ${AWAITING_LABEL} from #${issue.number}`);
    }
  }
}

module.exports = {
  CATEGORY_LABELS,
  PLATFORM_LABELS,
  field,
  openedLabels,
  parseVersion,
  compareVersions,
  minSupported,
  versionComment,
  awaitingResponse,
  onOpened,
  onComment,
  onSweep,
};
