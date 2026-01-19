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

from log import log
import os
import re
import run
import tempfile
import unpack

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


def release(config, urls):
    log("Releasing Defold to Steam")

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
            unpack.zip(filepath, linux_dir)
        elif "x86_64-win32" in filepath:
            unpack.zip(filepath, windows_dir)
        elif "x86_64-macos" in filepath:
            unpack.dmg(filepath, macos_dir)
    
    # upload to steam
    manifest = write_manifest(upload_dir)
    run.shell_command("steamcmd +login defoldengine +run_app_build %s +quit" % (manifest))
    log("Released Defold to Steam")


if __name__ == '__main__':
    print("For testing only")
    print(get_current_repo())
