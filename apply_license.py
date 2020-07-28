import os

LICENSE = '''Copyright 2020 The Defold Foundation
Licensed under the Defold License version 1.0 (the "License"); you may not use
this file except in compliance with the License.

You may obtain a copy of the License, together with FAQs at
https://www.defold.com/license

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.'''

extensions = [ ".h", ".c", ".cpp", ".inl", ".m", ".mm", ".sh", ".py", ".java", ".clj", ".go", ".lua" ]

ext_to_comment = {
    ".h": "// ",
    ".c": "// ",
    ".cpp": "// ",
    ".inl": "// ",
    ".m": "// ",
    ".mm": "// ",
    ".java": "// ",
    ".sh": "# ",
    ".py": "# ",
    ".clj": ";; ",
    ".go": "// ",
    ".lua": "-- ",
}

exluded_files = [
    "edn.lua",
    "mobdebug.lua",
    "test_props.lua",
    "test_props_url.lua",
    "test_props_number.lua",
    "test_props_hash.lua",
    "test_props_vec3.lua",
    "test_props_vec4.lua",
    "test_props_quat.lua",
    "test_props_bool.lua",
    "test_props_material.lua"
]

exluded_patterns = [
    "build/"
    "dynamo_home/"
    "engine/glfw/tests",
    "engine/glfw/examples",
    "engine/sound/src/openal",
    "dlib/src/zlib",
    "dlib/src/webp",
    "lua/src/lua",
    "lua/scripts",
    "script/src/luasocket",
    "script/src/bitop",
    "com.dynamo.cr.bob/generated",
    "luajit",
    "jagatoo",
    "Box2D",
    "jsmn",
    "msbranco"
]

include_patterns = [
    "rig.cpp",
    "dstrings.cpp",
    "package_win32_sdk.sh",
    "Protocol.java",
    "script_bitop.cpp",
]

def match_patterns(s, patterns):
    for pattern in patterns:
        if pattern in s:
            return True
    return False


def force_include(fullpath):
    if match_patterns(fullpath, include_patterns):
        return True
    return False

def skip_filename(fullpath, basename):
    if basename in exluded_files:
        return True

    if match_patterns(fullpath, exluded_patterns):
        return True

    return False

def has_other_license(s):
    return "Copyright" in s or "License" in s

def get_comment_style_for_file(fullpath):
    ext = os.path.splitext(fullpath)[1]
    if ext == ".go" and not fullpath.startswith("./go/"):
        return None
    return ext_to_comment.get(ext)



for subdir, dirs, files in os.walk("."):
    # Ignore tmp and build folders
    if subdir.startswith("./tmp") or "build" in subdir:
        continue
    for file in files:
        filepath = subdir + os.sep + file
        ext = os.path.splitext(filepath)[1]
        if ext not in extensions:
            continue

        comment = get_comment_style_for_file(filepath)
        if comment is None:
            continue

        should_include = force_include(filepath)
        if not should_include and skip_filename(filepath, file):
            print("Excluded " + filepath)
            continue

        with open(filepath, "rb+") as f:
            contents = f.read()
            # Already applied the Defold License?
            if "Defold Foundation" in contents:
                print("Already applied Defold license to " + filepath)
                continue;

            # Some other license in the file
            if not should_include and has_other_license(contents):
                print("Other license in " + filepath)
                continue;

            # Make the license text into a language specific comment, based on extension
            lines = LICENSE.split("\n")
            lines = [comment + line for line in lines]
            license = "\n".join(lines)

            # Preserve shebang
            if contents.startswith("#!"):
                firstline = contents.partition('\n')[0]
                contents = contents.replace(firstline, "")
                license = firstline + "\n" + license

            print("Applying Defold License to " + filepath + ("  (force included)" if should_include else ""))
            f.seek(0)
            f.write(license + "\n\n")
            f.write(contents)
            f.truncate()
