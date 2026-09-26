---
name: draft-pr-description
description: Draft a pull request title and description from the current Git branch compared with a base branch, using relevant issue links when provided. Use when the user asks for PR text, a PR body, a merge request description, release-note-style PR summary, or wants branch changes summarized for reviewers.
---

# Draft PR Description

## Workflow

1. Identify the base branch and any issue links from the user request.
   - If no base is provided, use `origin/dev` when available, then `dev`, `origin/main`, `main`, `origin/master`, `master`.
   - Do not fetch remote branches unless the user asks.
2. Run the bundled context helper from the repository root. Add `--issue <issue-url>` for each supplied issue link; omit it when none are provided:

```bash
python3 .agents/skills/draft-pr-description/scripts/branch_pr_context.py --base <base-branch>
```

3. Review the helper output, then inspect additional files or diffs directly when the patch preview is truncated or a change is unclear.
4. Follow the user's requested template or format. Otherwise, use the default format below.

## Output Contract

By default, return one fenced Markdown code block with the PR title and body. Include relevant issue links in the body when supplied; use a closing keyword only for an issue the PR resolves.

```markdown
# Short title for PR

A few sentences explaining the purpose and effect of the change. Describe visible behavior when there is any; for internal changes, describe the affected developer or build workflow.

### Technical changes

- Summarize what changed in the code compared with the base branch.
- Call out meaningful modules, APIs, migrations, tests, build changes, and compatibility considerations.
- Include test evidence only if it is present in the branch, command history, or user request.
```

## Writing Rules

- Keep the title short and specific to the change.
- Preserve the exact URL of any issue link included in the draft. Treat links supplied only as background as context unless the user asks to include them.
- Use `Fix` or another closing keyword only when the PR fully resolves the linked GitHub issue; otherwise reference relevant links without implying closure.
- Do not invent issue links, issue titles, test results, performance numbers, platforms, or user impact.
- Prefer concrete behavior over vague wording such as "improves" or "updates".
- Make the opening paragraph understandable to the affected user, developer, or QA tester.
- Make `### Technical changes` useful to a reviewer who will inspect the diff.
- Mention risky areas, compatibility effects, or follow-up work only when the diff supports them.
- If the diff is too large or ambiguous, inspect the touched files before drafting. If important intent is still unknowable, say so briefly outside the code block and ask for the missing context.

## Helper Notes

The helper compares `merge-base(<base>, HEAD)..HEAD`, so it reflects what the branch would contribute to a PR. It reports uncommitted working-tree changes separately because they are not part of the branch diff.
