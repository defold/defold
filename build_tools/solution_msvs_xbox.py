#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import argparse
import copy
import os
import re
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ElementTree
from glob import glob
from os.path import join, normpath, relpath

import solution_msvs


_DLL_EXCLUDE_NAMES = {
    'xgs12_pc_x.dll',
    'xgs12_pc_xs.dll',
    'umd12_pc.dll',
}

_ARCHIVE_NAMES = (
    'game.projectc',
    'game.arci',
    'game.arcd',
    'game.dmanifest',
    'game.public.der',
    'ssl_keys.pem',
)

_ARTWORK_NAMES = (
    'StoreLogo.png',
    'Square150x150Logo.png',
    'Square44x44Logo.png',
    'Square480x480Logo.png',
    'SplashScreenImage.png',
)


def _copy_if_exists(src, dst_dir):
    if src and os.path.exists(src):
        os.makedirs(dst_dir, exist_ok=True)
        dst = os.path.join(dst_dir, os.path.basename(src))
        if os.path.normcase(os.path.abspath(src)) == os.path.normcase(os.path.abspath(dst)):
            return True
        shutil.copy2(src, dst)
        return True
    return False


def _sanitize_identity_name(name):
    sanitized = ''.join(c if c.isalnum() or c in '-.' else '-' for c in name.replace('_', '-').replace(' ', '-'))
    sanitized = sanitized.strip('-')
    return sanitized or 'Title'


def _local_name(tag):
    return tag.rsplit('}', 1)[-1] if tag.startswith('{') else tag


def _copy_manifest(manifest, layout, app_name, exe_name):
    if not _copy_if_exists(manifest, layout):
        return

    manifest_dst = os.path.join(layout, os.path.basename(manifest))
    try:
        tree = ElementTree.parse(manifest_dst)
        root = tree.getroot()
        for element in root.iter():
            if _local_name(element.tag) == 'Identity':
                element.set('Name', _sanitize_identity_name(app_name))
            elif _local_name(element.tag) == 'Executable':
                element.set('Name', exe_name)
        tree.write(manifest_dst, encoding='utf-8', xml_declaration=True)
    except Exception:
        pass


def _copy_artwork(artwork_dir, layout):
    if not artwork_dir or not os.path.isdir(artwork_dir):
        return
    for artwork_name in _ARTWORK_NAMES:
        _copy_if_exists(os.path.join(artwork_dir, artwork_name), layout)


def stage_xbone_debug_layout(exe, layout, source_dir, manifest, artwork_dir, app_name, pdbs=None):
    exe = os.path.abspath(exe)
    layout = os.path.abspath(layout)
    source_dir = os.path.abspath(source_dir) if source_dir else os.path.dirname(exe)
    exe_name = os.path.basename(exe)
    pdbs = pdbs or []

    if not os.path.exists(exe):
        raise RuntimeError('Xbox debug executable was not built: %s' % exe)

    os.makedirs(layout, exist_ok=True)
    _copy_if_exists(exe, layout)
    copied_pdb = False
    for pdb in list(pdbs) + [os.path.splitext(exe)[0] + '.pdb']:
        if _copy_if_exists(os.path.abspath(pdb), layout):
            copied_pdb = True
    if not copied_pdb:
        print('Warning: Xbox debug PDB was not found for %s' % exe)

    if manifest:
        _copy_manifest(manifest, layout, app_name, exe_name)
    else:
        _copy_manifest(os.path.join(os.path.dirname(exe), 'MicrosoftGame.config'), layout, app_name, exe_name)
    _copy_artwork(artwork_dir, layout)

    for archive_name in _ARCHIVE_NAMES:
        for archive_dir in (source_dir, os.path.join(source_dir, 'data')):
            if _copy_if_exists(os.path.join(archive_dir, archive_name), layout):
                break

    for dll_dir in (os.path.dirname(exe), source_dir):
        if not os.path.isdir(dll_dir):
            continue
        for name in os.listdir(dll_dir):
            if name.lower() in _DLL_EXCLUDE_NAMES or not name.lower().endswith('.dll'):
                continue
            _copy_if_exists(os.path.join(dll_dir, name), layout)

    print('Xbox debug layout staged: %s' % layout)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='command')

    stage = subparsers.add_parser('stage', help='Stage an Xbox debug layout')
    stage.add_argument('--exe', required=True)
    stage.add_argument('--layout', required=True)
    stage.add_argument('--source-dir')
    stage.add_argument('--manifest')
    stage.add_argument('--artwork-dir')
    stage.add_argument('--app-name', default='Defold')
    stage.add_argument('--pdb', action='append', default=[])

    args = parser.parse_args()
    if args.command == 'stage':
        stage_xbone_debug_layout(args.exe, args.layout, args.source_dir, args.manifest, args.artwork_dir, args.app_name, args.pdb)
        return 0

    parser.print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())


# Visual Studio Xbox solution generation
_XBONE_DEBUG_PROJECT_TYPE_GUID = '{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
_VC_PROJECT_TYPE_GUID = '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}'
_SOLUTION_FOLDER_PROJECT_TYPE_GUID = '{2150E333-8FDC-42A3-9474-1A3956D46DE8C}'
_XBONE_DEBUG_SOLUTION_GUID = uuid.UUID('6bb87999-3409-40c0-95ca-849ff4c7fc58')
_XBONE_DEBUG_CONFIGURATIONS = ('Debug', 'RelWithDebInfo')
_XBONE_DEBUG_ENGINE_TARGETS = ('dmengine', 'dmengine_release', 'dmengine_headless')
_XBONE_VS_PLATFORM = 'Gaming.Xbox.XboxOne.x64'
_XBONE_CMAKE_PROJECT_PLATFORM = 'x64'
_XBONE_DEBUG_PROJECT_PLATFORMS = (_XBONE_VS_PLATFORM,)
_MSBUILD_NS = 'http://schemas.microsoft.com/developer/msbuild/2003'
_XBONE_DEFAULT_REMOTE_ADDRESS = None

ElementTree.register_namespace('', _MSBUILD_NS)


def platform_toolset(generator):
    if generator:
        match = re.search(r'Visual Studio\s+(\d+)', generator)
        if match:
            toolsets = {
                '18': 'v145',
                '17': 'v143',
                '16': 'v142',
                '15': 'v141',
            }
            return toolsets.get(match.group(1))
    return None


def _xml_escape(value):
    if value is None:
        return ''
    return (
        str(value)
        .replace('&', '&amp;')
        .replace('<', '&lt;')
        .replace('>', '&gt;')
        .replace('"', '&quot;'))


def _msbuild_command_arg(value):
    return f'&quot;{_xml_escape(value)}&quot;'


def _command_arg(value):
    return f'"{value}"'


def _tool_path(name, fallback=None):
    return shutil.which(name) or fallback or name


def _gdk_tool_path(name):
    tool = shutil.which(name)
    if tool:
        return tool
    candidate_roots = [
        os.environ.get('GameDKXboxLatest'),
        os.environ.get('GameDKCoreLatest'),
        os.environ.get('GameDK'),
        os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'), 'Microsoft GDK'),
    ]
    for root in candidate_roots:
        if not root:
            continue
        candidate = os.path.join(root, 'bin', name)
        if os.path.exists(candidate):
            return candidate
    return name


