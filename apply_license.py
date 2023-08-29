#!/usr/bin/env python3
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


import os
import time
import re
import subprocess

RE_LICENSE = r"(.*?\n?\r?)[/#;-]?[/#;-]\sCopyright .* The Defold Foundation.*specific language governing permissions and limitations under the License.(.*)"

YEAR = str(time.localtime().tm_year)
LICENSE = ('''Copyright 2020-%s The Defold Foundation
Copyright 2014-2020 King
Copyright 2009-2014 Ragnar Svensson, Christian Murray
Licensed under the Defold License version 1.0 (the "License"); you may not use
this file except in compliance with the License.

You may obtain a copy of the License, together with FAQs at
https://www.defold.com/license

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.''') % YEAR

def license(comment):
    return "\n".join([comment + line for line in LICENSE.split("\n")])

# map extensions to strings with the commented license
ext_to_license = {
    ".h":             license("// "),
    ".c":             license("// "),
    ".cpp":           license("// "),
    ".inl":           license("// "),
    ".m":             license("// "),
    ".mm":            license("// "),
    ".java":          license("// "),
    ".sh":            license("# "),
    ".py":            license("# "),
    ".clj":           license(";; "),
    ".lua":           license("-- "),
    ".script":        license("-- "),
    ".gui_script":    license("-- "),
    ".render_script": license("-- "),
}

excluded_files = [
    "apply_license.py",
    "engine/ddf/src/ddfc.py",
    "engine/engine/contents/builtins/edn.lua",
    "engine/engine/contents/builtins/mobdebug.lua",
    "editor/bundle-resources/_defold/debugger/start.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_url.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_number.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_hash.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_vec3.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_vec4.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_quat.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_bool.lua",
    "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/test_props_material.lua",
    "editor/resources/templates/template.script",
    "editor/resources/templates/template.gui_script",
    "editor/resources/templates/template.render_script",
    "editor/resources/templates/template.lua",
    "engine/resource/src/test/archive_data/file5.script",
    "engine/resource/src/test/archive_data/liveupdate.file6.script",
    "engine/liveupdate/src/ringbuffer.h",
]

excluded_paths = [
    "./.git",
    "./.github",
    "./external",
    "./engine/dlib/src/basis/encoder",
    "./engine/dlib/src/basis/transcoder",
    "./engine/dlib/src/dlib/jsmn",
    "./engine/dlib/src/lz4",
    "./engine/dlib/src/mbedtls",
    "./engine/dlib/src/stb",
    "./engine/dlib/src/zlib",
    "./engine/dlib/src/zip",
    "./engine/glfw/tests",
    "./engine/glfw/examples",
    "./engine/lua",
    "./engine/physics/src/box2d",
    "./engine/sound/src/openal",
    "./engine/script/src/bitop",
    "./engine/script/src/luasocket",
    "./com.dynamo.cr/com.dynamo.cr.bob/src/org/jagatoo",
]

dryrun = False


def match_patterns(s, patterns):
    for pattern in patterns:
        if pattern in s:
            return True
    return False

def skip_path(path):
    return match_patterns(path, excluded_paths)

def skip_filename(filepath):
    return match_patterns(filepath, excluded_files)

def has_defold_license(s):
    return re.search(RE_LICENSE, s[0:2000], flags=re.DOTALL) is not None

def has_other_license(s):
    return ("Copyright" in s or "License" in s) and not ("The Defold Foundation" in s)

def get_license_for_file(filepath):
    ext = os.path.splitext(filepath)[1]
    if not ext in ext_to_license:
        return None
    return ext_to_license[ext]

def apply_license(license, contents):
    # Preserve shebang
    if contents.startswith("#!"):
        firstline = contents.partition('\n')[0]
        contents = contents.replace(firstline, "")
        license = firstline + "\n" + license
    return license  + "\n\n" + contents

def check_ignored(path):
    return subprocess.call(['git', 'check-ignore', '-q', path]) == 0



def process_file(filepath):
    # skip ignored files
    if skip_filename(filepath) or check_ignored(filepath):
        return

    license = get_license_for_file(filepath)
    if not license:
        return

    with open(filepath, "rb+") as f:
        contents = f.read().decode('utf-8')

        # Some other license in the file
        if has_other_license(contents):
            return;

        # Already applied the Defold License?
        # Remove it and reapply if file has been modified after license year
        if has_defold_license(contents):
            contents = re.sub(RE_LICENSE, r"\1" + license + r"\2", contents, flags=re.DOTALL)
            print("Updated: " + filepath)
        else:
            contents = apply_license(license, contents)
            print("Applied: " + filepath)

        if not dryrun:
            f.seek(0)
            f.write(contents.encode('utf-8'))
            f.truncate()


for root, dirs, files in os.walk(".", topdown=True):
    # exclude dirs to avoid traversing them at all
    # with topdown set to True we can make in place modifications of dirs to
    # have os.walk() skip directories
    dirs[:] = [ d for d in dirs if not skip_path(os.path.join(root, d)) and not check_ignored(os.path.join(root, d)) ]

    for file in files:
        process_file(os.path.join(root, file))

print("NOTE! Manually update ddfc.py, editor/bundle-resources/Info.plist, editor/resources/splash.fxml and editor/resources/about.fxml!")
