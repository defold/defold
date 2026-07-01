#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

# add build_tools folder to the import search path
import sys, os, io, re
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath
sys.path.append(os.path.join(normpath(join(dirname(abspath(__file__)), '..')), "build_tools"))

import optparse
import github
import json
import time
import math
import subprocess

token = None

# Check commit branch membership via the GitHub compare API instead of local
# `git branch --contains`. Off by default (local git is fast and needs no API
# calls); CI turns it on because a shallow clone has no branch history.
use_github_compare = False

TYPE_BREAKING_CHANGE = "BREAKING CHANGE"
TYPE_FIX = "FIX"
TYPE_NEW = "NEW"


QUERY_ISSUE = r"""
{
  organization(login: "defold") {
    repository(name: "%s") {
      issue(number: %s) {
        id
        closed
        title
        number
        body
        url
        author {
          login
        }
        repository {
          name
        }
        labels(first: 10) {
          nodes {
            name
          }
        }
        timelineItems(first: 250) {
          nodes {
            __typename
            ... on CrossReferencedEvent {
              source {
                ... on PullRequest {
                  number
                  merged
                  repository {
                    name
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
"""


QUERY_PULLREQUEST = r"""
{
  organization(login: "defold") {
    repository(name: "%s") {
      pullRequest(number: %s) {
        id
        merged
        title
        number
        body
        url
        baseRefName
        headRefName
        author {
          login
        }
        repository {
          name
        }
        labels(first: 10) {
          nodes {
            name
          }
        }
        closingIssuesReferences(first: 10) {
            nodes {
                number,
                repository {
                    name,
                    url
                }
            }
        }
        timelineItems(first: 250) {
          nodes {
            __typename
            ... on MergedEvent {
              commit {
                  oid
              }
              mergeRefName
            }
            ... on ReferencedEvent {
              commit {
                  oid
              }
            }
            ... on CrossReferencedEvent {
              source {
                ... on Issue {
                  number
                }
              }
            }
          }
        }
      }
    }
  }
}
"""

QUERY_PULLREQUEST_TIMELINE_EVENTS = r"""
{
  organization(login: "defold") {
    repository(name: "%s") {
      pullRequest(number: %s) {
        timelineItems(first: 250, itemTypes: [MERGED_EVENT, REFERENCED_EVENT]) {
          nodes {
            __typename
            ... on MergedEvent {
              commit {
                  oid
              }
              mergeRefName
            }
            ... on ReferencedEvent {
              commit {
                  oid
              }
            }
          }
        }
      }
    }
  }
}
"""

# https://docs.github.com/en/graphql/overview/explorer
QUERY_PROJECT_ISSUES_AND_PRS = r"""
{
  organization(login: "defold") {
    projectV2(number: %s) {
      id
      title
      items(first: 100) {
        nodes {
          type
          content {
            ... on Issue {
              closed
              number
              repository {
                name
              }
            }
            ... on PullRequest {
              merged
              number
              repository {
                name
              }
            }
          }
        }
      }
    }
  }
}
"""

QUERY_PROJECT_NUMBER = r"""
{
    organization(login: "defold") {
        projectsV2(first: 1, query: "%s") {
            nodes {
                id
                title
                number
            }
        }
    }
}
"""

def pprint(d):
    print(json.dumps(d, indent=4, sort_keys=True))

def red(s, **kwargs): print("\033[31m{}\033[00m" .format(s), **kwargs)
def green(s, **kwargs): print("\033[32m{}\033[00m" .format(s), **kwargs)
def yellow(s, **kwargs): print("\033[33m{}\033[00m" .format(s), **kwargs)

def _print_errors(response):
    for error in response['errors']:
        print(error['message'])

def github_query(query):
    response = github.query(query, token)
    if response is None:
        print("No response from GitHub")
        sys.exit(1)
    if 'errors' in response:
        # NOT_FOUND usually just means we looked in the wrong repo (a cross-repo
        # reference). Not fatal: hand back the (empty) data so the caller can skip
        # it. Any other kind of error is still fatal.
        if all(error.get('type') == 'NOT_FOUND' for error in response['errors']):
            return response.get("data")
        print(response)
        _print_errors(response)
        sys.exit(1)
    return response["data"]

