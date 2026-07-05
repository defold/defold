#!/usr/bin/env python3

import argparse
import datetime
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid


API_ROOT = "https://api.github.com"


def branch_release_settings(branch):
    if branch == "master":
        return "stable", True
    if branch == "beta":
        return "beta", True
    return "alpha", branch == "dev"


def api_request(method, path, token, payload=None):
    url = API_ROOT + path
    data = None
    headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": "Bearer %s" % token,
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    request = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request) as response:
            body = response.read().decode("utf-8")
            return json.loads(body) if body else None
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        raise RuntimeError("%s %s failed: HTTP %s\n%s" % (method, url, e.code, body))


def parse_github_time(value):
    return datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))


def find_run(owner_repo, workflow, token, request_id, dispatched_at):
    query = urllib.parse.urlencode({"event": "workflow_dispatch", "per_page": 30})
    path = "/repos/%s/actions/workflows/%s/runs?%s" % (owner_repo, workflow, query)
    runs = api_request("GET", path, token).get("workflow_runs", [])
    for run in runs:
        created_at = parse_github_time(run["created_at"])
        if created_at < dispatched_at:
            continue

        run_name = run.get("display_title") or run.get("name") or ""
        if request_id in run_name:
            return run
    return None


def poll_run(owner_repo, token, run_id, interval_seconds):
    path = "/repos/%s/actions/runs/%s" % (owner_repo, run_id)
    while True:
        run = api_request("GET", path, token)
        print("Private Xbox run %s status=%s conclusion=%s" % (run["html_url"], run["status"], run.get("conclusion")))
        if run["status"] == "completed":
            return run
        time.sleep(interval_seconds)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--token", default=os.environ.get("SERVICES_GITHUB_TOKEN"), help="GitHub token allowed to dispatch the private workflow")
    parser.add_argument("--private-repo", default="defold/defold-xbox")
    parser.add_argument("--private-ref", default=os.environ.get("XBOX_PRIVATE_REF", "dev"))
    parser.add_argument("--workflow", default="private-ci.yml")
    parser.add_argument("--public-repo", default=os.environ.get("GITHUB_REPOSITORY", "defold/defold"))
    parser.add_argument("--public-branch", default=os.environ.get("GITHUB_REF_NAME", ""))
    parser.add_argument("--public-sha", default=os.environ.get("GITHUB_SHA", ""))
    parser.add_argument("--source-run-id", default=os.environ.get("GITHUB_RUN_ID", ""))
    parser.add_argument("--source-run-attempt", default=os.environ.get("GITHUB_RUN_ATTEMPT", ""))
    parser.add_argument("--targets", default="x86_64-xbone")
    parser.add_argument("--poll-interval", type=int, default=15)
    parser.add_argument("--match-timeout", type=int, default=300)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    missing = [name for name in ("token", "public_branch", "public_sha", "source_run_id", "source_run_attempt") if not getattr(args, name)]
    if missing:
        raise SystemExit("Missing required value(s): %s" % ", ".join(missing))

    channel, release = branch_release_settings(args.public_branch)
    request_id = "%s-%s-%s" % (args.source_run_id, args.source_run_attempt, uuid.uuid4().hex[:12])
    inputs = {
        "public_repo": args.public_repo,
        "public_branch": args.public_branch,
        "public_sha": args.public_sha,
        "source_run_id": args.source_run_id,
        "source_run_attempt": args.source_run_attempt,
        "request_id": request_id,
        "targets": args.targets,
        "channel": channel,
        "release": str(release).lower(),
    }
    payload = {"ref": args.private_ref, "inputs": inputs}

    print(json.dumps(payload, indent=2, sort_keys=True))
    if args.dry_run:
        return

    dispatched_at = datetime.datetime.now(datetime.timezone.utc)
    dispatch_path = "/repos/%s/actions/workflows/%s/dispatches" % (args.private_repo, args.workflow)
    api_request("POST", dispatch_path, args.token, payload)

    deadline = time.time() + args.match_timeout
    run = None
    while time.time() < deadline:
        run = find_run(args.private_repo, args.workflow, args.token, request_id, dispatched_at)
        if run:
            print("Matched private Xbox run: %s" % run["html_url"])
            break
        print("Waiting for private Xbox run request_id=%s" % request_id)
        time.sleep(args.poll_interval)

    if not run:
        raise SystemExit("Timed out waiting for private Xbox run request_id=%s" % request_id)

    completed = poll_run(args.private_repo, args.token, run["id"], args.poll_interval)
    if completed.get("conclusion") != "success":
        raise SystemExit("Private Xbox run failed: %s conclusion=%s" % (completed["html_url"], completed.get("conclusion")))


if __name__ == "__main__":
    main()
