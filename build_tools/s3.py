import run
from log import log
import os
import re
import sys
import urlparse
from datetime import datetime
from ConfigParser import ConfigParser



s3buckets = {}

def get_archive_prefix(archive_path, sha1):
    u = urlparse.urlparse(archive_path)
    assert (u.scheme == 's3')
    prefix = os.path.join(u.path, sha1)[1:]
    return prefix


def get_bucket(bucket_name):
    if bucket_name in s3buckets:
        return s3buckets[bucket_name]

    configpath = os.path.expanduser("~/.s3cfg")
    if os.path.exists(configpath):
        config = ConfigParser()
        config.read(configpath)
        key = config.get('default', 'access_key')
        secret = config.get('default', 'secret_key')
    else:
        key = os.getenv("S3_ACCESS_KEY")
        secret = os.getenv("S3_SECRET_KEY")

    log("get_bucket key %s" % (key))

    if not (key and secret):
        log('S3 key and/or secret not found in .s3cfg or environment variables')
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
    root = urlparse.urlparse(archive_path).path[1:]
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
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "engine", '.*(/dmengine.*|builtins.zip|classes.dex|android-resources.zip|android.jar|gdc.*|defoldsdk.zip)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "bob", '.*(/bob.jar)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "editor", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "alpha", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "beta", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "stable", '.*(/Defold-.*)$')
    files = files + find_files_in_bucket(archive_path, bucket, sha1, "editor-alpha", '.*(/Defold-.*)$')
    return files

def get_tagged_releases(archive_path):
    u = urlparse.urlparse(archive_path)
    bucket = get_bucket(u.hostname)

    tags = run.shell_command("git for-each-ref --sort=taggerdate --format '%(*objectname) %(refname)' refs/tags").split('\n')
    tags.reverse()
    releases = []
    for line in tags:
        line = line.strip()
        if not line:
            continue
        m = re.match('(.*?) refs/tags/(.*?)$', line)
        if not m:
            continue
        sha1, tag = m.groups()
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

def get_single_release(archive_path, version_tag, sha1):
    u = urlparse.urlparse(archive_path)
    bucket = get_bucket(u.hostname)
    files = get_files(archive_path, bucket, sha1)

    return {'tag': version_tag,
            'sha1': sha1,
            'abbrevsha1': sha1[:7],
            'files': files}
