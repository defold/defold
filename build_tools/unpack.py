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
import run
import platform
import tempfile
import shutil

def zip(filepath, dest_dir):
    run.shell_command('unzip %s -d %s' % (filepath, dest_dir))

def dmg(filepath, dest_dir):
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
        log("Unpacking .dmg is not supported on %d", system)
