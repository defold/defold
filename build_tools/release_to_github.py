import urlparse
import re
import github
import mimetypes
import s3
from log import log

def release(config):
    log("Releasing Defold %s to GitHub" % (config.version))
    if config.github_token is None:
        log("No GitHub authorization token")
        return

    # get the sha for the version we are releasing by
    # searching all tags for the version
    ref_tag = None
    ref_tags = github.get("https://api.github.com/repos/defold/defold/git/refs/tags", config.github_token)
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
    release_tag = github.get("https://api.github.com/repos/defold/defold/git/tags/%s" % (ref_tag.get("object").get("sha")), config.github_token)
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
    response = github.get("https://api.github.com/repos/defold/defold/releases", config.github_token)
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
        release = github.post("https://api.github.com/repos/defold/defold/releases", config.github_token, json = data)
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
        github.delete("https://api.github.com/repos/defold/defold/releases/assets/%s" % (asset.get("id")), config.github_token)

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