def _default_xbox_console_address():
    global _XBONE_DEFAULT_REMOTE_ADDRESS

    if _XBONE_DEFAULT_REMOTE_ADDRESS is not None:
        return _XBONE_DEFAULT_REMOTE_ADDRESS

    _XBONE_DEFAULT_REMOTE_ADDRESS = ''
    xbconnect_path = _gdk_tool_path('xbconnect.exe')
    if not xbconnect_path:
        candidate_roots = [
            os.environ.get('GameDKXboxLatest'),
            os.environ.get('GameDKCoreLatest'),
            os.environ.get('GameDK'),
            os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'), 'Microsoft GDK'),
        ]
        for root in candidate_roots:
            if not root:
                continue
            candidate = os.path.join(root, 'bin', 'xbconnect.exe')
            if os.path.exists(candidate):
                xbconnect_path = candidate
                break
    if not xbconnect_path:
        return _XBONE_DEFAULT_REMOTE_ADDRESS

    for args in ([xbconnect_path, '/Q'], [xbconnect_path]):
        try:
            output = subprocess.check_output(
                args,
                stderr=subprocess.DEVNULL,
                universal_newlines=True,
                timeout=5)
        except Exception:
            continue

        for line in output.splitlines():
            match = re.search(r'\b\d{1,3}(?:\.\d{1,3}){3}(?:\+\S+)?\b', line)
            if match:
                _XBONE_DEFAULT_REMOTE_ADDRESS = match.group(0)
                return _XBONE_DEFAULT_REMOTE_ADDRESS

        for line in output.splitlines():
            match = re.search(r'(?i)\bdefault\s+console\s*[:=]\s*(\S+)', line)
            if match:
                _XBONE_DEFAULT_REMOTE_ADDRESS = match.group(1)
                return _XBONE_DEFAULT_REMOTE_ADDRESS

    return _XBONE_DEFAULT_REMOTE_ADDRESS


def _raw_command_path_prefix(paths):
    dirs = []
    seen = set()
    for path in paths:
        if not path:
            continue
        directory = path if os.path.isdir(path) else os.path.dirname(path)
        if not directory:
            continue
        key = normpath(directory).lower()
        if key in seen:
            continue
        seen.add(key)
        dirs.append(directory)
    if not dirs:
        return ''
    return f'set "PATH={";".join(dirs)};%PATH%" && '


def _command_path_prefix(paths):
    dirs = []
    seen = set()
    for path in paths:
        if not path:
            continue
        directory = path if os.path.isdir(path) else os.path.dirname(path)
        if not directory:
            continue
        key = normpath(directory).lower()
        if key in seen:
            continue
        seen.add(key)
        dirs.append(directory)
    if not dirs:
        return ''
    return f'set &quot;PATH={_xml_escape(";".join(dirs))};%PATH%&quot; &amp;&amp; '


def _target_name_from_project(project_path):
    return os.path.splitext(os.path.basename(project_path))[0]


def _is_xbone_debug_target(target_name):
    return target_name in _XBONE_DEBUG_ENGINE_TARGETS or target_name.startswith('test_')


def _is_visual_studio_executable_project(project_path):
    if not os.path.exists(project_path):
        return False
    try:
        tree = ElementTree.parse(project_path)
    except Exception:
        return False

    configuration_types = []
    target_exts = []
    for element in tree.iter():
        if element.tag.endswith('ConfigurationType') and element.text:
            configuration_types.append(element.text.strip())
        elif element.tag.endswith('TargetExt') and element.text:
            target_exts.append(element.text.strip().lower())

    return 'Application' in configuration_types and (not target_exts or '.exe' in target_exts)


def _read_vs_debugger_working_directory(project_path):
    user_path = f'{project_path}.user'
    if not os.path.exists(user_path):
        return None
    try:
        tree = ElementTree.parse(user_path)
    except Exception:
        return None
    for element in tree.iter():
        if element.tag.endswith('LocalDebuggerWorkingDirectory') and element.text:
            value = element.text.strip()
            if value:
                return value
    return None


def _xml_local_name(tag):
    return tag.rsplit('}', 1)[-1] if tag.startswith('{') else tag


def _xml_tag(parent, name):
    if parent.tag.startswith('{'):
        return f'{{{parent.tag[1:].split("}", 1)[0]}}}{name}'
    return name


def _xml_find_child(parent, name):
    for child in parent:
        if _xml_local_name(child.tag) == name:
            return child
    return None


def _xml_set_child(parent, name, value):
    child = _xml_find_child(parent, name)
    if child is None:
        child = ElementTree.SubElement(parent, _xml_tag(parent, name))
    child.text = value
    return child


def _sanitize_xbox_identity_name(name):
    sanitized = ''.join(c if c.isalnum() or c in '-.' else '-' for c in name.replace('_', '-').replace(' ', '-'))
    sanitized = sanitized.strip('-')
    return sanitized or 'Title'


def _write_xbone_project_manifest(project_dir, manifest_path, app_name, exe_name):
    if not manifest_path or not os.path.exists(manifest_path):
        return None

    manifest_dst = os.path.join(project_dir, 'MicrosoftGame.config')
    shutil.copy2(manifest_path, manifest_dst)
    try:
        tree = ElementTree.parse(manifest_dst)
        root = tree.getroot()
        for element in root.iter():
            if _xml_local_name(element.tag) == 'Identity':
                element.set('Name', _sanitize_xbox_identity_name(app_name))
            elif _xml_local_name(element.tag) == 'Executable':
                element.set('Name', exe_name)
        tree.write(manifest_dst, encoding='utf-8', xml_declaration=True)
    except Exception:
        return manifest_dst
    return manifest_dst


def _patch_xbone_slnx_platform(solution_path):
    return False


def _patch_xbone_sln_platform(solution_path):
    return False


def _solution_vcxproj_paths_from_slnx(solution_path):
    if not solution_path.endswith('.slnx') or not os.path.exists(solution_path):
        return []
    tree = ElementTree.parse(solution_path)
    solution_dir = os.path.dirname(os.path.abspath(solution_path))
    projects = []
    for project in tree.getroot().iter('Project'):
        project_path = project.get('Path')
        if not project_path or not project_path.lower().endswith('.vcxproj'):
            continue
        if not os.path.isabs(project_path):
            project_path = os.path.join(solution_dir, project_path)
        projects.append(os.path.abspath(project_path))
    return projects


def _solution_vcxproj_paths_from_sln(solution_path):
    if not solution_path.endswith('.sln') or not os.path.exists(solution_path):
        return []
    solution_dir = os.path.dirname(os.path.abspath(solution_path))
    project_re = re.compile(r'^Project\("[^"]+"\) = "[^"]+", "([^"]+\.vcxproj)", ')
    projects = []
    with open(solution_path, 'r', encoding='utf-8-sig', errors='replace') as f:
        for line in f:
            match = project_re.match(line.strip())
            if not match:
                continue
            project_path = match.group(1)
            if not os.path.isabs(project_path):
                project_path = os.path.join(solution_dir, project_path)
            projects.append(os.path.abspath(project_path))
    return projects


