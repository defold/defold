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

import json
import os


DEFOLD_PLATFORMS_FILE = '.defold-platforms'


def get_repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def get_platform_roots(platform):
    """Return private source roots for a target platform.

    The optional repo-local .defold-platforms file is JSON keyed by full target
    platform. Each platform entry may contain "root" for a single private repo
    and/or "roots" for an ordered list of private repos:

    {
        "arm64-nx64": {
            "root": "/path/to/defold-switch"
        },
        "x86_64-ps5": {
            "roots": ["/path/to/defold-ps5", "/path/to/shared-private"]
        }
    }

    The returned roots preserve file order: "root" first, then "roots".
    Missing files, missing platform entries, and empty entries return [].
    """
    config_path = os.path.join(get_repo_root(), DEFOLD_PLATFORMS_FILE)
    if not os.path.exists(config_path):
        return []
    with open(config_path, 'r') as f:
        platforms = json.load(f)
    platform_config = platforms.get(platform, {})
    roots = []
    if 'root' in platform_config:
        roots.append(platform_config['root'])
    roots.extend(platform_config.get('roots', []))
    return roots


def get_platform_file_tags(platform):
    target = platform
    if '-' in platform:
        target = platform.split('-')[-1]

    tags = []
    def append_tag(tag):
        if tag and tag not in tags:
            tags.append(tag)

    append_tag(target)

    if target == 'nx64':
        append_tag('switch')
        append_tag('nintendo')
    elif target in ('ps4', 'ps5'):
        append_tag('playstation')
        append_tag('sony')
    elif target == 'xbone':
        append_tag('xbox')
        append_tag('microsoft')
        append_tag('win32')

    if target in ('macos', 'ios'):
        append_tag('darwin')
        append_tag('apple')

    return tags


def get_platform_file_fallback_tags(platform):
    target = platform
    if '-' in platform:
        target = platform.split('-')[-1]

    tags = []
    def append_tag(tag):
        if tag and tag not in tags:
            tags.append(tag)

    if target in ('android', 'ios', 'linux', 'macos', 'web'):
        append_tag('posix')

    return tags
