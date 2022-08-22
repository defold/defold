#! /usr/bin/env python
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

import os, subprocess, builtins;

class BuildUtilityException(Exception):
    def __init__(self, msg):
        self.msg = msg

class BuildUtility:
    _platform = None
    _build_platform = None
    _dynamo_home = None
    _dynamo_ext = None
    _library_path = None
    _binary_path = None

    _supported_platforms = [
                            {'platform': 'x86_64-linux', 'os': 'linux', 'arch': 'x86_64'},
                            {'platform': 'x86_64-macos', 'os': 'macos', 'arch': 'x86_64'},
                            {'platform': 'arm64-macos', 'os': 'macos', 'arch': 'arm64'},
                            {'platform': 'win32', 'os': 'win', 'arch': 'x86'},
                            {'platform': 'x86_64-win32', 'os': 'win', 'arch': 'x86_64'},
                            {'platform': 'x86_64-ios', 'os': 'ios', 'arch': 'x86_64'},
                            {'platform': 'arm64-ios', 'os': 'ios', 'arch': 'arm64'},
                            {'platform': 'armv7-android', 'os': 'android', 'arch': 'armv7'},
                            {'platform': 'arm64-android', 'os': 'android', 'arch': 'arm64'},
                            {'platform': 'js-web', 'os': 'web', 'arch': 'js'},
                            {'platform': 'wasm-web', 'os': 'web', 'arch': 'wasm'},
                            {'platform': 'arm64-nx64', 'os': 'nx64', 'arch': 'arm64'},
                            ]

    def __init__(self, platform_id, build_platform_id, dynamo_home = None):
        self._initialise_paths(platform_id, build_platform_id, dynamo_home)
        pass
    # __init__

    def get_dynamo_home(self, *subdir):
        return self._build_path(self._dynamo_home, *subdir)
    # get_dynamo_home

    def get_dynamo_ext(self, *subdir):
        return self._build_path(self._dynamo_ext, *subdir)
    # get_dynamo_ext

    def get_target_platform(self):
        return self._platform['platform']
    # get_target_platform

    def get_library_path(self, *subdir):
        return self._build_path(self._library_path, *subdir)
    # get_library_path

    def get_binary_path(self, *subdir):
        return self._build_path(self._binary_path, *subdir)
    # get_binary_path

    def get_target_os(self):
        return self._platform['os']
    # get_target_os

    def get_target_architecture(self):
        return self._platform['arch']
    # get_target_architecture

    def git_sha1(self, ref = None):
        args = 'git log --pretty=%H -n1'.split()
        if ref:
            args.append(ref)
        process = subprocess.Popen(args, stdout = subprocess.PIPE)
        out, err = process.communicate()
        if process.returncode != 0:
            sys.exit(process.returncode)

        line = out.decode().split('\n')[0].strip()
        sha1 = line.split()[0]
        return sha1
    # get_sha1

    def _build_path(self, root, *subdir):
        ret = None
        if subdir:
            ret = os.path.join(root, *subdir)
        else:
            ret = root
        return ret
    # _build_path

    def _identify_platform(self, platform_id):
        platform = None
        for p in self._supported_platforms:
            if p['platform'] == platform_id:
                platform = p
                break
        if platform == None:
            raise BuildUtilityException(("Could not identify platform '%s'" % platform_id))
        return platform
    # _identify_platform

    def _initialise_paths(self, platform_id, build_platform_id, dynamo_home):
        self._platform = self._identify_platform(platform_id)
        self._build_platform = self._identify_platform(build_platform_id)
        if not dynamo_home:
            dynamo_home = os.getenv('DYNAMO_HOME')
        if not dynamo_home:
            raise BuildUtilityException("DYNAMO_HOME not set")
        self._dynamo_home = dynamo_home
        self._dynamo_ext = os.path.join(self._dynamo_home, 'ext')
        self._library_path = os.path.join(self._dynamo_home, 'lib', self._platform['platform'])
        self._binary_path = os.path.join(self._dynamo_home, 'bin', self._platform['platform'])
    # _initialise_paths

# class BuildUtility

def create_build_utility(env):
    return BuildUtility(env['PLATFORM'], env['BUILD_PLATFORM'])
# create_build_utility
