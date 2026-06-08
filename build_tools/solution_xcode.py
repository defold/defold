#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.

import getpass
import os
import plistlib
import re
from os.path import join, relpath
from xml.sax.saxutils import quoteattr

_STALE_SHARED_SCHEME_NAMES = ('engine', 'all tests')


def configure_project(xcode_project_path, build_configuration, target_platform, defold_home, defold_path, dynamo_home, log=None):
    _organize_project_outline(xcode_project_path)
    _write_scheme_management(xcode_project_path, build_configuration, defold_home, defold_path, dynamo_home, log)
    _warn_missing_cmake_sdk_headers(target_platform, dynamo_home, log)


def _log(log, msg):
    if log:
        log(msg)


def _mkdirs(path):
    if not os.path.exists(path):
        os.makedirs(path)


def _warn_missing_cmake_sdk_headers(target_platform, dynamo_home, log):
    required_headers = [
        join(dynamo_home, 'include', 'dlib', 'profile.h'),
        join(dynamo_home, 'include', 'graphics', 'graphics.h'),
        join(dynamo_home, 'include', 'hid', 'hid.h'),
        join(dynamo_home, 'include', 'input', 'input.h'),
        join(dynamo_home, 'include', 'particle', 'particle.h'),
        join(dynamo_home, 'sdk', 'include', 'dmsdk', 'platform', 'window.h'),
    ]
    missing_headers = [
        relpath(path, dynamo_home)
        for path in required_headers
        if not os.path.exists(path)
    ]
    if not missing_headers:
        return

    _log(
        log,
        'Note: DYNAMO_HOME SDK headers are currently incomplete. '
        'Xcode dependency builds install modeled SDK headers into DYNAMO_HOME before dependent targets compile. '
        'Missing examples before build: %s. build_engine for %s will also populate %s.'
        % (', '.join(missing_headers), target_platform, dynamo_home))


def _group_pattern(group_id):
    return re.compile(
        r'^(\t\t%s(?: /\* [^\n]*? \*/)? = \{\n.*?\n\t\t\};)' % re.escape(group_id),
        re.DOTALL | re.MULTILINE)


def _child_ref(child_line):
    match = re.search(r'([A-F0-9]{24})(?: /\* (.*?) \*/)?', child_line)
    if not match:
        return None, ''
    return match.group(1), match.group(2) or ''


def _sort_children_by_name(child_lines):
    return sorted(
        child_lines,
        key=lambda child_line: (
            _child_ref(child_line)[1].lower(),
            _child_ref(child_line)[0] or ''))


def _get_group_children(pbxproj, group_id):
    group_match = _group_pattern(group_id).search(pbxproj)
    if not group_match:
        return []

    children_match = re.search(r'(\t\t\tchildren = \(\n)(.*?)(\t\t\t\);\n)', group_match.group(1), re.DOTALL)
    if not children_match:
        return []

    return [line for line in children_match.group(2).splitlines() if line.strip()]


def _update_group_children(pbxproj, group_id, update_children):
    group_match = _group_pattern(group_id).search(pbxproj)
    if not group_match:
        return pbxproj, False

    block = group_match.group(1)
    children_match = re.search(r'(\t\t\tchildren = \(\n)(.*?)(\t\t\t\);\n)', block, re.DOTALL)
    if not children_match:
        return pbxproj, False

    children = [line for line in children_match.group(2).splitlines() if line.strip()]
    updated_children = update_children(children)
    if children == updated_children:
        return pbxproj, False

    updated_children_body = ''.join('%s\n' % child_line for child_line in updated_children)
    updated_block = (
        block[:children_match.start(2)] +
        updated_children_body +
        block[children_match.end(2):])
    updated_pbxproj = (
        pbxproj[:group_match.start(1)] +
        updated_block +
        pbxproj[group_match.end(1):])
    return updated_pbxproj, True


