#!/usr/bin/env python3
# Copyright 2020-2022 The Defold Foundation
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

owner = "defold"
repo = "defold"
token = None

TYPE_FIX = "FIX"
TYPE_NEW = "NEW"

def pprint(d):
    print(json.dumps(d, indent=4, sort_keys=True))


def get_project(name):
    response = github.get("repos/%s/%s/projects" % (owner, repo), token)
    for project in response:
        if project["name"] == name:
            return project


def get_cards_in_column(project, column_name):
    columns = github.get(project["columns_url"], token)
    for column in columns:
        if column["name"] == column_name:
            return github.get(column["cards_url"], token)


def get_issue_labels(issue):
    labels = []
    for label in issue["labels"]:
        labels.append(label["name"])
    return labels


def get_issue_timeline(issue, reverse = True):
    timeline = github.get(issue["timeline_url"], token)
    if reverse:
        timeline.reverse()
    return timeline


def get_closing_pull_request(issue):
    if "pull_request" in issue:
        return issue
    timeline = get_issue_timeline(issue)
    for t in timeline:
        if t["event"] in [ "connected", "cross-referenced"]:
            if not "source" in t:
                continue
            issue = t["source"]["issue"]
            if not "pull_request" in issue:
                continue
            if "merged_at" in issue["pull_request"]:
                return issue

def was_issue_pr_merged(issue):
    pr = issue["pull_request"]
    return pr["merged_at"] is not None


def get_issue_type(issue):
    labels = get_issue_labels(issue)
    if "bug" in labels:
        return TYPE_FIX
    elif "feature request" in labels:
        return TYPE_NEW
    return TYPE_FIX


def issue_to_markdown(issue, hide_details = True, title_only = False):
    if title_only:
        md = ("* __%s__: ([#%s](%s)) %s \n" % (issue["type"], issue["number"], issue["url"], issue["title"]))

    else:    
        md = ("__%s__: ([#%s](%s)) __%s__ \n" % (issue["type"], issue["number"], issue["url"], issue["title"]))
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
    for card in get_cards_in_column(project, "Done"):
        content_url = card.get("content_url")
        # do not process cards that doesn't reference an issue
        if not content_url:
            output.append({
                "title": "Note",
                "body": card["note"],
                "url": None,
                "labels": []
            })
            continue

        # get the issue associated with the card
        issue = github.get(content_url, token)
        print("Processing issue %s" % issue["html_url"])

        # only include issues that are closed
        # since we're getting issues from the "Done" column there really should be only closed issues..
        if issue["state"] != "closed":
            print("  Error: Issue is in the Done column but is not closed.")
            continue

        # get the pr that closed the issue
        entry = None
        pr = get_closing_pull_request(issue)

        if pr:
            if not was_issue_pr_merged(pr):
                continue
            entry = {
                "title": pr["title"],
                "body": pr["body"] if pr["body"] != None else "",
                "number": issue["number"],
                "url": issue["html_url"],
                "labels": get_issue_labels(issue),
                "type": get_issue_type(issue)
            }
        else:
            print("  Issue is not associated with a pull request.")
            entry = {
                "title": str(issue["title"]),
                "body": issue["body"] if issue["body"] != None else "",
                "number": issue["number"],
                "url": issue["html_url"],
                "labels": get_issue_labels(issue),
                "type": get_issue_type(issue)
            }
        entry["body"] = re.sub("Fixes #....", "", entry["body"]).strip()
        entry["body"] = re.sub("Fixes https.*", "", entry["body"]).strip()
        entry["body"] = re.sub("Fix #....", "", entry["body"]).strip()
        entry["body"] = re.sub("Fix https.*", "", entry["body"]).strip()
        output.append(entry)

    cards = []
    engine = []
    editor = []
    for o in output:
        if o["url"] is None:
            cards.append(o)
        elif "editor" in o["labels"]:
            editor.append(o)
        else:
            engine.append(o)


    content = ("# Defold %s\n" % version)
    for card in cards:
        content += ("%s\n" % card["body"])


    # list of issue titles
    content += ("\n## Summary\n")
    for issue_type in [TYPE_NEW, TYPE_FIX]:
        for issue in engine:
            if issue["type"] == issue_type:
                content += issue_to_markdown(issue, title_only = True)
        for issue in editor:
            if issue["type"] == issue_type:
                content += issue_to_markdown(issue, title_only = True)

    # the details
    content += ("\n## Engine\n")
    for issue_type in [TYPE_NEW, TYPE_FIX]:
        for issue in engine:
            if issue["type"] == issue_type:
                content += issue_to_markdown(issue, hide_details = hide_details)

    content += ("\n## Editor\n")
    for issue_type in [TYPE_NEW, TYPE_FIX]:
        for issue in editor:
            if issue["type"] == issue_type:
                content += issue_to_markdown(issue, hide_details = hide_details)

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