def _solution_vcxproj_paths(solution_path):
    projects = _solution_vcxproj_paths_from_slnx(solution_path)
    if not projects:
        projects = _solution_vcxproj_paths_from_sln(solution_path)

    filtered = []
    seen = set()
    for project_path in projects:
        key = normpath(project_path).lower()
        if key in seen or not os.path.exists(project_path):
            continue
        seen.add(key)
        filtered.append(project_path)
    return filtered


def _project_configuration_condition(configuration, platform):
    return f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"


def _add_xbone_project_configuration_aliases(project_path):
    if not os.path.exists(project_path):
        return False
    try:
        tree = ElementTree.parse(project_path)
    except Exception:
        return False

    root = tree.getroot()
    changed = False
    project_configurations = None
    for child in root:
        if _xml_local_name(child.tag) == 'ItemGroup' and child.get('Label') == 'ProjectConfigurations':
            project_configurations = child
            break

    if project_configurations is not None:
        existing_configurations = set()
        for project_configuration in project_configurations:
            if _xml_local_name(project_configuration.tag) == 'ProjectConfiguration':
                existing_configurations.add(project_configuration.get('Include'))

        for configuration in _XBONE_DEBUG_CONFIGURATIONS:
            source_include = f'{configuration}|{_XBONE_CMAKE_PROJECT_PLATFORM}'
            alias_include = f'{configuration}|{_XBONE_VS_PLATFORM}'
            if source_include not in existing_configurations or alias_include in existing_configurations:
                continue
            project_configuration = ElementTree.SubElement(
                project_configurations,
                _xml_tag(project_configurations, 'ProjectConfiguration'),
                {'Include': alias_include})
            _xml_set_child(project_configuration, 'Configuration', configuration)
            _xml_set_child(project_configuration, 'Platform', _XBONE_VS_PLATFORM)
            existing_configurations.add(alias_include)
            changed = True

    def duplicate_conditioned_children(parent):
        nonlocal changed
        original_children = list(parent)
        existing_conditions = {(child.tag, child.get('Condition')) for child in original_children}
        insert_offset = 0
        for index, child in enumerate(original_children):
            condition = child.get('Condition')
            if not condition:
                continue
            for configuration in _XBONE_DEBUG_CONFIGURATIONS:
                source_condition = _project_configuration_condition(configuration, _XBONE_CMAKE_PROJECT_PLATFORM)
                alias_condition = _project_configuration_condition(configuration, _XBONE_VS_PLATFORM)
                if condition != source_condition or (child.tag, alias_condition) in existing_conditions:
                    continue
                alias_child = copy.deepcopy(child)
                alias_child.set('Condition', alias_condition)
                parent.insert(index + 1 + insert_offset, alias_child)
                insert_offset += 1
                existing_conditions.add((child.tag, alias_condition))
                changed = True

        for child in original_children:
            duplicate_conditioned_children(child)

    duplicate_conditioned_children(root)

    if changed:
        try:
            ElementTree.indent(tree, space='  ')
        except AttributeError:
            pass
        tree.write(project_path, encoding='utf-8', xml_declaration=True)
    return changed


def _xbone_stage_post_build_command(project_path, private_repo_root, defold_root):
    target_binary_dir = os.path.dirname(os.path.abspath(project_path))
    working_dir = _read_vs_debugger_working_directory(project_path) or target_binary_dir
    stage_helper = os.path.join(defold_root, 'build_tools', 'solution_msvs_xbox.py')
    manifest_path = os.path.join(private_repo_root, 'scripts', 'xbox', 'MicrosoftGame.config')
    artwork_dir = os.path.join(private_repo_root, 'scripts', 'xbox', 'artwork')
    cmake_path = _tool_path('cmake')
    python_path = os.path.abspath(sys.executable or _tool_path('python'))
    git_path = _tool_path('git', '')
    path_prefix = _raw_command_path_prefix([cmake_path, python_path, git_path])
    return (
        f'{path_prefix}{_command_arg(python_path)} {_command_arg(stage_helper)} stage '
        f'--exe "$(TargetPath)" '
        f'--layout "$(TargetDir)." '
        f'--source-dir {_command_arg(working_dir)} '
        f'--manifest {_command_arg(manifest_path)} '
        f'--artwork-dir {_command_arg(artwork_dir)} '
        f'--app-name "$(TargetName)"')


def _patch_xbone_executable_vcxproj(project_path, private_repo_root, defold_root):
    tree = ElementTree.parse(project_path)
    root = tree.getroot()
    target_name = _target_name_from_project(project_path)
    stage_command = _xbone_stage_post_build_command(project_path, private_repo_root, defold_root)
    changed = False

    for property_group in root.iter():
        if _xml_local_name(property_group.tag) != 'PropertyGroup':
            continue
        condition = property_group.get('Condition') or ''
        if f'|{_XBONE_CMAKE_PROJECT_PLATFORM}' not in condition:
            continue

        desired_properties = {
            'DebuggerFlavor': 'XboxGamingVCppDebugger',
            'LocalDebuggerDebuggerType': 'NativeOnly',
            'DeployFromOutDir': 'true',
            'LayoutDir': '$(OutDir)',
            'DeployMode': 'Push',
            'LogModuleLoads': 'true',
        }
        debug_args = '--use-validation-layers' if target_name in _XBONE_DEBUG_ENGINE_TARGETS and 'Debug|' in condition else ''
        desired_properties['LocalDebuggerCommand'] = '$(TargetPath)'
        desired_properties['LocalDebuggerCommandArguments'] = debug_args
        desired_properties['LocalDebuggerWorkingDirectory'] = _read_vs_debugger_working_directory(project_path) or os.path.dirname(os.path.abspath(project_path))

        for name, value in desired_properties.items():
            child = _xml_find_child(property_group, name)
            if child is None or (child.text or '') != value:
                _xml_set_child(property_group, name, value)
                changed = True

    for item_definition_group in root.iter():
        if _xml_local_name(item_definition_group.tag) != 'ItemDefinitionGroup':
            continue
        condition = item_definition_group.get('Condition') or ''
        if f'|{_XBONE_CMAKE_PROJECT_PLATFORM}' not in condition:
            continue
        post_build = _xml_find_child(item_definition_group, 'PostBuildEvent')
        if post_build is None:
            post_build = ElementTree.SubElement(item_definition_group, _xml_tag(item_definition_group, 'PostBuildEvent'))
            changed = True
        command = _xml_find_child(post_build, 'Command')
        existing_command = command.text if command is not None and command.text else ''
        if 'solution_msvs_xbox.py' in existing_command:
            new_command = stage_command
        elif existing_command.strip():
            new_command = existing_command.rstrip() + os.linesep + stage_command
        else:
            new_command = stage_command
        if command is None or existing_command != new_command:
            _xml_set_child(post_build, 'Command', new_command)
            changed = True

    if changed:
        tree.write(project_path, encoding='utf-8', xml_declaration=True)
    return changed


