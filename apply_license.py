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

extensions = [ ".h", ".c", ".cpp", ".m", ".inl", ".sh", ".py", ".java", ".clj" ]

ext_to_comment = {
    ".h": "// ",
    ".c": "// ",
    ".cpp": "// ",
    ".m": "// ",
    ".inl": "// ",
    ".java": "// ",
    ".sh": "# ",
    ".py": "# ",
    ".clj": ";; ",
}

for subdir, dirs, files in os.walk("."):
    # Ignore tmp and build folders
    if subdir.startswith("./tmp") or subdir.startswith("./engine/glfw/examples") or "build" in subdir:
        continue
    for file in files:
        #print os.path.join(subdir, file)
        filepath = subdir + os.sep + file
        filename, ext = os.path.splitext(filepath)
        if ext not in extensions:
            continue

        with open(filepath, "rb+") as f:
            contents = f.read()
            # Already applied the Defold License?
            if "Defold Foundation" in contents:
                print("Already applied Defold license to " + filepath)
                continue;
            # Some other license in the file
            if "Copyright" in contents or "License" in contents:
                print("Other license in " + filepath)
                continue;

            # Make the license text into a language specific comment, based on extension
            comment = ext_to_comment.get(ext)
            lines = LICENSE.split("\n")
            lines = [comment + line for line in lines]
            license = "\n".join(lines)

            # Preserve shebang
            if contents.startswith("#!"):
                firstline = contents.partition('\n')[0]
                contents = contents.replace(firstline, "")
                license = firstline + "\n" + license

            print("Applying Defold License to " + filepath)
            f.seek(0)
            f.write(license + "\n")
            f.write(contents)
            f.truncate()