def get_project(name):
    data = github_query(QUERY_PROJECT_NUMBER % name)
    nodes = data["organization"]["projectsV2"]["nodes"]
    # Empty when no board matches (e.g. an old release's board was archived, or
    # the token can't read projects). Let the caller report it cleanly.
    return nodes[0] if nodes else None

def _entity(data, key):
    # Safely dig out organization.repository.<key>; any level is null on NOT_FOUND.
    return (((data or {}).get("organization") or {}).get("repository") or {}).get(key)

def get_issue(number, repository = "defold"):
    data = github_query(QUERY_ISSUE % (repository, number))
    issue = _entity(data, "issue")
    if issue is None:
        print("  WARNING: issue #%s not found in defold/%s (likely a cross-repo reference)" % (number, repository))
    return issue

def get_pullrequest(number, repository = "defold"):
    data = github_query(QUERY_PULLREQUEST % (repository, number))
    pr = _entity(data, "pullRequest")
    if pr is None:
        print("  WARNING: PR #%s not found in defold/%s (likely a cross-repo reference)" % (number, repository))
        return None
    if find_merge_commit(pr) is None and len(find_reference_commits(pr)) == 0:
        timeline_data = github_query(QUERY_PULLREQUEST_TIMELINE_EVENTS % (repository, number))
        timeline_pr = _entity(timeline_data, "pullRequest")
        if timeline_pr is not None:
            pr["timelineItems"] = timeline_pr["timelineItems"]
    return pr

def get_issues_and_prs(project):
    data = github_query(QUERY_PROJECT_ISSUES_AND_PRS % project.get("number"))
    return data["organization"]["projectV2"]["items"]["nodes"]

def get_labels(*args):
    labels = []
    for item in args:
        for label in item["labels"]["nodes"]:
            if not label["name"] in labels:
                labels.append(label["name"])
    return labels

def get_issue_type_from_labels(labels):
    if "breaking change" in labels:
        return TYPE_BREAKING_CHANGE
    elif "bug" in labels:
        return TYPE_FIX
    elif "task" in labels:
        return TYPE_NEW
    elif "feature request" in labels:
        return TYPE_NEW
    return TYPE_FIX

def get_closing_issue(pr):
    for node in reversed(pr["closingIssuesReferences"]["nodes"]):
        issue_number = node["number"]
        repository = node["repository"]["name"]
        issue = get_issue(issue_number, repository)
        if issue is not None:
            return issue
    return pr

def get_closing_pr(issue):
    issue_repository = issue.get("repository").get("name")
    # an issue may reference multiple merged items on the
    # timeline - pick the last one! (ie newest)
    for node in reversed(issue["timelineItems"]["nodes"]):
        if not node["__typename"] == "CrossReferencedEvent":
            continue
        source = node["source"]
        if source.get("merged") == True:
            closing_number = source["number"]
            # The closing PR can live in a different repo (e.g. an extension), so
            # use the source's own repository when present; fall back otherwise.
            repository = (source.get("repository") or {}).get("name") or issue_repository
            pr = get_pullrequest(closing_number, repository)
            if pr is not None:
                return pr
    return issue

def find_merge_commit(pr):
    commit = None
    for node in pr["timelineItems"]["nodes"]:
        if not node:
            continue
        if not node["__typename"] == "MergedEvent":
            continue
        if "commit" in node:
            commit = node["commit"]["oid"]
            break
    return commit

def find_reference_commits(pr):
    commits = []
    for node in pr["timelineItems"]["nodes"]:
        if not node:
            continue
        if not node["__typename"] == "ReferencedEvent":
            continue
        if "commit" in node:
            commits.append(node["commit"]["oid"])
    return commits

