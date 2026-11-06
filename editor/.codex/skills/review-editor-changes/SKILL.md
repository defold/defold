---
name: review-editor-changes
description: Review editor code changes, check Clojure formatting, and critique implementation choices.
---

# Review Editor Changes

Perform the following steps in order. Emit detailed output from each step before moving on to the next step. Use the branch-off point from the `origin/dev` branch to determine what changes should be considered. Include uncommitted working-tree changes.

- Use the projects $clojure-code-style skill in read-only mode to find and report formatting issues among the changed files without editing them.
- Run the $review-editor-file skill with 1 subagent per changed file and report the findings. When you hit the subagent limit, await results, then spawn additional subagents for each remaining file as needed.
- Critique the code changes and report things you would do differently. Explain your critiques in an elaborate fashion so that someone unfamiliar with the code can understand.
