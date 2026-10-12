#!/usr/bin/env python
import argparse, json, re, subprocess, sys, os
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
    parser = argparse.ArgumentParser()
    parser.add_argument('--channel', default='dev', help='Release channel to embed in Bob')
    args = parser.parse_args()
    channel = json.dumps(args.channel)

    engine_version_java = """
        package com.dynamo.bob.archive;
        public class EngineVersion {
            public static final String version = "%(version)s";
            public static final String sha1 = "%(sha1)s";
            public static final String timestamp = "%(timestamp)s";
            public static final String channel = %(channel)s;
        }
    """

    with open('../../VERSION', 'r') as f:
        version = f.readline().strip()
    sha1 = git_sha1()
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    fullpath_java = os.path.abspath('src/com/dynamo/bob/archive/EngineVersion.java')
    if os.path.exists(fullpath_java):
        with open(fullpath_java, 'r') as f:
            current = f.read()
        current_version = re.search(r'public static final String version = "([^"]*)";', current)
        current_sha1 = re.search(r'public static final String sha1 = "([^"]*)";', current)
        current_timestamp = re.search(r'public static final String timestamp = "([^"]*)";', current)
        current_channel = re.search(r'public static final String channel = (".*");', current)
        if (current_version and current_version.group(1) == version and
                current_sha1 and current_sha1.group(1) == sha1 and
                current_channel and current_channel.group(1) == channel and
                current_timestamp):
            timestamp = current_timestamp.group(1)

    content = engine_version_java % {"version": version, "sha1": sha1, "timestamp": timestamp, "channel": channel}
    if os.path.exists(fullpath_java):
        with open(fullpath_java, 'r') as f:
            if f.read() == content:
                sys.exit(0)

    with open(fullpath_java, 'w') as f:
        f.write(content)
