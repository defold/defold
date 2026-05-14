#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import importlib.util
import os
import sys

from cross_build import get_configured_platforms, get_platform_roots, get_repo_root


def get_target_os_from_platform(platform):
    if not platform:
        return None
    return platform.split('-')[-1]


def _hook_subdir(hook):
    if hook == 'build':
        return 'scripts'
    return 'build_tools'


def _add_private_paths(root):
    for subdir in ('build_tools', 'scripts'):
        path = os.path.join(root, subdir)
        if os.path.isdir(path) and path not in sys.path:
            sys.path.insert(0, path)


def _hook_file(root, hook, target_os):
    subdir = _hook_subdir(hook)
    return os.path.join(root, subdir, '%s_%s.py' % (hook, target_os))


def find_hook_file(hook, platform):
    target_os = get_target_os_from_platform(platform)
    if not target_os:
        return None

    roots = list(get_platform_roots(platform))
    repo_root = get_repo_root()
    if repo_root not in roots:
        roots.append(repo_root)

    for root in roots:
        path = _hook_file(root, hook, target_os)
        if os.path.exists(path):
            return path
    return None


def has_hook_module(hook, platform):
    return find_hook_file(hook, platform) is not None


def load_hook_module(hook, platform):
    path = find_hook_file(hook, platform)
    if not path:
        return None

    root = os.path.dirname(os.path.dirname(path))
    _add_private_paths(root)

    module_name = os.path.splitext(os.path.basename(path))[0]
    module = sys.modules.get(module_name)
    if module and os.path.abspath(getattr(module, '__file__', '')) == os.path.abspath(path):
        return module

    spec = importlib.util.spec_from_file_location(module_name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    print("Imported %s from %s" % (module_name, path))
    return module


def call_hook(hook, platform, name, default=None, *args):
    module = load_hook_module(hook, platform)
    func = getattr(module, name, None) if module else None
    return func(*args) if func else default


def get_hook_modules(hook, platforms=None):
    modules = []
    for platform in platforms or get_configured_platforms():
        module = load_hook_module(hook, platform)
        if module and module not in modules:
            modules.append(module)
    return modules


def find_hook_attr(hook, attr_name, platforms=None):
    for module in get_hook_modules(hook, platforms):
        attr = getattr(module, attr_name, None)
        if attr is not None:
            return attr
    return None
