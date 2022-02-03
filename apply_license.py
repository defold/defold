#!/usr/bin/env python
import os
import time
import re
import subprocess

RE_LICENSE = r"(.*?\n?\r?)[/#;-]?[/#;-]\sCopyright (\d\d\d\d) The Defold Foundation.*specific language governing permissions and limitations under the License.(.*)"

YEAR = str(time.localtime().tm_year)
LICENSE = ('''Copyright %s The Defold Foundation
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
    ".h":    license("// "),
    ".c":    license("// "),
    ".cpp":  license("// "),
    ".inl":  license("// "),
    ".m":    license("// "),
    ".mm":   license("// "),
    ".java": license("// "),
    ".sh":   license("# "),
    ".py":   license("# "),
    ".clj":  license(";; "),
    ".lua":  license("-- "),
}

excluded_files = [
    "edn.lua",
    "mobdebug.lua",
    "start.lua",
    "test_props.lua",
    "test_props_url.lua",
    "test_props_number.lua",
    "test_props_hash.lua",
    "test_props_vec3.lua",
    "test_props_vec4.lua",
    "test_props_quat.lua",
    "test_props_bool.lua",
    "test_props_material.lua",
    "apply_license.py"
]

excluded_paths = [
    "./.git",
    "./.github",
    "./external",
    "./engine/glfw/tests",
    "./engine/glfw/examples",
    "./engine/sound/src/openal",
    "./engine/dlib/src/zlib",
    "./engine/lua",
    "./engine/script/src/luasocket",
    "./engine/script/src/bitop",
    "./com.dynamo.cr/com.dynamo.cr.bob/src/org/jagatoo",
    "./engine/physics/src/box2d",
    "./engine/dlib/src/dlib/jsmn",
]

def match_patterns(s, patterns):
    for pattern in patterns:
        if pattern in s:
            return True
    return False

def skip_path(fullpath):
    return match_patterns(fullpath, excluded_paths)

def skip_filename(fullpath):
    return match_patterns(fullpath, excluded_files)

def has_defold_license(s):
    return "Licensed under the Defold License version" in s

def has_other_license(s):
    return ("Copyright" in s or "License" in s) and not has_defold_license(s)

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



dryrun = True

for root, dirs, files in os.walk(".", topdown=True):
    # exclude dirs to avoid traversing them at all
    # with topdown set to True we can make in place modifications of dirs to
    # have os.walk() skip directories
    dirs[:] = [ d for d in dirs if not skip_path(root + os.sep + d) and not check_ignored(root + os.sep + d) ]

    for file in files:
        filepath = root + os.sep + file

        # skip ignored files
        if skip_filename(filepath) or check_ignored(filepath):
            continue

        license = get_license_for_file(filepath)
        if not license:
            continue

        with open(filepath, "rb+") as f:
            contents = f.read()

            # Some other license in the file
            if has_other_license(contents):
                continue;

            # Already applied the Defold License?
            # Remove it and reapply if file has been modified after license year
            if has_defold_license(contents):
                modified_year = time.localtime(os.path.getmtime(filepath)).tm_year
                license_year = int(re.search(RE_LICENSE, contents, flags=re.DOTALL).group(2))
                if modified_year <= license_year:
                    continue
                license = license.replace(YEAR, str(modified_year))
                contents = re.sub(RE_LICENSE, r"\1" + license + r"\3", contents, flags=re.DOTALL)
                print("Updated: " + filepath)
            else:
                contents = apply_license(license, contents)
                print("Applied: " + filepath)

            if not dryrun:
                f.seek(0)
                f.write(contents)
                f.truncate()
