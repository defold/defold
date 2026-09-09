#!/usr/bin/env python3
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

"""Download artifacts published by Defold's main CI workflow."""

import argparse
import json
import os
import platform as host_platform
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import uuid
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


ARCHIVE_BASE_URL = "https://d.defold.com/archive"
DOWNLOAD_BASE_URL = "https://d.defold.com"
GITHUB_API_URL = "https://api.github.com/repos/defold/defold"
WORKFLOW = "main-ci.yml"
ARCHIVE_CHANNELS = ("dev", "alpha", "beta", "stable")
PUBLISHED_CHANNELS = ("alpha", "beta", "stable")
CHANNEL_BRANCHES = {"alpha": "dev", "beta": "beta", "stable": "master"}
EDITOR_FILENAMES = {
    "arm64-macos": "Defold-arm64-macos.dmg",
    "x86_64-macos": "Defold-x86_64-macos.dmg",
    "x86_64-linux": "Defold-x86_64-linux.zip",
    "x86_64-win32": "Defold-x86_64-win32.zip",
}
ENGINE_ARTIFACTS = ("dmengine", "dmengine-release", "dmengine-headless")


class DownloadArtifactError(Exception):
    pass


@dataclass(frozen=True)
class Build:
    sha1: str
    archive_channel: str
    description: str
    hash_suffix: str | None = None


@dataclass(frozen=True)
class Artifact:
    name: str
    archive_path: str
    filename: str
    install_type: str | None = None


class HttpClient:
    def __init__(self, github_token=None):
        self.github_token = github_token

    def _request(self, url, method="GET"):
        headers = {"User-Agent": "defold-download-artifact"}
        if url.startswith(GITHUB_API_URL):
            headers["Accept"] = "application/vnd.github+json"
            headers["X-GitHub-Api-Version"] = "2022-11-28"
            if self.github_token:
                headers["Authorization"] = "Bearer %s" % self.github_token
        return urllib.request.Request(url, headers=headers, method=method)

    def get_json(self, url):
        try:
            with urllib.request.urlopen(self._request(url), timeout=30) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            detail = ""
            try:
                payload = json.loads(error.read().decode("utf-8"))
                detail = ": %s" % payload.get("message", "")
            except Exception:
                pass
            raise DownloadArtifactError("HTTP %s for %s%s" % (error.code, url, detail)) from error
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
            raise DownloadArtifactError("Unable to read %s: %s" % (url, error)) from error

    def exists(self, url):
        try:
            with urllib.request.urlopen(self._request(url, method="HEAD"), timeout=30):
                return True
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return False
            raise DownloadArtifactError("HTTP %s while checking %s" % (error.code, url)) from error
        except (urllib.error.URLError, TimeoutError) as error:
            raise DownloadArtifactError("Unable to check %s: %s" % (url, error)) from error

    def download(self, url, destination):
        destination = Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary_name = tempfile.mkstemp(prefix=destination.name + ".", suffix=".part", dir=destination.parent)
        downloaded = 0
        try:
            with os.fdopen(fd, "wb") as output:
                try:
                    with urllib.request.urlopen(self._request(url), timeout=60) as response:
                        expected = response.headers.get("Content-Length")
                        while True:
                            chunk = response.read(1024 * 1024)
                            if not chunk:
                                break
                            output.write(chunk)
                            downloaded += len(chunk)
                except urllib.error.HTTPError as error:
                    if error.code == 404:
                        raise DownloadArtifactError(
                            "Artifact does not exist:\n%s\nNo fallback was attempted." % url) from error
                    raise DownloadArtifactError("HTTP %s while downloading %s" % (error.code, url)) from error
                except (urllib.error.URLError, TimeoutError) as error:
                    raise DownloadArtifactError("Unable to download %s: %s" % (url, error)) from error

            if expected is not None and downloaded != int(expected):
                raise DownloadArtifactError(
                    "Incomplete download from %s: expected %s bytes, received %s" % (url, expected, downloaded))
            os.replace(temporary_name, destination)
        except Exception:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass
            raise


