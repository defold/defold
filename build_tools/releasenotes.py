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
import os


def build_json(config):
    # Returns the per-version release notes JSON (releasenotes/<version>.json)
    # as produced by releasenotes_github_projectv2.py (which already includes
    # the forum 'external-link'). None if it hasn't been generated yet.
    release_notes_path = os.path.join(config.defold_root, 'releasenotes', '%s.json' % config.version)
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


def update_manifest(config, bucket):
    # The manifest is an ordered (newest first) JSON array of every version
    # that has published notes on this channel. The editor reads it to work
    # out which versions sit between the running version and the update, then
    # fetches only those per-version files.
    import botocore
    key = 'editor2/channels/%s/release-notes/manifest.json' % config.channel
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
        versions = []
    versions = sorted(set(versions) | {config.version}, key=_parse_semver, reverse=True)
    config._log("Updating release notes manifest (%d versions) -> %s" % (len(versions), key))
    obj.put(Body=json.dumps(versions), ContentType='application/json')


def upload(config, bucket, required=False):
    # Publishes the update dialog's release notes for the current channel:
    #   - release-notes/<version>.json  per-version, accumulates across releases
    #   - release-notes/manifest.json   ordered version list the editor walks
    # Alpha/dev builds don't ship notes yet, but for beta/stable the release must
    # fail before it starts offering the update if notes are missing.
    json_content = build_json(config)
    if json_content is None:
        message = "No release notes for %s in releasenotes/" % config.version
        if required:
            config.fatal(message)
        config._log("%s; skipping upload" % message)
        return

    version_obj = bucket.Object('editor2/channels/%s/release-notes/%s.json' % (config.channel, config.version))
    config._log("Uploading per-version release notes for %s -> %s" % (config.version, version_obj.key))
    version_obj.put(Body=json_content, ContentType='application/json')

    update_manifest(config, bucket)
