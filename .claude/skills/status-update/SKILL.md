---
name: status-update
description: Update docs/STATUS.md and TODO.md checkboxes after completing or blocking work, following the evidence rules in docs/TODO.md. Use after finishing a TODO task, hitting a blocker, or when asked for project status.
---

Bring `docs/STATUS.md` and `docs/TODO.md` in line with the actual repository state.

1. Gather facts (do not guess):
   - `git rev-parse HEAD` and `git status --short` — record the exact commit and whether the tree is dirty.
   - Which TODO tasks have a deliverable in the repo (file paths, test output, build logs, bench evidence).
   - Current gate (G0–G5 from docs/SPEC.md §11) and whether its required evidence exists.

2. Update `docs/TODO.md`:
   - Check a box only if the deliverable AND its acceptance evidence exist. Cite the evidence path inline.
   - Hardware/bench tasks stay unchecked unless real bench evidence exists — CI, mocks or a successful build never count.
   - For blocked tasks, leave unchecked and append `— BLOCKED: <reason>; needs <action/owner>`.
   - Conditional tasks may be closed as N/A only with cited evidence and rationale.

3. Rewrite `docs/STATUS.md` (create if missing) with these sections:
   - **Commit:** exact SHA (+ dirty flag)
   - **Current gate:** G-number and what's missing to pass it
   - **Completed since last update:** task IDs with evidence paths
   - **Evidence:** paths to logs, captures manifests, test reports
   - **Blockers:** item, cause, required action
   - **Next action:** the next TODO task(s) in dependency order
   - **Owner assistance required:** concrete asks (photos, bench runs, approvals), or "none"

4. Show the user a short diff summary. Do not commit unless asked.
