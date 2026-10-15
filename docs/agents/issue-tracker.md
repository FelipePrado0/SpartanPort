# Issue tracker: GitHub

Issues and specs for this repo live as GitHub issues on `FelipePrado0/gow3_pc`. Use the `gh` CLI for all operations.

## Which repo

This clone has two GitHub remotes and no `gh` default repo:

- `origin`: `FelipePrado0/gow3_pc`, where issues live.
- `jleandro`: `jleandrocampos/gow3_pc`, a fork with issues disabled.

Pass `-R FelipePrado0/gow3_pc` on every `gh issue` and `gh pr` command. Don't rely on `gh` inferring the repo from `git remote -v`.

## Conventions

- **Create an issue**: `gh issue create -R FelipePrado0/gow3_pc --title "..." --body "..."`. Use a heredoc for multi-line bodies.
- **Read an issue**: `gh issue view <number> -R FelipePrado0/gow3_pc --comments`, filtering comments by `jq` and also fetching labels.
- **List issues**: `gh issue list -R FelipePrado0/gow3_pc --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'` with appropriate `--label` and `--state` filters.
- **Comment on an issue**: `gh issue comment <number> -R FelipePrado0/gow3_pc --body "..."`
- **Apply / remove labels**: `gh issue edit <number> -R FelipePrado0/gow3_pc --add-label "..."` / `--remove-label "..."`
- **Close**: `gh issue close <number> -R FelipePrado0/gow3_pc --comment "..."`

## Comments are bilingual

Every comment posted to GitHub (issues, PRs, reviews, close comments) goes in English first, then Brazilian Portuguese, with the same content in each. This holds even when the other person writes only English.

```
**EN:**

<text>

---

**PT-BR:**

<same text translated>
```

Use exactly one `---`, between the two languages, and none inside either section.

If a comment already went out in one language only, edit it in place with `gh api -X PATCH repos/FelipePrado0/gow3_pc/issues/comments/<id> -F body=@file`. Don't post a second comment.

## Pull requests as a triage surface

**PRs as a request surface: no.** _(Set to `yes` if this repo treats external PRs as feature requests; `/triage` reads this flag.)_

When set to `yes`, PRs run through the same labels and states as issues, using the `gh pr` equivalents:

- **Read a PR**: `gh pr view <number> -R FelipePrado0/gow3_pc --comments` and `gh pr diff <number> -R FelipePrado0/gow3_pc` for the diff.
- **List external PRs for triage**: `gh pr list -R FelipePrado0/gow3_pc --state open --json number,title,body,labels,author,authorAssociation,comments` then keep only `authorAssociation` of `CONTRIBUTOR`, `FIRST_TIME_CONTRIBUTOR`, or `NONE` (drop `OWNER`/`MEMBER`/`COLLABORATOR`).
- **Comment / label / close**: `gh pr comment`, `gh pr edit --add-label`/`--remove-label`, `gh pr close`, each with `-R FelipePrado0/gow3_pc`.

GitHub shares one number space across issues and PRs, so a bare `#42` may be either: resolve with `gh pr view 42 -R FelipePrado0/gow3_pc` and fall back to `gh issue view 42 -R FelipePrado0/gow3_pc`.

## When a skill says "publish to the issue tracker"

Create a GitHub issue on `FelipePrado0/gow3_pc`.

## When a skill says "fetch the relevant ticket"

Run `gh issue view <number> -R FelipePrado0/gow3_pc --comments`.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a single issue with **child** issues as tickets.

- **Map**: a single issue labelled `wayfinder:map`, holding the Notes / Decisions-so-far / Fog body. `gh issue create -R FelipePrado0/gow3_pc --label wayfinder:map`.
- **Child ticket**: an issue linked to the map as a GitHub sub-issue (`gh api` on the sub-issues endpoint). Where sub-issues aren't enabled, add the child to a task list in the map body and put `Part of #<map>` at the top of the child body. Labels: `wayfinder:<type>` (`research`/`prototype`/`grilling`/`task`). Once claimed, the ticket is assigned to the driving dev.
- **Blocking**: GitHub's **native issue dependencies**, the canonical, UI-visible representation. Add an edge with `gh api --method POST repos/FelipePrado0/gow3_pc/issues/<child>/dependencies/blocked_by -F issue_id=<blocker-db-id>`, where `<blocker-db-id>` is the blocker's numeric **database id** (`gh api repos/FelipePrado0/gow3_pc/issues/<n> --jq .id`, _not_ the `#number` or `node_id`). GitHub reports `issue_dependencies_summary.blocked_by` (open blockers only, the live gate). Where dependencies aren't available, fall back to a `Blocked by: #<n>, #<n>` line at the top of the child body. A ticket is unblocked when every blocker is closed.
- **Frontier query**: list the map's open children (`gh issue list -R FelipePrado0/gow3_pc --state open`, scoped to the map's sub-issues / task list), drop any with an open blocker (`issue_dependencies_summary.blocked_by > 0`, or an open issue in the `Blocked by` line) or an assignee; first in map order wins.
- **Claim**: `gh issue edit <n> -R FelipePrado0/gow3_pc --add-assignee @me`, the session's first write.
- **Resolve**: `gh issue comment <n> -R FelipePrado0/gow3_pc --body "<answer>"` (bilingual, see above), then `gh issue close <n> -R FelipePrado0/gow3_pc`, then append a context pointer (gist + link) to the map's Decisions-so-far.