def get_host_platform():
    machine = host_platform.machine().lower()
    if machine == "amd64":
        machine = "x86_64"
    if machine == "aarch64":
        machine = "arm64"
    if sys.platform == "linux":
        return "%s-linux" % machine
    if sys.platform == "win32":
        return "%s-win32" % ("x86_64" if machine == "arm64" else machine)
    if sys.platform == "darwin":
        return "%s-macos" % machine
    raise DownloadArtifactError("Unsupported host platform: %s, %s" % (sys.platform, machine))


def archive_channel_for_branch(branch):
    return {"dev": "alpha", "beta": "beta", "master": "stable"}.get(branch, "dev")


def github_url(path, query=None):
    url = "%s/%s" % (GITHUB_API_URL, path.lstrip("/"))
    if query:
        url += "?" + urllib.parse.urlencode(query)
    return url


def resolve_commit(ref, http):
    data = http.get_json(github_url("commits/%s" % urllib.parse.quote(ref, safe="")))
    sha1 = data.get("sha", "")
    if not re.fullmatch(r"[0-9a-f]{40}", sha1):
        raise DownloadArtifactError("GitHub returned an invalid commit SHA for %r" % ref)
    return sha1


def require_successful_ci(sha1, http):
    data = http.get_json(github_url(
        "actions/workflows/%s/runs" % WORKFLOW,
        {"head_sha": sha1, "per_page": 10},
    ))
    runs = data.get("workflow_runs", [])
    if not runs:
        raise DownloadArtifactError(
            "Commit %s has no %s workflow run and has not been built." % (sha1, WORKFLOW))

    run = runs[0]
    status = run.get("status")
    conclusion = run.get("conclusion")
    run_url = run.get("html_url", "")
    if status != "completed":
        raise DownloadArtifactError(
            "The %s build for commit %s is %s.\n%s\nNo older build was selected."
            % (WORKFLOW, sha1, status or "not completed", run_url))
    if conclusion != "success":
        raise DownloadArtifactError(
            "The %s build for commit %s concluded with %s.\n%s\nNo older build was selected."
            % (WORKFLOW, sha1, conclusion or "an unknown result", run_url))


def resolve_build(args, http):
    if args.hash:
        if args.channel:
            raise DownloadArtifactError("--hash and --channel cannot be combined; use --archive-channel with --hash")
        if not re.fullmatch(r"[0-9a-fA-F]{7,40}", args.hash):
            raise DownloadArtifactError("--hash must be a 7 to 40 character hexadecimal commit SHA")
        supplied_hash = args.hash.lower()
        sha1 = resolve_commit(supplied_hash, http)
        require_successful_ci(sha1, http)
        archive_channel = args.archive_channel or archive_channel_for_branch(args.branch or "dev")
        return Build(sha1, archive_channel, "commit %s" % supplied_hash, supplied_hash)

    if args.archive_channel:
        raise DownloadArtifactError("--archive-channel is only valid with --hash")

    if args.channel:
        branch = CHANNEL_BRANCHES[args.channel]
        sha1 = resolve_commit(branch, http)
        require_successful_ci(sha1, http)
        data = http.get_json("%s/%s/info.json" % (DOWNLOAD_BASE_URL, args.channel))
        published_sha1 = data.get("sha1", "")
        if not re.fullmatch(r"[0-9a-f]{40}", published_sha1):
            raise DownloadArtifactError("Channel %r returned an invalid commit SHA" % args.channel)
        if published_sha1 != sha1:
            raise DownloadArtifactError(
                "Channel %s currently publishes %s, but its source branch %s points to %s.\n"
                "The current branch HEAD has not been published; no older build was selected."
                % (args.channel, published_sha1, branch, sha1))
        return Build(sha1, args.channel, "channel %s" % args.channel)

    branch = args.branch or "dev"
    sha1 = resolve_commit(branch, http)
    require_successful_ci(sha1, http)
    return Build(sha1, archive_channel_for_branch(branch), "branch %s" % branch)


