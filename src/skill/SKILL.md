---
name: drafts
description: >
  Capture, query, update, and run automation actions on notes in the Drafts
  app for Omarchy Linux via its headless CLI.
---

# Drafts CLI

Use `drafts` headless commands (all support `--json` and exit 0 on success, 1 on error):

- Create a draft: `drafts new --text "Meeting notes" --tag work/meetings,status::active --json`
- Pipe stdin into a new draft: `echo "# Idea" | drafts new --tag inbox --json`
- List drafts: `drafts list --folder inbox --workspace Writing --json`
- Search drafts (supports `"exact"`, `-omit`, `tag:foo`, `is:flagged`, `/regex/`): `drafts search 'tag:work -done' --json`
- Read a draft by UUID or title: `drafts get "Meeting notes" --json`
- Append / prepend text: `drafts append <uuid-or-title> "- [ ] Follow up" --json`
- Run an Action on a draft: `drafts run "Copy as HTML" <uuid-or-title> --json`
- List Workspaces or Actions: `drafts workspaces --json` / `drafts actions --json`
- Export / import full JSON backup: `drafts export backup.json --json`
- Open a `drafts://` URL: `drafts open-url "drafts://create?text=Hello&tag=quick" --json`
- Open the GUI or Quick Capture window: `drafts open [uuid]` / `drafts capture`
