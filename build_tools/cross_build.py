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


def _path_key(path):
    return os.path.normcase(os.path.normpath(path))


def _dedupe_paths(paths):
    unique = []
    seen = set()
    for path in paths:
        key = _path_key(path)
        if key not in seen:
            seen.add(key)
            unique.append(path)
    return unique


def _existing_dirs(paths):
    return [path for path in paths if os.path.isdir(path)]


def get_private_dynamo_homes(platform):
    return [os.path.join(root, 'tmp', 'dynamo_home') for root in get_platform_roots(platform)]


def get_private_include_paths(platform):
    paths = []
    for dynamo_home in get_private_dynamo_homes(platform):
        paths += [
            os.path.join(dynamo_home, 'sdk', 'include'),
            os.path.join(dynamo_home, 'include', platform),
            os.path.join(dynamo_home, 'include'),
            os.path.join(dynamo_home, 'ext', 'include', platform),
            os.path.join(dynamo_home, 'ext', 'include'),
        ]
    return _dedupe_paths(_existing_dirs(paths))


def get_private_library_paths(platform):
    paths = []
    for dynamo_home in get_private_dynamo_homes(platform):
        paths += [
            os.path.join(dynamo_home, 'lib', platform),
            os.path.join(dynamo_home, 'ext', 'lib', platform),
        ]
    return _dedupe_paths(_existing_dirs(paths))


def get_private_path_mirrors(platform, base_path, paths):
    repo_root = get_repo_root()
    mirrors = []
    for path in paths:
        if hasattr(path, 'abspath'):
            absolute_path = path.abspath()
        elif os.path.isabs(path):
            absolute_path = path
        else:
            absolute_path = os.path.normpath(os.path.join(base_path, path))

        try:
            relative_path = os.path.relpath(absolute_path, repo_root)
        except ValueError:
            continue

        if relative_path == os.pardir or relative_path.startswith(os.pardir + os.sep):
            continue

        for root in get_platform_roots(platform):
            private_path = os.path.join(root, relative_path)
            if os.path.isdir(private_path):
                mirrors.append(private_path)
    return _dedupe_paths(mirrors)


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


def get_private_platform_file_tags(platform):
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


def is_private_platform_file(platform, path):
    path = path.replace('\\', '/').lower()
    return any(tag.lower() in path for tag in get_private_platform_file_tags(platform))


def find_platform_file(bld, platform, path, public_fallback = True, private_roots = True):
    repo_root = get_repo_root()
    base_path = os.path.relpath(bld.path.abspath(), repo_root)
    if private_roots:
        for root in get_platform_roots(platform):
            absolute_path = os.path.join(root, base_path, path)
            private_path = os.path.join(base_path, path)
            if os.path.exists(absolute_path) and is_private_platform_file(platform, private_path):
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


def append_source_file(sources, source):
    if source and source_file_path(source) not in [source_file_path(x) for x in sources]:
        sources.append(source)


def remove_source_files(sources, remove_sources):
    remove_paths = set(source_file_path(x) for x in remove_sources)
    return [x for x in sources if source_file_path(x) not in remove_paths]


def find_platform_files(bld, platform, path, public_fallback = True, private_roots = True):
    repo_root = get_repo_root()
    base_path = os.path.relpath(bld.path.abspath(), repo_root)
    files = []

    if public_fallback:
        append_source_file(files, bld.path.find_node(path))

    if private_roots:
        for root in get_platform_roots(platform):
            absolute_path = os.path.join(root, base_path, path)
            private_path = os.path.join(base_path, path)
            if os.path.exists(absolute_path) and is_private_platform_file(platform, private_path):
                node = bld.root.find_node(absolute_path)
                append_source_file(files, node if node else absolute_path)

    return files


def get_platform_variant_tags(platform):
    tags = []
    for tag in reversed(get_platform_file_tags(platform)):
        if tag not in tags:
            tags.append(tag)
    for tag in reversed(get_private_platform_file_tags(platform)):
        if tag not in tags:
            tags.append(tag)
    return tags


def find_platform_variant_files(bld, platform, input_path):
    files = []
    append_source_file(files, bld.path.find_node(input_path))

    directory, filename = os.path.split(input_path)
    basename, extension = os.path.splitext(filename)
    if basename.endswith('_input'):
        basename = basename[:-len('_input')]

    for tag in get_platform_variant_tags(platform):
        path = os.path.join(directory, '%s_%s%s' % (basename, tag, extension))
        for node in find_platform_files(bld, platform, path):
            append_source_file(files, node)

    return files


def find_configured_platform_variant_files(bld, input_path):
    files = []
    append_source_file(files, bld.path.find_node(input_path))

    directory, filename = os.path.split(input_path)
    basename, extension = os.path.splitext(filename)
    if basename.endswith('_input'):
        basename = basename[:-len('_input')]

    for platform in get_configured_platforms():
        for tag in get_platform_variant_tags(platform):
            path = os.path.join(directory, '%s_%s%s' % (basename, tag, extension))
            for node in find_platform_files(bld, platform, path):
                append_source_file(files, node)

    return files


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


def find_feature_files(bld, feature_name, platform, extra_tags = None, preferred_tags = None, required = True):
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
    * missing feature files or missing selected files fail the build when required is True.
    """
    files = []
    feature_files = []

    def find_file(path, public_fallback = True, private_roots = True):
        return find_platform_file(bld, platform, path, public_fallback, private_roots)

    feature_base, extension = os.path.splitext(feature_name)
    extensions = ['.cpp', '.c', '.cc', '.cxx', '.mm', '.m']
    if extension:
        extensions = [extension]

    feature_patterns = []
    for extension in extensions:
        feature_patterns += [feature_base + extension,
                             feature_base + '_*' + extension]
    for node in bld.path.ant_glob(feature_patterns):
        append_source_file(feature_files, node)

    # Core implementation: <feature>.<ext> is shared by all platforms.
    for extension in extensions:
        node = find_file(feature_base + extension, True, False)
        if node:
            append_source_file(files, node)
            append_source_file(feature_files, node)

    # Preferred tags: explicit feature choices such as mbedtls override platform tags.
    tag_files = []
    for tag in get_feature_extra_tags(platform, preferred_tags):
        for extension in extensions:
            node = find_file('%s_%s%s' % (feature_base, tag, extension))
            if node:
                append_source_file(tag_files, node)
                append_source_file(feature_files, node)

    # Platform tags: target-specific files, optionally extended with feature tags.
    if not tag_files:
        for tag in get_platform_file_tags(platform) + get_feature_extra_tags(platform, extra_tags):
            for extension in extensions:
                node = find_file('%s_%s%s' % (feature_base, tag, extension))
                if node:
                    append_source_file(tag_files, node)
                    append_source_file(feature_files, node)

    # Fallback tags: public shared implementations used when no platform file matched.
    if not tag_files:
        for tag in get_platform_file_fallback_tags(platform) + ['default']:
            for extension in extensions:
                node = find_file('%s_%s%s' % (feature_base, tag, extension), True, False)
                if node:
                    append_source_file(tag_files, node)
                    append_source_file(feature_files, node)

    for node in tag_files:
        append_source_file(files, node)

    if required and not feature_files:
        bld.fatal('Could not find any source files for feature %s' % feature_name)
    if required and not files:
        bld.fatal('Could not find selected source files for feature %s on platform %s' % (feature_name, platform))

    return files, feature_files