def engine_filenames(name, platform):
    archive_name = name.replace("-", "_")
    if platform in ("win32", "x86_64-win32"):
        return [archive_name + ".exe"]
    if "android" in platform:
        return ["lib" + archive_name + ".so"]
    if platform in ("wasm-web", "wasm_pthread-web"):
        return [archive_name + ".js", archive_name + ".wasm"]
    return [archive_name]


def artifacts_for(name, platform, channel):
    if name == "editor":
        filename = EDITOR_FILENAMES.get(platform)
        if not filename:
            raise DownloadArtifactError(
                "Artifact 'editor' is unavailable for platform %r; choose one of: %s"
                % (platform, ", ".join(sorted(EDITOR_FILENAMES))))
        install_type = "dmg" if filename.endswith(".dmg") else "zip"
        return [Artifact(name, "%s/editor2/%s" % (channel, filename), filename, install_type)]
    if name == "bob":
        return [Artifact(name, "bob/bob.jar", "bob.jar")]
    if name == "sdk":
        return [Artifact(name, "engine/defoldsdk.zip", "defoldsdk.zip")]
    if name == "platform-sdk":
        return [Artifact(name, "engine/%s/defoldsdk.zip" % platform, "defoldsdk-%s.zip" % platform)]
    if name in ENGINE_ARTIFACTS:
        return [Artifact(name, "engine/%s/%s" % (platform, filename), filename)
                for filename in engine_filenames(name, platform)]
    raise DownloadArtifactError(
        "Unknown artifact %r; choose from: %s"
        % (name, ", ".join(("editor", "bob", "sdk", "platform-sdk") + ENGINE_ARTIFACTS)))


def add_filename_suffix(filename, label=None, hash_suffix=None):
    path = Path(filename)
    suffix = ""
    if label:
        suffix += "-" + label
    if hash_suffix:
        suffix += "-" + hash_suffix
    if not suffix:
        return filename
    return path.stem + suffix + path.suffix


def output_filename(artifact, label=None, hash_suffix=None):
    if artifact.install_type == "dmg":
        filename = Path(artifact.filename).stem + ".app"
    elif artifact.install_type == "zip":
        filename = Path(artifact.filename).stem
    else:
        filename = artifact.filename
    return add_filename_suffix(filename, label, hash_suffix)


def remove_path(path):
    path = Path(path)
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.exists():
        shutil.rmtree(path)


def replace_path(source, destination, force):
    source = Path(source)
    destination = Path(destination)
    if not destination.exists() and not destination.is_symlink():
        os.replace(source, destination)
        return
    if not force:
        raise DownloadArtifactError("Output path already exists: %s" % destination)

    backup = destination.parent / (".%s.backup-%s" % (destination.name, uuid.uuid4().hex))
    os.replace(destination, backup)
    try:
        os.replace(source, destination)
    except Exception:
        os.replace(backup, destination)
        raise
    remove_path(backup)


def extract_editor_zip(archive, destination, force):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".defold-editor-", dir=destination.parent) as temporary_directory:
        extract_root = Path(temporary_directory) / "extract"
        extract_root.mkdir()
        try:
            with zipfile.ZipFile(archive, "r") as zip_file:
                for info in zip_file.infolist():
                    normalized_name = info.filename.replace("\\", "/")
                    archive_path = PurePosixPath(normalized_name)
                    parts = archive_path.parts
                    if (not normalized_name or archive_path.is_absolute() or ".." in parts
                            or (parts and ":" in parts[0])):
                        raise DownloadArtifactError("Unsafe path in editor zip: %s" % info.filename)
                    zip_file.extract(info, extract_root)
                    extracted_path = extract_root.joinpath(*parts)
                    permissions = (info.external_attr >> 16) & 0o7777
                    if permissions and extracted_path.exists():
                        os.chmod(extracted_path, permissions)
        except (OSError, zipfile.BadZipFile) as error:
            raise DownloadArtifactError("Unable to extract editor zip %s: %s" % (archive, error)) from error

        editor_directory = extract_root / "Defold"
        if not editor_directory.is_dir():
            raise DownloadArtifactError("Editor zip does not contain the expected Defold directory")
        replace_path(editor_directory, destination, force)


