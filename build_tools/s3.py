# Copyright 2020-2023 The Defold Foundation
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

import run
from log import log
import os
import re
import sys
import urllib
from urllib.parse import urlparse
from datetime import datetime
from configparser import ConfigParser

s3buckets = {}

def get_archive_prefix(archive_path, sha1):
    u = urlparse(archive_path)
    assert (u.scheme == 's3')
    prefix = os.path.join(u.path, sha1)[1:]
    return prefix

def get_config_key_and_secret():
    s3configpath = os.path.expanduser("~/.s3cfg")
    awsconfigpath = os.path.expanduser("~/.aws/credentials")

    if os.path.exists(s3configpath):
        config = ConfigParser()
        config.read(s3configpath)
        key = config.get('default', 'access_key')
        secret = config.get('default', 'secret_key')
    elif os.path.exists(awsconfigpath):
        config = ConfigParser()
        config.read(awsconfigpath)
        key = config.get('default', 'aws_access_key_id')
        secret = config.get('default', 'aws_secret_access_key')
    else:
        key = os.getenv("S3_ACCESS_KEY")
        secret = os.getenv("S3_SECRET_KEY")
    return (key, secret)

def get_bucket(bucket_name):
    if bucket_name in s3buckets:
        return s3buckets[bucket_name]

    ks = get_config_key_and_secret()
    key = ks[0]
    secret = ks[1]

    log("get_bucket using key %s" % (key))

    if not (key and secret):
        log('S3 key and/or secret not found in ~/.s3cfg, ~/.aws/credentials or environment variables')
        sys.exit(5)

    from boto.s3.connection import S3Connection
    from boto.s3.connection import OrdinaryCallingFormat
    from boto.s3.key import Key

    # NOTE: We hard-code host (region) here and it should not be required.
    # but we had problems with certain buckets with period characters in the name.
    # Probably related to the following issue https://github.com/boto/boto/issues/621
    conn = S3Connection(key, secret, host='s3-eu-west-1.amazonaws.com', calling_format=OrdinaryCallingFormat())
    bucket = conn.get_bucket(bucket_name)
    s3buckets[bucket_name] = bucket
    return bucket


def find_files_in_bucket(archive_path, bucket, sha1, path, pattern):
    root = urlparse(archive_path).path[1:]
    base_prefix = os.path.join(root, sha1)
    prefix = os.path.join(base_prefix, path)
    files = []
    for x in bucket.list(prefix = prefix):
        if x.name[-1] != '/':
            # Skip directory "keys". When creating empty directories
            # a psudeo-key is created. Directories isn't a first-class object on s3
            if re.match(pattern, x.name):
                name = os.path.relpath(x.name, base_prefix)
                files.append({'name': name, 'path': '/' + x.name})
    return files

# Get archive files for a single release/sha1
def get_files(archive_path, bucket, sha1):
    files = []
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "engine", '.*(/dmengine.*|builtins.zip|classes.dex|android-resources.zip|android.jar|gdc.*|defoldsdk.zip|ref-doc.zip)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "bob", '.*(/bob.jar)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "editor", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "dev", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "alpha", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "beta", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "stable", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "editor-alpha", '.*(/Defold-.*)$')
    return files

def get_tagged_releases(archive_path, pattern=None, num_releases=10):
    u = urlparse(archive_path)
    bucket = get_bucket(u.hostname)

    if pattern is None:
        pattern = "(.*?)$" # captures all tags

    tags = run.shell_command("git for-each-ref --sort=taggerdate --format '%(*objectname) %(refname)' refs/tags").split('\n')
    tags.reverse()
    releases = []

    matches = []
    for line in tags:
        line = line.strip()
        if not line:
            continue
        p = '(.*?) refs/tags/%s' % pattern
        m = re.match('(.*?) refs/tags/%s' % pattern, line)
        if not m:
            continue
        sha1, tag = m.groups()
        matches.append((sha1, tag))

    for sha1, tag in matches[:num_releases]: # Only the first releases
        epoch = run.shell_command('git log -n1 --pretty=%%ct %s' % sha1.strip())
        date = datetime.fromtimestamp(float(epoch))
        files = get_files(archive_path, bucket, sha1)
        if len(files) > 0:
            releases.append({'tag': tag,
                             'sha1': sha1,
                             'abbrevsha1': sha1[:7],
                             'date': str(date),
                             'files': files})

    return releases

def get_single_release(archive_path, version_tag, sha1=None):
    if sha1 is None:
        sha1 = run.shell_command('git rev-list -n 1 tags/%s' % version_tag)
    
    u = urlparse(archive_path)
    bucket = get_bucket(u.hostname)
    files = get_files(archive_path, bucket, sha1)

    return {'tag': version_tag,
            'sha1': sha1,
            'abbrevsha1': sha1[:7],
            'files': files}

def move_release(archive_path, sha1, channel):
    u = urlparse(archive_path)
    # get archive root and bucket name
    # archive root:     s3://d.defold.com/archive -> archive
    # bucket name:      s3://d.defold.com/archive -> d.defold.com
    archive_root = u.path[1:]
    bucket_name = u.hostname
    bucket = get_bucket(bucket_name)

    # the search prefix we use when listing keys
    # we only want the keys associated with specifed sha1
    prefix = "%s/%s/" % (archive_root, sha1)
    keys = bucket.get_all_keys(prefix = prefix)
    for key in keys:
        # get the name of the file this key points to
        # archive/sha1/engine/arm64-android/android.jar -> engine/arm64-android/android.jar
        name = key.name.replace(prefix, "")

        # destination
        new_key = "archive/%s/%s/%s" % (channel, sha1, name)

        # the keys in archive/sha1/* are all redirects to files in archive/channel/sha1/*
        # get the actual file from the redirect
        redirect_path = key.get_redirect()
        if not redirect_path:
            # the key is an actual file and not a redirect
            # it shouldn't really happen but it's better to check
            print("The file %s has no redirect. The file will not be moved" % name)
            continue

        # resolve the redirect and get a key to the file
        redirect_name = urlparse(redirect_path).path[1:]
        redirect_key = bucket.get_key(redirect_name)
        if not redirect_key:
            print("Invalid redirect for %s. The file will not be moved" % redirect_path)
            continue

        # copy the file to the new location
        print("Copying %s to %s" % (redirect_key.name, new_key))
        bucket.copy_key(new_key, bucket_name, redirect_key.name)

        # update the redirect
        new_redirect = "http://%s/%s" % (bucket_name, new_key)
        key.set_redirect(new_redirect)