def _organize_project_outline(xcode_project_path):
    pbxproj_path = os.path.join(xcode_project_path, 'project.pbxproj')
    if not os.path.exists(pbxproj_path):
        return

    with open(pbxproj_path, 'r', encoding='utf-8') as f:
        pbxproj = f.read()

    main_group_match = re.search(r'\n\s*mainGroup = ([A-F0-9]{24});', pbxproj)
    if not main_group_match:
        return

    outline = {
        'engine_group_id': None,
        'cmake_group_id': None,
        'all_build_lines': [],
        'engine_module_group_ids': [],
    }

    def organize_root(children):
        engine_lines = []
        cmake_lines = []
        other_lines = []

        for child_line in children:
            child_id, child_name = _child_ref(child_line)
            if child_name == 'Engine':
                outline['engine_group_id'] = child_id
                engine_lines.append(child_line)
            elif child_name == 'CMake configs':
                outline['cmake_group_id'] = child_id
                cmake_lines.append(child_line)
            elif child_name == 'ALL_BUILD':
                outline['all_build_lines'].append(child_line)
            else:
                other_lines.append(child_line)

        return engine_lines + cmake_lines + other_lines

    pbxproj, changed = _update_group_children(pbxproj, main_group_match.group(1), organize_root)

    if outline['engine_group_id']:
        def organize_engine(children):
            sorted_children = _sort_children_by_name(children)
            outline['engine_module_group_ids'] = [_child_ref(child_line)[0] for child_line in sorted_children]
            return sorted_children

        pbxproj, engine_changed = _update_group_children(pbxproj, outline['engine_group_id'], organize_engine)
        changed = changed or engine_changed

    if outline['cmake_group_id']:
        def organize_cmake_configs(children):
            existing_ids = set(_child_ref(child_line)[0] for child_line in children)
            updated_children = list(children)
            for child_line in outline['all_build_lines']:
                child_id, _child_name = _child_ref(child_line)
                if child_id not in existing_ids:
                    updated_children.append(child_line)
            return _sort_children_by_name(updated_children)

        pbxproj, cmake_changed = _update_group_children(pbxproj, outline['cmake_group_id'], organize_cmake_configs)
        changed = changed or cmake_changed

    for module_group_id in outline['engine_module_group_ids']:
        module_children = _get_group_children(pbxproj, module_group_id)
        if not module_children:
            continue

        def organize_module(children):
            sources = []
            tests = []
            other = []
            for child_line in children:
                _child_id, child_name = _child_ref(child_line)
                if child_name in ('source', 'sources'):
                    sources.append(child_line)
                elif child_name in ('test', 'tests'):
                    tests.append(child_line)
                else:
                    other.append(child_line)
            return sources + tests + _sort_children_by_name(other)

        pbxproj, module_changed = _update_group_children(pbxproj, module_group_id, organize_module)
        changed = changed or module_changed

    if changed:
        with open(pbxproj_path, 'w', encoding='utf-8') as f:
            f.write(pbxproj)


def _write_scheme_management(xcode_project_path, build_configuration, defold_home, defold_path, dynamo_home, log):
    pbxproj_path = os.path.join(xcode_project_path, 'project.pbxproj')
    if not os.path.exists(pbxproj_path):
        return

    with open(pbxproj_path, 'r', encoding='utf-8') as f:
        pbxproj = f.read()

    target_ids = _get_target_ids(pbxproj)
    targets = _get_schemeable_targets(pbxproj)
    test_scheme_metadata = _read_test_scheme_metadata(xcode_project_path)
    for target in targets:
        if target['name'] in test_scheme_metadata:
            target.update(test_scheme_metadata[target['name']])
    scheme_targets = _get_scheme_targets(targets)

    if scheme_targets:
        _write_shared_schemes(xcode_project_path, scheme_targets, build_configuration, defold_home, defold_path, dynamo_home)

    if not target_ids:
        return

    scheme_names = [target.get('scheme_name', target['name']) for target in scheme_targets]
    _write_scheme_user_state(xcode_project_path, target_ids, scheme_names)

    _log(
        log,
        'Xcode schemes generated for %d targets; automatic scheme creation suppressed for %d targets.'
        % (len(scheme_targets), len(target_ids)))


