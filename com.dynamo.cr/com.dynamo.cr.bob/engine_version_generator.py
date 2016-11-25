#!/usr/bin/env python

import subprocess, sys, os
from datetime import datetime

def git_sha1():
    args = 'git log --pretty=%H -n1'.split()
    process = subprocess.Popen(args, stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        sys.exit(process.returncode)

    line = out.split('\n')[0].strip()
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