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
import optparse
import os
import sys

from log import log

DEFOLD_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def build_json(version):
    # Returns the per-version release notes JSON (releasenotes/<version>.json)
    # as produced by releasenotes_github_projectv2.py (which already includes
    # the forum 'external-link'). None if it hasn't been generated yet.
    release_notes_path = os.path.join(DEFOLD_ROOT, 'releasenotes', '%s.json' % version)
    if not os.path.exists(release_notes_path):
        return None
    with open(release_notes_path) as f:
        return f.read()


def _parse_semver(version):
    # "1.13.2" -> (1, 13, 2). Non-numeric parts sort lowest so a malformed
    # entry never displaces real versions at the top of the manifest.
    parts = []
    for part in str(version).split('.'):
        try:
            parts.append(int(part))
        except ValueError:
            parts.append(-1)
    return tuple(parts)


def update_manifest(bucket, version, channel):
    # The manifest is an ordered (newest first) JSON array of every version
    # that has published notes on this channel. The editor reads it to work
    # out which versions sit between the running version and the update, then
    # fetches only those per-version files.
    import botocore
    key = 'editor2/channels/%s/release-notes/manifest.json' % channel
    obj = bucket.Object(key)
    versions = []
    try:
        versions = json.loads(obj.get()['Body'].read())
    except botocore.exceptions.ClientError as e:
        # First release on this channel -> no manifest yet. Any other error
        # (auth, throttling, network) must NOT be ignored, or we'd publish a
        # manifest with only this version and lose the channel's history.
        code = e.response.get('Error', {}).get('Code')
        if code not in ('NoSuchKey', 'NoSuchBucket', '404'):
            raise
    if not isinstance(versions, list):
        sys.exit("%s is not a JSON array (got %s)" % (key, type(versions).__name__))
    versions = sorted(set(versions) | {version}, key=_parse_semver, reverse=True)
    log("Updating release notes manifest (%d versions) -> %s" % (len(versions), key))
    obj.put(Body=json.dumps(versions), ContentType='application/json')


def upload(bucket, version, channel, required=False):
    # Publishes the update dialog's release notes for the current channel:
    #   - release-notes/<version>.json  per-version, accumulates across releases
    #   - release-notes/manifest.json   ordered version list the editor walks
    json_content = build_json(version)
    if json_content is None:
        message = "No release notes for %s in releasenotes/" % version
        if required:
            log(message)
            sys.exit(1)
        log("%s; skipping upload" % message)
        return

    version_obj = bucket.Object('editor2/channels/%s/release-notes/%s.json' % (channel, version))
    log("Uploading per-version release notes for %s -> %s" % (version, version_obj.key))
    version_obj.put(Body=json_content, ContentType='application/json')

    update_manifest(bucket, version, channel)


def _main():
    import s3
    parser = optparse.OptionParser(usage = "usage: %prog --version VERSION --channel CHANNEL")
    parser.add_option('--version', dest = 'version', help = 'Version to upload notes for, e.g. 1.13.0')
    parser.add_option('--channel', dest = 'channel', help = 'Channel to publish to: alpha, beta or stable')
    parser.add_option('--archive-domain', dest = 'archive_domain',
                      default = os.environ.get("DM_ARCHIVE_DOMAIN", "d.defold.com"),
                      help = 'Domain the notes are published to. Default is %default')
    options, _args = parser.parse_args()
    if not options.version:
        parser.error('--version is required')
    if not options.channel:
        parser.error('--channel is required')

    s3.init_boto_data_path()
    bucket = s3.get_bucket(options.archive_domain)
    upload(bucket, options.version, options.channel, required = True)


if __name__ == '__main__':
    _main()