def _patch_xbone_vcxproj_user(project_path):
    user_path = f'{project_path}.user'
    if os.path.exists(user_path):
        tree = ElementTree.parse(user_path)
        root = tree.getroot()
    else:
        root = ElementTree.Element(f'{{{_MSBUILD_NS}}}Project', {'ToolsVersion': 'Current'})
        tree = ElementTree.ElementTree(root)

    changed = False
    for child in list(root):
        if _xml_local_name(child.tag) == 'PropertyGroup':
            condition = child.get('Condition') or ''
            if f'|{_XBONE_VS_PLATFORM}' in condition:
                root.remove(child)
                changed = True

    root_property_group = None
    for child in root:
        if _xml_local_name(child.tag) == 'PropertyGroup' and not child.get('Condition'):
            root_property_group = child
            break
    if root_property_group is None:
        root_property_group = ElementTree.SubElement(root, _xml_tag(root, 'PropertyGroup'))
        changed = True
    remote_address = _xml_find_child(root_property_group, 'RemoteAddress')
    default_remote_address = _default_xbox_console_address()
    if remote_address is None or (default_remote_address and not (remote_address.text or '').strip()):
        _xml_set_child(root_property_group, 'RemoteAddress', default_remote_address)
        changed = True

    target_name = _target_name_from_project(project_path)
    working_dir = _read_vs_debugger_working_directory(project_path) or os.path.dirname(os.path.abspath(project_path))
    for configuration in _XBONE_DEBUG_CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{_XBONE_CMAKE_PROJECT_PLATFORM}'"
        property_group = None
        for child in root:
            if _xml_local_name(child.tag) == 'PropertyGroup' and child.get('Condition') == condition:
                property_group = child
                break
        if property_group is None:
            property_group = ElementTree.SubElement(root, _xml_tag(root, 'PropertyGroup'), {'Condition': condition})
            changed = True
        debug_args = '--use-validation-layers' if configuration == 'Debug' and target_name in _XBONE_DEBUG_ENGINE_TARGETS else ''
        desired_properties = {
            'LocalDebuggerCommand': '$(LayoutDir)\\$(TargetName).exe',
            'LocalDebuggerCommandArguments': debug_args,
            'LocalDebuggerWorkingDirectory': working_dir,
            'DeployMode': 'Push',
            'LogModuleLoads': 'true',
        }
        for name, value in desired_properties.items():
            child = _xml_find_child(property_group, name)
            if child is None or (child.text or '') != value:
                _xml_set_child(property_group, name, value)
                changed = True

    if changed:
        tree.write(user_path, encoding='utf-8', xml_declaration=True)
    return changed


def patch_xbone_cmake_solution(build_dir, solution_path, private_repo_root, defold_root, log):
    if not private_repo_root:
        log('Warning: Xbox CMake solution was not patched because no private Xbox repository root is configured')
        return

    if solution_path.endswith('.slnx'):
        _patch_xbone_slnx_platform(solution_path)
    else:
        _patch_xbone_sln_platform(solution_path)

    projects = _solution_vcxproj_paths(solution_path)
    executable_projects = 0
    for project_path in projects:
        _replace_xbone_vcxproj_platform(project_path)
        if _is_visual_studio_executable_project(project_path):
            _patch_xbone_executable_vcxproj(project_path, os.path.abspath(private_repo_root), os.path.abspath(defold_root))
            _patch_xbone_vcxproj_user(project_path)
            executable_projects += 1

    log(f'Xbox CMake solution patched for x64 build + Xbox debugger: {len(projects)} projects, {executable_projects} launchable executables/tests')


def _xbone_debug_project_guid(project_name):
    return '{%s}' % str(uuid.uuid5(_XBONE_DEBUG_SOLUTION_GUID, project_name)).upper()


def _solution_folder_guid(folder):
    return '{%s}' % str(uuid.uuid5(_XBONE_DEBUG_SOLUTION_GUID, f'folder:{folder}')).upper()


def _solution_project_guid(project_path):
    return '{%s}' % str(uuid.uuid5(_XBONE_DEBUG_SOLUTION_GUID, f'project:{normpath(project_path).lower()}')).upper()


def _resolve_solution_project_path(solution_dir, project_path):
    if not project_path:
        return None
    project_path = project_path.replace('/', os.sep).replace('\\', os.sep)
    if os.path.isabs(project_path):
        return os.path.abspath(project_path)
    return os.path.abspath(os.path.join(solution_dir, project_path))


def _format_solution_guid(guid):
    if not guid:
        return None
    guid = guid.strip('{}').upper()
    return '{%s}' % guid


def _slnx_folder_path(folder_name):
    if not folder_name:
        return None
    folder = folder_name.strip('/\\').replace('\\', '/')
    return folder or None


def _collect_xbone_cmake_solution_projects(solution_path):
    if not solution_path or not solution_path.endswith('.slnx') or not os.path.exists(solution_path):
        return []

    try:
        tree = ElementTree.parse(solution_path)
    except Exception:
        return []

    solution_dir = os.path.dirname(os.path.abspath(solution_path))
    projects = []
    path_to_guid = {}

    def collect_projects(parent, folder=None):
        for child in list(parent):
            if child.tag == 'Folder':
                collect_projects(child, _slnx_folder_path(child.get('Name')) or folder)
            elif child.tag == 'Project':
                project_path = child.get('Path')
                resolved_path = _resolve_solution_project_path(solution_dir, project_path)
                if not resolved_path or not project_path.lower().endswith('.vcxproj'):
                    continue
                if solution_msvs._is_hidden_solution_project(project_path) or not os.path.exists(resolved_path):
                    continue

                guid = _format_solution_guid(child.get('Id')) or _solution_project_guid(resolved_path)
                item = {
                    'name': _target_name_from_project(resolved_path),
                    'path': resolved_path,
                    'guid': guid,
                    'type_guid': _format_solution_guid(child.get('Type')) or _VC_PROJECT_TYPE_GUID,
                    'source_path': resolved_path,
                    'kind': 'cmake',
                    'dependencies': [],
                    'folder': folder,
                }
                projects.append((child, item))
                path_to_guid[normpath(resolved_path).lower()] = guid

    collect_projects(tree.getroot())

    collected = []
    for project_element, item in projects:
        for dependency in project_element.iter('BuildDependency'):
            dependency_path = dependency.get('Project')
            resolved_dependency = _resolve_solution_project_path(solution_dir, dependency_path)
            if not resolved_dependency:
                continue
            dependency_guid = path_to_guid.get(normpath(resolved_dependency).lower())
            if dependency_guid and dependency_guid != item['guid']:
                item['dependencies'].append(dependency_guid)
        item['dependencies'] = sorted(set(item['dependencies']))
        collected.append(item)

    return collected

