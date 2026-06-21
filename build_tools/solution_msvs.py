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
import re
import shutil
import subprocess
import xml.etree.ElementTree as ElementTree
from glob import glob
from os.path import join, normpath, relpath


def is_visual_studio_platform(platform):
    return platform == 'win32' or platform.endswith('-win32')


def is_visual_studio_generator(generator):
    return bool(generator and generator.startswith('Visual Studio'))


def arch_args(platform):
    if platform == 'win32' or platform.startswith('x86-'):
        return ['-A', 'Win32']
    if platform.startswith('x86_64-'):
        return ['-A', 'x64']
    if platform.startswith('arm64-'):
        return ['-A', 'ARM64']
    return []


def cmake_args(generator, instance, windows_sdk_version):
    args = [
        '-DCMAKE_CONFIGURATION_TYPES:STRING=Debug;RelWithDebInfo',
        '-DCMAKE_SUPPRESS_REGENERATION:BOOL=ON'
    ]
    if instance:
        args += [
            f'-DCMAKE_GENERATOR_INSTANCE:PATH={instance}',
            f'-DDEFOLD_VISUAL_STUDIO_ROOT:PATH={instance}'
        ]
    if windows_sdk_version:
        args += [
            f'-DCMAKE_SYSTEM_VERSION:STRING={windows_sdk_version}',
            f'-DDEFOLD_WINDOWS_SDK_VERSION:STRING={windows_sdk_version}'
        ]
    return args


def msbuild_path(visual_studio_instance):
    if not visual_studio_instance:
        return None
    msbuild = join(visual_studio_instance, 'MSBuild', 'Current', 'Bin', 'amd64', 'MSBuild.exe')
    if os.path.exists(msbuild):
        return msbuild
    msbuild = join(visual_studio_instance, 'MSBuild', 'Current', 'Bin', 'MSBuild.exe')
    if os.path.exists(msbuild):
        return msbuild
    return None


def log_selection(log, generator, instance, windows_sdk_version):
    msbuild = msbuild_path(instance)
    log(
        'Visual Studio selection: generator=%s, instance=%s, windows_sdk=%s%s' % (
            generator,
            instance or 'CMake default',
            windows_sdk_version or 'CMake default',
            f', msbuild={msbuild}' if msbuild else ''))


def final_solution_path(build_dir, target_name):
    slnx_path = join(os.path.abspath(build_dir), f'{target_name}.slnx')
    sln_path = join(os.path.abspath(build_dir), f'{target_name}.sln')
    return slnx_path if os.path.exists(slnx_path) else sln_path


def cleanup_stale_solutions(build_dir, final_path, old_project_name, log):
    solution_build_dir = os.path.abspath(build_dir)
    stale_solution_paths = [
        join(solution_build_dir, f'{old_project_name}.sln'),
        join(solution_build_dir, f'{old_project_name}.slnx'),
    ]
    for stale_path in stale_solution_paths:
        if stale_path != final_path and os.path.exists(stale_path):
            try:
                if os.path.isdir(stale_path):
                    shutil.rmtree(stale_path)
                else:
                    os.remove(stale_path)
            except Exception as e:
                log(f'Warning: Failed to remove stale generated solution {stale_path}: {e}')


def organize_solution(solution_path, defold_root, log):
    if solution_path.endswith('.slnx') and os.path.exists(solution_path):
        organize_slnx(solution_path, defold_root, log)


def _solution_folder_for_project(project_path):
    project_path = project_path.replace('\\', '/')
    project_path_lower = project_path.lower()

    engine_match = re.search(r'(^|/)engine/([^/]+)/', project_path_lower)
    if engine_match:
        engine_module = engine_match.group(2)
        return f'Engine/{engine_module}/targets'

    if '/share/extender/' in project_path_lower:
        return 'Extender'

    return 'CMake configs'


def _project_path_exists(solution_dir, project_path):
    if not project_path:
        return False
    project_path = project_path.replace('/', os.sep).replace('\\', os.sep)
    if os.path.isabs(project_path):
        return os.path.exists(project_path)
    return os.path.exists(join(solution_dir, project_path))


def _project_key(project_path):
    return normpath(project_path.replace('/', os.sep).replace('\\', os.sep)).lower()


def _is_hidden_solution_project(project_path):
    project_filename = os.path.basename(project_path.replace('\\', '/')).lower()
    return project_filename.startswith('run_test_') and project_filename.endswith('.vcxproj')


def _solution_file_path(path):
    return os.path.abspath(path).replace('\\', '/')


