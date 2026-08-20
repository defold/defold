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

import os, sys, platform, subprocess

PLATFORMS_NINTENDO = ['arm64-nx64']
PLATFORMS_SONY     = ['x86_64-ps4', 'x86_64-ps5']
PLATFORMS_XBOX     = ['x86_64-xbone']
PLATFORMS_PRIVATE = PLATFORMS_NINTENDO + PLATFORMS_SONY + PLATFORMS_XBOX

PRIVATE_PLATFORM_ALWAYS_BUILD_BRANCHES = ('dev', 'beta', 'master')

PRIVATE_PLATFORM_SOURCE_PATH_PREFIXES = (
    'engine/crash/',
    'engine/dlib/',
    'engine/engine/',
    'engine/graphics/',
    'engine/hid/',
    'engine/platform/',
    'engine/profiler/',
    'engine/sound/',
    'engine/testmain/',
    'engine/tools/',
)

PRIVATE_PLATFORM_BUILD_PATH_PREFIXES = (
    'scripts/cmake/',
    'build_tools/',
)

PRIVATE_PLATFORM_BUILD_PATHS = (
    'scripts/build.py',
    'ci/ci.py',
    'ci/ci.sh',
    'ci/ci_helper.py',
)

PRIVATE_PLATFORM_PACKAGE_TAGS = (
    '-common.',
    '-win32.',
)

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

def is_private_platform_build_path(path):
    path = path.replace('\\', '/').removeprefix('./')
    if path in PRIVATE_PLATFORM_BUILD_PATHS:
        return True
    if path.startswith(PRIVATE_PLATFORM_SOURCE_PATH_PREFIXES + PRIVATE_PLATFORM_BUILD_PATH_PREFIXES):
        return True
    if path.startswith('packages/'):
        filename = path.rsplit('/', 1)[-1]
        return any(tag in filename for tag in PRIVATE_PLATFORM_PACKAGE_TAGS)
    return False

def run_gh(args):
    try:
        result = subprocess.run(
            ['gh'] + args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=60)
    except (OSError, subprocess.TimeoutExpired) as error:
        print("Could not query pull requests: %s" % error, file=sys.stderr)
        return None

    if result.returncode != 0:
        error = result.stderr.strip() or 'unknown gh error'
        print("Could not query pull requests: %s" % error, file=sys.stderr)
        return None
    return result.stdout

def should_build_private_platform(branch, repository):
    if branch in PRIVATE_PLATFORM_ALWAYS_BUILD_BRANCHES:
        print("Building private platforms for release branch '%s'" % branch, file=sys.stderr)
        return True

    if not branch or not repository:
        print("Skipping private platforms because the branch or repository is unknown", file=sys.stderr)
        return False

    pull_request_output = run_gh([
        'pr', 'list',
        '--repo', repository,
        '--state', 'open',
        '--head', branch,
        '--json', 'number',
        '--jq', '.[].number',
    ])
    if pull_request_output is None:
        return False

    pull_request_numbers = [number for number in pull_request_output.splitlines() if number]
    if not pull_request_numbers:
        print("Skipping private platforms because branch '%s' has no open pull request" % branch, file=sys.stderr)
        return False

    changed_paths = set()
    for pull_request_number in pull_request_numbers:
        changed_path_output = run_gh([
            'api', '--paginate',
            'repos/%s/pulls/%s/files' % (repository, pull_request_number),
            '--jq', '.[] | .filename, (.previous_filename // empty)',
        ])
        if changed_path_output is None:
            return False
        changed_paths.update(path for path in changed_path_output.splitlines() if path)

    for path in sorted(changed_paths):
        if is_private_platform_build_path(path):
            print("Building private platforms because pull request path '%s' is relevant" % path, file=sys.stderr)
            return True

    print("Skipping private platforms because the pull request has no relevant changes", file=sys.stderr)
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
