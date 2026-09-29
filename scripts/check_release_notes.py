#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import argparse
import json
import os
import re
import sys

import releasenotes_github_projectv2 as release_notes


COMMENT_MARKER = "<!-- defold-release-notes-check -->"
SUCCESS_COMMENT = COMMENT_MARKER + "\n✅ **Release notes check passed.**"

QUERY_COMMENTS = r"""
{
  repository(owner: "defold", name: %s) {
    pullRequest(number: %d) {
      id
      comments(first: 100, after: %s) {
        pageInfo {
          endCursor
          hasNextPage
        }
        nodes {
          id
          body
          author {
            __typename
            login
          }
        }
      }
    }
  }
}
"""

MUTATION_ADD_COMMENT = r"""
mutation {
  addComment(input: {subjectId: %s, body: %s}) {
    commentEdge { node { id } }
  }
}
"""

MUTATION_UPDATE_COMMENT = r"""
mutation {
  updateIssueComment(input: {id: %s, body: %s}) {
    issueComment { id }
  }
}
"""


QUERY_PROJECTS = r"""
{
  node(id: %s) {
    ... on %s {
      projectItems(first: 100, after: %s) {
        pageInfo {
          endCursor
          hasNextPage
        }
        nodes {
          project {
            title
            owner {
              ... on Organization {
                login
              }
            }
          }
        }
      }
    }
  }
}
"""


def find_comment(number, repository, comment_token):
    cursor = None
    while True:
        data = release_notes.github_query(QUERY_COMMENTS % (json.dumps(repository), number, json.dumps(cursor)),
                                          query_token = comment_token)
        repo = data["repository"]
        pr = repo["pullRequest"] if repo else None
        if pr is None:
            raise RuntimeError("Unable to read comments for defold/%s#%s" % (repository, number))
        page = pr["comments"]
        for comment in page["nodes"]:
            author = comment.get("author") or {}
            if (author.get("__typename") == "Bot" and author.get("login") == "github-actions[bot]"
                    and comment["body"].startswith(COMMENT_MARKER)):
                return pr["id"], comment
        if not page["pageInfo"]["hasNextPage"]:
            return pr["id"], None
        cursor = page["pageInfo"]["endCursor"]


def update_comment(number, repository, errors, comment_token):
    pr_id, comment = find_comment(number, repository, comment_token)
    if not errors:
        # A successful first run stays silent; only clear an existing failure.
        if comment is None:
            return
        body = SUCCESS_COMMENT
    else:
        body = COMMENT_MARKER + "\n❌ **Release notes check failed.**\n\n"
        body += "\n".join("- %s" % error for error in errors)
        body += "\n\nRe-run this check after changing projects or issue labels."
        body += "\n\n[View check details](https://github.com/defold/%s/pull/%s/checks)." % (repository, number)

    if comment is not None:
        if comment["body"] == body:
            return
        query = MUTATION_UPDATE_COMMENT % (json.dumps(comment["id"]), json.dumps(body))
    else:
        query = MUTATION_ADD_COMMENT % (json.dumps(pr_id), json.dumps(body))
    release_notes.github_query(query, query_token = comment_token)


def report_error(message, errors):
    print("ERROR: %s" % message)
    errors.append(message)


def has_version_project(item, item_type):
    cursor = None
    while True:
        data = release_notes.github_query(QUERY_PROJECTS % (json.dumps(item["id"]), item_type, json.dumps(cursor)))
        if data["node"] is None:
            sys.exit("Unable to read projects for %s" % item["url"])
        page = data["node"]["projectItems"]
        for node in page["nodes"]:
            project = node["project"]
            if (project["owner"].get("login", "").lower() == "defold"
                    and re.fullmatch(r"1\.[0-9]+\.[0-9]+", project["title"])):
                return True
        if not page["pageInfo"]["hasNextPage"]:
            return False
        cursor = page["pageInfo"]["endCursor"]


def check_pullrequest(number, repository = "defold", errors = None):
    if errors is None:
        errors = []
    pr = release_notes.get_pullrequest_metadata(number, repository)
    if release_notes.should_skip_release_notes(pr):
        print("OK: %s has 'skip release notes'." % pr["url"])
        return True

    issues = release_notes.get_closing_issues(pr)
    item_type = "Issue" if issues else "PullRequest"
    missing = []
    needs_text = False
    for item in issues or [pr]:
        if release_notes.should_skip_release_notes(item):
            print("OK: %s has 'skip release notes'." % item["url"])
            continue
        needs_text = True
        if has_version_project(item, item_type):
            print("OK: %s belongs to a Defold version project." % item["url"])
        else:
            missing.append(item["url"])

    for url in missing:
        report_error("Add %s to a defold organization project named 1.x.x (for example, 1.12.0), "
                     "or apply 'skip release notes' if it should be omitted." % url, errors)
    missing_text = needs_text and not release_notes.get_release_notes_body(pr["body"])
    if missing_text:
        report_error("%s has no release notes text after removing the checklist, issue references and technical notes. "
                     "Add a description of the user-facing changes before those sections, "
                     "or apply 'skip release notes' if this PR should be omitted." % pr["url"], errors)
    if missing:
        print("After updating projects or issue labels, re-run this check to refresh the result.")
    return not missing and not missing_text


def main():
    parser = argparse.ArgumentParser(description = "Check PR release notes text, labels, closing issues and version projects.")
    parser.add_argument("--pull-request", type = int, required = True)
    parser.add_argument("--repository", default = "defold", help = "Repository name in the defold organization")
    parser.add_argument("--comment", action = "store_true", help = "Update the PR comment using the GitHub Actions bot's GH_TOKEN")
    args = parser.parse_args()
    release_notes.token = os.environ.get("GITHUB_TOKEN")
    if not release_notes.token:
        parser.error("Set GITHUB_TOKEN to a token with repository and read:project access.")
    comment_token = os.environ.get("GH_TOKEN")
    if args.comment and not comment_token:
        parser.error("Set GH_TOKEN to the GitHub Actions token with pull-requests: write access for --comment.")

    errors = []
    try:
        valid = check_pullrequest(args.pull_request, args.repository, errors)
    except (Exception, SystemExit):
        if not args.comment:
            raise
        import traceback
        traceback.print_exc()
        report_error("The release notes check could not complete. Re-run the check; if it still fails, "
                     "ask a maintainer to check the workflow log and the SERVICES_GITHUB_TOKEN repository "
                     "and read:project permissions.", errors)
        valid = False

    if args.comment:
        try:
            update_comment(args.pull_request, args.repository, errors, comment_token)
        except (Exception, SystemExit) as error:
            print("ERROR: Could not update the release notes comment: %s. "
                  "Check GH_TOKEN's pull-requests: write permission and re-run the check." % error)
            return 1
    return 0 if valid else 1


if __name__ == "__main__":
    sys.exit(main())