def install_editor_dmg(archive, destination, force, runner=subprocess.run):
    if sys.platform != "darwin":
        raise DownloadArtifactError("Installing a macOS editor DMG requires running this script on macOS")

    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    attach = runner(
        ["hdiutil", "attach", "-nobrowse", "-readonly", "-plist", str(archive)],
        check=False, capture_output=True)
    if attach.returncode != 0:
        message = attach.stderr.decode("utf-8", errors="replace").strip()
        raise DownloadArtifactError("Unable to mount editor DMG: %s" % message)

    device = None
    mount_point = None
    try:
        try:
            data = plistlib.loads(attach.stdout)
            for entity in data.get("system-entities", []):
                if entity.get("mount-point"):
                    mount_point = entity["mount-point"]
                    device = entity.get("dev-entry")
                    break
        except (plistlib.InvalidFileException, ValueError) as error:
            raise DownloadArtifactError("Unable to parse hdiutil output: %s" % error) from error
        if not mount_point:
            raise DownloadArtifactError("Mounted editor DMG did not report a volume path")

        source_application = Path(mount_point) / "Defold.app"
        if not source_application.is_dir():
            raise DownloadArtifactError("Editor DMG does not contain Defold.app")
        with tempfile.TemporaryDirectory(prefix=".defold-editor-", dir=destination.parent) as temporary_directory:
            staged_application = Path(temporary_directory) / destination.name
            shutil.copytree(source_application, staged_application, symlinks=True)
            replace_path(staged_application, destination, force)
    finally:
        detach_target = device or mount_point
        if detach_target:
            detach = runner(["hdiutil", "detach", detach_target], check=False, capture_output=True)
            if detach.returncode != 0:
                message = detach.stderr.decode("utf-8", errors="replace").strip()
                print("warning: unable to detach %s: %s" % (detach_target, message), file=sys.stderr)


def install_editor_archive(artifact, archive, destination, force):
    try:
        if artifact.install_type == "dmg":
            install_editor_dmg(archive, destination, force)
        elif artifact.install_type == "zip":
            extract_editor_zip(archive, destination, force)
        else:
            raise DownloadArtifactError("Unsupported editor archive format: %s" % artifact.filename)
    except DownloadArtifactError:
        raise
    except OSError as error:
        raise DownloadArtifactError("Unable to install editor at %s: %s" % (destination, error)) from error


def validate_label(label):
    if label and not re.fullmatch(r"[A-Za-z0-9._-]+", label):
        raise DownloadArtifactError("--label may only contain letters, numbers, '.', '_' and '-'")


def branch_filename_label(branch):
    if not branch:
        return None
    label = re.sub(r"[^A-Za-z0-9._-]+", "-", branch).strip("-.")
    if not label:
        raise DownloadArtifactError("--branch does not contain any characters usable in an output filename")
    return label


def default_output_directory():
    dynamo_home = os.environ.get("DYNAMO_HOME")
    if dynamo_home:
        return Path(dynamo_home) / "tmp" / "artifacts"
    return Path(__file__).resolve().parent.parent / "tmp" / "dynamo_home" / "tmp" / "artifacts"


