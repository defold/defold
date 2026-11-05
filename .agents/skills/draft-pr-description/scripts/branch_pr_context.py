#!/usr/bin/env python3
"""Collect Git context for drafting a PR description."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from typing import Iterable, List, Optional, Sequence


BASE_CANDIDATES = (
    "origin/dev",
    "dev",
    "origin/main",
    "main",
    "origin/master",
    "master",
)


def run_git(args: Sequence[str], cwd: Optional[str] = None, check: bool = True) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if check and result.returncode != 0:
        message = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(f"git {' '.join(args)} failed: {message}")
    return result.stdout.rstrip("\n")


def git_ok(args: Sequence[str], cwd: Optional[str] = None) -> bool:
    result = subprocess.run(
        ["git", *args],
        cwd=cwd,
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return result.returncode == 0


def first_existing_ref(candidates: Iterable[str], cwd: str) -> Optional[str]:
    for ref in candidates:
        if git_ok(["rev-parse", "--verify", "--quiet", ref], cwd=cwd):
            return ref
    return None


def truncate(text: str, max_chars: int) -> str:
    if max_chars <= 0 or len(text) <= max_chars:
        return text
    omitted = len(text) - max_chars
    return f"{text[:max_chars]}\n\n[truncated {omitted} characters]"


def section(title: str, body: str) -> str:
    clean = body.strip()
    if not clean:
        clean = "(none)"
    return f"## {title}\n\n{clean}"


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect current-branch Git context for PR description drafting."
    )
    parser.add_argument(
        "--base",
        help="Base branch or ref. Defaults to origin/dev, dev, origin/main, main, origin/master, then master.",
    )
    parser.add_argument(
        "--issue",
        action="append",
        default=[],
        help="Issue link to include. Repeat for multiple links.",
    )
    parser.add_argument(
        "--max-diff-chars",
        type=int,
        default=30000,
        help="Maximum characters of patch preview to print. Use 0 for no truncation.",
    )
    parser.add_argument(
        "issues",
        nargs="*",
        help="Additional issue links, accepted for convenience.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)

    try:
        repo_root = run_git(["rev-parse", "--show-toplevel"])
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    repo_root = os.path.abspath(repo_root)
    base = args.base or first_existing_ref(BASE_CANDIDATES, repo_root)
    if not base:
        print(
            "Could not infer a base branch. Pass --base <branch>.",
            file=sys.stderr,
        )
        return 2

    try:
        current_branch = run_git(["branch", "--show-current"], cwd=repo_root)
        head = run_git(["rev-parse", "--short", "HEAD"], cwd=repo_root)
        merge_base = run_git(["merge-base", base, "HEAD"], cwd=repo_root)
        merge_base_short = run_git(["rev-parse", "--short", merge_base], cwd=repo_root)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    range_expr = f"{merge_base}..HEAD"
    issues = [*args.issue, *args.issues]
    issue_lines = "\n".join(f"- {issue}" for issue in issues)

    try:
        commits = run_git(
            ["log", "--reverse", "--date=short", "--pretty=format:%h %ad %s", range_expr],
            cwd=repo_root,
        )
        changed_files = run_git(
            ["diff", "--find-renames", "--find-copies", "--name-status", range_expr],
            cwd=repo_root,
        )
        diff_stat = run_git(
            ["diff", "--find-renames", "--find-copies", "--stat", range_expr],
            cwd=repo_root,
        )
        patch = run_git(
            ["diff", "--find-renames", "--find-copies", "--unified=3", range_expr],
            cwd=repo_root,
        )
        status = run_git(["status", "--short"], cwd=repo_root)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    branch_label = current_branch or "(detached HEAD)"
    header = "\n".join(
        [
            "# Branch PR Context",
            "",
            f"- Repository: {repo_root}",
            f"- Branch: {branch_label}",
            f"- HEAD: {head}",
            f"- Base: {base}",
            f"- Merge base: {merge_base_short}",
        ]
    )

    output: List[str] = [
        header,
        section("Provided Issues", issue_lines),
        section("Branch Commits", commits),
        section("Changed Files", changed_files),
        section("Diff Stat", diff_stat),
        section(
            "Working Tree Status",
            status
            or "clean; uncommitted changes are not included in the branch comparison",
        ),
        section("Patch Preview", truncate(patch, args.max_diff_chars)),
    ]

    print("\n\n".join(output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
