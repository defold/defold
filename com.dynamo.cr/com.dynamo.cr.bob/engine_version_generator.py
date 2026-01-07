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



import subprocess, sys, os
from datetime import datetime

def git_sha1():
    args = 'git log --pretty=%H -n1'.split()
    process = subprocess.Popen(args, stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        sys.exit(process.returncode)

    line = str(out.decode()).split('\n')[0].strip()
    sha1 = line.split()[0]
    return sha1

if __name__ == '__main__':
    engine_version_java = """
        package com.dynamo.bob.archive;
        public class EngineVersion {
            public static final String version = "%(version)s";
            public static final String sha1 = "%(sha1)s";
            public static final String timestamp = "%(timestamp)s";
        }
    """

    with open('../../VERSION', 'r') as f:
        version = f.readline().strip()
    sha1 = git_sha1()
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    fullpath_java = os.path.abspath('src/com/dynamo/bob/archive/EngineVersion.java')
    with open(fullpath_java, 'w') as f:
        f.write(engine_version_java % {"version": version, "sha1": sha1, "timestamp": timestamp})