def _find_xbone_debug_target_projects_from_slnx(solution_path):
    if not solution_path or not solution_path.endswith('.slnx') or not os.path.exists(solution_path):
        return []
    try:
        tree = ElementTree.parse(solution_path)
    except Exception:
        return []

    projects = []
    solution_dir = os.path.dirname(os.path.abspath(solution_path))
    for project in tree.getroot().iter('Project'):
        project_path = project.get('Path')
        if not project_path:
            continue
        if not os.path.isabs(project_path):
            project_path = os.path.join(solution_dir, project_path)
        target_name = _target_name_from_project(project_path)
        if _is_xbone_debug_target(target_name) and _is_visual_studio_executable_project(project_path):
            projects.append((target_name, os.path.abspath(project_path)))
    return projects


def _find_xbone_debug_target_projects_from_sln(solution_path):
    if not solution_path or not solution_path.endswith('.sln') or not os.path.exists(solution_path):
        return []
    projects = []
    solution_dir = os.path.dirname(os.path.abspath(solution_path))
    project_re = re.compile(r'^Project\("[^"]+"\) = "[^"]+", "([^"]+\.vcxproj)", ')
    with open(solution_path, 'r', encoding='utf-8-sig', errors='replace') as f:
        for line in f:
            match = project_re.match(line.strip())
            if not match:
                continue
            project_path = match.group(1)
            if not os.path.isabs(project_path):
                project_path = os.path.join(solution_dir, project_path)
            target_name = _target_name_from_project(project_path)
            if _is_xbone_debug_target(target_name) and _is_visual_studio_executable_project(project_path):
                projects.append((target_name, os.path.abspath(project_path)))
    return projects


def _find_xbone_debug_target_projects(build_dir, solution_path=None):
    projects = _find_xbone_debug_target_projects_from_slnx(solution_path)
    if not projects:
        projects = _find_xbone_debug_target_projects_from_sln(solution_path)
    if not projects:
        projects = []
        search_roots = [os.path.abspath(build_dir)]
        for project_path in glob(os.path.join(os.path.abspath(build_dir), '..', '..', 'engine', '*', 'build', '*', '**', '*.vcxproj'), recursive=True):
            search_roots.append(os.path.dirname(os.path.abspath(project_path)))
        search_roots = sorted(set(search_roots), key=lambda path: path.lower())
        for root in search_roots:
            for project_path in glob(os.path.join(root, '**', '*.vcxproj'), recursive=True):
                target_name = _target_name_from_project(project_path)
                if _is_xbone_debug_target(target_name) and _is_visual_studio_executable_project(project_path):
                    projects.append((target_name, os.path.abspath(project_path)))

    generated_roots = [
        os.path.join(os.path.abspath(build_dir), 'xbox'),
        os.path.join(os.path.abspath(build_dir), 'xbone-debug'),
    ]
    filtered = []
    seen = set()
    for target_name, project_path in projects:
        project_path = os.path.abspath(project_path)
        for generated_root in generated_roots:
            if os.path.commonpath([generated_root, project_path]) == generated_root:
                break
        else:
            key = normpath(project_path).lower()
            if key in seen:
                continue
            seen.add(key)
            filtered.append((target_name, project_path))
    filtered.sort(key=lambda item: (0 if item[0] in _XBONE_DEBUG_ENGINE_TARGETS else 1, item[0].lower()))
    return filtered


def _copy_xbone_project_filters(target_project_path, project_path):
    filters_path = f'{target_project_path}.filters'
    if os.path.exists(filters_path):
        shutil.copy2(filters_path, f'{project_path}.filters')


def _source_items_from_project(target_project_path):
    if not os.path.exists(target_project_path):
        return ''

    source_item_types = {
        'ClCompile',
        'ClInclude',
        'CustomBuild',
        'Midl',
        'None',
        'ResourceCompile',
        'Text',
    }
    try:
        tree = ElementTree.parse(target_project_path)
    except Exception:
        return ''

    items_by_type = {}
    seen = set()
    for item_group in tree.getroot():
        if _xml_local_name(item_group.tag) != 'ItemGroup':
            continue
        for item in item_group:
            item_type = _xml_local_name(item.tag)
            include = item.get('Include')
            if item_type not in source_item_types or not include:
                continue
            key = (item_type, normpath(include).lower())
            if key in seen:
                continue
            seen.add(key)
            items_by_type.setdefault(item_type, []).append(include)

    groups = []
    for item_type in sorted(items_by_type):
        item_lines = [
            f'    <{item_type} Include="{_xml_escape(include)}" />'
            for include in sorted(items_by_type[item_type], key=lambda path: path.lower())
        ]
        groups.append(f'''  <ItemGroup>
{os.linesep.join(item_lines)}
  </ItemGroup>''')
    return os.linesep.join(groups)


