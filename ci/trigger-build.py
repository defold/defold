#!/usr/bin/env python
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



import sys
import os
import re
import requests
from argparse import ArgumentParser
from urllib.parse import quote

def main(argv):
    parser = ArgumentParser()
    parser.add_argument("--token", dest="token", help="GitHub API personal access token ")
    # A trusted build with the full set of secrets. Use pr-ok-to-test.yml to build an
    # external contribution, not this script.
    parser.add_argument("--action", dest="action", default="build", help="The trigger action (event_type)")
    parser.add_argument("--branch", dest="branch", required=True, help="The branch, tag, or commit to build")
    parser.add_argument("--skip-engine", dest="skip_engine", action='store_true', default=False, help="Skip building the engine")
    parser.add_argument("--skip-sdk", dest="skip_sdk", action='store_true', default=False, help="Skip building the Defold SDK")
    parser.add_argument("--skip-bob", dest="skip_bob", action='store_true', default=False, help="Skip building bob")
    parser.add_argument("--skip-editor", dest="skip_editor", action='store_true', default=False, help="Skip building the editor")
    parser.add_argument("--skip-sign", dest="skip_sign", action='store_true', default=False, help="Skip signing the artefacts")

    args = parser.parse_args(argv)

    if not args.token:
        args.token = os.environ.get("GITHUB_TOKEN")
    if not args.token:
        print("You must provide a GitHub token")
        exit(1)

    headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": "token %s" % args.token
    }

    # Freeze the requested ref before dispatch so all jobs use the same commit.
    response = requests.get('https://api.github.com/repos/defold/defold/commits/%s' % quote(args.branch, safe=''),
                            headers=headers, timeout=(10, 60))
    response.raise_for_status()
    commit = response.json()
    sha = commit.get('sha') if isinstance(commit, dict) else None
    if not isinstance(sha, str) or not re.fullmatch('[0-9a-f]{40}', sha):
        parser.error('Unable to resolve --branch to a full commit SHA')

    post_data = {
        "event_type": args.action,
        "client_payload": {
            "skip_engine": args.skip_engine,
            "skip_sdk": args.skip_sdk,
            "skip_bob": args.skip_bob,
            "skip_editor": args.skip_editor,
            "skip_sign": args.skip_sign,
            "branch": args.branch,
            "sha": sha
        }
    }

    r = requests.post('https://api.github.com/repos/defold/defold/dispatches', json=post_data, headers=headers,
                      timeout=(10, 60))
    r.raise_for_status()
    print(r)

if __name__ == "__main__":
    main(sys.argv[1:])
