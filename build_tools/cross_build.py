#!/usr/bin/env python
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

import json
import os


DEFOLD_PLATFORMS_FILE = '.defold-platforms'


def get_repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def get_platforms_config_path():
    return os.path.join(get_repo_root(), DEFOLD_PLATFORMS_FILE)


def load_platforms_config():
    config_path = get_platforms_config_path()
    if not os.path.exists(config_path):
        return {}
    with open(config_path, 'r') as f:
        return json.load(f)


def save_platforms_config(platforms):
    config_path = get_platforms_config_path()
    with open(config_path, 'w') as f:
        json.dump(platforms, f, indent=4, sort_keys=True)
        f.write('\n')


def get_configured_platforms():
    return sorted(load_platforms_config().keys())


def get_platform_roots(platform):
    """Return private source roots for a target platform.

    The optional repo-local .defold-platforms file is JSON keyed by full target
    platform. Each platform entry may contain "root" for a single private repo
    and/or "roots" for an ordered list of private repos.
    """
    platforms = load_platforms_config()
    platform_config = platforms.get(platform, {})
    roots = []
    if 'root' in platform_config:
        roots.append(platform_config['root'])
    roots.extend(platform_config.get('roots', []))
    return roots


def get_platform_file_tags(platform):
    target = platform
    if '-' in platform:
        target = platform.split('-')[-1]

    tags = []
    def append_tag(tag):
        if tag and tag not in tags:
            tags.append(tag)

    append_tag(target)

    if target == 'nx64':
        append_tag('switch')
        append_tag('nintendo')
    elif target in ('ps4', 'ps5'):
        append_tag('playstation')
        append_tag('sony')
    elif target == 'xbone':
        append_tag('xbox')
        append_tag('microsoft')
        append_tag('win32')

    if target in ('macos', 'ios'):
        append_tag('darwin')
        append_tag('apple')

    return tags


def get_platform_file_fallback_tags(platform):
    target = platform
    if '-' in platform:
        target = platform.split('-')[-1]

    tags = []
    def append_tag(tag):
        if tag and tag not in tags:
            tags.append(tag)

    if target in ('android', 'ios', 'linux', 'macos', 'web'):
        append_tag('posix')

    return tags


def find_platform_file(bld, platform, path, public_fallback = True, private_roots = True):
    repo_root = get_repo_root()
    base_path = os.path.relpath(bld.path.abspath(), repo_root)
    if private_roots:
        for root in get_platform_roots(platform):
            absolute_path = os.path.join(root, base_path, path)
            if os.path.exists(absolute_path):
                node = bld.root.find_node(absolute_path)
                if node:
                    return node
                return absolute_path

    if public_fallback:
        return bld.path.find_node(path)
    return None


def source_file_path(source):
    if hasattr(source, 'abspath'):
        return source.abspath()
    return source


def remove_source_files(sources, remove_sources):
    remove_paths = set(source_file_path(x) for x in remove_sources)
    return [x for x in sources if source_file_path(x) not in remove_paths]


def get_feature_extra_tags(platform, extra_tags):
    if not extra_tags:
        return []

    target = platform
    if '-' in platform:
        target = platform.split('-')[-1]

    if isinstance(extra_tags, dict):
        if platform in extra_tags:
            extra_tags = extra_tags[platform]
        elif target in extra_tags:
            extra_tags = extra_tags[target]
        else:
            extra_tags = extra_tags.get('*', [])
    if isinstance(extra_tags, str):
        extra_tags = [extra_tags]

    tags = []
    def append_tag(tag):
        if tag and tag not in tags:
            tags.append(tag)

    for tag in extra_tags:
        append_tag(tag)
    return tags


def find_feature_files(bld, feature_name, platform, extra_tags = None, preferred_tags = None):
    """Return (selected_files, feature_files) for feature_name.

    Rules:
    * feature_files contains <feature>.<ext> and <feature>_*.ext when found.
    * selected_files contains all <feature>.<ext> core files when found.
    * selected_files contains all matching <feature>_<tag>.ext files for the platform.
    * preferred_tags may be a list for all platforms, or a dict where platform/target override '*'.
    * preferred tag matches replace platform tag matches when found.
    * extra_tags may be a list for all platforms, or a dict where platform/target override '*'.
    * extra tag matches are appended to platform tag matches before fallback tags.
    * fallback tags and default are used only when no platform file matched.
    * platform roots are searched before the public repo for platform tag matches.
    * missing feature files or missing selected files fail the build.
    """
    files = []
    feature_files = []

    def find_file(path, public_fallback = True, private_roots = True):
        return find_platform_file(bld, platform, path, public_fallback, private_roots)

    def append_file(files, node):
        if node and source_file_path(node) not in [source_file_path(x) for x in files]:
            files.append(node)

    feature_base, extension = os.path.splitext(feature_name)
    extensions = ['.cpp', '.c', '.cc', '.cxx', '.mm', '.m']
    if extension:
        extensions = [extension]

    feature_patterns = []
    for extension in extensions:
        feature_patterns += [feature_base + extension,
                             feature_base + '_*' + extension]
    for node in bld.path.ant_glob(feature_patterns):
        append_file(feature_files, node)

    # Core implementation: <feature>.<ext> is shared by all platforms.
    for extension in extensions:
        node = find_file(feature_base + extension, True, False)
        if node:
            append_file(files, node)
            append_file(feature_files, node)

    # Preferred tags: explicit feature choices such as mbedtls override platform tags.
    tag_files = []
    for tag in get_feature_extra_tags(platform, preferred_tags):
        for extension in extensions:
            node = find_file('%s_%s%s' % (feature_base, tag, extension))
            if node:
                append_file(tag_files, node)
                append_file(feature_files, node)

    # Platform tags: target-specific files, optionally extended with feature tags.
    if not tag_files:
        for tag in get_platform_file_tags(platform) + get_feature_extra_tags(platform, extra_tags):
            for extension in extensions:
                node = find_file('%s_%s%s' % (feature_base, tag, extension))
                if node:
                    append_file(tag_files, node)
                    append_file(feature_files, node)

    # Fallback tags: public shared implementations used when no platform file matched.
    if not tag_files:
        for tag in get_platform_file_fallback_tags(platform) + ['default']:
            for extension in extensions:
                node = find_file('%s_%s%s' % (feature_base, tag, extension), True, False)
                if node:
                    append_file(tag_files, node)
                    append_file(feature_files, node)

    for node in tag_files:
        append_file(files, node)

    if not feature_files:
        bld.fatal('Could not find any source files for feature %s' % feature_name)
    if not files:
        bld.fatal('Could not find selected source files for feature %s on platform %s' % (feature_name, platform))

    return files, feature_files