def _engine_source_files(defold_root):
    source_roots = ('include', 'proto', 'scripts', 'src')
    root_file_names = {
        'CMakeLists.txt',
        'README.md',
        'sdk_gen.json',
        'wscript',
    }
    source_extensions = {
        '.bat', '.c', '.cc', '.cmake', '.cpp', '.cxx', '.h', '.hpp',
        '.hxx', '.inc', '.inl', '.java', '.json', '.lua', '.m', '.md',
        '.mm', '.proto', '.py', '.script', '.sh', '.txt', '.yml',
        '.yaml',
    }
    header_extensions = {'.h', '.hpp', '.hxx', '.inc', '.inl'}
    excluded_dirs = {'__pycache__', '.git', '.gradle', '.vs', 'build', 'tmp'}
    folder_files = {}

    engine_root = join(defold_root, 'engine')
    if not os.path.isdir(engine_root):
        return folder_files

    for module_root in sorted(glob(join(engine_root, '*'))):
        if not os.path.isdir(module_root):
            continue
        module_name = os.path.basename(module_root)

        def add_file(path, folder_override=None):
            rel_path = relpath(path, module_root).replace('\\', '/')
            folder = f'Engine/{module_name}'
            if folder_override:
                folder = f'{folder}/{folder_override}'
            else:
                rel_dir = os.path.dirname(rel_path).replace('\\', '/')
                if rel_dir:
                    folder = f'{folder}/{rel_dir}'
            folder_files.setdefault(folder, set()).add(_solution_file_path(path))

        for file_name in root_file_names:
            path = join(module_root, file_name)
            if os.path.isfile(path):
                add_file(path)

        for source_root in source_roots:
            root_path = join(module_root, source_root)
            if not os.path.isdir(root_path):
                continue
            for current_root, dir_names, file_names in os.walk(root_path):
                dir_names[:] = sorted(name for name in dir_names if name not in excluded_dirs and not name.startswith('.'))
                for file_name in sorted(file_names):
                    extension = os.path.splitext(file_name)[1].lower()
                    if extension in source_extensions:
                        path = join(current_root, file_name)
                        if source_root == 'src' and extension in header_extensions:
                            rel_source_path = relpath(path, root_path).replace('\\', '/')
                            rel_source_dir = os.path.dirname(rel_source_path).replace('\\', '/')
                            include_folder = 'include'
                            if rel_source_dir:
                                include_folder = f'{include_folder}/{rel_source_dir}'
                            add_file(path, include_folder)
                        else:
                            add_file(path)

    return {
        folder: sorted(files, key=lambda path: path.lower())
        for folder, files in folder_files.items()
    }


def organize_slnx(solution_path, defold_root, log):
    try:
        tree = ElementTree.parse(solution_path)
    except Exception as e:
        log(f'Warning: Failed to parse Visual Studio solution for grouping: {e}')
        return

    root = tree.getroot()
    root_attributes = dict(root.attrib)
    configurations = None
    root_children = []
    projects = []

    def collect_projects(parent):
        nonlocal configurations
        for child in list(parent):
            if child.tag == 'Configurations' and parent is root:
                configurations = child
            elif child.tag == 'Project':
                projects.append(child)
            elif child.tag == 'Folder':
                collect_projects(child)
            elif parent is root:
                root_children.append(child)

    collect_projects(root)
    if not projects:
        return

    solution_dir = os.path.dirname(solution_path)
    folder_projects = {}
    default_startup_project = None
    default_startup_fallback_project = None
    missing_project_paths = set()
    hidden_project_paths = set()
    for project in projects:
        project_path = project.get('Path')
        if not project_path:
            continue
        if _is_hidden_solution_project(project_path):
            hidden_project_paths.add(_project_key(project_path))
            continue
        if not _project_path_exists(solution_dir, project_path):
            missing_project_paths.add(_project_key(project_path))
            continue
        project.attrib.pop('DefaultStartup', None)
        project_filename = os.path.basename(project_path).lower()
        if project_filename == 'dmengine.vcxproj':
            default_startup_project = project
        elif project_filename in ('dmengine_release.vcxproj', 'dmengine_headless.vcxproj'):
            default_startup_fallback_project = project
        folder = _solution_folder_for_project(project_path)
        folder_projects.setdefault(folder, []).append(project)

    if default_startup_project is None:
        default_startup_project = default_startup_fallback_project
    if default_startup_project is not None:
        default_startup_project.set('DefaultStartup', 'true')
    else:
        log(f'Warning: Could not find dmengine project to set as Visual Studio startup project in {solution_path}')

    folder_files = _engine_source_files(defold_root)
    folder_names = set()
    for folder in folder_projects:
        parts = folder.split('/')
        for i in range(1, len(parts) + 1):
            folder_names.add('/'.join(parts[:i]))
    for folder in folder_files:
        parts = folder.split('/')
        for i in range(1, len(parts) + 1):
            folder_names.add('/'.join(parts[:i]))

    removed_project_paths = missing_project_paths | hidden_project_paths

    def remove_removed_project_dependencies(parent):
        for child in list(parent):
            if child.tag == 'BuildDependency':
                dependency_path = child.get('Project')
                if dependency_path and _project_key(dependency_path) in removed_project_paths:
                    parent.remove(child)
            else:
                remove_removed_project_dependencies(child)

    for project in projects:
        remove_removed_project_dependencies(project)

    def folder_sort_key(folder):
        top_level_order = {
            'Engine': 0,
            'Extender': 1,
            'CMake configs': 2,
        }
        top_level = folder.split('/')[0]
        return (top_level_order.get(top_level, 3), folder.lower())

    root.clear()
    root.attrib.update(root_attributes)
    if configurations is not None:
        root.append(configurations)
    for child in root_children:
        root.append(child)

    for folder in sorted(folder_names, key=folder_sort_key):
        folder_element = ElementTree.Element('Folder', {'Name': f'/{folder}/'})
        for file_path in folder_files.get(folder, []):
            folder_element.append(ElementTree.Element('File', {'Path': file_path}))
        for project in sorted(folder_projects.get(folder, []), key=lambda p: p.get('Path', '').lower()):
            folder_element.append(project)
        root.append(folder_element)

    try:
        ElementTree.indent(tree, space='\t')
    except AttributeError:
        pass
    tree.write(solution_path, encoding='utf-8', xml_declaration=True)
    if hidden_project_paths:
        log(f'Hidden {len(hidden_project_paths)} Visual Studio run_test_* projects from solution')
    log(f'Organized Visual Studio solution folders: {solution_path}')


