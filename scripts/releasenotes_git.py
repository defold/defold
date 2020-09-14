#!/usr/bin/env python
# Copyright 2020 The Defold Foundation
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



import os, sys, subprocess, re

BETA_INTRO = """# Defold %s BETA
The latest beta is now released, and we invite those interested in beta testing the new features in the editor and engine to join now.
The beta period will be 2 weeks and the next planned stable release is two weeks from now.

We hope this new workflow will highlight any issues earlier, and also get valuable feedback from our users. And please comment if you come up with ideas on improving on this new workflow.

Please report any engine issues in this thread or in [issues](https://github.com/defold/defold/issues) using Help -> Report Issue

Thx for helping out!

## Disclaimer
This is a BETA release, and it might have issues that could potentially be disruptive for you and your teams workflow. Use with caution. Use of source control for your projects is strongly recommended.

## Access to the beta
Download the editor or bob.jar from http://d.defold.com/beta/

Set your build server to https://build-stage.defold.com

"""

def run(cmd, shell=False):
    p = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, shell=shell)
    p.wait()
    out, err = p.communicate()
    if p.returncode != 0:
        raise Exception("Failed to run: " + cmd)

    return out

def read_version():
    # read the version number from the VERSION file
    with open('VERSION', 'rb') as f:
        d = f.read()
        tokens = d.split('.')
        return map(int, tokens)
    return None

def get_sha1_from_tag(tag):
    return run('git log -1 --format=format:%%H %s' % tag)


def git_log(sha1):
    return run("git log %s -1" % sha1)


def git_merge_desc(sha1):
    s = run("git show %s" % sha1)
    desc = ''
    skip_lines = 1
    for l in s.split('\n'):
        l = l.strip()
        words = l.split()
        if not words:
            continue
        if words[0] in ('Merge:', 'Author:', 'Date:', 'commit'):
            continue
        if skip_lines > 0:
            skip_lines = skip_lines-1
            continue
        return l # we only need first line
    return ""

def match_issue(line):
    # 974d82a24 Issue-4684 - Load vulkan functions dynamically on android (#4692)
    issue_match = re.search("^(?i)([a-fA-F0-9]+) (?:issue[\-\s]?)?#?(\d+)[:.]? (.*)", line)
    if issue_match:
        sha1 = issue_match.group(1)
        issue = issue_match.group(2)
        desc = issue_match.group(3)
        # get rid of PR number at the end of the commit
        m = re.search("^(.*) \(\#\d+\)$", desc)
        if m:
            desc = m.group(1)
        return (sha1, issue, desc)
    return (None, None, None)

def match_merge(line):
    # 3bd2324df Merge pull request #5061 from defold/issue-5060-engine-info-platform
    # 3bd2324df Revert "Merge pull request #5030 from defold/issue-5029-emscripten-1-39-20"
    merge_match = re.search("(\w+)\s(?:Revert\s\")?Merge pull request\s#(\d+)\s.+?(?:Issue|issue[\-]?)?(\d+).+", line)
    if merge_match:
        sha1  = merge_match.group(1)
        pr    = merge_match.group(2)
        issue = merge_match.group(3)
        desc  = git_merge_desc(sha1)

        if 'Revert' in line:
            print "MAWE", line, ":"
            print "MAWE", sha1, pr, issue, desc

        # get rid of PR number at the end of the commit
        m = re.search("^(?:Issue|issue)(?:[\-\s]?)?#?(\d+)[:.\-\s]+(.+)", desc)
        if m:
            desc = m.group(2)
            if not issue:
                issue = m.group(1)
        if 'Revert' in line:
            print "MAWE", sha1, pr, issue, desc
        return (sha1, issue, desc)
    return (None, None, None)


def match_pullrequest(line):
    # bca92cc0f Check that there's a world before creating a collision object (#4747)
    pull_match = re.search("([a-fA-F0-9]+) (.*) \(\#(\d+)\)$", line)
    if pull_match:
        sha1 = pull_match.group(1)
        desc = pull_match.group(2)
        pr = pull_match.group(3)
        return (sha1, pr, desc)
    return (None, None, None)

def get_engine_issues(lines):
    issues = []
    for line in lines:
        (sha1, issue, desc) = match_issue(line)
        if issue:
            issues.append("[`Issue-%s`](https://github.com/defold/defold/issues/%s) - **Fixed**: %s" % (issue, issue, desc))
            print(git_log(sha1))
            continue

        (sha1, issue, desc) = match_merge(line)
        if issue:
            issues.append("[`Issue-%s`](https://github.com/defold/defold/issues/%s) - **Fixed**: %s" % (issue, issue, desc))

            if '1' == issue:
                print "MAWE fail:", line, ":"
                print "MAWE fail:", sha1, issue, desc, issues[-1]


            if 'Revert' in line:
                print "MAWE result:", line, ":"
                print "MAWE result:", sha1, issue, desc, issues[-1]


            print(git_log(sha1))
            continue

        (sha1, issue, desc) = match_pullrequest(line)
        if issue:
            issues.append("[`Issue-%s`](https://github.com/defold/defold/issues/%s) - **Fixed**: %s" % (issue, issue, desc))
            print(git_log(sha1))
            continue

    return issues

def get_editor_issues(lines):
    issues = []
    for line in lines:
        # bca92cc0f Foobar (DEFEDIT-4747)
        m = re.search("^([a-fA-F0-9]+) (.*) \(DEFEDIT-(\d+)\)", line)
        if m:
            sha1 = m.group(1)
            desc = m.group(2)
            issue = m.group(3)
            issues.append("[`DEFEDIT-%s`](https://github.com/defold/defold/search?q=hash%%3A%s&type=Commits) - **Fixed**: %s" % (issue, sha1, desc))
            print(git_log(sha1))
    return issues

def get_all_changes(version, sha1):
    out = run("git log %s..HEAD --oneline" % sha1)
    lines = out.split('\n')

    print out
    print "#" + "*" * 64

    engine_issues = get_engine_issues(lines)
    editor_issues = get_editor_issues(lines)

    print ""
    print "#" + "*" * 64
    print ""
    print BETA_INTRO % version
    print "# Engine"

    for issue in sorted(list(set(engine_issues))):
        print("  * " + issue)

    print "# Editor"

    for issue in sorted(list(set(editor_issues))):
        print("  * " + issue)


def get_contributors(tag):
    print ""
    print ""
    print "# Contributors"
    print ""
    print "We'd also like to take the opportunity to thank our community for contributing to the source code."
    print "This is the number of contributions since the last release."
    print ""

    r = run("scripts/list_contributors.sh %s" % tag)
    print r


if __name__ == '__main__':
    current_version = read_version()
    if current_version is None:
        print >>sys.stderr, "Failed to open VERSION"
        sys.exit(1)

    tag = "%d.%d.%d" % (current_version[0], current_version[1], current_version[2]-1)
    sha1 = get_sha1_from_tag(tag)
    if sha1 is None:
        print >>sys.stderr, "Failed to rad tag '%s'" % tag
        sys.exit(1)

    print "Found previous version", tag, sha1

    version = "%d.%d.%d" % (current_version[0], current_version[1], current_version[2])
    get_all_changes(version, sha1)

    #get_contributors(tag)