def _write_xbone_debug_project(project_dir, project_name, target_name, target_project_path, build_dir, private_repo_root, defold_root, toolset=None, windows_sdk_version=None):
    os.makedirs(project_dir, exist_ok=True)

    project_guid = _xbone_debug_project_guid(project_name)
    toolset = toolset or 'v143'
    target_binary_dir = os.path.dirname(os.path.abspath(target_project_path))
    exe_path = os.path.join(target_binary_dir, '$(Configuration)', f'{target_name}.exe')
    pdb_candidates = [
        os.path.join(target_binary_dir, '$(Configuration)', f'{target_name}.pdb'),
        os.path.join(target_binary_dir, f'{target_name}.pdb'),
    ]
    layout_dir = os.path.join(project_dir, 'layout', '$(Configuration)')
    working_dir = _read_vs_debugger_working_directory(target_project_path) or target_binary_dir
    stage_helper = os.path.join(defold_root, 'build_tools', 'solution_msvs_xbox.py')
    manifest_path = os.path.join(private_repo_root, 'scripts', 'xbox', 'MicrosoftGame.config')
    project_manifest_path = _write_xbone_project_manifest(project_dir, manifest_path, project_name, f'{target_name}.exe')
    artwork_dir = os.path.join(private_repo_root, 'scripts', 'xbox', 'artwork')
    cmake_path = _tool_path('cmake')
    python_path = os.path.abspath(sys.executable or _tool_path('python'))
    git_path = _tool_path('git', '')
    xbapp_path = _gdk_tool_path('xbapp.exe')
    path_prefix = _command_path_prefix([cmake_path, python_path, git_path, xbapp_path])
    remote_address = _default_xbox_console_address()

    pdb_args = ''.join(
        f' --pdb {_msbuild_command_arg(pdb_path)}'
        for pdb_path in pdb_candidates)
    stage_invocation = (
        f'{_msbuild_command_arg(python_path)} {_msbuild_command_arg(stage_helper)} stage '
        f'--exe {_msbuild_command_arg(exe_path)} '
        f'--layout {_msbuild_command_arg(layout_dir)} '
        f'--source-dir {_msbuild_command_arg(working_dir)} '
        f'--manifest {_msbuild_command_arg(manifest_path)} '
        f'--artwork-dir {_msbuild_command_arg(artwork_dir)} '
        f'--app-name "{_xml_escape(project_name)}"'
        f'{pdb_args}')
    deploy_invocation = (
        f'{_msbuild_command_arg(xbapp_path)} deploy {_msbuild_command_arg(layout_dir)} '
        f'/S /Drive:development')
    if remote_address:
        deploy_invocation += f' /X:{_xml_escape(remote_address)}'
    stage_command = f'{path_prefix}{stage_invocation}'
    build_command = (
        f'{path_prefix}{_msbuild_command_arg(cmake_path)} --build {_msbuild_command_arg(build_dir)} '
        f'--config "$(Configuration)" --target "{_xml_escape(target_name)}"'
        f' &amp;&amp; {stage_invocation}'
        f' &amp;&amp; {deploy_invocation}')
    symbol_search_path = ';'.join([layout_dir, os.path.dirname(exe_path), target_binary_dir])

    project_path = os.path.join(project_dir, f'{project_name}.vcxproj')
    project_user_path = os.path.join(project_dir, f'{project_name}.vcxproj.user')
    source_item_groups = _source_items_from_project(target_project_path)

    configuration_items = []
    configuration_groups = []
    nmake_groups = []
    user_groups = []
    for configuration in _XBONE_DEBUG_CONFIGURATIONS:
        for platform_name in _XBONE_DEBUG_PROJECT_PLATFORMS:
            condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform_name}'"
            configuration_items.append(f'''    <ProjectConfiguration Include="{configuration}|{platform_name}">
      <Configuration>{configuration}</Configuration>
      <Platform>{platform_name}</Platform>
    </ProjectConfiguration>''')
            configuration_groups.append(f'''  <PropertyGroup Condition="{_xml_escape(condition)}" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType>
    <UseDebugLibraries>{str(configuration == 'Debug').lower()}</UseDebugLibraries>
    <PlatformToolset>{_xml_escape(toolset)}</PlatformToolset>
  </PropertyGroup>''')
            nmake_groups.append(f'''  <PropertyGroup Condition="{_xml_escape(condition)}">
    <DebuggerFlavor>XboxGamingVCppDebugger</DebuggerFlavor>
    <LocalDebuggerDebuggerType>NativeOnly</LocalDebuggerDebuggerType>
    <NMakeBuildCommandLine>{build_command}</NMakeBuildCommandLine>
    <NMakeReBuildCommandLine>{build_command}</NMakeReBuildCommandLine>
    <NMakeCleanCommandLine>@echo Clean skipped.</NMakeCleanCommandLine>
    <NMakeOutput>{_xml_escape(os.path.join(layout_dir, f'{target_name}.exe'))}</NMakeOutput>
  </PropertyGroup>''')
            debug_args = '--use-validation-layers' if configuration == 'Debug' and target_name in _XBONE_DEBUG_ENGINE_TARGETS else ''
            user_groups.append(f'''  <PropertyGroup Condition="{_xml_escape(condition)}">
    <LocalDebuggerCommand>{_xml_escape(exe_path)}</LocalDebuggerCommand>
    <LocalDebuggerCommandArguments>{_xml_escape(debug_args)}</LocalDebuggerCommandArguments>
    <LocalDebuggerWorkingDirectory>{_xml_escape(working_dir)}</LocalDebuggerWorkingDirectory>
    <LocalDebuggerSymbolSearchPath>{_xml_escape(symbol_search_path)}</LocalDebuggerSymbolSearchPath>
    <DeployMode>Push</DeployMode>
    <LogModuleLoads>true</LogModuleLoads>
  </PropertyGroup>''')

    manifest_item_group = ''
    if project_manifest_path:
        manifest_item_group = f'''  <ItemGroup>
    <MGCCompile Include="{_xml_escape(project_manifest_path)}">
      <DefaultApplyTo>true</DefaultApplyTo>
    </MGCCompile>
  </ItemGroup>'''

    pdb_deployment_items = []
    for configuration in _XBONE_DEBUG_CONFIGURATIONS:
        condition = _project_configuration_condition(configuration, _XBONE_VS_PLATFORM)
        pdb_path = os.path.join(project_dir, 'layout', configuration, f'{target_name}.pdb')
        pdb_deployment_items.append(f'''    <None Include="{_xml_escape(pdb_path)}">
      <DeploymentContent Condition="{_xml_escape(condition)}">true</DeploymentContent>
    </None>''')
    pdb_deployment_item_group = f'''  <ItemGroup>
{os.linesep.join(pdb_deployment_items)}
  </ItemGroup>'''

    project_xml = f'''<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
{os.linesep.join(configuration_items)}
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{project_guid}</ProjectGuid>
    <RootNamespace>{_xml_escape(project_name)}</RootNamespace>
    <DefaultLanguage>en-US</DefaultLanguage>
    <Keyword>Win32Proj</Keyword>
    <MinimumVisualStudioVersion>17.0</MinimumVisualStudioVersion>
    <TargetRuntime>Native</TargetRuntime>
    <PreferredToolArchitecture>x64</PreferredToolArchitecture>
    <GDKCrossPlatform>true</GDKCrossPlatform>
    {f'<WindowsTargetPlatformVersion>{_xml_escape(windows_sdk_version)}</WindowsTargetPlatformVersion>' if windows_sdk_version else ''}
  </PropertyGroup>
  <Import Condition="Exists($(ATGBuildProps))" Project="$(ATGBuildProps)" />
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup>
    <GDKCrossPlatformPath Condition="'$(GDKCrossPlatformPath)'==''">$(GameDKCoreLatest)</GDKCrossPlatformPath>
    <GDKCrossPlatformPath Condition="'$(GDKCrossPlatformPath)'==''">$(GameDKXboxLatest)</GDKCrossPlatformPath>
  </PropertyGroup>
{os.linesep.join(configuration_groups)}
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <PropertyGroup>
    <OutDir>{_xml_escape(os.path.join(layout_dir, ''))}</OutDir>
    <IntDir>$(ProjectDir)intermediate\\</IntDir>
    <LayoutDir>{_xml_escape(layout_dir)}</LayoutDir>
    <DeployFromOutDir>true</DeployFromOutDir>
    <RemoveExtraDeployFiles>true</RemoveExtraDeployFiles>
  </PropertyGroup>
{os.linesep.join(nmake_groups)}
{source_item_groups}
{manifest_item_group}
{pdb_deployment_item_group}
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
'''

    user_xml = f'''<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <RemoteAddress>{_xml_escape(remote_address)}</RemoteAddress>
  </PropertyGroup>
{os.linesep.join(user_groups)}
</Project>
'''

    with open(project_path, 'w', encoding='utf-8') as f:
        f.write(project_xml)
    with open(project_user_path, 'w', encoding='utf-8') as f:
        f.write(user_xml)
    _copy_xbone_project_filters(target_project_path, project_path)

    return {
        'name': project_name,
        'path': project_path,
        'guid': project_guid,
        'source_path': os.path.abspath(target_project_path),
        'kind': 'xbone',
        'type_guid': _VC_PROJECT_TYPE_GUID,
        'dependencies': [],
    }


def _folder_sort_key(folder):
    top_level_order = {
        'Engine': 0,
        'Extender': 1,
        'CMake configs': 2,
    }
    top_level = folder.split('/')[0]
    return (top_level_order.get(top_level, 3), folder.lower())


