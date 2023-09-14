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

from log import log
import os
import re
import run
import urllib
import shutil
import tempfile
import platform
from urllib.parse import urlparse

MANIFEST_VDF = """
"AppBuild"
{
    "AppID" "1365760"
    "Desc" "Stable editor release"
    "BuildOutput" "%s/output"
    "ContentRoot" "%s"

    "Depots"
    {
        "1365761"
        {
            "FileMapping"
            {
                "LocalPath" "./x86_64-macos/*"
                "DepotPath" "."
                "recursive" "1"
            }
        }
        "1365762"
        {
            "FileMapping"
            {
                "LocalPath" "./x86_64-win32/Defold/*"
                "DepotPath" "."
                "recursive" "1"
            }
        }
        "1365763"
        {
            "FileMapping"
            {
                "LocalPath" "./x86_64-linux/Defold/*"
                "DepotPath" "."
                "recursive" "1"
            }
        }
    }
}
"""


def write_manifest(dir):
    filename = os.path.join(dir, "manifest.vdf")
    with open(filename, "w") as f:
        f.write(MANIFEST_VDF % (dir, dir))
    return filename

def unzip(filepath, dest_dir):
    run.shell_command('unzip %s -d %s' % (filepath, dest_dir))

def unpack_dmg(filepath, dest_dir):
    system = platform.system()
    if system == "Darwin":
        run.shell_command("hdiutil attach %s" % filepath)
        shutil.copytree("/Volumes/Defold/Defold.app", os.path.join(dest_dir, "Defold.app"))
        run.shell_command("hdiutil detach /Volumes/Defold")
    elif system == "Linux":
        mountpoint = tempfile.mkdtemp()
        run.shell_command("mount -t hfsplus %s %s" % (filepath, mountpoint))
        shutil.copytree(os.path.join(mountpoint, "Defold.app"), os.path.join(dest_dir, "Defold.app"))
        run.shell_command("umount %s" % (mountpoint))
    else:
        log("Releasing to Steam is not supported on %d", system)

def release(config, tag_name, s3_release):
    log("Releasing Defold %s to Steam" % tag_name)

    if not s3_release.get("files"):
        log("No files found on S3")
        exit(1)

    def is_editor_file(path):
        return os.path.basename(path) in ('Defold-x86_64-macos.dmg',
                                          'Defold-x86_64-linux.zip',
                                          'Defold-x86_64-win32.zip')

    # get a set of editor files only
    # for some reasons files are listed more than once in s3_releases
    urls = set()
    base_url = "https://" + urlparse(config.archive_path).hostname
    for file in s3_release.get("files", None):
        path = file.get("path")
        if not is_editor_file(path):
            continue
        download_url = base_url + path
        urls.add(download_url)

    # create dir from where we upload to steam
    upload_dir = tempfile.mkdtemp()
    linux_dir = os.path.join(upload_dir, "x86_64-linux")
    windows_dir = os.path.join(upload_dir, "x86_64-win32")
    macos_dir = os.path.join(upload_dir, "x86_64-macos")
    os.makedirs(linux_dir)
    os.makedirs(windows_dir)
    os.makedirs(macos_dir)

    # download and unpack editor files
    for download_url in urls:
        filepath = config._download(download_url)
        if "x86_64-linux" in filepath:
            unzip(filepath, linux_dir)
        elif "x86_64-win32" in filepath:
            unzip(filepath, windows_dir)
        elif "x86_64-macos" in filepath:
            unpack_dmg(filepath, macos_dir)
    
    # upload to steam
    manifest = write_manifest(upload_dir)
    run.shell_command("steamcmd +login defoldengine +run_app_build %s +quit" % (manifest))
    log("Released Defold %s to Steam" % tag_name)


if __name__ == '__main__':
    print("For testing only")
    print(get_current_repo())
