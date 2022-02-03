#!/usr/bin/env python
import os
import time
import re
import subprocess

RE_LICENSE = r"(.*)^.*Copyright (\d\d\d\d) The Defold Foundation.*specific language governing permissions and limitations under the License.(.*)"

LICENSE = ('''Copyright %d The Defold Foundation
Licensed under the Defold License version 1.0 (the "License"); you may not use
this file except in compliance with the License.

You may obtain a copy of the License, together with FAQs at
https://www.defold.com/license

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.''') % time.localtime().tm_year

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

# include_patterns = [
#     "rig.cpp",
#     "dstrings.cpp",
#     "package_win32_sdk.sh",
#     "Protocol.java",
#     "script_bitop.cpp",
# ]

def match_patterns(s, patterns):
    for pattern in patterns:
        if pattern in s:
            return True
    return False


# def force_include(fullpath):
#     return match_patterns(fullpath, include_patterns)

def skip_path(fullpath):
    return match_patterns(fullpath, excluded_paths)


def skip_filename(fullpath, basename):
    if basename in excluded_files:
        return True

    if match_patterns(fullpath, excluded_paths):
        return True

    return False

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

def git_ignored(path):
    return subprocess.call(['git', 'check-ignore', '-q', path]) == 0



dryrun = True

for root, dirs, files in os.walk(".", topdown=True):
    included_dirs = []
    for d in dirs:
        path = os.path.join(root, d)
        if not skip_path(path) and not git_ignored(path):
            included_dirs.append(d)
    dirs[:] = included_dirs

# for subdir, dirs, files in os.walk("."):
#     if skip_path(subdir) or git_ignored(subdir):
#         print("Ignored: " + subdir)
#         continue

    for file in files:
        filepath = os.path.join(root, file)

        if git_ignored(filepath):
            # print("Ignored: " + filepath)
            continue

        license = get_license_for_file(filepath)
        if not license:
            continue

        # should_include = force_include(filepath)
        # if not should_include and skip_filename(filepath, file):
        if skip_filename(filepath, file):
            # print("Excluded " + filepath)
            continue

        with open(filepath, "rb+") as f:
            contents = f.read()

            # Some other license in the file
            if has_other_license(contents):
                continue;

            # Make the license text into a language specific comment, based on extension

            # Already applied the Defold License?
            if has_defold_license(contents):
                year = time.localtime().tm_year
                modified_year = time.localtime(os.path.getmtime(filepath)).tm_year
                license_year = re.search(RE_LICENSE, contents, flags=re.S).group(2)
                if modified_year <= license_year:
                    print("Up-to-date: " + filepath)
                    continue
                contents = re.sub(RE_LICENSE, r"\1" + license + r"\3", contents, flags=re.S)
                print("Updated:    " + filepath)
            else:
                contents = apply_license(license, contents)
                print("Applied:    " + filepath)

            # if not dryrun:
            #     f.seek(0)
            #     f.write(contents)
            #     f.truncate()