# Branches a release-note issue's fix has to be on. get_commit_branches checks
# membership with local git by default, or GitHub's "compare two commits" API
# when --use-github-compare is set (below). AUDIT_BRANCHES only matters for the
# API path, which has to name the branches to compare against.
AUDIT_BRANCHES = ["dev", "beta"]

def commit_in_branch(branch, commit, repository = "defold", max_retries = 6):
    # True when `branch` already contains `commit`. GitHub's compare endpoint
    # tells us: comparing <branch>...<commit> comes back with ahead_by == 0.
    # A commit that exists but hasn't landed yet has ahead_by > 0 (so False).
    # Only an unreachable repo or unknown sha gives a null response, which we
    # retry a few times; if it still can't be confirmed we treat it as missing
    # (False) so it blocks the release.
    url = "/repos/defold/%s/compare/%s...%s" % (repository, branch, commit)
    for attempt in range(max_retries):
        response = github.get(url, token)
        if response is not None:
            return response.get("ahead_by") == 0
        time.sleep(min(60, 2 ** attempt))
    red("    Could not verify commit %s against %s after %d attempts" % (commit[:8], branch, max_retries))
    return False

def git_branch_contains(commit):
    # Local branches that contain the commit. Only sees what's in the checkout,
    # so it needs a full clone with dev/beta fetched (not a shallow CI clone).
    result = subprocess.run(["git", "branch", "--contains", commit], capture_output = True)
    if result.returncode == 0:
        return [line.replace("*", "").strip() for line in result.stdout.decode('utf-8').splitlines()]
    red(result.stderr.decode('utf-8'))
    sys.exit(result.returncode)

def get_commit_branches(commit):
    # Which branches contain this commit. Locally we just ask git; on a shallow
    # CI clone there's no history, so --use-github-compare asks GitHub instead.
    if use_github_compare:
        return [branch for branch in AUDIT_BRANCHES if commit_in_branch(branch, commit)]
    return git_branch_contains(commit)

def check_commit_branches(commit, branches):
    result = get_commit_branches(commit)
    for branch in branches:
        if not branch in result:
            return False
    return True

def issue_to_markdown(issue, hide_details = True, title_only = False):
    closed_issues = []
    for x in issue["closed_issues"]:
        if issue.get("repository") == "defold":
            closed_issues.append("#" + str(x))
        else:
            closed_issues.append(issue.get("repository") + "#" + str(x))

    if title_only:
        md = ("* __%s__: ([%s](%s)) %s (by %s)\n" % (issue["type"], ",".join(closed_issues), issue["url"], issue["title"], issue["author"]))

    else:    
        md = ("__%s__: ([%s](%s)) __'%s'__ by %s\n" % (issue["type"], ",".join(closed_issues), issue["url"], issue["title"], issue["author"]))
        if hide_details: md += ("[details=\"Details\"]\n")
        md += ("%s\n" % issue["body"])
        if hide_details: md += ("\n---\n[/details]\n")
        md += ("\n")

    return md


def fetch_item(item):
    # Turns one project item into its (issue, pr, labels) with a few GraphQL
    # calls and applies the per-item skip checks. parse_github_project then
    # assembles the results.
    content = item.get("content")
    if not content:
        return None

    repository = content.get("repository").get("name")
    record = {"type": item.get("type"), "repository": repository, "number": content.get("number")}
    if content.get("merged", False) == False and content.get("closed", False) == False:
        return dict(record, status = "ignored", reason = "not closed/merged")

    if item.get("type") == "ISSUE":
        issue = get_issue(content.get("number"), repository = repository)
        if issue is None:
            return dict(record, status = "ignored", reason = "issue #%s not resolvable in %s" % (content.get("number"), repository))
        pr = get_closing_pr(issue)
    elif item.get("type") == "PULL_REQUEST":
        pr = get_pullrequest(content.get("number"), repository = repository)
        if pr is None:
            return dict(record, status = "ignored", reason = "PR #%s not resolvable in %s" % (content.get("number"), repository))
        issue = get_closing_issue(pr)
        issue_number_matching = pr.get("number") == issue.get("number")
        repository_matching = pr.get("repository").get("name") == issue.get("repository").get("name")
        if repository_matching and not issue_number_matching:
            return dict(record, status = "ignored", reason = "both PR and issue #%s added to the project" % issue.get("number"))
    else:
        return None

    labels = get_labels(issue, pr)
    if "skip release notes" in labels:
        return dict(record, status = "ignored", reason = "skip release notes")

    return dict(record, status = "ok", issue = issue, pr = pr, labels = labels)

