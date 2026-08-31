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

import argparse
import glob
import json
import os


SOURCE_EXTENSIONS = [".cpp", ".c", ".cc", ".cxx", ".mm", ".m"]


def path_key(path):
    return os.path.normcase(os.path.normpath(path))


def append_unique(paths, path):
    if not path:
        return
    normalized = os.path.abspath(path)
    if path_key(normalized) not in {path_key(existing) for existing in paths}:
        paths.append(normalized)


def platform_target(platform):
    return platform.rsplit("-", 1)[-1] if "-" in platform else platform


def platform_file_tags(platform):
    target = platform_target(platform)
    tags = []

    def add(tag):
        if tag and tag not in tags:
            tags.append(tag)

    add(target)
    if target == "nx64":
        add("switch")
        add("nintendo")
    elif target == "ps5":
        add("ps4")
    elif target == "xbone":
        add("xbox")
        add("microsoft")
        add("win32")

    if target in ("macos", "ios"):
        add("darwin")
        add("apple")

    return tags


def private_platform_file_tags(platform):
    target = platform_target(platform)
    tags = []

    def add(tag):
        if tag and tag not in tags:
            tags.append(tag)

    add(target)
    if target == "nx64":
        add("switch")
        add("nintendo")
    elif target == "ps5":
        add("ps4")
    elif target == "xbone":
        add("xbox")
        add("microsoft")

    return tags


def fallback_tags(platform):
    target = platform_target(platform)
    tags = []
    if target in ("android", "ios", "linux", "macos", "web"):
        tags.append("posix")
    tags.append("default")
    return tags


def is_private_platform_file(platform, path):
    normalized = path.replace("\\", "/").lower()
    return any(tag.lower() in normalized for tag in private_platform_file_tags(platform))


def load_private_root(repo_root, platform):
    platforms_path = os.path.join(repo_root, ".defold-platforms")
    if not os.path.exists(platforms_path):
        return ""
    with open(platforms_path, "r", encoding="utf-8") as stream:
        platforms = json.load(stream)
    root = platforms.get(platform, {}).get("root", "")
    if root and os.path.isdir(root):
        return os.path.abspath(root)
    return ""


def find_file(repo_root, base_dir, private_root, platform, path, public_fallback=True, use_private_root=True):
    if use_private_root and private_root:
        try:
            base_path = os.path.relpath(base_dir, repo_root)
        except ValueError:
            base_path = ""
        private_relative = os.path.normpath(os.path.join(base_path, path))
        private_absolute = os.path.join(private_root, private_relative)
        if os.path.exists(private_absolute) and is_private_platform_file(platform, private_relative):
            return private_absolute

    public_path = os.path.join(base_dir, path)
    if public_fallback and os.path.exists(public_path):
        return public_path
    return ""


def select_feature_sources(repo_root, base_dir, private_root, platform, feature_name, extra_tags, preferred_tags):
    selected = []
    feature_files = []

    feature_base, extension = os.path.splitext(feature_name)
    extensions = [extension] if extension else SOURCE_EXTENSIONS

    for ext in extensions:
        for pattern in (feature_base + ext, feature_base + "_*" + ext):
            for path in sorted(glob.glob(os.path.join(base_dir, pattern))):
                append_unique(feature_files, path)

    for ext in extensions:
        path = find_file(repo_root, base_dir, private_root, platform, feature_base + ext, True, False)
        append_unique(selected, path)
        append_unique(feature_files, path)

    tag_files = []
    for tag in preferred_tags:
        for ext in extensions:
            path = find_file(repo_root, base_dir, private_root, platform, f"{feature_base}_{tag}{ext}")
            append_unique(tag_files, path)
            append_unique(feature_files, path)

    if not tag_files:
        for tag in platform_file_tags(platform) + extra_tags:
            for ext in extensions:
                path = find_file(repo_root, base_dir, private_root, platform, f"{feature_base}_{tag}{ext}")
                append_unique(tag_files, path)
                append_unique(feature_files, path)

    if not tag_files:
        for tag in fallback_tags(platform):
            for ext in extensions:
                path = find_file(repo_root, base_dir, private_root, platform, f"{feature_base}_{tag}{ext}", True, False)
                append_unique(tag_files, path)
                append_unique(feature_files, path)

    for path in tag_files:
        append_unique(selected, path)

    return selected, feature_files


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--base-dir", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--feature", required=True)
    parser.add_argument("--private-root", default="")
    parser.add_argument("--extra-tag", action="append", default=[])
    parser.add_argument("--preferred-tag", action="append", default=[])
    args = parser.parse_args()

    repo_root = os.path.abspath(args.repo_root)
    base_dir = os.path.abspath(args.base_dir)
    private_root = os.path.abspath(args.private_root) if args.private_root else load_private_root(repo_root, args.platform)

    selected, feature_files = select_feature_sources(
        repo_root,
        base_dir,
        private_root,
        args.platform,
        args.feature,
        args.extra_tag,
        args.preferred_tag,
    )
    print(json.dumps({"selected": selected, "all": feature_files}, sort_keys=True))


if __name__ == "__main__":
    main()