def _available_cmake_generators():
    try:
        cmake_help = subprocess.check_output(
            ['cmake', '--help'],
            stderr=subprocess.STDOUT,
            universal_newlines=True)
    except Exception:
        return []

    generators = re.findall(r'Visual Studio \d+ \d{4}', cmake_help)
    return sorted(set(generators), key=_version_sort_key, reverse=True)


def _version_sort_key(version):
    return [int(part) for part in re.findall(r'\d+', version)]


def _generator_from_major_version(major_version):
    generator_years = {
        '18': '2026',
        '17': '2022',
        '16': '2019',
        '15': '2017',
        '14': '2015',
    }
    year = generator_years.get(major_version)
    if not year:
        return None
    return f'Visual Studio {major_version} {year}'


def _vswhere_path():
    search_roots = [
        os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
        os.environ.get('ProgramFiles', r'C:\Program Files')
    ]
    for search_root in search_roots:
        vswhere = join(search_root, 'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
        if os.path.exists(vswhere):
            return vswhere
    return None


def _installations(log):
    vswhere = _vswhere_path()
    if not vswhere:
        return []

    try:
        vswhere_output = subprocess.check_output([
            vswhere,
            '-all',
            '-products', '*',
            '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '-format', 'json'
        ], stderr=subprocess.STDOUT, universal_newlines=True)
        installations = json.loads(vswhere_output)
    except Exception as e:
        log(f'Warning: Failed to query Visual Studio installations with vswhere: {e}')
        return []

    return sorted(
        installations,
        key=lambda installation: _version_sort_key(installation.get('installationVersion', '')),
        reverse=True)


def latest_selection(log):
    generator_override = os.environ.get('DEFOLD_VISUAL_STUDIO_GENERATOR')
    instance_override = os.environ.get('DEFOLD_VISUAL_STUDIO_ROOT') or os.environ.get('DEFOLD_VISUAL_STUDIO_INSTANCE')
    if generator_override:
        return {
            'generator': generator_override,
            'instance': instance_override
        }

    available_generators = _available_cmake_generators()
    available_generator_set = set(available_generators)
    installations = _installations(log)

    if instance_override:
        normalized_instance_override = normpath(instance_override)
        for installation in installations:
            installation_path = installation.get('installationPath')
            if installation_path and normpath(installation_path) == normalized_instance_override:
                major_version = installation.get('installationVersion', '').split('.')[0]
                generator = _generator_from_major_version(major_version)
                if generator and (not available_generator_set or generator in available_generator_set):
                    return {
                        'generator': generator,
                        'instance': installation_path
                    }

    for installation in installations:
        major_version = installation.get('installationVersion', '').split('.')[0]
        generator = _generator_from_major_version(major_version)
        if generator and (not available_generator_set or generator in available_generator_set):
            return {
                'generator': generator,
                'instance': installation.get('installationPath')
            }

    if available_generators:
        return {
            'generator': available_generators[0],
            'instance': instance_override
        }

    return {
        'generator': 'Visual Studio 17 2022',
        'instance': instance_override
    }


def installed_windows_sdk_versions():
    versions = set()
    search_roots = [
        os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
        os.environ.get('ProgramFiles', r'C:\Program Files')
    ]
    for search_root in search_roots:
        for layout in ('Windows Kits', 'WindowsKits'):
            include_root = join(search_root, layout, '10', 'Include')
            if not os.path.isdir(include_root):
                continue
            for version in os.listdir(include_root):
                version_root = join(include_root, version)
                if os.path.isdir(version_root) and os.path.isdir(join(version_root, 'um')):
                    versions.add(version)
    return sorted(versions, key=_version_sort_key, reverse=True)


def latest_windows_sdk_version():
    sdk_override = os.environ.get('DEFOLD_WINDOWS_SDK_VERSION')
    if sdk_override:
        return sdk_override

    versions = installed_windows_sdk_versions()
    if versions:
        return versions[0]

    return None


