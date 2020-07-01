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
import urlparse

def get_default_repo():
    url = run.shell_command('git remote get-url origin')
    if not 'github.com' in url:
        return None
    # git@github.com:defold/defold.git
    return url.replace('git@github.com:', '').replace('.git', '')

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

def release(config):
    log("Releasing Defold %s to GitHub" % (config.version))
    if config.github_token is None:
        log("No GitHub authorization token")
        return

    repo = config.github_target_repo
    if repo is None:
        repo = "defold/defold"
    repo = "/repos/%s" % repo

    # get the sha for the version we are releasing by
    # searching all tags for the version
    ref_tag = None
    ref_tags = github.get("%s/git/refs/tags" % repo, config.github_token)
    if not ref_tags:
        log("Unable to get ref tags")
        exit(1)
    for tag in ref_tags:
        if config.version in tag.get("ref"):
            ref_tag = tag
            break
    if not ref_tag:
        log("Unable to find ref tag for version %s" % (config.version))
        exit(1)
    release_tag = github.get("%s/git/tags/%s" % (repo, ref_tag.get("object").get("sha")), config.github_token)
    if not release_tag:
        log("Unable to get release tag")
        exit(1)
    release_sha = release_tag.get("object").get("sha")

    # get the release on S3
    log("Getting S3 release for version %s with sha %s" % (config.version, release_sha))
    s3_release = s3.get_single_release(config.archive_path, config.version, release_sha)
    if not s3_release.get("files"):
        log("No files found on S3")
        exit(1)

    # try to find existing release for the current version
    release = None
    response = github.get("%s/releases" % repo, config.github_token)
    if not response:
        log("Unable to get releases")
        exit(1)
    for r in response:
        if r.get("tag_name") == config.version:
            release = r
            break

    # no existing relase, create one!
    if not release:
        data = {
            "tag_name": config.version,
            "name": "v" + config.version,
            "body": "Defold version " + config.version,
            "draft": False,
            "prerelease": False
        }
        release = github.post("%s/releases" % repo, config.github_token, json = data)
        if not release:
            log("Unable to create new release")
            exit(1)

    # check that what we found/created a release
    if not release:
        log("Unable to create GitHub release for %s" % (config.version))
        exit(1)

    # remove existing uploaded assets
    log("Deleting existing artifacts from the release")
    for asset in release.get("assets", []):
        log("Deleting %s" % (asset.get("id")))
        github.delete("%s/releases/assets/%s" % (repo, asset.get("id")), config.github_token)

    # upload_url is a Hypermedia link (https://developer.github.com/v3/#hypermedia)
    # Example: https://uploads.github.com/repos/defold/defold/releases/25677114/assets{?name,label}
    # Can be parsed and expanded using: https://pypi.org/project/uritemplate/
    # for now we ignore this and fix it ourselves (note this may break if GitHub
    # changes the way uploads are done)
    log("Uploading artifacts to GitHub from S3")
    upload_url = release.get("upload_url").replace("{?name,label}", "?name=%s")
    base_url = "https://" + urlparse.urlparse(config.archive_path).hostname
    for file in s3_release.get("files", None):
        # download file
        download_url = base_url + file.get("path")
        filepath = config._download(download_url)
        filename = re.sub(r'https://d.defold.com/archive/(.*?)/', '', download_url)
        # file stream upload to GitHub
        log("Uploading to GitHub " + filename)
        with open(filepath, 'rb') as f:
            content_type,_ = mimetypes.guess_type(filename)
            headers = { "Content-Type": content_type or "application/octet-stream" }
            github.post(upload_url % (filename), config.github_token, data = f, headers = headers)

    log("Released Defold %s to GitHub" % (config.version))


MARKDOWN_TEMPLATE="""
<details><summary>%(version)s - <code>%(branch)s</code> - <code>%(sha1)s</code> - <code>%(date)s</code></summary>

Branch: `%(branch)s`<br/>
Sha1:   `%(sha1)s`<br/>

* [bob.jar](%(url_bob)s)
* [Editor OSX](%(url_editor_osx)s)
* [Editor Windows](%(url_editor_win)s)
* [Editor Linux](%(url_editor_lin)s)

</details>
"""

def release_markdown(config):
    """
    Produce a markdown with each release item
    Pushes changes to a file releases.md
    Currently only grabs bob and editor
    """

    log("Releasing Defold %s to GitHub" % (config.version))
    if config.github_token is None:
        log("No GitHub authorization token")
        return

    repo = config.github_target_repo
    if repo is None:
        repo = get_default_repo()
    repo = "/repos/%s" % repo

    target_url = "%s/contents/RELEASES.md" % repo

    release_sha = config.github_sha1
    if not release_sha:
        release_sha = get_git_sha1()

    # get the release on S3
    log("Getting S3 release for version %s with sha %s" % (config.version, release_sha))
    s3_release = s3.get_single_release(config.archive_path, config.version, release_sha)
    if not s3_release.get("files"):
        log("No files found on S3")
        exit(1)

    def test_path(path):
        if path.endswith('bob.jar'):
            return True
        if 'Defold-x86_64-' in path:
            return True
        return False

    release_info = {}

    base_url = "https://" + urlparse.urlparse(config.archive_path).hostname
    for file in s3_release.get("files", None):
        # download file
        download_url = base_url + file.get("path")
        if not test_path(download_url):
            continue

        if download_url.endswith('bob.jar'):
            release_info['url_bob'] = download_url
        if 'Defold-x86_64-' in download_url:
            editor_platform = 'osx'
            if 'win32' in download_url:
                editor_platform = 'win'
            if 'linux' in download_url:
                editor_platform = 'lin'
            release_info['url_editor_%s' % editor_platform] = download_url

        print "url", download_url

    release_info['version'] = config.version
    release_info['sha1'] = release_sha
    release_info['branch'] = get_git_branch()
    release_info['date'] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    response = github.get(target_url, config.github_token)
    if not response:
        log("Unable to get target_url: " + target_url)
        exit(1)

    release_data = ''
    target_sha = None
    for r in response:
        if r == 'sha':
            target_sha = response.get(r)
        if r == 'content':
            data = response.get(r)
            release_data = base64.b64decode(data).decode('ascii')

    new_data =  MARKDOWN_TEMPLATE % release_info
    new_data += '\n\n' + release_data

    log("Uploading to GitHub " + target_url)
    post_data = {}
    post_data['message'] = "Release %s - %s" % (config.version, release_sha)
    post_data['content'] = base64.b64encode(new_data.encode('ascii')).decode('ascii')
    post_data['sha'] = target_sha # we need the current sha of the target file in order to update it

    headers = { "Content-Type": "application/octet-stream" }
    github.put(target_url, config.github_token, json = post_data, headers = headers)

    log("Released Defold %s - %s to %s" % (config.version, release_sha, target_url))
