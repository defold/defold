#!/usr/bin/env python3
"""Report Actions cache usage and remove caches that are no longer needed."""

import argparse
import base64
from collections import defaultdict
from functools import lru_cache
from html import escape
import json
import os
from pathlib import Path
import re
from urllib.error import HTTPError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen

from cache_key import cache_key


DOWNLOAD_KEY = re.compile(r"^((?:Linux|Windows|macOS)-[\w-]+)-dcache(?:-dependencies-v[23])?-[0-9a-f]{64}$")


class GitHub:
    def __init__(self, repository, token):
        self.base = f"{os.environ.get('GITHUB_API_URL', 'https://api.github.com')}/repos/{repository}/"
        self.headers = {"Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json",
                        "X-GitHub-Api-Version": "2026-03-10"}

    def request(self, path, method="GET", **params):
        url = self.base + path + ("?" + urlencode(params) if params else "")
        with urlopen(Request(url, headers=self.headers, method=method), timeout=30) as response:
            return json.load(response) if response.status != 204 else None

    def optional(self, path, **params):
        try:
            return self.request(path, **params)
        except HTTPError as error:
            if error.code != 404:
                raise
            error.close()
            return None

    def list_items(self, path, field, **params):
        items = []
        page = 1
        while True:
            batch = self.request(path, per_page=100, page=page, **params)[field]
            items.extend(batch)
            if len(batch) < 100:
                return items
            page += 1

    def caches(self, **params):
        return self.list_items("actions/caches", "actions_caches", **params)

    def read_text(self, path, sha):
        data = self.request("contents/" + quote(path), ref=sha)
        return base64.b64decode(data["content"]).decode("utf-8")


def active_refs(api):
    refs = set()
    for status in ("queued", "in_progress", "requested", "waiting", "pending"):
        for run in api.list_items("actions/runs", "workflow_runs", status=status):
            if str(run["id"]) == os.environ.get("GITHUB_RUN_ID"):
                continue
            if run["head_branch"]:
                refs.add("refs/heads/" + run["head_branch"])
            for pr in run["pull_requests"]:
                refs.add(f"refs/pull/{pr['number']}/merge")
    return refs


def ref_state(api, ref):
    if ref.startswith("refs/heads/"):
        data = api.optional("git/ref/" + quote(ref.removeprefix("refs/")))
        return ("branch", data["object"]["sha"]) if data else ("deleted branch", None)
    pr = re.fullmatch(r"refs/pull/(\d+)/merge", ref)
    if pr:
        data = api.optional(f"pulls/{pr[1]}")
        if data and data["state"] == "closed":
            return ("closed pull request", None)
    return ("keep", None)


def branch_cache_key(api, sha):
    read_text = lru_cache(maxsize=None)(lambda path: api.read_text(path, sha))
    try:
        # Only interpret branches using this key algorithm; never execute their code.
        if read_text("ci/cache_key.py") != Path(__file__).with_name("cache_key.py").read_text(encoding="utf-8"):
            return None
        if "${{ steps.dcache-key.outputs.key }}" not in read_text(".github/workflows/main-ci.yml"):
            return None
        return cache_key(read_text)
    except HTTPError as error:
        if error.code != 404:
            raise
        error.close()
    except (ValueError, SyntaxError) as error:
        print(f"Keep download caches for {sha}: {error}")
    return None


def obsolete_caches(caches, state, expected_key):
    if state[0] in ("deleted branch", "closed pull request"):
        return [(cache, state[0]) for cache in caches]
    if state[0] != "branch" or not expected_key:
        return []
    keys = {cache["key"] for cache in caches}
    result = []
    for cache in caches:
        match = DOWNLOAD_KEY.fullmatch(cache["key"])
        if match:
            replacement = f"{match[1]}-{expected_key}"
            # Require the replacement in the same branch and OS/platform group.
            if replacement in keys and cache["key"] != replacement:
                result.append((cache, "superseded download cache"))
    return result


def prune(api, delete=False):
    before = api.caches()
    groups = defaultdict(list)
    for cache in before:
        groups[cache["ref"]].append(cache)
    busy = active_refs(api)
    plans = []
    for ref, caches in groups.items():
        if ref in busy:
            continue
        state = ref_state(api, ref)
        expected = None
        if state[0] == "branch" and any(DOWNLOAD_KEY.fullmatch(cache["key"]) for cache in caches):
            expected = branch_cache_key(api, state[1])
        candidates = obsolete_caches(caches, state, expected)
        if candidates:
            plans.append((ref, state, expected, candidates))

    affected = []
    if delete and plans:
        busy = active_refs(api)
    for ref, state, expected, candidates in plans:
        if delete:
            # A reopened PR, recreated/updated branch, recent access, or evicted
            # replacement cancels deletion. List every page before deleting IDs.
            if ref in busy or ref_state(api, ref) != state:
                continue
            fresh = {cache["id"]: cache for cache, _ in obsolete_caches(api.caches(ref=ref), state, expected)}
        for cache, reason in candidates:
            if delete:
                if fresh.get(cache["id"]) != cache:
                    continue
                try:
                    api.request(f"actions/caches/{cache['id']}", method="DELETE")
                except HTTPError as error:
                    if error.code != 404:
                        raise
                    error.close()
                    continue
            affected.append((cache, reason))
    after = api.caches() if delete else before
    return before, after, affected


def summary(before, after, affected, delete):
    gib = lambda caches: sum(cache["size_in_bytes"] for cache in caches) / 1024 ** 3
    action = "Deleted" if delete else "Would delete"
    lines = ["## Actions cache maintenance", "",
             f"Before: {len(before)} caches, {gib(before):.2f} GiB.",
             f"{action}: {len(affected)} caches, {gib([cache for cache, _ in affected]):.2f} GiB.",
             f"After: {len(after)} caches, {gib(after):.2f} GiB." if delete else "Dry run: no caches changed.", ""]
    if affected:
        lines += ["| ID | Ref | Key | Reason |", "| --- | --- | --- | --- |"]
        for cache, reason in affected:
            cells = [str(cache["id"]), cache["ref"], cache["key"], reason]
            lines.append("| " + " | ".join(escape(cell).replace("|", "&#124;") for cell in cells) + " |")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--delete", action="store_true", help="Apply cleanup; the default is a read-only dry run")
    args = parser.parse_args()
    api = GitHub(os.environ["GITHUB_REPOSITORY"], os.environ["GH_TOKEN"])
    report = summary(*prune(api, delete=args.delete), delete=args.delete)
    print(report)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as output:
            output.write(report)
