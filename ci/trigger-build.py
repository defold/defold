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
import requests
import json
from argparse import ArgumentParser

def main(argv):
    parser = ArgumentParser()
    parser.add_argument("--token", dest="token", help="GitHub API personal access token ")
    parser.add_argument("--action", dest="action", default="build", help="The trigger action")
    parser.add_argument("--branch", dest="branch", help="The branch to build")
    parser.add_argument("--skip-engine", dest="skip_engine", action='store_true', default=False, help="Skip building the engine")
    parser.add_argument("--skip-sdk", dest="skip_sdk", action='store_true', default=False, help="Skip building the Defold SDK")
    parser.add_argument("--skip-bob", dest="skip_bob", action='store_true', default=False, help="Skip building bob")
    parser.add_argument("--skip-editor", dest="skip_editor", action='store_true', default=False, help="Skip building the editor")
    parser.add_argument("--skip-sign", dest="skip_sign", action='store_true', default=False, help="Skip signing the artefacts")

    args = parser.parse_args()

    if not args.token:
        args.token = os.environ.get("GITHUB_TOKEN")
    if not args.token:
        print("You must provide a GitHub token")
        exit(1)

    post_data = {
        "event_type": args.action,
        "client_payload": {
            "skip_engine": args.skip_engine,
            "skip_sdk": args.skip_sdk,
            "skip_bob": args.skip_bob,
            "skip_editor": args.skip_editor,
            "skip_sign": args.skip_sign,
            "branch": args.branch
        }
    }

    headers = {
        "Accept": "application/vnd.github.everest-preview+json",
        "Authorization": "token %s" % args.token
    }

    r = requests.post('https://api.github.com/repos/defold/defold/dispatches', json=post_data, headers=headers)
    print(r)

if __name__ == "__main__":
    main(sys.argv[1:])
