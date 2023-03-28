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

from datetime import datetime
from log import log
from string import Template
import base64
import github
import json
import mimetypes
import os
import re
import run
import s3
import subprocess
import urllib
from urllib.parse import urlparse

def get_current_repo():
    # git@github.com:defold/defold.git
    # https://github.com/defold/defold.git
    url = run.shell_command('git remote get-url origin')
    url = url.replace('.git', '')
    url = url.strip()

    domain = "github.com"
    index = url.index(domain)
    if index < 0:
        return None
    return url[index+len(domain)+1:]

def get_git_sha1(ref = 'HEAD'):
    process = subprocess.Popen(['git', 'rev-parse', ref], stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        sys.exit("Unable to find git sha from ref: %s" % (ref))
    return out.strip()

def get_git_branch():
    return run.shell_command('git rev-parse --abbrev-ref HEAD').strip()

def get_defold_version_from_file():
    """ Gets the version number and checks if that tag exists """
    with open('../VERSION', 'r') as version_file:
        version = version_file.read().strip()

    process = subprocess.Popen(['git', 'rev-list', '-n', '1', version], stdout = subprocess.PIPE)
    out, err = process.communicate()
    if process.returncode != 0:
        return None
    return out.strip()

def release(config, tag_name, release_sha, s3_release, editor_only=False, engine_channel=None, editor_channel=None):
    log("Releasing Defold %s to GitHub" % tag_name)
    if config.github_token is None:
        log("No GitHub authorization token")
        return

    channel = config.channel or engine_channel or editor_channel
    if channel is None:
        log("No release channel specified")
        return

    log("tag name: %s" % tag_name)
    log("release sha1: %s" % release_sha)

    if engine_channel or editor_channel:
        if engine_channel:
            log("engine_channel: %s" % engine_channel)
        if editor_channel:
            log("editor_channel: %s" % editor_channel)
    else:
        log("channel: %s" % channel)

    source_repo = os.environ.get('GITHUB_REPOSITORY', "defold/defold")
    source_repo = "/repos/%s" % source_repo

    log("source repo: %s" % source_repo)

    target_repo = config.github_target_repo
    if target_repo is None:
        target_repo = os.environ.get('GITHUB_REPOSITORY', None)
    if target_repo is None:
        target_repo = "defold/defold"
    target_repo = "/repos/%s" % target_repo

    log("target repo: %s" % target_repo)

    release_name = 'v%s - %s' % (config.version, channel)
    draft = False # If true, it won't create a tag

    pre_release = True # If true, it will be marked as "Pre-release" in the UI
    if channel in ('stable','beta') or engine_channel in ('stable','beta') or editor_channel in ('editor-alpha',):
        pre_release = False

    if not s3_release.get("files"):
        log("No files found on S3 with sha %s" % release_sha)
        exit(1)

    release = None
    response = github.get("%s/releases/tags/%s" % (target_repo, tag_name), config.github_token)
    if response:
        release = response

    body = "Defold version %s<br>channel=%s<br>sha1=%s<br>date=%s" % (config.version, channel, release_sha, datetime.now().strftime("%Y-%m-%d %H:%M:%S")),

    if engine_channel or editor_channel:
        body = "Defold version %s<br>engine_channel=%s<br>editor_channel=%s<br>sha1=%s<br>date=%s" % (config.version, engine_channel, editor_channel, release_sha, datetime.now().strftime("%Y-%m-%d %H:%M:%S")),

    data = {
        "tag_name": tag_name,
        "name": release_name,
        "body": body,
        "draft": draft,
        "prerelease": pre_release
    }

    if not release:
        log("No release found with tag %s, creating a new one" % tag_name)
        release = github.post("%s/releases" % target_repo, config.github_token, json = data)
    else:
        release_id = release['id']
        log("Updating existing release: %s - %s - %s" % (tag_name, release_id, release.get('url')) )
        release = github.patch("%s/releases/%s" % (target_repo, release_id), config.github_token, json = data)

    # check that what we created/updated a release
    if not release:
        log("Unable to update GitHub release for %s" % (config.version))
        exit(1)

    # remove existing uploaded assets (It's not currently possible to update a release asset
    log("Deleting existing artifacts from the release")
    prev_assets = {}
    for asset in release.get("assets", []):
        prev_assets[asset.get("name")] = asset

    # upload_url is a Hypermedia link (https://developer.github.com/v3/#hypermedia)
    # Example: https://uploads.github.com/repos/defold/defold/releases/25677114/assets{?name,label}
    # Can be parsed and expanded using: https://pypi.org/project/uritemplate/
    # for now we ignore this and fix it ourselves (note this may break if GitHub
    # changes the way uploads are done)
    log("Uploading artifacts to GitHub from S3")
    base_url = "https://" + urlparse(config.archive_path).hostname

    def is_editor_file(path):
        return os.path.basename(path) in ('Defold-x86_64-macos.dmg',
                                          'Defold-x86_64-linux.zip',
                                          'Defold-x86_64-win32.zip')

    def is_main_file(path):
        return os.path.basename(path) in ('bob.jar',) or is_editor_file(path)

    def is_platform_file(path):
        return os.path.basename(path) in ('gdc',
                                          'gdc.exe')

    def get_platform(path):
        if 'linux' in path: return 'linux'
        if 'darwin' in path: return 'macos'
        if 'macos' in path: return 'macos'
        if 'win32' in path: return 'win'
        return ''

    def convert_to_platform_name(path):
        platform = get_platform(path)
        basename = os.path.basename(path)
        name, ext = os.path.splitext(basename)
        return '%s-%s%s' % (name, platform, ext if ext else '')

    urls = set() # not sure why some files are reported twice, but we don't want to download/upload them twice
    for file in s3_release.get("files", None):
        # download file
        path = file.get("path")

        if editor_only and not is_editor_file(path):
            continue

        keep = False
        if is_main_file(path) or is_platform_file(path):
            keep = True

        if not keep:
            continue

        download_url = base_url + path
        urls.add(download_url)

    upload_url = release.get("upload_url").replace("{?name,label}", "?name=%s")

    for download_url in urls:
        filepath = config._download(download_url)
        filename = re.sub(r'https://%s/archive/(.*?)/' % config.archive_path, '', download_url)
        basename = os.path.basename(filename)
        # file stream upload to GitHub
        with open(filepath, 'rb') as f:
            content_type,_ = mimetypes.guess_type(basename)
            headers = { "Content-Type": content_type or "application/octet-stream" }
            name = filename
            if is_main_file(download_url):
                name = basename
            elif is_platform_file(download_url):
                name = convert_to_platform_name(download_url)

            # Since there is no way to update an asset, we need to remove it first.
            old_asset = prev_assets.get(name, None)
            if old_asset is not None:
                asset_url = old_asset.get("url")
                log("Deleting %s -  %s" % (old_asset.get("id"), old_asset.get("name")))
                github.delete(asset_url, config.github_token)

            url = upload_url % (name)
            log("Uploading to GitHub " + url)
            github.post(url, config.github_token, data = f, headers = headers)

    log("Released Defold %s to GitHub" % tag_name)


if __name__ == '__main__':
    print("For testing only")
    print(get_current_repo())
