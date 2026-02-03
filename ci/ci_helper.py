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



""" Since github actions are quite difficult to use when writing scriptable things.
We rely on this script being able to output the unaltered text.
"""

import os, sys, platform

PLATFORMS_NINTENDO = ['arm64-nx64']
PLATFORMS_SONY     = ['x86_64-ps4', 'x86_64-ps5']
PLATFORMS_XBOX     = ['x86_64-xbone']
PLATFORMS_PRIVATE = PLATFORMS_NINTENDO + PLATFORMS_SONY + PLATFORMS_XBOX

def repo_name_to_platforms(repository):
    # repo is of the form defold/defold-switch
    if 'defold-switch' in repository:
        return PLATFORMS_NINTENDO
    if 'defold-ps4' in repository:
        return PLATFORMS_SONY
    if 'defold-xbox' in repository:
        return PLATFORMS_XBOX
    return []

# We use this function to determine if we should skip this platform
def can_build_private_platform(repository, platform):
    platforms = repo_name_to_platforms(repository)
    return platform in platforms

def is_platform_private(platform):
    return platform in PLATFORMS_PRIVATE

def is_platform_supported(platform):
    repository = os.environ.get('GITHUB_REPOSITORY', None)
    if repository is None:
        return True # probably a local build so we assume the dev knows how to build
    if is_platform_private(platform):
        return can_build_private_platform(repository, platform)
    return True

def is_repo_private():
    repository = os.environ.get('GITHUB_REPOSITORY', None)
    if repository is None:
        return False # probably a local build

    # get the platforms
    platforms = repo_name_to_platforms(repository)
    if platforms is not None:
        for platform in platforms:
            if is_platform_private(platform):
                return True
    return False

# output commands

def is_platform_supported_by_repo(args):
    platform=args[0]
    if not is_platform_private(platform):
        # By default all public platforms are supported
        return True

    repository = os.environ.get('GITHUB_REPOSITORY', None)
    if repository is None:
        return True # probably a local build

    platforms = repo_name_to_platforms(repository)
    if platforms is not None and platform in platforms:
        return True

    return False

def print_values(values):
    with sys.stdout as f:
        for i, value in enumerate(values):
            f.write("%s" % value)
            if i < len(values)-1:
                f.write(" ")

# For use from the ci.yml
if __name__ == '__main__':
    command = sys.argv[1]
    if command == 'is_platform_supported_by_repo':
        result = is_platform_supported_by_repo(sys.argv[2:])
        print_values([result])