def _solution_item_path(solution_dir, file_path):
    try:
        path = relpath(file_path, solution_dir)
    except ValueError:
        path = file_path
    return path.replace(os.sep, '\\')


def _collect_solution_folder_structure(solution_path, projects, defold_root=None, include_solution_items=False):
    solution_dir = os.path.dirname(os.path.abspath(solution_path))
    folder_files = solution_msvs._engine_source_files(defold_root) if defold_root and include_solution_items else {}
    folder_projects = {}
    folder_names = set()

    for project in projects:
        source_path = project.get('source_path') or project.get('path')
        folder = project.get('folder') or solution_msvs._solution_folder_for_project(source_path)
        folder_projects.setdefault(folder, []).append(project)

    for folder in set(folder_projects) | set(folder_files):
        parts = folder.split('/')
        for i in range(1, len(parts) + 1):
            folder_names.add('/'.join(parts[:i]))

    folders = []
    for folder in sorted(folder_names, key=_folder_sort_key):
        parent = folder.rsplit('/', 1)[0] if '/' in folder else None
        folders.append({
            'name': folder.rsplit('/', 1)[-1],
            'path': folder,
            'guid': _solution_folder_guid(folder),
            'parent': parent,
            'files': [_solution_item_path(solution_dir, path) for path in folder_files.get(folder, [])],
        })

    project_parent_guids = {}
    for folder, folder_project_items in folder_projects.items():
        folder_guid = _solution_folder_guid(folder)
        for project in folder_project_items:
            project_parent_guids[project['guid']] = folder_guid

    folder_parent_guids = {}
    for folder in folders:
        if folder['parent']:
            folder_parent_guids[folder['guid']] = _solution_folder_guid(folder['parent'])

    return folders, project_parent_guids, folder_parent_guids


def _solution_project_configuration(project, configuration):
    if project.get('kind') == 'xbone':
        return f'{configuration}|{_XBONE_VS_PLATFORM}'
    return f'{configuration}|{_XBONE_CMAKE_PROJECT_PLATFORM}'


