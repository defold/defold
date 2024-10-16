#!/usr/bin/env python3
# Copyright 2020-2024 The Defold Foundation
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

token = None

TYPE_BREAKING_CHANGE = "BREAKING CHANGE"
TYPE_FIX = "FIX"
TYPE_NEW = "NEW"

# https://docs.github.com/en/graphql/overview/explorer
QUERY_CLOSED_ISSUES = r"""
{
  organization(login: "defold") {
    id
    projectV2(number: %s) {
      id
      title
      items(first: 100) {
        nodes {
          content {
            ... on Issue {
              id
              closed
              title
              bodyText
            }
          }
        }
      }
    }
  }
}
"""

QUERY_PROJECT_ISSUES_AND_PRS = r"""
{
  organization(login: "defold") {
    id
    projectV2(number: %s) {
      id
      title
      items(first: 100) {
        nodes {
          type
          content {
            ... on Issue {
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
              timelineItems(first: 100) {
                nodes {
                  ... on CrossReferencedEvent {
                    source {
                      ... on PullRequest {
                        id
                        body
                        number
                        merged
                        title
                        url
                        author {
                          login
                        }
                        labels(first: 10) {
                          nodes {
                            name
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
            ... on PullRequest {
              id
              merged
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
              timelineItems(first: 100) {
                nodes {
                  ... on CrossReferencedEvent {
                    source {
                      ... on Issue {
                        id
                        body
                        number
                        closed
                        title
                        url
                        author {
                          login
                        }
                        labels(first: 10) {
                          nodes {
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

def _print_errors(response):
    for error in response['errors']:
        print(error['message'])

def get_project(name):
    query = QUERY_PROJECT_NUMBER % name
    response = github.query(query, token)
    if 'errors' in response:
        _print_errors(response)
        sys.exit(1)
    return response["data"]["organization"]["projectsV2"]["nodes"][0]

def get_issues_and_prs(project):
    query = QUERY_PROJECT_ISSUES_AND_PRS % project.get("number")
    response = github.query(query, token)
    if 'errors' in response:
        _print_errors(response)
        sys.exit(1)
    response = response["data"]["organization"]["projectV2"]["items"]["nodes"]
    return response

def get_labels(issue_or_pr):
    labels = []
    for l in issue_or_pr["labels"]["nodes"]:
        labels.append(l["name"])
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

def get_closing_pr(issue):
    closing_pr = None
    # an issue may reference multiple merged items on the
    # timeline - pick the last one! (ie newest)
    for t in issue["timelineItems"]["nodes"]:
        if t and "source" in t and t["source"]:
            if t["source"].get("merged") == True:
                closing_pr = t["source"]
    return closing_pr

def issue_to_markdown(issue, hide_details = True, title_only = False):
    if title_only:
        md = ("* __%s__: ([#%s](%s)) %s \n" % (issue["type"], issue["number"], issue["url"], issue["title"]))

    else:    
        md = ("__%s__: ([#%s](%s)) __'%s' by %s__ \n" % (issue["type"], issue["number"], issue["url"], issue["title"], issue["author"]))
        if hide_details: md += ("[details=\"Details\"]\n")
        md += ("%s\n" % issue["body"])
        if hide_details: md += ("\n---\n[/details]\n")
        md += ("\n")

    return md

def generate(version, hide_details = False):
    print("Generating release notes for %s" % version)
    project = get_project(version)
    if not project:
        print("Unable to find GitHub project for version %s" % version)
        return None

    output = []
    merged = get_issues_and_prs(project)
    for m in merged:
        content = m.get("content")
        if not content:
            continue

        # if content.get("number") != 1234: continue
        # if content.get("number") != 9376: continue

        is_issue = m.get("type") == "ISSUE"
        is_pr = m.get("type") == "PULL_REQUEST"
        if is_issue and content.get("closed") == False:
            continue
        if is_pr and content.get("merged") == False:
            continue
        issue_labels = get_labels(content)
        if "skip release notes" in issue_labels:
            continue

        if is_issue:
            closing_pr = get_closing_pr(content)
            if closing_pr:
                # print("Issue #%d '%s' was closed by #%d '%s'" % (content.get("number"), content.get("title"), closing_pr.get("number"), closing_pr.get("title")))
                content = closing_pr
                # merge labels skipping duplicates
                for label in get_labels(content):
                    if not label in issue_labels:
                        issue_labels.append(label)


        issue_type = get_issue_type_from_labels(issue_labels)

        entry = {
            "title": content.get("title"),
            "body": content.get("body"),
            "url": content.get("url"),
            "number": content.get("number"),
            "author": content.get("author").get("login"),
            "labels": issue_labels,
            "is_pr": is_pr,
            "is_issue": is_issue,
            "type": issue_type
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
        entry["body"] = re.sub("Fixes .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fix .*/.*#.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fixes #.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fix #.....*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fixes https.*", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("Fix https.*", "", entry["body"], flags=re.IGNORECASE).strip()

        # Remove "user facing changes" header
        entry["body"] = re.sub("User-facing changes.", "", entry["body"], flags=re.IGNORECASE).strip()
        entry["body"] = re.sub("### User-facing changes", "", entry["body"], flags=re.IGNORECASE).strip()

        duplicate = False
        for o in output:
            if o.get("number") == entry.get("number"):
                duplicate = True
                break
        if not duplicate:
            output.append(entry)

    engine = []
    editor = []
    for o in output:
        if "editor" in o["labels"]:
            editor.append(o)
        else:
            engine.append(o)
 
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

    content = ("# Defold %s\n" % version) + summary + details_engine + details_editor
    with io.open("releasenotes-forum-%s.md" % version, "wb") as f:
        f.write(content.encode('utf-8'))


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