def _read_test_scheme_metadata(xcode_project_path):
    metadata_path = os.path.join(os.path.dirname(xcode_project_path), 'defold_xcode_test_schemes.tsv')
    metadata = {}
    if not os.path.exists(metadata_path):
        return metadata

    with open(metadata_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            parts = line.split('\t', 1)
            if len(parts) != 2:
                continue
            target_name, working_directory = parts
            metadata[target_name] = {
                'working_directory': working_directory,
            }
    return metadata


def _get_target_ids(pbxproj):
    return sorted(set(re.findall(
        r'^\s*([A-F0-9]{24}) /\* .* \*/ = \{\n\s*isa = PBX(?:Aggregate|Legacy|Native)Target;',
        pbxproj,
        re.MULTILINE)))


def _get_schemeable_targets(pbxproj):
    return _get_targets_in_section(pbxproj, 'PBXNativeTarget') + _get_targets_in_section(pbxproj, 'PBXAggregateTarget')


def _get_targets_in_section(pbxproj, section_name):
    section_match = re.search(
        r'/\* Begin %s section \*/(.*?)/\* End %s section \*/' % (section_name, section_name),
        pbxproj,
        re.DOTALL)
    if not section_match:
        return []

    targets = []
    target_pattern = re.compile(r'\n\s*([A-F0-9]{24}) /\* (.*?) \*/ = \{(.*?)\n\s*\};', re.DOTALL)
    for match in target_pattern.finditer(section_match.group(1)):
        target_id, comment, body = match.groups()
        name_match = re.search(r'\n\s*name = ([^;]+);', body)
        product_name_match = re.search(r'\n\s*productName = ([^;]+);', body)
        product_type_match = re.search(r'\n\s*productType = "?([^;"\n]+)"?;', body)
        product_reference_match = re.search(r'\n\s*productReference = [A-F0-9]{24} /\* (.*?) \*/;', body)

        name = name_match.group(1) if name_match else comment
        product_name = product_name_match.group(1) if product_name_match else name
        buildable_name = product_reference_match.group(1) if product_reference_match else product_name

        targets.append({
            'id': target_id,
            'name': name,
            'buildable_name': buildable_name,
            'target_type': section_name,
            'product_type': product_type_match.group(1) if product_type_match else '',
        })

    return targets


def _get_scheme_targets(targets):
    dmengine_targets = [
        target for target in targets
        if target['name'] == 'dmengine' and target['product_type'] == 'com.apple.product-type.tool'
    ]
    test_targets = sorted([
        target for target in targets
        if target['name'].startswith('test_') and target['product_type'] == 'com.apple.product-type.tool'
    ], key=lambda target: target['name'])
    run_tests_targets = [
        target for target in targets
        if target['name'] == 'run_tests' and target['target_type'] == 'PBXAggregateTarget'
    ]
    scheme_targets = []
    if dmengine_targets:
        dmengine_scheme_target = dict(dmengine_targets[0])
        dmengine_scheme_target['environment'] = [('DM_QUIT_ON_ESC', '1')]
        scheme_targets.append(dmengine_scheme_target)
        if run_tests_targets:
            run_tests_scheme_target = dict(run_tests_targets[0])
            run_tests_scheme_target['scheme_name'] = 'Run Tests'
            run_tests_scheme_target['build_only'] = True
            scheme_targets.append(run_tests_scheme_target)
    elif run_tests_targets:
        run_tests_scheme_target = dict(run_tests_targets[0])
        run_tests_scheme_target['scheme_name'] = 'Run Tests'
        run_tests_scheme_target['build_only'] = True
        scheme_targets.append(run_tests_scheme_target)
    scheme_targets += test_targets
    return scheme_targets


def _write_shared_schemes(xcode_project_path, targets, build_configuration, defold_home, defold_path, dynamo_home):
    scheme_dir = os.path.join(xcode_project_path, 'xcshareddata', 'xcschemes')
    _mkdirs(scheme_dir)

    project_container = os.path.abspath(xcode_project_path)
    scheme_names = []
    for target in targets:
        scheme_name = target.get('scheme_name', target['name'])
        scheme_names.append(scheme_name)
        scheme_path = os.path.join(scheme_dir, f"{scheme_name}.xcscheme")
        with open(scheme_path, 'w', encoding='utf-8') as f:
            f.write(_make_scheme(project_container, target, build_configuration, defold_home, defold_path, dynamo_home))
    _remove_stale_shared_schemes(scheme_dir, scheme_names)


def _remove_stale_shared_schemes(scheme_dir, scheme_names):
    desired_scheme_files = set(f'{scheme_name}.xcscheme' for scheme_name in scheme_names)
    for stale_scheme_name in _STALE_SHARED_SCHEME_NAMES:
        stale_scheme_file = f'{stale_scheme_name}.xcscheme'
        if stale_scheme_file in desired_scheme_files:
            continue
        stale_scheme_path = os.path.join(scheme_dir, stale_scheme_file)
        if os.path.exists(stale_scheme_path):
            os.unlink(stale_scheme_path)


def _write_scheme_user_state(xcode_project_path, target_ids, scheme_names):
    user_name = getpass.getuser()
    scheme_dir = os.path.join(xcode_project_path, 'xcuserdata', f'{user_name}.xcuserdatad', 'xcschemes')
    _mkdirs(scheme_dir)

    plist_path = os.path.join(scheme_dir, 'xcschememanagement.plist')
    plist_data = {}
    if os.path.exists(plist_path):
        with open(plist_path, 'rb') as f:
            plist_data = plistlib.load(f)

    scheme_user_state = plist_data.setdefault('SchemeUserState', {})
    desired_shared_scheme_keys = set(f'{scheme_name}.xcscheme_^#shared#^_' for scheme_name in scheme_names)
    for stale_scheme_name in _STALE_SHARED_SCHEME_NAMES:
        stale_key = f'{stale_scheme_name}.xcscheme_^#shared#^_'
        if stale_key not in desired_shared_scheme_keys:
            scheme_user_state.pop(stale_key, None)
    for order_hint, scheme_name in enumerate(scheme_names):
        scheme_user_state[f'{scheme_name}.xcscheme_^#shared#^_'] = {'orderHint': order_hint}

    plist_data['SuppressBuildableAutocreation'] = dict((target_id, {'primary': True}) for target_id in target_ids)

    with open(plist_path, 'wb') as f:
        plistlib.dump(plist_data, f)


def _buildable_reference(project_container, target, indentation):
    return '\n'.join([
        f'{indentation}<BuildableReference',
        f'{indentation}   BuildableIdentifier = "primary"',
        f'{indentation}   BlueprintIdentifier = {quoteattr(target["id"])}',
        f'{indentation}   BuildableName = {quoteattr(target["buildable_name"])}',
        f'{indentation}   BlueprintName = {quoteattr(target["name"])}',
        f'{indentation}   ReferencedContainer = {quoteattr(f"container:{project_container}")}>',
        f'{indentation}</BuildableReference>',
    ])


def _scheme_environment_variables(indentation, defold_home, defold_path, dynamo_home, extra_environment=None):
    pythonpath = os.path.pathsep.join([
        '%s/lib/python' % dynamo_home,
        '%s/build_tools' % defold_path,
        '%s/ext/lib/python' % dynamo_home,
    ])
    env = [
        ('DEFOLD_HOME', defold_home),
        ('DYNAMO_HOME', dynamo_home),
        ('PYTHONPATH', pythonpath),
    ]
    if extra_environment:
        env.extend(extra_environment)

    lines = [f'{indentation}<EnvironmentVariables>']
    for key, value in env:
        lines.extend([
            f'{indentation}   <EnvironmentVariable',
            f'{indentation}      key = {quoteattr(key)}',
            f'{indentation}      value = {quoteattr(value)}',
            f'{indentation}      isEnabled = "YES">',
            f'{indentation}   </EnvironmentVariable>',
        ])
    lines.append(f'{indentation}</EnvironmentVariables>')
    return '\n'.join(lines)


def _build_action_entry(project_container, target):
    buildable_reference = _buildable_reference(project_container, target, '               ')
    return f'''         <BuildActionEntry
            buildForTesting = "YES"
            buildForRunning = "YES"
            buildForProfiling = "YES"
            buildForArchiving = "YES"
            buildForAnalyzing = "YES">
{buildable_reference}
         </BuildActionEntry>'''


def _make_scheme(project_container, target, build_configuration, defold_home, defold_path, dynamo_home):
    build_targets = target.get('build_targets', [target])
    build_action_entries = '\n'.join(_build_action_entry(project_container, build_target) for build_target in build_targets)
    runnable_reference = _buildable_reference(project_container, target, '            ')
    build_configuration_attr = quoteattr(build_configuration)
    parallelize_buildables = target.get('parallelize_buildables', 'YES')
    build_only = target.get('build_only', False)
    is_runnable = target['product_type'] == 'com.apple.product-type.tool' and not build_only
    working_directory = target.get('working_directory')
    if working_directory:
        launch_working_directory_attributes = '\n'.join([
            '      useCustomWorkingDirectory = "YES"',
            f'      customWorkingDirectory = {quoteattr(working_directory)}'])
    else:
        launch_working_directory_attributes = '      useCustomWorkingDirectory = "NO"'
    scheme_environment_variables = _scheme_environment_variables(
        '      ',
        defold_home,
        defold_path,
        dynamo_home,
        target.get('environment')) if is_runnable else ''
    launch_runnable = ''
    profile_runnable = ''
    if is_runnable:
        launch_runnable = f'''      <BuildableProductRunnable
         runnableDebuggingMode = "0">
{runnable_reference}
      </BuildableProductRunnable>
'''
        profile_runnable = f'''      <BuildableProductRunnable
         runnableDebuggingMode = "0">
{runnable_reference}
      </BuildableProductRunnable>
'''
    elif target.get('launch_path'):
        launch_path_attr = quoteattr(target['launch_path'])
        launch_runnable = f'''      <PathRunnable
         runnableDebuggingMode = "0"
         FilePath = {launch_path_attr}>
      </PathRunnable>
'''
        profile_runnable = f'''      <PathRunnable
         runnableDebuggingMode = "0"
         FilePath = {launch_path_attr}>
      </PathRunnable>
'''
    launch_actions = ''
    if not build_only:
        launch_actions = f'''   <LaunchAction
      buildConfiguration = {build_configuration_attr}
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      launchStyle = "0"
{launch_working_directory_attributes}
      ignoresPersistentStateOnLaunch = "NO"
      debugDocumentVersioning = "YES"
      debugServiceExtension = "internal"
      allowLocationSimulation = "YES">
{scheme_environment_variables}
{launch_runnable.rstrip()}
   </LaunchAction>
   <ProfileAction
      buildConfiguration = {build_configuration_attr}
      shouldUseLaunchSchemeArgsEnv = "YES"
      savedToolIdentifier = ""
{launch_working_directory_attributes}
      debugDocumentVersioning = "YES">
{scheme_environment_variables}
{profile_runnable.rstrip()}
   </ProfileAction>
'''

    return f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme
   LastUpgradeVersion = "1600"
   version = "1.3">
   <BuildAction
      parallelizeBuildables = {quoteattr(parallelize_buildables)}
      buildImplicitDependencies = "YES">
      <BuildActionEntries>
{build_action_entries}
      </BuildActionEntries>
   </BuildAction>
   <TestAction
      buildConfiguration = {build_configuration_attr}
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      shouldUseLaunchSchemeArgsEnv = "YES">
      <Testables>
      </Testables>
   </TestAction>
{launch_actions.rstrip()}
   <AnalyzeAction
      buildConfiguration = {build_configuration_attr}>
   </AnalyzeAction>
   <ArchiveAction
      buildConfiguration = {build_configuration_attr}
      revealArchiveInOrganizer = "YES">
   </ArchiveAction>
</Scheme>
'''