def create_parser():
    parser = argparse.ArgumentParser(
        description="Download artifacts from an exact Defold CI build. No older-build fallback is performed.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""examples:
  %(prog)s editor --platform=arm64-macos --label=metal-improvements --hash=124991951
  %(prog)s bob --channel=alpha
  %(prog)s dmengine --platform=x86_64-win32
  %(prog)s bob sdk --branch=dev --out=~/dev/tmp/my-builds

Selectors:
  --hash selects that exact commit (and takes precedence over --branch).
  --channel selects the current HEAD of that channel's source branch and requires
            that exact commit to be green and published.
  --branch selects the exact current branch HEAD. The default is branch 'dev'.

For a hash archived from a feature branch, pass --archive-channel=dev. A hash
without --archive-channel uses the channel implied by --branch, or alpha by default.

Editor DMGs are mounted and copied as renamed .app bundles. Linux and Windows
editor ZIPs are extracted into renamed directories. The archives are not retained.
When --branch is explicitly supplied without --label, its name is used as the
filename label; for example, feature/editor-work becomes feature-editor-work.
""",
    )
    parser.add_argument("artifacts", nargs="+", help="Artifacts to download")
    parser.add_argument("--platform", default=get_host_platform(), help="Target platform (default: host platform)")
    parser.add_argument("--out", type=Path, default=default_output_directory(), help="Output directory")
    parser.add_argument("--label", help="Suffix added to downloaded filenames")
    parser.add_argument("--branch", help="Exact branch HEAD to download (default: dev)")
    parser.add_argument("--channel", choices=PUBLISHED_CHANNELS,
                        help="Exact current, green and published channel build to download")
    parser.add_argument("--hash", help="Exact commit SHA or SHA prefix to download")
    parser.add_argument("--archive-channel", choices=ARCHIVE_CHANNELS,
                        help="CDN archive channel containing an explicit --hash")
    parser.add_argument("--force", action="store_true", help="Overwrite existing files")
    parser.add_argument("--dry-run", action="store_true", help="Resolve and verify artifacts without downloading")
    return parser


def run(args, http=None):
    validate_label(args.label)
    if args.channel and args.branch:
        raise DownloadArtifactError("--channel and --branch cannot be combined")

    http = http or HttpClient(os.environ.get("GITHUB_TOKEN"))
    build = resolve_build(args, http)
    output_directory = args.out.expanduser().resolve()
    filename_label = args.label or branch_filename_label(args.branch)
    planned = []
    for requested_name in args.artifacts:
        for artifact in artifacts_for(requested_name, args.platform, build.archive_channel):
            filename = output_filename(artifact, filename_label, build.hash_suffix)
            destination = output_directory / filename
            if artifact.install_type == "dmg" and sys.platform != "darwin" and not args.dry_run:
                raise DownloadArtifactError(
                    "Installing the macOS editor DMG requires running this script on macOS; use --dry-run to verify it")
            url = "%s/%s/%s/%s" % (
                ARCHIVE_BASE_URL, build.archive_channel, build.sha1, artifact.archive_path)
            planned.append((artifact, url, destination))

    destinations = [destination for _, _, destination in planned]
    if len(destinations) != len(set(destinations)):
        raise DownloadArtifactError("Two requested artifacts resolve to the same output filename")
    if not args.force:
        existing = [str(path) for path in destinations if path.exists() or path.is_symlink()]
        if existing:
            raise DownloadArtifactError(
                "Output file already exists (pass --force to overwrite):\n%s" % "\n".join(existing))

    print("Resolved %s to %s" % (build.description, build.sha1))
    print("Archive channel: %s" % build.archive_channel)
    for artifact, url, destination in planned:
        print("%s -> %s" % (url, destination))
        if args.dry_run:
            if not http.exists(url):
                raise DownloadArtifactError("Artifact does not exist:\n%s\nNo fallback was attempted." % url)
            print("Verified")
        else:
            if artifact.install_type:
                output_directory.mkdir(parents=True, exist_ok=True)
                with tempfile.TemporaryDirectory(prefix=".defold-download-", dir=output_directory) as directory:
                    archive = Path(directory) / artifact.filename
                    http.download(url, archive)
                    install_editor_archive(artifact, archive, destination, args.force)
                print("Installed %s" % destination)
            else:
                http.download(url, destination)
                print("Downloaded %s" % destination)
    return destinations


def main(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)
    try:
        run(args)
    except DownloadArtifactError as error:
        parser.exit(1, "error: %s\n" % error)


if __name__ == "__main__":
    main()
