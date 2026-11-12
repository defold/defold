---
name: review-editor-file
description: Review changes to a single specified editor code file against the branch-off point from the `origin/dev` branch.
---

# Review Editor File

Review changes against the branch-off point from the `origin/dev` branch for the file provided by the user.

Input file is most probably provided as a relative path to git repo root, not to cwd.

Focus findings on issues localized to changes in that file.

Assume all tests succeeded.
