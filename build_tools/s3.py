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

import run
from log import log
import os
import re
import sys
from urllib.parse import urlparse
from datetime import datetime
from configparser import ConfigParser

s3buckets = {}

def init_boto_data_path():
    data_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../packages/boto3_data'))
    os.environ['AWS_DATA_PATH'] = data_path

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

    import boto3.session

    session = boto3.session.Session(aws_access_key_id=key, aws_secret_access_key=secret, region_name="eu-west-1")
    s3_client = session.resource('s3')    
    bucket = s3_client.Bucket(bucket_name)
    s3buckets[bucket_name] = bucket
    return bucket


def find_files_in_bucket(archive_path, bucket, sha1, path, pattern):
    root = urlparse(archive_path).path[1:]
    base_prefix = os.path.join(root, sha1)
    prefix = os.path.join(base_prefix, path)
    files = []
    for x in bucket.objects.filter(Prefix=prefix):
        if x.key[-1] != '/':
            # Skip directory "keys". When creating empty directories
            # a psudeo-key is created. Directories isn't a first-class object on s3
            if re.match(pattern, x.key):
                name = os.path.relpath(x.key, base_prefix)
                files.append({'name': name, 'path': '/' + x.key})
    return files

# Get archive files for a single release/sha1
def get_files(archive_path, bucket, sha1):
    files = []
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "engine", '.*(/dmengine.*|builtins.zip|classes.dex|android-resources.zip|android.jar|gdc.*|defoldsdk.*|ref-doc.zip)$'))
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "bob", '.*(/bob.jar)$'))
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "editor", '.*(/Defold-.*)$'))
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "dev", '.*(/Defold-.*)$'))
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "alpha", '.*(/Defold-.*)$'))
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "beta", '.*(/Defold-.*)$'))
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "stable", '.*(/Defold-.*)$'))
    files.extend(find_files_in_bucket(archive_path, bucket, sha1, "editor-alpha", '.*(/Defold-.*)$'))
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
    objects = bucket.objects.filter(Prefix=prefix)
    for obj in objects:
        # get the name of the file this key points to
        # archive/sha1/engine/arm64-android/android.jar -> engine/arm64-android/android.jar
        name = obj.key.replace(prefix, "")
        # destination
        new_key = "archive/%s/%s/%s" % (channel, sha1, name)

        print("Prepair %s to be moved to: %s" % (name, new_key))

        # the keys in archive/sha1/* are all redirects to files in archive/channel/sha1/*
        # get the actual file from the redirect
        redirect_path = bucket.Object(obj.key).website_redirect_location
        if not redirect_path:
            # the key is an actual file and not a redirect
            # it shouldn't really happen but it's better to check
            print("The file %s has no redirect. The file will not be moved" % name)
            continue

        # resolve the redirect and get a key to the file
        redirect_name = urlparse(redirect_path).path[1:]
        if redirect_name == new_key:
            print("Skip key `%s` because it's already exist\n" % new_key)
            continue

        # copy the file to the new location
        new_object = bucket.Object(new_key)

        print("Copy object: %s -> %s" % (redirect_name, new_key))
        new_object.copy_from(
            CopySource={'Bucket': bucket_name, 'Key': redirect_name}
        )

        new_redirect = "http://%s/%s" % (bucket_name, new_key)
        print("Create redirection %s to %s\n" % (obj.key, new_redirect))
        # set redirect for old object to new object
        obj.copy_from(
            CopySource={'Bucket': bucket_name, 'Key': obj.key},
            WebsiteRedirectLocation=new_redirect
        )


def redirect_release(archive_path, sha1, channel):
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

    # collect the redirects, ensuring all destination keys point to existing files
    redirected_prefixes = [
        prefix + "engine/",
        prefix + "bob/"
    ]

    redirects = []

    for obj in bucket.objects.filter(Prefix=prefix):
        obj_key = obj.key
        if not any(obj_key.startswith(x) for x in redirected_prefixes):
            continue

        old_target_url = bucket.Object(obj_key).website_redirect_location

        if not old_target_url:
            continue

        name = obj_key.replace(prefix, "")
        new_target_path = "archive/%s/%s/%s" % (channel, sha1, name)
        new_target_url = "http://%s/%s" % (bucket_name, new_target_path)

        if new_target_url == old_target_url:
            continue

        print("Preparing %s" % obj_key)
        print("     From %s" % old_target_url)
        print("       To %s" % new_target_url)

        # ensure we're redirecting to an existing file
        new_target_obj = bucket.Object(new_target_path)
        try:
            new_target_obj.load()
            assert(not new_target_obj.website_redirect_location)
        except:
            raise FileNotFoundError("%s not exist" % new_target_path)

        redirects.append((obj_key, new_target_url))

    if len(redirects) > 0:
        print()

        for (obj_key, new_target_url) in redirects:
            print("Setting %s" % obj_key)
            print("     To %s" % new_target_url)
            obj = bucket.Object(obj_key)
            obj.copy_from(
                CopySource={'Bucket': bucket_name, 'Key': obj_key},
                WebsiteRedirectLocation=new_target_url
            )
    
        # output a list of Objects that should be invalidated in CloudFront.
        # TODO: Use the boto.cloudfront API to do this automatically.
        print()
        print("You should invalidate these Objects in CloudFront:")
        print("https://console.aws.amazon.com/cloudfront/v3/home")
        print()

        for (obj_key, _) in redirects:
            print("/%s" % obj_key)

        print()