def parse_github_project(version):
    project = get_project(version)
    if not project:
        print("Unable to find GitHub project for version %s" % version)
        return None


    print("Parsing GitHub project for version %s" % version)
    issues = []
    items = get_issues_and_prs(project)
    print("Fetching %d items..." % len(items))
    records = [fetch_item(item) for item in items]

    for record in records:
        if record is None:
            continue

        print("  %12s %-12s #%-8s - " % (record["type"], record["repository"], record["number"]), end = "")
        if record["status"] == "ignored":
            yellow("IGNORED (%s)" % record["reason"])
            continue

        issue = record["issue"]
        pr = record["pr"]
        labels = record["labels"]

        # Make sure to ignore duplicates
        duplicate = False
        for existing_issue in issues:
            if existing_issue.get("number") == issue.get("number") and existing_issue.get("repository") == issue.get("repository"):
                duplicate = True
                break

        # Multiple issues closed by the same PR
        for existing_issue in issues:
            if existing_issue["pr_number"] == pr.get("number"):
                existing_issue["closed_issues"].append(issue.get("number"))
                duplicate = True
                break

        entry = {
            "title": pr.get("title"),
            "body": pr.get("body"),
            "url": pr.get("url"),
            "issue_number": issue.get("number"),
            "pr_number": pr.get("number"),
            "closed_issues": [ issue.get("number") ],
            "author": pr.get("author").get("login"),
            "labels": labels,
            "type": get_issue_type_from_labels(labels),
            "mergecommit": find_merge_commit(pr),
            "referencecommits": find_reference_commits(pr),
            "duplicate": duplicate,
            "repository": issue.get("repository").get("name")
        }
        # strip from match to end of file
        flags = re.DOTALL|re.IGNORECASE
        entry["body"] = re.sub(r"## PR checklist.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"#* Technical changes.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Technical changes.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"#* Technical notes.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Technical notes.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"#* Technical details.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Technical details.*", "", entry["body"], flags=flags).strip()

        # Remove closing keywords
        flags = re.IGNORECASE
        entry["body"] = re.sub(r"Resolves https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Resolves #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Resolved https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Resolved #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Resolve https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Resolve #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Closes https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Closes #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Closed https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Closed #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Close https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Close #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Fixes https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Fixes #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Fixed https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Fixed #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Fix https.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Fix #\d*.*", "", entry["body"], flags=flags).strip()

        # Remove other common ways to reference issues
        flags = re.IGNORECASE
        entry["body"] = re.sub(r"Also related to #\d*.*", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub(r"Related to #\d*.*", "", entry["body"], flags=flags).strip()

        # Remove "user facing changes" header
        flags = re.IGNORECASE
        entry["body"] = re.sub("User-facing changes.", "", entry["body"], flags=flags).strip()
        entry["body"] = re.sub("### User-facing changes", "", entry["body"], flags=flags).strip()

        issues.append(entry)
        green("OK")

    return issues

def audit_branches_for_channel(channel):
    # Which branches a note's fix has to be on, per release channel.
    # beta/stable ship from beta with everything merged forward -> require both.
    # alpha (dev track) and custom validation channels build off dev / a feature
    # branch, so requiring beta would fail them for fixes that just haven't
    # reached beta yet -> require dev only. Unknown channel (manual run with no
    # --channel) keeps the strict default.
    if channel in ("beta", "stable", None):
        return ["dev", "beta"]
    return ["dev"]

def check_issue_commits(issues, required_branches = ("dev", "beta")):
    print("\nChecking issue commits for presence on: %s" % ", ".join(required_branches))
    merge_dev_count = 0
    merge_beta_count = 0
    merge_dev_beta_count = 0
    reference_dev_count = 0
    reference_beta_count = 0
    reference_dev_beta_count = 0
    ignored_count = 0
    missing_dev_count = 0
    missing_beta_count = 0
    missing_dev_beta_count = 0
    required_missing_count = 0
    for issue in issues:
        dev_ok = False
        beta_ok = False
        print("  Checking #%s '%s' (%s)" % (issue["issue_number"], issue["title"], issue["url"]))
        if issue.get("repository") != "defold":
            yellow("    Ignored since issue is not from the defold repository")
            ignored_count = ignored_count + 1
            continue

        if issue.get("mergecommit") != None:
            branches = get_commit_branches(issue["mergecommit"])
            if "dev" in branches and "beta" in branches:
                merge_dev_beta_count = merge_dev_beta_count + 1
            if "dev" in branches:
                green("    dev  OK via merge commit (%s)" % (issue["mergecommit"]))
                dev_ok = True
                merge_dev_count = merge_dev_count + 1
            if "beta" in branches:
                green("    beta OK via merge commit (%s)" % (issue["mergecommit"]))
                beta_ok = True
                merge_beta_count = merge_beta_count + 1

        for referencecommit in issue.get("referencecommits"):
            if dev_ok and beta_ok: break
            branches = get_commit_branches(referencecommit)
            if "dev" in branches and "beta" in branches:
                reference_dev_beta_count = reference_dev_beta_count + 1
            if not dev_ok and "dev" in branches:
                yellow("    dev  OK via reference commit (%s)" % referencecommit)
                dev_ok = True
                reference_dev_count = reference_dev_count + 1
            if not beta_ok and "beta" in branches:
                yellow("    beta OK via reference commit (%s)" % referencecommit)
                beta_ok = True
                reference_beta_count = reference_beta_count + 1

        if not dev_ok and not beta_ok:
            red("    Missing from dev+beta")
            missing_dev_beta_count = missing_dev_beta_count + 1
        else:
            if not dev_ok:
                red("    Missing from dev")
                missing_dev_count = missing_dev_count + 1
            if not beta_ok:
                red("    Missing from beta")
                missing_beta_count = missing_beta_count + 1

        # Only the branches THIS channel ships from gate the release; the dev/beta
        # tallies above stay for diagnostics.
        branch_ok = {"dev": dev_ok, "beta": beta_ok}
        if any(not branch_ok.get(b, False) for b in required_branches):
            required_missing_count = required_missing_count + 1

    print("\nSummary (%d issues)" % len(issues))
    print("  %d issue(s) from external repositories not checked" % ignored_count)
    green("  %d issue(s) present on dev+beta via merge commits" % merge_dev_beta_count)
    green("  %d issue(s) present on dev+beta via reference commits" % reference_dev_beta_count)
    yellow("  %d issue(s) present on dev via merge commits" % merge_dev_count)
    yellow("  %d issue(s) present on beta via merge commits" % merge_beta_count)
    yellow("  %d issue(s) present on dev via reference commits" % reference_dev_count)
    yellow("  %d issue(s) present on beta via reference commits" % reference_beta_count)
    red("  %d issue(s) not present on dev" % missing_dev_count)
    red("  %d issue(s) not present on beta" % missing_beta_count)
    red("  %d issue(s) not present on dev+beta" % missing_dev_beta_count)

    if required_missing_count > 0:
        red("\nRelease notes audit FAILED: %d issue(s) missing from required branch(es): %s" % (required_missing_count, ", ".join(required_branches)))
        sys.exit(1)
    green("\nRelease notes audit passed: all issues present on required branch(es): %s" % ", ".join(required_branches))



def generate_markdown(version, issues, hide_details = False):
    engine = []
    editor = []
    other = []
    for issue in issues:
        if issue.get("repository") != "defold":
            other.append(issue)
        elif "editor" in issue["labels"]:
            editor.append(issue)
        else:
            engine.append(issue)
 
    types = [ TYPE_BREAKING_CHANGE, TYPE_NEW, TYPE_FIX ]
    summary = ""
    details_engine = ""
    details_editor = ""
    details_other = ""
    for issue_type in types:
        for issue in engine:
            if issue["type"] == issue_type and issue["duplicate"] == False:
                summary += issue_to_markdown(issue, title_only = True)
                details_engine += issue_to_markdown(issue, hide_details = hide_details)
        for issue in editor:
            if issue["type"] == issue_type and issue["duplicate"] == False:
                summary += issue_to_markdown(issue, title_only = True)
                details_editor += issue_to_markdown(issue, hide_details = hide_details)
        for issue in other:
            if issue["type"] == issue_type and issue["duplicate"] == False:
                summary += issue_to_markdown(issue, title_only = True)
                details_other += issue_to_markdown(issue, hide_details = hide_details)

    output = ("# Defold %s\n" % version)
    output = output + "\n## Summary\n" + summary
    if engine:
        output = output + "\n## Engine\n" + details_engine
    if editor:
        output = output + "\n## Editor\n" + details_editor
    if other:
        output = output + "\n## Other\n" + details_other

    file = "releasenotes/%s.md" % version
    with io.open(file, "wb") as f:
        f.write(output.encode('utf-8'))
        print("Wrote %s" % file)


def release_announcement_url(version, channel):
    slug = version.replace(".", "-")
    if channel == "beta":
        return "https://forum.defold.com/t/defold-%s-beta/" % slug
    if channel == "stable":
        return "https://forum.defold.com/t/defold-%s-has-been-released/" % slug
    return "https://forum.defold.com/c/releasenotes/"


def generate_json(version, issues, channel = None):
    output = {
        "version": version,
        "timestamp": time.time(),
        "external-link": release_announcement_url(version, channel),
        "issues": issues
    }

    file = "releasenotes/%s.json" % version
    with io.open(file, "w") as f:
        json.dump(output, f, indent=4, sort_keys=True)
        print("Wrote %s" % file)


def generate(version, hide_details = False, channel = None):
    print("Generating release notes for %s" % version)

    issues = parse_github_project(version)
    if not issues:
        # No board (e.g. an alpha version ahead of any open release board) or an
        # empty one - nothing to generate. Exit 0 and leave no file behind.
        print("No release notes found for %s - skipping" % version)
        return

    # The commit audit verifies every issue's fix is present on the branch(es)
    # this channel ships from (via the GitHub compare API, no local history
    # needed) and fails the run if any are missing, before we publish notes for
    # an unreleased fix.
    check_issue_commits(issues, audit_branches_for_channel(channel))
    generate_markdown(version, issues, hide_details)
    generate_json(version, issues, channel)



if __name__ == '__main__':
    usage = '''usage: %prog [options] command(s)

Commands:
generate - Generate release notes
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--version', dest='version',
                      default = None,
                      help = 'Version to genereate release notes for')

    parser.add_option('--token', dest='token',
                      default = None,
                      help = 'GitHub API topken')

    parser.add_option('--hide-details', dest='hide_details',
                      default = False,
                      action = "store_true",
                      help = 'Hide details for each entry')

    parser.add_option('--channel', dest='channel',
                      default = None,
                      help = 'Release channel; selects which branch(es) the commit audit requires')

    parser.add_option('--use-github-compare', dest='use_github_compare',
                      default = False,
                      action = "store_true",
                      help = 'Check commit branch membership via the GitHub compare API instead of local git (use on shallow clones, e.g. CI)')

    options, args = parser.parse_args()

    if not args:
        parser.print_help()
        exit(1)

    if not options.token:
        print("No token specified")
        parser.print_help()
        exit(1)

    if not options.version:
        print("No version specified")
        parser.print_help()
        exit(1)

    token = options.token
    use_github_compare = options.use_github_compare
    for cmd in args:
        if cmd == "generate":
            generate(options.version, options.hide_details, options.channel)


    print('Done')
