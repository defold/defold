#!/usr/bin/env python3
# Copyright 2020-2025 The Defold Foundation
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
import subprocess
import math

token = None

TYPE_BREAKING_CHANGE = "BREAKING CHANGE"
TYPE_FIX = "FIX"
TYPE_NEW = "NEW"


QUERY_ISSUE = r"""
{
  organization(login: "defold") {
    repository(name: "defold") {
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
    repository(name: "defold") {
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
        labels(first: 10) {
          nodes {
            name
          }
        }
        closingIssuesReferences(first: 10) {
            nodes {
                number
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
            }
            ... on PullRequest {
              merged
              number
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
    if 'errors' in response:
        _print_errors(response)
        sys.exit(1)
    return response["data"]

def get_project(name):
    data = github_query(QUERY_PROJECT_NUMBER % name)
    return data["organization"]["projectsV2"]["nodes"][0]

def get_issue(number):
    data = github_query(QUERY_ISSUE % number)
    return data["organization"]["repository"]["issue"]

def get_pullrequest(number):
    data = github_query(QUERY_PULLREQUEST % number)
    return data["organization"]["repository"]["pullRequest"]

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
        return get_issue(issue_number)
    return pr

def get_closing_pr(issue):
    # an issue may reference multiple merged items on the
    # timeline - pick the last one! (ie newest)
    for node in reversed(issue["timelineItems"]["nodes"]):
        if not node["__typename"] == "CrossReferencedEvent":
            continue
        if node["source"].get("merged") == True:
            closing_number = node["source"]["number"]
            return get_pullrequest(closing_number)
    return issue

def find_merge_commit(pr):
    commit = None
    for node in pr["timelineItems"]["nodes"]:
        if not node["__typename"] == "MergedEvent":
            continue
        if "commit" in node:
            commit = node["commit"]["oid"]
            break
    return commit

def find_reference_commits(pr):
    commits = []
    for node in pr["timelineItems"]["nodes"]:
        if not node["__typename"] == "ReferencedEvent":
            continue
        if "commit" in node:
            commits.append(node["commit"]["oid"])
    return commits

def get_commit_branches(commit):
    result = subprocess.run(["git", "branch", "--contains", commit], capture_output = True)
    out = result.stdout.decode('utf-8')
    if result.returncode == 0:
        return [line.replace("*", "").strip() for line in out.splitlines()]
    print(out)
    sys.exit(result.returncode)

def check_commit_branches(commit, branches):
    result = get_commit_branches(commit)
    for branch in branches:
        if not branch in result:
            return False
    return True

def issue_to_markdown(issue, hide_details = True, title_only = False):
    if title_only:
        md = ("* __%s__: ([#%s](%s)) %s (by %s)\n" % (issue["type"], issue["number"], issue["url"], issue["title"], issue["author"]))

    else:    
        md = ("__%s__: ([#%s](%s)) __'%s'__ by %s\n" % (issue["type"], issue["number"], issue["url"], issue["title"], issue["author"]))
        if hide_details: md += ("[details=\"Details\"]\n")
        md += ("%s\n" % issue["body"])
        if hide_details: md += ("\n---\n[/details]\n")
        md += ("\n")

    return md


def parse_github_project(version):
    project = get_project(version)
    if not project:
        print("Unable to find GitHub project for version %s" % version)
        return None


    print("Parsing GitHub project for version %s" % version)
    issues = []
    items = get_issues_and_prs(project)
    for item in items:
        content = item.get("content")
        if not content:
            continue

        # if content.get("number") != 10678: continue
        # pprint(content)
        # pprint(item)

        print("  %s #%s - " % (item.get("type"), content.get("number")), end = "")
        if content.get("merged", False) == False and content.get("closed", False) == False:
            yellow("IGNORED (not closed/merged)")
            continue

        issue = None
        pr = None
        if item.get("type") == "ISSUE":
            issue = get_issue(content.get("number"))
            pr = get_closing_pr(issue)
        elif item.get("type") == "PULL_REQUEST":
            pr = get_pullrequest(content.get("number"))
            issue = get_closing_issue(pr)

        labels = get_labels(issue, pr)

        # skip release notes if label is set
        if "skip release notes" in labels:
            yellow("IGNORED (skip release notes)")
            continue

        # Make sure to ignore duplicates
        for existing_issue in issues:
            if existing_issue.get("number") == issue.get("number"):
                yellow("IGNORED (duplicate issue)")
                continue

        entry = {
            "title": pr.get("title"),
            "body": pr.get("body"),
            "url": pr.get("url"),
            "number": issue.get("number"),
            "author": pr.get("author").get("login"),
            "labels": labels,
            "type": get_issue_type_from_labels(labels),
            "mergecommit": find_merge_commit(pr),
            "referencecommits": find_reference_commits(pr),
        }
        # strip from match to end of file
        entry["body"] = re.sub("## PR checklist.*", "", entry["body"], flags=re.DOTALL).strip()
        entry["body"] = re.sub("### Technical changes.*", "", entry["body"], flags=re.DOTALL).strip()
        entry["body"] = re.sub("### Technical changes.*", "", entry["body"], flags=re.DOTALL).strip()
        entry["body"] = re.sub("# Technical changes.*", "", entry["body"], flags=re.DOTALL).strip()
        entry["body"] = re.sub("Technical changes.*", "", entry["body"], flags=re.DOTALL).strip()
        entry["body"] = re.sub("Technical notes.*", "", entry["body"], flags=re.DOTALL).strip()
        entry["body"] = re.sub("## Technical details.*", "", entry["body"], flags=re.DOTALL).strip()

        # Remove closing keywords
        entry["body"] = re.sub("Resolves .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Resolved .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Resolve .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Closes https.*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Closes .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Closed .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Close .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fixes https.*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fixes #.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fixes .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fixed .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fix https.*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fix .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fix #.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Related to #.....*", "", entry["body"], flags=re.IGNORECASE).strip()

        # Remove "user facing changes" header
        entry["body"] = re.sub("User-facing changes.", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("### User-facing changes", "", entry["body"], flags=re.IGNORECASE).strip()

        issues.append(entry)
        green("OK")

    return issues

def check_issue_commits(issues):
    print("Checking issue commits for dev and beta presence")
    merge_count = 0
    reference_count = 0
    missing_count = 0
    for issue in issues:
        dev_ok = False
        beta_ok = False
        print("  Checking #%s '%s' (%s)" % (issue["number"], issue["title"], issue["url"]))
        if issue.get("mergecommit") != None:
            branches = get_commit_branches(issue["mergecommit"])
            if "dev" in branches:
                green("    dev  OK via merge commit (%s)" % (issue["mergecommit"]))
                dev_ok = True
                merge_count = merge_count + 0.5
            if "beta" in branches:
                green("    beta OK via merge commit (%s)" % (issue["mergecommit"]))
                beta_ok = True
                merge_count = merge_count + 0.5

        for referencecommit in issue.get("referencecommits"):
            if dev_ok and beta_ok: break
            branches = get_commit_branches(referencecommit)
            if not dev_ok and "dev" in branches:
                yellow("    dev  OK via reference commit (%s)" % referencecommit)
                dev_ok = True
                reference_count = reference_count + 0.5
            if not beta_ok and "beta" in branches:
                yellow("    beta OK via reference commit (%s)" % referencecommit)
                beta_ok = True
                reference_count = reference_count + 0.5

        if not dev_ok or not beta_ok:
            red("    Missing from dev and/or beta")
            missing_count = missing_count + 1

    print("Summary")
    green("  %d issue(s) present on dev+beta via merge commits" % math.floor(merge_count))
    yellow("  %d issue(s) present on dev+beta via reference commits" % math.ceil(reference_count))
    red("  %d issue(s) not present on dev+beta" % missing_count)



def generate_markdown(version, issues, hide_details = False):
    engine = []
    editor = []
    for issue in issues:
        if "editor" in issue["labels"]:
            editor.append(issue)
        else:
            engine.append(issue)
 
    types = [ TYPE_BREAKING_CHANGE, TYPE_NEW, TYPE_FIX ]
    summary = ("\n## Summary\n")
    details_engine = ("\n## Engine\n")
    details_editor = ("\n## Editor\n")
    for issue_type in types:
        for issue in engine:
            if issue["type"] == issue_type:
                summary += issue_to_markdown(issue, title_only = True)
                details_engine += issue_to_markdown(issue, hide_details = hide_details)
        for issue in editor:
            if issue["type"] == issue_type:
                summary += issue_to_markdown(issue, title_only = True)
                details_editor += issue_to_markdown(issue, hide_details = hide_details)

    output = ("# Defold %s\n" % version) + summary + details_engine + details_editor

    with io.open("releasenotes/%s.md" % version, "wb") as f:
        f.write(output.encode('utf-8'))


def generate_json(version, issues):
    output = {
        "version": version,
        "timestamp": time.time(),
        "issues": issues
    }

    with io.open("releasenotes/%s.json" % version, "w") as f:
        json.dump(output, f, indent=4, sort_keys=True)


def generate(version, hide_details = False):
    print("Generating release notes for %s" % version)

    issues = parse_github_project(version)
    if issues is None:
        return
    
    check_issue_commits(issues)
    generate_markdown(version, issues, hide_details)
    generate_json(version, issues)



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
    for cmd in args:
        if cmd == "generate":
            generate(options.version, options.hide_details)


    print('Done')
