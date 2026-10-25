# Triage Labels

The skills speak in terms of five canonical triage roles. This file maps those roles to the actual label strings used in this repo's issue tracker.

| Label in mattpocock/skills | Label in our tracker | Meaning                                  |
| -------------------------- | -------------------- | ---------------------------------------- |
| `needs-triage`             | _(not used)_         | Don't apply; see notes                   |
| `needs-info`               | `question`           | Waiting on reporter for more information |
| `ready-for-agent`          | `ready-to-implement` | Fully specified, ready for an AFK agent  |
| `ready-for-human`          | `ready-for-human`    | Requires human implementation            |
| `wontfix`                  | `wontfix`            | Will not be actioned                     |

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding label string from this table.

Edit the right-hand column to match whatever vocabulary you actually use.

## Notes

- `awaiting-confirmation` is not a triage role. It marks an issue whose fix has shipped and is waiting for the reporter to confirm; it is closed after 7 days without a reply. Never use it for `needs-info`.
- `needs-triage` is not used. Issue forms ask for a category, and `.github/workflows/issue-opened.yml` turns it and the platform into labels (`crash`, `infinite-loading`, `performance`, `gameplay`, `saves`, `platform-support`, `area:*`, `platform:*`); the **Game won't launch** form adds `startup`. A new dropdown option must be mapped in `issue-triage.js` (a test checks it).
- `awaiting-response` is automatic (`.github/workflows/issue-awaiting-response.yml`): an open issue whose last human message is from a non-collaborator and older than 48h. A collaborator comment removes it. It is never added while the issue has `question`, `awaiting-test` or `awaiting-confirmation`.
- A bug opened with a port version older than the previous minor line of the newest stable release gets a retest comment and `question`. Remove `question` if the bug is known to still happen.
- `ready-for-human` does not exist on GitHub yet. Create it the first time it is needed.