def _write_xbone_debug_solution(solution_path, projects, defold_root=None):
    solution_dir = os.path.dirname(solution_path)
    folders, project_parent_guids, folder_parent_guids = _collect_solution_folder_structure(
        solution_path,
        projects,
        defold_root,
        include_solution_items=False)
    lines = [
        'Microsoft Visual Studio Solution File, Format Version 12.00',
        '# Visual Studio Version 17',
        'VisualStudioVersion = 17.0.31912.275',
        'MinimumVisualStudioVersion = 10.0.40219.1',
    ]
    for project in projects:
        project_rel = relpath(project['path'], solution_dir).replace(os.sep, '\\')
        lines.append(f'Project("{project.get("type_guid", _VC_PROJECT_TYPE_GUID)}") = "{project["name"]}", "{project_rel}", "{project["guid"]}"')
        dependencies = project.get('dependencies', [])
        if dependencies:
            lines.append('\tProjectSection(ProjectDependencies) = postProject')
            for dependency_guid in dependencies:
                lines.append(f'\t\t{dependency_guid} = {dependency_guid}')
            lines.append('\tEndProjectSection')
        lines.append('EndProject')

    for folder in folders:
        folder_rel = folder['path'].replace('/', '\\')
        lines.append(f'Project("{_SOLUTION_FOLDER_PROJECT_TYPE_GUID}") = "{folder["name"]}", "{folder_rel}", "{folder["guid"]}"')
        if folder['files']:
            lines.append('\tProjectSection(SolutionItems) = preProject')
            for file_path in folder['files']:
                lines.append(f'\t\t{file_path} = {file_path}')
            lines.append('\tEndProjectSection')
        lines.append('EndProject')

    lines.append('Global')
    lines.append('\tGlobalSection(SolutionConfigurationPlatforms) = preSolution')
    platform_name = 'Gaming.Xbox.XboxOne.x64'
    for configuration in _XBONE_DEBUG_CONFIGURATIONS:
        lines.append(f'\t\t{configuration}|{platform_name} = {configuration}|{platform_name}')
    lines.append('\tEndGlobalSection')
    lines.append('\tGlobalSection(ProjectConfigurationPlatforms) = postSolution')
    for project in projects:
        for configuration in _XBONE_DEBUG_CONFIGURATIONS:
            project_configuration = _solution_project_configuration(project, configuration)
            lines.append(f'\t\t{project["guid"]}.{configuration}|{platform_name}.ActiveCfg = {project_configuration}')
            lines.append(f'\t\t{project["guid"]}.{configuration}|{platform_name}.Build.0 = {project_configuration}')
            if project.get('kind') == 'xbone':
                lines.append(f'\t\t{project["guid"]}.{configuration}|{platform_name}.Deploy.0 = {project_configuration}')
    lines.append('\tEndGlobalSection')
    nested_items = {}
    nested_items.update(folder_parent_guids)
    nested_items.update(project_parent_guids)
    if nested_items:
        lines.append('\tGlobalSection(NestedProjects) = preSolution')
        for child_guid in sorted(nested_items, key=lambda guid: guid.upper()):
            lines.append(f'\t\t{child_guid} = {nested_items[child_guid]}')
        lines.append('\tEndGlobalSection')
    lines.append('\tGlobalSection(SolutionProperties) = preSolution')
    lines.append('\t\tHideSolutionNode = FALSE')
    lines.append('\tEndGlobalSection')
    lines.append('EndGlobal')

    with open(solution_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
        f.write('\n')


def _cleanup_xbone_debug_projects_from_slnx(solution_path):
    if not solution_path or not solution_path.endswith('.slnx') or not os.path.exists(solution_path):
        return False

    tree = ElementTree.parse(solution_path)
    root = tree.getroot()
    changed = False

    configurations = _xml_find_child(root, 'Configurations')
    if configurations is not None:
        for child in list(configurations):
            if child.tag == 'Platform' and child.get('Name') == _XBONE_VS_PLATFORM:
                configurations.remove(child)
                changed = True

    for project in root.iter('Project'):
        project_path = project.get('Path', '').replace('\\', '/')
        if project_path.startswith('xbone-debug/'):
            if project.attrib.pop('DefaultStartup', None) is not None:
                changed = True

    for folder in list(root):
        if folder.tag == 'Folder' and folder.get('Name') == '/Xbox Debug/':
            root.remove(folder)
            changed = True

    if changed:
        try:
            ElementTree.indent(tree, space='\t')
        except AttributeError:
            pass
        tree.write(solution_path, encoding='utf-8', xml_declaration=True)
    return changed


def _slnx_project_reference(solution_dir, project_path):
    try:
        return relpath(project_path, solution_dir).replace(os.sep, '/')
    except ValueError:
        return os.path.abspath(project_path).replace('\\', '/')


def _replace_xbone_slnx_projects(solution_path, source_slnx_path, debug_projects_by_source, log):
    if not source_slnx_path or not source_slnx_path.endswith('.slnx') or not os.path.exists(source_slnx_path):
        return False

    try:
        tree = ElementTree.parse(source_slnx_path)
    except Exception as e:
        log(f'Warning: Failed to parse organized Visual Studio solution for Xbox rewrite: {e}')
        return False

    solution_dir = os.path.dirname(os.path.abspath(solution_path))
    source_solution_dir = os.path.dirname(os.path.abspath(source_slnx_path))
    root = tree.getroot()
    configurations = _xml_find_child(root, 'Configurations')
    if configurations is not None:
        for child in list(configurations):
            if child.tag == 'Platform' and child.get('Name') != _XBONE_VS_PLATFORM:
                configurations.remove(child)
        existing_platforms = {child.get('Name') for child in configurations if child.tag == 'Platform'}
        if _XBONE_VS_PLATFORM not in existing_platforms:
            configurations.insert(0, ElementTree.Element('Platform', {'Name': _XBONE_VS_PLATFORM}))

    replacement_paths = {}
    for source_path, debug_project in debug_projects_by_source.items():
        replacement_paths[source_path] = _slnx_project_reference(solution_dir, debug_project['path'])

    replaced_projects = 0
    for project in root.iter('Project'):
        project_path = project.get('Path')
        resolved_path = _resolve_solution_project_path(source_solution_dir, project_path)
        source_key = normpath(resolved_path).lower() if resolved_path else None
        replacement_path = replacement_paths.get(source_key)
        project.attrib.pop('DefaultStartup', None)
        if replacement_path:
            debug_project = debug_projects_by_source[source_key]
            project.set('Path', replacement_path)
            project.set('Type', _VC_PROJECT_TYPE_GUID.strip('{}').lower())
            project.set('Id', debug_project['guid'].strip('{}').lower())
            if debug_project['name'] == 'dmengine':
                project.set('DefaultStartup', 'true')
            for dependency in list(project):
                if dependency.tag == 'BuildDependency':
                    project.remove(dependency)
            replaced_projects += 1
        elif resolved_path:
            project.set('Path', _slnx_project_reference(solution_dir, resolved_path))

        for dependency in project.iter('BuildDependency'):
            dependency_path = dependency.get('Project')
            resolved_dependency = _resolve_solution_project_path(source_solution_dir, dependency_path)
            dependency_key = normpath(resolved_dependency).lower() if resolved_dependency else None
            replacement_dependency_path = replacement_paths.get(dependency_key)
            if replacement_dependency_path:
                dependency.set('Project', replacement_dependency_path)
            elif resolved_dependency:
                dependency.set('Project', _slnx_project_reference(solution_dir, resolved_dependency))

    try:
        ElementTree.indent(tree, space='\t')
    except AttributeError:
        pass
    tree.write(solution_path, encoding='utf-8', xml_declaration=True)
    log(f'Xbox solution generated from organized Visual Studio solution: {solution_path} ({replaced_projects} launchable projects)')
    return True


def generate_xbone_solution(solution_dir, build_dir, target_name, private_repo_root, defold_root, log, solution_path=None, visual_studio_generator=None, windows_sdk_version=None):
    if not private_repo_root:
        log('Warning: Xbox solution skipped because no private Xbox repository root is configured')
        return None

    cmake_projects = _collect_xbone_cmake_solution_projects(solution_path)
    target_projects = _find_xbone_debug_target_projects(build_dir, solution_path)
    if not target_projects:
        log('Warning: Xbox solution skipped because no dmengine/test Visual Studio projects were found')
        return None

    aliased_projects = 0
    for cmake_project in cmake_projects:
        if _add_xbone_project_configuration_aliases(cmake_project['path']):
            aliased_projects += 1
    if aliased_projects:
        log(f'Added Xbox Visual Studio platform aliases to {aliased_projects} CMake projects')

    solution_dir = os.path.abspath(solution_dir)
    project_root = os.path.join(solution_dir, 'xbox')
    old_project_root = os.path.join(solution_dir, 'xbone-debug')
    if os.path.basename(old_project_root) == 'xbone-debug' and os.path.isdir(old_project_root):
        shutil.rmtree(old_project_root)
    os.makedirs(project_root, exist_ok=True)

    toolset = platform_toolset(visual_studio_generator)
    debug_projects_by_source = {}
    for target, project_path in target_projects:
        project_name = target
        project_dir = os.path.join(project_root, project_name)
        debug_project = _write_xbone_debug_project(
            project_dir,
            project_name,
            target,
            project_path,
            os.path.abspath(build_dir),
            os.path.abspath(private_repo_root),
            os.path.abspath(defold_root),
            toolset,
            windows_sdk_version)
        debug_projects_by_source[normpath(os.path.abspath(project_path)).lower()] = debug_project

    projects = []
    added_project_guids = set()
    guid_replacements = {}
    for cmake_project in cmake_projects:
        replacement = debug_projects_by_source.get(normpath(cmake_project['source_path']).lower())
        if replacement:
            guid_replacements[cmake_project['guid']] = replacement['guid']

    def add_project(project):
        if project['guid'] in added_project_guids:
            return
        added_project_guids.add(project['guid'])
        projects.append(project)

    dmengine_project = None
    for debug_project in debug_projects_by_source.values():
        if debug_project['name'] == 'dmengine':
            dmengine_project = debug_project
            break
    if dmengine_project:
        add_project(dmengine_project)

    if cmake_projects:
        for cmake_project in cmake_projects:
            replacement = debug_projects_by_source.get(normpath(cmake_project['source_path']).lower())
            if replacement:
                project = dict(replacement)
                project['folder'] = cmake_project.get('folder')
            else:
                project = cmake_project
            if project.get('kind') == 'cmake':
                project = dict(project)
                project['dependencies'] = sorted(set(guid_replacements.get(guid, guid) for guid in project.get('dependencies', [])))
            add_project(project)
    else:
        for debug_project in sorted(debug_projects_by_source.values(), key=lambda project: (0 if project['name'] in _XBONE_DEBUG_ENGINE_TARGETS else 1, project['name'].lower())):
            add_project(debug_project)

    solution_file = os.path.join(solution_dir, f'{target_name}.slnx')
    stale_paths = [
        os.path.join(solution_dir, f'{target_name}.sln'),
        os.path.join(solution_dir, f'{target_name}-debug.sln'),
    ]
    for stale_path in stale_paths:
        if stale_path != solution_file and os.path.exists(stale_path):
            try:
                os.remove(stale_path)
            except Exception as e:
                log(f'Warning: Failed to remove stale Xbox solution {stale_path}: {e}')

    if not _replace_xbone_slnx_projects(solution_file, solution_path, debug_projects_by_source, log):
        solution_file = os.path.join(solution_dir, f'{target_name}.sln')
        _write_xbone_debug_solution(solution_file, projects, os.path.abspath(defold_root))
        log(f'Xbox solution generated: {solution_file}')
    return solution_file


def generate_xbone_debug_solution(build_dir, target_name, private_repo_root, defold_root, log, solution_path=None, visual_studio_generator=None, windows_sdk_version=None):
    return generate_xbone_solution(
        build_dir,
        build_dir,
        target_name,
        private_repo_root,
        defold_root,
        log,
        solution_path,
        visual_studio_generator,
        windows_sdk_version)
