#!/usr/bin/env python
"""Dispatch all console builds from one GitHub Actions runner."""

import os
import subprocess
import uuid


CONSOLES = (
    ("Switch", "defold/defold-switch", "arm64-nx64", "SWITCH_PRIVATE_REF"),
    ("PlayStation", "defold/defold-ps4", "x86_64-ps4,x86_64-ps5", "PS_PRIVATE_REF"),
    ("Xbox", "defold/defold-xbox", "x86_64-xbone", "XBOX_PRIVATE_REF"),
)


def dispatch_builds(env):
    branch = env["BUILD_BRANCH"]
    private_ref = branch if branch in ("master", "beta", "dev") else "dev"
    channel = {"master": "stable", "beta": "beta"}.get(branch, "alpha")
    release = "true" if branch in ("master", "beta", "dev") else "false"
    failed = []
    for title, repo, targets, ref_variable in CONSOLES:
        ref = env.get(ref_variable) or private_ref
        request_id = f'{env["GITHUB_RUN_ID"]}-{env["GITHUB_RUN_ATTEMPT"]}-{uuid.uuid4().hex[:12]}'
        print(f"Dispatching {title}: public branch '{branch}', private ref '{ref}'", flush=True)
        inputs = {
            "public_repo": env["GITHUB_REPOSITORY"],
            "public_branch": branch,
            "public_sha": env["BUILD_SHA"],
            "source_run_id": env["GITHUB_RUN_ID"],
            "source_run_attempt": env["GITHUB_RUN_ATTEMPT"],
            "request_id": request_id,
            "targets": targets,
            "channel": channel,
            "release": release,
        }
        command = ["gh", "workflow", "run", "private-ci.yml", "--repo", repo, "--ref", ref]
        for name, value in inputs.items():
            command.extend(["-f", f"{name}={value}"])
        if subprocess.run(command, check=False).returncode:
            failed.append(title)
            print(f"::error::{title} build dispatch failed", flush=True)
    if failed:
        print(f'::error::Failed console dispatches: {", ".join(failed)}', flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(dispatch_builds(os.environ))
