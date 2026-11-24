#!/usr/bin/env python3
import os
import time
import re
import subprocess

from license_data import excluded_files, excluded_paths, ext_to_license as ext_to_comment

RE_LICENSE = r"(.*?\n?\r?)[/#;-]?[/#;-]\sCopyright .* The Defold Foundation.*specific language governing permissions and limitations under the License.(.*)"

YEAR = str(time.localtime().tm_year)
DEFOLD_COPYRIGHT = ('Copyright 2020-%s The Defold Foundation' % YEAR).encode('ascii')
RE_DEFOLD_COPYRIGHT = re.compile(rb'Copyright 2020-\d{4} The Defold Foundation')
LICENSE_HEADER_BYTES = 4096
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
    license = ""
    for line in LICENSE.split("\n"):
        if len(line.strip()) == 0:
            license = license + comment.strip() + "\n"
        else:
            license = license + comment + line + "\n"
    return license.strip()
    # return "\n".join([comment + line for line in LICENSE.split("\n")])

# map extensions to strings with the commented license
ext_to_license = {ext: license(comment) for ext, comment in ext_to_comment.items()}

dryrun = False


def match_patterns(s, patterns):
    for pattern in patterns:
        if pattern in s:
            return True
    return False

def skip_path(path):
    if os.path.isabs(path):
        try:
            path = os.path.relpath(path)
        except ValueError:
            return False
    path = os.path.normpath(path)
    for pattern in excluded_paths:
        excluded = os.path.normpath(pattern)
        if path == excluded or path.startswith(excluded + os.sep):
            return True
    return False

def skip_filename(filepath):
    return match_patterns(filepath, excluded_files)

def has_defold_license(s):
    return re.search(RE_LICENSE, s[0:2000], flags=re.DOTALL) is not None

def find_defold_license_header(contents, comment):
    marker = comment.strip().encode('ascii')
    offset = 0
    start = None
    newline = b'\n'
    for line in contents[:LICENSE_HEADER_BYTES].splitlines(keepends=True):
        text = line.lstrip(b' \t')
        if start is None:
            if text.startswith(marker) and b'Copyright' in text and b'The Defold Foundation' in text:
                start = offset
                if line.endswith(b'\r\n'):
                    newline = b'\r\n'
        elif text.strip() and not text.startswith(marker):
            return None

        if start is not None:
            # Do not replace a mixed header if it contains an unrelated copyright notice.
            if b'Copyright' in text and not any(notice in text for notice in (
                    b'The Defold Foundation', b'Copyright 2014-2020 King',
                    b'Copyright 2009-2014 Ragnar Svensson',
                    b'Copyright 2009-2014 Christian Murray')):
                return None
            if b'specific language governing permissions and limitations under the License.' in text:
                return start, offset + len(line), marker, newline
        offset += len(line)
    return None

def has_full_defold_license(header, marker):
    expected = [line.encode('utf-8') for line in LICENSE.splitlines() if line]
    matched = 0
    for line in header.splitlines():
        text = line.strip()
        if text.startswith(marker) and text[len(marker):].strip() == expected[matched]:
            matched += 1
            if matched == len(expected):
                return True
    return False

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



def update_defold_copyright_year(contents):
    return RE_DEFOLD_COPYRIGHT.sub(DEFOLD_COPYRIGHT, contents)


def refresh_defold_copyright_year(filepath):
    # Copyright headers are at the start of files. Keep the rest, including line endings, untouched.
    with open(filepath, 'rb') as f:
        header = f.read(LICENSE_HEADER_BYTES)
    updated = update_defold_copyright_year(header)
    if updated == header:
        return

    print('Updated year: ' + filepath)
    if not dryrun:
        with open(filepath, 'r+b') as f:
            f.write(updated)


def refresh_tracked_copyright_years():
    # Refresh existing Defold headers even where adding a new header is intentionally excluded.
    result = subprocess.run(['git', 'ls-files', '-z'], stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, check=False)
    if result.returncode != 0:
        return
    for name in result.stdout.split(b'\0'):
        if not name:
            continue
        filepath = os.fsdecode(name)
        if os.path.isfile(filepath) and not os.path.islink(filepath):
            refresh_defold_copyright_year(filepath)


def process_file(filepath):
    # skip ignored files
    if skip_path(os.path.dirname(filepath)) or skip_filename(filepath) or check_ignored(filepath):
        return

    license = get_license_for_file(filepath)
    if not license:
        return

    with open(filepath, 'rb') as f:
        original = f.read()
    contents = original.decode('utf-8')

    # Some other license in the file
    if has_other_license(contents):
        return

    if has_defold_license(contents):
        updated = update_defold_copyright_year(original)
        span = find_defold_license_header(updated, ext_to_comment[os.path.splitext(filepath)[1]])
        if span:
            start, end, marker, newline = span
            if not has_full_defold_license(updated[start:end], marker):
                replacement = license.encode('utf-8').replace(b'\n', newline) + newline
                updated = updated[:start] + replacement + updated[end:]
                print('Reapplied: ' + filepath)
            elif updated != original:
                print('Updated year: ' + filepath)
        elif updated != original:
            print('Updated year: ' + filepath)
        if updated == original:
            return
    else:
        updated = apply_license(license, contents).encode('utf-8')
        print('Applied: ' + filepath)

    if not dryrun:
        with open(filepath, 'wb') as f:
            f.write(updated)


if __name__ == "__main__":
    refresh_tracked_copyright_years()
    for root, dirs, files in os.walk(".", topdown=True):
        # exclude dirs to avoid traversing them at all
        # with topdown set to True we can make in place modifications of dirs to
        # have os.walk() skip directories
        dirs[:] = [ d for d in dirs if not skip_path(os.path.join(root, d)) and not check_ignored(os.path.join(root, d)) ]

        for file in files:
            process_file(os.path.join(root, file))

    print("NOTE! Manually update ddfc.py, editor/bundle-resources/Info.plist, editor/resources/splash.fxml and editor/resources/about.fxml!")
