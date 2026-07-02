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

import argparse
import datetime
import glob
import hashlib
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass


IOS_TEST_CONTAINER_ROOT = 'Documents/defold-tests'
IOS_DEFAULT_BUNDLE_ID_PREFIX = 'com.defold.tests'
IOS_TEST_AUTO_EXIT_ENV = 'DEFOLD_TEST_AUTO_EXIT'
IOS_TEST_READY_PATHS_ENV = 'DEFOLD_TEST_READY_PATHS'
IOS_DEVICECTL_TIMEOUT_ENV = 'IOS_DEVICECTL_TIMEOUT'
IOS_DEVICECTL_RETRIES_ENV = 'IOS_DEVICECTL_RETRIES'
IOS_DEFAULT_DEVICECTL_TIMEOUT = 60
IOS_DEFAULT_DEVICECTL_RETRIES = 2
IOS_DEVICECTL_RETRY_DELAY_SECONDS = 2.0
IOS_DEVICECTL_PROCESS_TIMEOUT_MARGIN_SECONDS = 15
IOS_PROVISIONING_PROFILE_DIR = os.path.expanduser('~/Library/MobileDevice/Provisioning Profiles')
IOS_DEVICE_PLATFORM = 'arm64-ios'
IOS_SIMULATOR_PLATFORM = 'x86_64-ios'
IOS_TEST_PLATFORMS = (IOS_DEVICE_PLATFORM, IOS_SIMULATOR_PLATFORM)
IOS_XCODE_SOLUTION_DEFAULT_BUNDLE_ID = 'com.defold.dmengine'
IOS_XCODE_UNSIGNED_WARNING = (
    'Warning: generating an arm64-ios Xcode solution without --ios-mobileprovision or IOS_MOBILEPROVISION. '
    'The generated dmengine app is not configured for physical-device signing, so launching it from Xcode '
    'may fail with a "not codesigned" error. Configure signing in Xcode before running dmengine on a device.')


class IOSTestError(RuntimeError):
    pass


@dataclass
class IOSDevice(object):
    device_id: str
    udid: str
    name: str
    platform: str
    raw: dict


@dataclass
class IOSSimulator(object):
    udid: str
    name: str
    runtime: str
    state: str
    raw: dict


@dataclass
class IOSCodeSignIdentity(object):
    sha1: str
    name: str

    def display(self):
        return '%s "%s"' % (self.sha1, self.name)


@dataclass
class IOSDevelopmentTeam(object):
    team_id: str
    name: str
    profile_count: int
    ios_profile_count: int
    valid_ios_profile_count: int


@dataclass
class IOSProvisioningProfile(object):
    path: str
    name: str
    uuid: str
    team_ids: list
    platforms: list
    expiration: datetime.datetime
    provisioned_devices: list
    entitlements: dict
    developer_certificate_sha1s: set
    team_name: str = ''

    def display(self):
        return '%s (%s, %s)' % (self.name or os.path.basename(self.path), self.uuid or 'no-uuid', self.path)


def _copy_env(env=None):
    if env is None:
        return dict(os.environ)
    return dict(env)


def _log(log_fn, message):
    if log_fn:
        log_fn(message)


def _to_string_list(value):
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        return [str(v) for v in value if v]
    return [str(value)]


def _env_positive_int(env, name, default):
    value = env.get(name)
    if not value:
        return default
    try:
        parsed = int(value)
    except ValueError as e:
        raise IOSTestError('%s must be a positive integer, got %s' % (name, value)) from e
    if parsed <= 0:
        raise IOSTestError('%s must be a positive integer, got %s' % (name, value))
    return parsed


def _env_nonnegative_int(env, name, default):
    value = env.get(name)
    if not value:
        return default
    try:
        parsed = int(value)
    except ValueError as e:
        raise IOSTestError('%s must be a non-negative integer, got %s' % (name, value)) from e
    if parsed < 0:
        raise IOSTestError('%s must be a non-negative integer, got %s' % (name, value))
    return parsed


def _sanitize_bundle_component(value):
    value = os.path.splitext(os.path.basename(value or 'test'))[0].lower()
    value = re.sub(r'[^a-z0-9-]+', '-', value)
    value = value.strip('-')
    if not value:
        value = 'test'
    if not re.match(r'^[a-z]', value):
        value = 't%s' % value
    return value


def make_bundle_id(target_name=None, program=None, bundle_id_prefix=None, env=None):
    env = _copy_env(env)
    prefix = bundle_id_prefix or env.get('IOS_TEST_BUNDLE_ID_PREFIX') or IOS_DEFAULT_BUNDLE_ID_PREFIX
    suffix_source = target_name or program or 'test'
    return '%s.%s' % (prefix.rstrip('.'), _sanitize_bundle_component(suffix_source))


def is_ios_test_platform(target_platform):
    return target_platform in IOS_TEST_PLATFORMS


def ios_test_cmake_args(target_platform, bundle_id_prefix=None, env=None):
    if not is_ios_test_platform(target_platform):
        return []

    env = _copy_env(env)
    bundle_id_prefix = bundle_id_prefix or env.get('IOS_TEST_BUNDLE_ID_PREFIX')
    if not bundle_id_prefix:
        return []

    return ['-DDEFOLD_IOS_TEST_BUNDLE_ID_PREFIX:STRING=%s' % bundle_id_prefix]


def apply_build_options_to_env(env, target_platform, test_device=None, identity=None,
                               mobileprovision=None, team_id=None, bundle_id_prefix=None):
    if test_device and is_ios_test_platform(target_platform):
        env['IOS_DEVICE_ID'] = test_device
    if test_device and target_platform == IOS_SIMULATOR_PLATFORM:
        env['IOS_SIMULATOR_ID'] = test_device
    if identity:
        env['IOS_CODESIGN_IDENTITY'] = identity
    if mobileprovision:
        env['IOS_MOBILEPROVISION'] = mobileprovision
    if team_id:
        env['IOS_DEVELOPMENT_TEAM'] = team_id
    if bundle_id_prefix:
        env['IOS_TEST_BUNDLE_ID_PREFIX'] = bundle_id_prefix
    return env


def _run_command(command_runner, cmd, env=None, capture=True, timeout=None):
    kwargs = {
        'env': env,
        'check': False,
    }
    if timeout is not None:
        kwargs['timeout'] = timeout
    if capture:
        kwargs['stdout'] = subprocess.PIPE
        kwargs['stderr'] = subprocess.PIPE
    return command_runner(cmd, **kwargs)


def _completed_output(result):
    out = getattr(result, 'stdout', b'') or b''
    if isinstance(out, str):
        return out.encode('utf-8')
    return out


def _completed_error(result):
    err = getattr(result, 'stderr', b'') or b''
    if isinstance(err, bytes):
        return err.decode('utf-8', errors='replace')
    return err


def _load_plist_bytes(data):
    data = data.strip()
    if data.startswith(b'<?xml') or data.startswith(b'bplist'):
        return plistlib.loads(data)

    xml_start = data.find(b'<?xml')
    if xml_start != -1:
        return plistlib.loads(data[xml_start:])

    bplist_start = data.find(b'bplist')
    if bplist_start != -1:
        return plistlib.loads(data[bplist_start:])

    return plistlib.loads(data)


def decode_mobileprovision(path, command_runner=subprocess.run, env=None):
    env = _copy_env(env)
    commands = [
        ['security', 'cms', '-D', '-i', path],
        ['openssl', 'smime', '-inform', 'DER', '-verify', '-noverify', '-in', path],
        ['openssl', 'cms', '-inform', 'DER', '-verify', '-noverify', '-in', path],
    ]
    errors = []
    for cmd in commands:
        try:
            result = _run_command(command_runner, cmd, env=env, capture=True)
        except OSError as e:
            errors.append('%s: %s' % (cmd[0], e))
            continue

        output = _completed_output(result)
        if getattr(result, 'returncode', 1) == 0 and output:
            try:
                return _load_plist_bytes(output)
            except Exception as e:
                errors.append('%s: decoded output was not a plist: %s' % (' '.join(cmd), e))
                continue

        err = _completed_error(result).strip()
        errors.append('%s: exit %s%s' % (' '.join(cmd), getattr(result, 'returncode', '?'), (': ' + err) if err else ''))

    raise IOSTestError('Failed to decode provisioning profile %s\n%s' % (path, '\n'.join(errors)))


def parse_mobileprovision(path, command_runner=subprocess.run, env=None):
    data = decode_mobileprovision(path, command_runner=command_runner, env=env)
    certificates = data.get('DeveloperCertificates') or []
    certificate_sha1s = set()
    for cert in certificates:
        if isinstance(cert, str):
            cert = cert.encode('utf-8')
        certificate_sha1s.add(hashlib.sha1(cert).hexdigest().upper())

    return IOSProvisioningProfile(
        path=path,
        name=data.get('Name', ''),
        uuid=data.get('UUID', ''),
        team_ids=list(data.get('TeamIdentifier') or data.get('ApplicationIdentifierPrefix') or []),
        platforms=list(data.get('Platform') or []),
        expiration=data.get('ExpirationDate'),
        provisioned_devices=list(data.get('ProvisionedDevices') or []),
        entitlements=dict(data.get('Entitlements') or {}),
        developer_certificate_sha1s=certificate_sha1s,
        team_name=data.get('TeamName', ''))


def find_codesign_identities(command_runner=subprocess.run, env=None):
    env = _copy_env(env)
    try:
        result = _run_command(command_runner, ['security', 'find-identity', '-v', '-p', 'codesigning'], env=env, capture=True)
    except OSError:
        return []

    if getattr(result, 'returncode', 1) != 0:
        return []

    text = _completed_output(result).decode('utf-8', errors='replace')
    identities = []
    identity_re = re.compile(r'^\s*\d+\)\s+([0-9A-Fa-f]{40})\s+"([^"]+)"')
    for line in text.splitlines():
        match = identity_re.match(line)
        if match:
            identities.append(IOSCodeSignIdentity(match.group(1).upper(), match.group(2)))
    return identities


def _now_for_expiration(expiration):
    if expiration is not None and getattr(expiration, 'tzinfo', None) is not None:
        return datetime.datetime.now(expiration.tzinfo)
    return datetime.datetime.now()


def _bundle_id_matches_app_identifier(app_identifier, bundle_id, team_ids):
    if not app_identifier:
        return False

    bundle_pattern = app_identifier
    for team_id in team_ids:
        prefix = '%s.' % team_id
        if bundle_pattern.startswith(prefix):
            bundle_pattern = bundle_pattern[len(prefix):]
            break

    if bundle_pattern == bundle_id:
        return True

    pattern = '^%s$' % re.escape(bundle_pattern).replace('\\*', '.*')
    return re.match(pattern, bundle_id) is not None


def profile_bundle_id_pattern(profile):
    app_identifier = profile.entitlements.get('application-identifier', '')
    team_ids = list(profile.team_ids)
    entitlement_team = profile.entitlements.get('com.apple.developer.team-identifier')
    if entitlement_team:
        team_ids.append(entitlement_team)

    for team_id in team_ids:
        prefix = '%s.' % team_id
        if app_identifier.startswith(prefix):
            return app_identifier[len(prefix):]
    return app_identifier


def xcode_solution_bundle_id(profile):
    pattern = profile_bundle_id_pattern(profile)
    if not pattern:
        return IOS_XCODE_SOLUTION_DEFAULT_BUNDLE_ID
    if '*' not in pattern:
        return pattern
    if pattern == '*':
        return IOS_XCODE_SOLUTION_DEFAULT_BUNDLE_ID
    if pattern.endswith('.*'):
        return '%sdmengine' % pattern[:-1]
    return pattern.replace('*', 'dmengine')


def install_mobileprovision_for_xcode(profile, log_fn=None):
    if not profile.uuid:
        raise IOSTestError('Provisioning profile %s has no UUID and cannot be used for Xcode signing' % profile.path)

    source = os.path.abspath(os.path.expanduser(profile.path))
    profiles_dir = os.path.expanduser(IOS_PROVISIONING_PROFILE_DIR)
    destination = os.path.join(profiles_dir, '%s.mobileprovision' % profile.uuid)
    if os.path.abspath(destination) == source:
        return destination

    try:
        os.makedirs(profiles_dir, exist_ok=True)
        if not os.path.exists(destination) or not os.path.samefile(source, destination):
            shutil.copy2(source, destination)
            _log(log_fn, 'Installed iOS provisioning profile for Xcode signing: %s' % destination)
    except OSError as e:
        raise IOSTestError('Failed to install provisioning profile %s into %s: %s' % (source, profiles_dir, e)) from e

    return destination


def xcode_solution_signing_cmake_args(identity=None, mobileprovision=None, team_id=None,
                                      env=None, log_fn=None, command_runner=subprocess.run):
    env = _copy_env(env)
    profile_path = mobileprovision or env.get('IOS_MOBILEPROVISION')
    if not profile_path:
        return []

    profile_path = os.path.abspath(os.path.expanduser(profile_path))
    profile = parse_mobileprovision(profile_path, command_runner=command_runner, env=env)

    if not profile_is_ios(profile):
        raise IOSTestError('Provisioning profile %s is not an iOS profile' % profile.display())
    if not profile_is_not_expired(profile):
        raise IOSTestError('Provisioning profile %s is expired or has no expiration date' % profile.display())

    team_id = team_id or env.get('IOS_DEVELOPMENT_TEAM')
    if not team_id:
        if profile.team_ids:
            team_id = profile.team_ids[0]
        else:
            team_id = profile.entitlements.get('com.apple.developer.team-identifier')
    if not team_id:
        raise IOSTestError('Provisioning profile %s has no team id' % profile.display())
    if not profile_matches_team(profile, team_id):
        raise IOSTestError('Provisioning profile %s does not match team id %s' % (profile.display(), team_id))

    bundle_id = xcode_solution_bundle_id(profile)
    if not profile_matches_bundle_id(profile, bundle_id):
        raise IOSTestError('Provisioning profile %s does not match generated dmengine bundle id %s' % (
            profile.display(), bundle_id))

    identity_hint = identity or env.get('IOS_CODESIGN_IDENTITY')
    signing_identity = select_identity(
        profile,
        find_codesign_identities(command_runner=command_runner, env=env),
        identity_hint)

    installed_profile = install_mobileprovision_for_xcode(profile, log_fn=log_fn)
    _log(log_fn, 'Using iOS Xcode signing profile: %s' % profile.display())
    _log(log_fn, 'Using iOS Xcode signing identity: %s' % signing_identity.display())
    _log(log_fn, 'Using iOS Xcode bundle id: %s' % bundle_id)

    return [
        '-DDEFOLD_IOS_MOBILEPROVISION:PATH=%s' % installed_profile,
        '-DDEFOLD_IOS_DEVELOPMENT_TEAM:STRING=%s' % team_id,
        '-DDEFOLD_IOS_PROVISIONING_PROFILE_UUID:STRING=%s' % profile.uuid,
        '-DDEFOLD_IOS_PROVISIONING_PROFILE_SPECIFIER:STRING=%s' % profile.name,
        '-DDEFOLD_IOS_CODESIGN_IDENTITY:STRING=%s' % signing_identity.name,
        '-DDEFOLD_IOS_BUNDLE_IDENTIFIER:STRING=%s' % bundle_id,
    ]


def profile_matches_bundle_id(profile, bundle_id):
    app_identifier = profile.entitlements.get('application-identifier', '')
    return _bundle_id_matches_app_identifier(app_identifier, bundle_id, profile.team_ids)


def generated_test_bundle_id_examples(bundle_id_prefix=None):
    prefix = (bundle_id_prefix or IOS_DEFAULT_BUNDLE_ID_PREFIX).rstrip('.')
    return [
        '%s.test-testmain' % prefix,
        '%s.test-engine' % prefix,
    ]


def profile_matches_generated_test_bundle_ids(profile, bundle_id_prefix=None):
    return all(profile_matches_bundle_id(profile, bundle_id)
               for bundle_id in generated_test_bundle_id_examples(bundle_id_prefix))


def profile_matches_device(profile, device_udid):
    if not device_udid:
        return False
    devices = [str(d).lower() for d in profile.provisioned_devices]
    return device_udid.lower() in devices


def profile_is_ios(profile):
    return 'iOS' in profile.platforms


def profile_is_not_expired(profile):
    if not profile.expiration:
        return False
    return profile.expiration > _now_for_expiration(profile.expiration)


def profile_matches_team(profile, team_id):
    if not team_id:
        return True
    team_ids = set(profile.team_ids)
    entitlement_team = profile.entitlements.get('com.apple.developer.team-identifier')
    if entitlement_team:
        team_ids.add(entitlement_team)
    return team_id in team_ids


def profile_matches(profile, device_udid, bundle_id, team_id=None):
    return (profile_is_ios(profile) and
            profile_is_not_expired(profile) and
            profile_matches_device(profile, device_udid) and
            profile_matches_team(profile, team_id) and
            profile_matches_bundle_id(profile, bundle_id))


def _profile_summary(profile):
    expiration = profile.expiration.isoformat() if profile.expiration else 'no-expiration'
    app_identifier = profile.entitlements.get('application-identifier', 'no-application-identifier')
    return '%s, app=%s, platforms=%s, expires=%s' % (
        profile.display(), app_identifier, ','.join(profile.platforms), expiration)


def _profile_device_sample(profile, limit=5):
    devices = [str(device) for device in profile.provisioned_devices]
    if not devices:
        return 'none'
    sample = devices[:limit]
    suffix = ', ...' if len(devices) > limit else ''
    return '%s%s' % (', '.join(sample), suffix)


def profile_preflight_mismatch_reasons(profile, device_udid, team_id=None, bundle_id_prefix=None):
    reasons = []
    if not profile_is_ios(profile):
        reasons.append('platforms are %s, expected iOS' % (', '.join(profile.platforms) or 'none'))
    if not profile.expiration:
        reasons.append('has no expiration date')
    elif not profile_is_not_expired(profile):
        reasons.append('expired at %s' % profile.expiration.isoformat())
    if not profile_matches_device(profile, device_udid):
        reasons.append('does not include selected device UDID %s; profile devices: %s' % (
            device_udid, _profile_device_sample(profile)))
    if not profile_matches_team(profile, team_id):
        reasons.append('team ids are %s, expected %s' % (
            ', '.join(_profile_team_ids(profile)) or 'none', team_id))
    if not profile_matches_generated_test_bundle_ids(profile, bundle_id_prefix):
        reasons.append('app id %s does not match generated test bundle ids %s' % (
            profile_bundle_id_pattern(profile) or 'none',
            ', '.join(generated_test_bundle_id_examples(bundle_id_prefix))))
    return reasons


def _collect_mobileprovision_paths(extra_paths=None):
    paths = []
    paths.extend(_to_string_list(extra_paths))
    paths.extend(glob.glob(os.path.join(IOS_PROVISIONING_PROFILE_DIR, '*.mobileprovision')))
    paths = [os.path.abspath(os.path.expanduser(path)) for path in paths if path]

    unique = []
    seen = set()
    for path in paths:
        if path not in seen:
            seen.add(path)
            unique.append(path)
    return unique


def load_mobileprovision_profiles(paths, command_runner=subprocess.run, env=None):
    profiles = []
    errors = []
    for path in paths:
        if not os.path.exists(path):
            errors.append('%s: not found' % path)
            continue
        try:
            profiles.append(parse_mobileprovision(path, command_runner=command_runner, env=env))
        except Exception as e:
            errors.append('%s: %s' % (path, e))
    return profiles, errors


def _normalize_stage_target(source, target):
    target = (target or '').replace('\\', '/').replace(os.sep, '/').lstrip('/')
    if not target:
        target = os.path.basename(os.path.normpath(source))
    return target


def _launch_ready_paths(configfile, folders=None, copied_targets=None):
    ready_paths = []
    if copied_targets is None and folders:
        for source, target in folders.items():
            normalized_target = _normalize_stage_target(source, target)
            copied_targets = list(copied_targets or [])
            copied_targets.append(normalized_target)
    for copied_target in copied_targets or []:
        path_parts = [part for part in copied_target.split('/') if part]
        for i in range(1, len(path_parts) + 1):
            ready_paths.append('/'.join(path_parts[:i]))
    if configfile:
        ready_paths.append('unittest.cfg')
    return list(dict.fromkeys(path for path in ready_paths if path))


def _write_test_info_plist(app_dir, bundle_id, executable_name, supported_platform):
    info = {
        'CFBundleDevelopmentRegion': 'en',
        'CFBundleExecutable': executable_name,
        'CFBundleIdentifier': bundle_id,
        'CFBundleInfoDictionaryVersion': '6.0',
        'CFBundleName': executable_name,
        'CFBundlePackageType': 'APPL',
        'CFBundleShortVersionString': '1.0',
        'CFBundleSupportedPlatforms': [supported_platform],
        'CFBundleVersion': '1',
        'LSRequiresIPhoneOS': True,
        'MinimumOSVersion': '11.0',
        'NSAppTransportSecurity': {
            'NSAllowsArbitraryLoads': True,
            'NSAllowsLocalNetworking': True,
        },
        'NSBonjourServices': [
            '_defold._tcp',
            '_services._dns-sd._udp',
        ],
        'NSLocalNetworkUsageDescription': 'Defold iOS tests connect to the local test server running on the build host.',
        'UIDeviceFamily': [1, 2],
        'UISupportedInterfaceOrientations': [
            'UIInterfaceOrientationPortrait',
            'UIInterfaceOrientationLandscapeLeft',
            'UIInterfaceOrientationLandscapeRight',
        ],
    }
    with open(os.path.join(app_dir, 'Info.plist'), 'wb') as f:
        plistlib.dump(info, f)


def _create_app_bundle(program, app_dir, bundle_id, supported_platform):
    executable_name = os.path.basename(program)
    os.makedirs(app_dir)
    shutil.copy2(program, os.path.join(app_dir, executable_name))
    os.chmod(os.path.join(app_dir, executable_name), 0o755)
    _write_test_info_plist(app_dir, bundle_id, executable_name, supported_platform)


def _copy_stage_source(cwd, source, target, stage_root, log_fn=None):
    source_path = source if os.path.isabs(source) else os.path.join(cwd, source)
    source_path = os.path.normpath(source_path)
    if not os.path.exists(source_path):
        _log(log_fn, 'ios-test: source %s not found, skipping' % source_path)
        return None

    target = _normalize_stage_target(source_path, target)
    target_path = os.path.join(stage_root, *target.split('/'))
    os.makedirs(os.path.dirname(target_path), exist_ok=True)

    if os.path.isdir(source_path):
        shutil.copytree(source_path, target_path, dirs_exist_ok=True)
    else:
        shutil.copy2(source_path, target_path)
    return target


def _create_stage_root(cwd, configfile, folders, stage_root, log_fn=None):
    if os.path.exists(stage_root):
        shutil.rmtree(stage_root)
    os.makedirs(stage_root)
    copied_targets = []
    if folders:
        for source, target in folders.items():
            copied_target = _copy_stage_source(cwd, source, target, stage_root, log_fn=log_fn)
            if copied_target:
                copied_targets.append(copied_target)

    if configfile:
        config_path = configfile if os.path.isabs(configfile) else os.path.join(cwd, configfile)
        if not os.path.exists(config_path):
            raise IOSTestError('iOS test config file not found: %s' % config_path)
        shutil.copy2(config_path, os.path.join(stage_root, 'unittest.cfg'))
    return copied_targets


def _profile_team_ids(profile):
    team_ids = []
    for team_id in profile.team_ids:
        if team_id and team_id not in team_ids:
            team_ids.append(team_id)

    entitlement_team = profile.entitlements.get('com.apple.developer.team-identifier')
    if entitlement_team and entitlement_team not in team_ids:
        team_ids.append(entitlement_team)
    return team_ids


def development_teams_from_profiles(profiles):
    team_data = {}
    for profile in profiles:
        for team_id in _profile_team_ids(profile):
            if team_id not in team_data:
                team_data[team_id] = {
                    'name': '',
                    'profile_count': 0,
                    'ios_profile_count': 0,
                    'valid_ios_profile_count': 0,
                }

            data = team_data[team_id]
            if profile.team_name and not data['name']:
                data['name'] = profile.team_name
            data['profile_count'] += 1
            if profile_is_ios(profile):
                data['ios_profile_count'] += 1
                if profile_is_not_expired(profile):
                    data['valid_ios_profile_count'] += 1

    return [
        IOSDevelopmentTeam(
            team_id=team_id,
            name=data['name'],
            profile_count=data['profile_count'],
            ios_profile_count=data['ios_profile_count'],
            valid_ios_profile_count=data['valid_ios_profile_count'])
        for team_id, data in sorted(team_data.items())
    ]


def find_development_teams(extra_paths=None, command_runner=subprocess.run, env=None):
    env = _copy_env(env)
    paths = _to_string_list(extra_paths)
    if env.get('IOS_MOBILEPROVISION'):
        paths.append(env.get('IOS_MOBILEPROVISION'))
    profiles, errors = load_mobileprovision_profiles(
        _collect_mobileprovision_paths(paths),
        command_runner=command_runner,
        env=env)
    return development_teams_from_profiles(profiles), errors


def _resolve_wildcard_entitlement_value(value, team_id, bundle_id):
    if not isinstance(value, str) or '*' not in value:
        return value
    team_prefix = '%s.' % team_id
    if value.startswith(team_prefix):
        return '%s%s' % (team_prefix, bundle_id)
    return value.replace('*', bundle_id)


def make_entitlements(profile, bundle_id):
    if not profile.team_ids:
        raise IOSTestError('Provisioning profile %s has no TeamIdentifier' % profile.display())

    team_id = profile.team_ids[0]
    entitlements = dict(profile.entitlements)
    entitlements['application-identifier'] = '%s.%s' % (team_id, bundle_id)
    entitlements['com.apple.developer.team-identifier'] = team_id

    keychain_groups = entitlements.get('keychain-access-groups')
    if isinstance(keychain_groups, list):
        entitlements['keychain-access-groups'] = [
            _resolve_wildcard_entitlement_value(value, team_id, bundle_id)
            for value in keychain_groups
        ]

    return entitlements


def select_identity(profile, identities, identity_hint=None):
    matching = [identity for identity in identities if identity.sha1 in profile.developer_certificate_sha1s]

    if identity_hint:
        hint = identity_hint.lower()
        hinted = [
            identity for identity in identities
            if identity.sha1.lower() == hint or identity.name == identity_hint or hint in identity.name.lower()
        ]
        if len(hinted) == 0:
            raise IOSTestError('Requested iOS signing identity not found: %s' % identity_hint)
        if len(hinted) > 1:
            raise IOSTestError('Requested iOS signing identity is ambiguous: %s\n%s' % (
                identity_hint, '\n'.join(identity.display() for identity in hinted)))
        if profile.developer_certificate_sha1s and hinted[0].sha1 not in profile.developer_certificate_sha1s:
            raise IOSTestError('Requested iOS signing identity is not included in provisioning profile %s: %s' % (
                profile.display(), hinted[0].display()))
        return hinted[0]

    if len(matching) == 1:
        return matching[0]
    if len(matching) == 0:
        raise IOSTestError('No installed code signing identity matches provisioning profile %s' % profile.display())

    raise IOSTestError('Multiple installed code signing identities match provisioning profile %s:\n%s' % (
        profile.display(), '\n'.join(identity.display() for identity in matching)))


def select_profile(paths, device_udid, bundle_id, team_id=None, command_runner=subprocess.run, env=None, log_fn=None):
    profiles, errors = load_mobileprovision_profiles(paths, command_runner=command_runner, env=env)
    candidates = [profile for profile in profiles if profile_matches(profile, device_udid, bundle_id, team_id)]
    if len(candidates) == 1:
        return candidates[0]

    if len(candidates) == 0:
        details = ['No provisioning profile matches bundle id %s, device %s%s.' % (
            bundle_id, device_udid, (', team %s' % team_id) if team_id else '')]
        ios_profiles = [profile for profile in profiles if profile_is_ios(profile)]
        if ios_profiles:
            details.append('Decoded iOS profile candidates:')
            details.extend('  %s' % _profile_summary(profile) for profile in ios_profiles)
        if errors:
            details.append('Profile decode errors:')
            details.extend('  %s' % error for error in errors)
        raise IOSTestError('\n'.join(details))

    raise IOSTestError('Multiple provisioning profiles match bundle id %s and device %s:\n%s' % (
        bundle_id, device_udid, '\n'.join('  %s' % _profile_summary(profile) for profile in candidates)))


def _walk_dicts(value):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            for item in _walk_dicts(child):
                yield item
    elif isinstance(value, list):
        for child in value:
            for item in _walk_dicts(child):
                yield item


def _dict_get_any(data, names):
    for name in names:
        if name in data and data[name]:
            return data[name]
    return None


def _stringify(value):
    if value is None:
        return ''
    if isinstance(value, (dict, list)):
        return json.dumps(value, sort_keys=True)
    return str(value)


def _devicectl_error_indicates_locked(message):
    message = (message or '').lower()
    return any(token in message for token in [
        'device is locked',
        'devicelocked',
        'mobileimagemounterdevicelocked',
        'kamdmobileimagemounterdevicelocked',
        'passcode locked',
    ])


def _devicectl_error_is_retryable(message):
    message = (message or '').lower()
    return any(token in message for token in [
        'timed out waiting for coredeviceservice',
        'timed out after',
        'connection was invalidated',
        'xpcerror(errorcode: 1001',
        'coredevice.coredeviceservice',
    ])


def _as_dict(value):
    return value if isinstance(value, dict) else {}


def _is_ios_device_record(data):
    text = ' '.join(_stringify(data.get(key)) for key in (
        'platform', 'platformIdentifier', 'deviceType', 'deviceTypeIdentifier',
        'name', 'modelName', 'hardwareProperties', 'connectionProperties', 'deviceProperties'))
    return 'ios' in text.lower() and 'simulator' not in text.lower()


def _has_device_record_shape(data):
    if any(isinstance(data.get(key), dict) for key in ('hardwareProperties', 'connectionProperties', 'deviceProperties')):
        return True
    if _dict_get_any(data, ('identifier', 'id', 'deviceIdentifier', 'coreDeviceIdentifier')):
        return True
    return bool(_dict_get_any(data, ('name', 'deviceName', 'displayName')) and
                _dict_get_any(data, ('udid', 'UDID', 'uniqueDeviceIdentifier')))


def _device_connection_state(record):
    connection = _as_dict(record.get('connectionProperties'))
    fields = []
    for data in (record, connection):
        for key in (
            'isConnected', 'connected', 'isAvailable', 'available',
            'connectionState', 'state', 'status', 'availability',
            'transportType', 'tunnelState'):
            if key in data:
                fields.append(data[key])

    for value in fields:
        if isinstance(value, bool):
            return value

    text = ' '.join(_stringify(value).lower() for value in fields)
    if any(value in text for value in ('disconnected', 'not connected', 'unavailable', 'offline', 'unpaired')):
        return False
    if any(value in text for value in ('connected', 'available', 'online', 'wired', 'usb', 'localnetwork', 'local network')):
        return True

    transport = _stringify(connection.get('transportType')).strip().lower()
    if transport and transport not in ('none', 'unknown', 'unavailable'):
        return True
    return None


def _device_sort_score(device):
    state = _device_connection_state(device.raw)
    score = 0
    if state is True:
        score += 100
    elif state is None:
        score += 10

    if isinstance(device.raw.get('connectionProperties'), dict):
        score += 10
    if isinstance(device.raw.get('hardwareProperties'), dict):
        score += 5
    if device.device_id and device.udid and device.device_id != device.udid:
        score += 3
    if device.name:
        score += 1
    return score


def _filter_connected_devices(devices):
    connected = [device for device in devices if _device_connection_state(device.raw) is True]
    return connected or devices


def _parse_device_lock_state(data):
    for record in _walk_dicts(data):
        for key, value in record.items():
            key_lower = str(key).lower()
            if key_lower in ('locked', 'islocked', 'passcodelocked', 'devicelocked'):
                if isinstance(value, bool):
                    return value, _stringify(value)
                value_text = _stringify(value).strip().lower()
                if value_text in ('true', 'yes', '1', 'locked'):
                    return True, _stringify(value)
                if value_text in ('false', 'no', '0', 'unlocked', 'not locked'):
                    return False, _stringify(value)
                return None, _stringify(value)
            if key_lower in ('lockstate', 'lock_state'):
                value_text = _stringify(value).strip().lower()
                if 'unlocked' in value_text or 'not locked' in value_text:
                    return False, _stringify(value)
                if 'locked' in value_text:
                    return True, _stringify(value)
                return None, _stringify(value)
    return None, ''


def device_lock_state_from_json(data):
    lock_state, _raw_value = _parse_device_lock_state(data)
    return lock_state


def devices_from_devicectl_json(data):
    devices_by_udid = {}
    for record in _walk_dicts(data):
        if not _is_ios_device_record(record) or not _has_device_record_shape(record):
            continue

        hardware = _as_dict(record.get('hardwareProperties'))
        connection = _as_dict(record.get('connectionProperties'))
        device_properties = _as_dict(record.get('deviceProperties'))

        name = (_dict_get_any(record, ('name', 'deviceName', 'displayName')) or
                _dict_get_any(device_properties, ('name', 'deviceName', 'displayName')) or
                _dict_get_any(hardware, ('name', 'marketingName', 'modelName')) or '')
        identifier = _dict_get_any(record, ('identifier', 'id', 'deviceIdentifier', 'coreDeviceIdentifier'))
        udid = _dict_get_any(record, ('udid', 'UDID', 'uniqueDeviceIdentifier'))
        serial = _dict_get_any(record, ('serialNumber', 'serial'))

        udid = udid or _dict_get_any(hardware, ('udid', 'UDID', 'serialNumber')) or _dict_get_any(connection, ('udid', 'UDID', 'serialNumber'))
        identifier = identifier or udid or serial or name
        if not identifier:
            continue

        platform = (_dict_get_any(record, ('platform', 'platformIdentifier', 'deviceTypeIdentifier')) or
                    _dict_get_any(hardware, ('platform', 'platformIdentifier', 'deviceTypeIdentifier')) or '')
        device = IOSDevice(
            device_id=_stringify(identifier),
            udid=_stringify(udid or identifier),
            name=_stringify(name),
            platform=_stringify(platform),
            raw=record)

        key = device.udid.lower() if device.udid else device.device_id.lower()
        previous = devices_by_udid.get(key)
        if previous is None or _device_sort_score(device) > _device_sort_score(previous):
            devices_by_udid[key] = device

    return _filter_connected_devices(list(devices_by_udid.values()))


def _device_match_values(device):
    values = [device.device_id, device.udid, device.name]
    hardware = _as_dict(device.raw.get('hardwareProperties'))
    connection = _as_dict(device.raw.get('connectionProperties'))
    raw_values = [
        _dict_get_any(device.raw, ('serialNumber', 'serial')),
        _dict_get_any(hardware, ('serialNumber', 'serial')),
        _dict_get_any(connection, ('serialNumber', 'serial')),
        _dict_get_any(device.raw, ('ecid', 'ECID')),
        _dict_get_any(hardware, ('ecid', 'ECID')),
        _dict_get_any(connection, ('ecid', 'ECID')),
        _dict_get_any(device.raw, ('dnsName', 'dns_name')),
        _dict_get_any(connection, ('dnsName', 'dns_name')),
    ]
    values.extend(_stringify(value) for value in raw_values if value)
    return values


def device_matches_request(device, requested):
    requested_lower = requested.lower()
    return any(_stringify(value).lower() == requested_lower for value in _device_match_values(device) if value)


def simulators_from_simctl_json(data):
    simulators = []
    devices = data.get('devices') if isinstance(data, dict) else {}
    if not isinstance(devices, dict):
        return simulators

    for runtime, records in devices.items():
        if 'iOS' not in _stringify(runtime):
            continue
        for record in records or []:
            if not isinstance(record, dict):
                continue
            if record.get('isAvailable') is False:
                continue
            udid = _stringify(record.get('udid'))
            name = _stringify(record.get('name'))
            if not udid:
                continue
            simulators.append(IOSSimulator(
                udid=udid,
                name=name,
                runtime=_stringify(runtime),
                state=_stringify(record.get('state')),
                raw=record))
    return simulators


def _simulator_match_values(simulator):
    return [
        simulator.udid,
        simulator.name,
        simulator.runtime,
        _dict_get_any(simulator.raw, ('deviceTypeIdentifier', 'deviceType')),
    ]


def simulator_matches_request(simulator, requested):
    requested_lower = requested.lower()
    return any(_stringify(value).lower() == requested_lower for value in _simulator_match_values(simulator) if value)


def no_available_simulators_message():
    return (
        'No available iOS simulators found.\n'
        'How to fix:\n'
        '  1. Make sure xcrun points at a full Xcode: xcode-select -p\n'
        '  2. Complete Xcode first-launch tasks: sudo xcodebuild -runFirstLaunch\n'
        '  3. Install an iOS Simulator runtime in Xcode Settings > Platforms, or run:\n'
        '       xcodebuild -downloadPlatform iOS -architectureVariant universal\n'
        '  4. Create or boot an iPhone simulator, then verify:\n'
        '       xcrun simctl list devices available\n'
        '       python3 build_tools/build_ios.py list-simulators\n'
        'For the current x86_64-ios test runner, use a universal iOS simulator runtime on Apple Silicon hosts.')


class IOSTestRunner(object):
    def __init__(self, env=None, log_fn=None, device=None, identity=None, mobileprovision=None,
                 team_id=None, bundle_id_prefix=None, command_runner=subprocess.run):
        self._env = _copy_env(env)
        self._log_fn = log_fn
        self._device = device or self._env.get('IOS_DEVICE_ID')
        self._identity = identity or self._env.get('IOS_CODESIGN_IDENTITY')
        self._mobileprovision = mobileprovision or self._env.get('IOS_MOBILEPROVISION')
        self._team_id = team_id or self._env.get('IOS_DEVELOPMENT_TEAM')
        self._bundle_id_prefix = bundle_id_prefix or self._env.get('IOS_TEST_BUNDLE_ID_PREFIX')
        self._command_runner = command_runner
        self._devicectl_timeout = _env_positive_int(self._env, IOS_DEVICECTL_TIMEOUT_ENV, IOS_DEFAULT_DEVICECTL_TIMEOUT)
        self._devicectl_retries = _env_nonnegative_int(self._env, IOS_DEVICECTL_RETRIES_ENV, IOS_DEFAULT_DEVICECTL_RETRIES)
        self._device_lock_status_cache = {}
        self._device_developer_services_cache = {}

    def _log(self, message):
        _log(self._log_fn, message)

    def _run(self, cmd, capture=True, env=None, timeout=None):
        self._log('ios-test: %s' % subprocess.list2cmdline(cmd))
        try:
            return _run_command(self._command_runner, cmd, env=env or self._env, capture=capture, timeout=timeout)
        except FileNotFoundError as e:
            raise IOSTestError('%s not found' % cmd[0]) from e
        except subprocess.TimeoutExpired as e:
            raise IOSTestError('%s timed out after %s seconds' % (
                subprocess.list2cmdline(cmd), e.timeout)) from e

    def _devicectl_json(self, subcommand, timeout=None, allow_failure=False):
        timeout = timeout or self._devicectl_timeout
        attempts = self._devicectl_retries + 1
        for attempt in range(attempts):
            json_path = None
            try:
                fd, json_path = tempfile.mkstemp(prefix='defold-devicectl-', suffix='.json')
                os.close(fd)
                cmd = ['xcrun', 'devicectl', '--timeout', str(timeout), '--json-output', json_path]
                cmd.extend(subcommand)
                try:
                    result = self._run(
                        cmd,
                        capture=True,
                        timeout=timeout + IOS_DEVICECTL_PROCESS_TIMEOUT_MARGIN_SECONDS)
                except IOSTestError as e:
                    error = str(e)
                    if _devicectl_error_is_retryable(error) and attempt + 1 < attempts:
                        self._log('ios-test: devicectl CoreDevice startup failed; retrying %d/%d' % (attempt + 1, self._devicectl_retries))
                        time.sleep(IOS_DEVICECTL_RETRY_DELAY_SECONDS)
                        continue
                    if allow_failure and _devicectl_error_is_retryable(error):
                        return None
                    raise
                if getattr(result, 'returncode', 1) != 0:
                    error = _completed_error(result).strip()
                    if _devicectl_error_is_retryable(error) and attempt + 1 < attempts:
                        self._log('ios-test: devicectl CoreDevice startup failed; retrying %d/%d' % (attempt + 1, self._devicectl_retries))
                        time.sleep(IOS_DEVICECTL_RETRY_DELAY_SECONDS)
                        continue
                    if allow_failure:
                        return None
                    if _devicectl_error_is_retryable(error):
                        raise IOSTestError(
                            'devicectl could not initialize CoreDeviceService after %d attempt%s. '
                            'Unlock and reconnect the iOS device, quit Xcode, then retry. '
                            'If it still fails, restart Apple CoreDeviceService and verify with '
                            "`xcrun devicectl list devices`.\nLast devicectl error: %s" % (
                                attempts,
                                '' if attempts == 1 else 's',
                                error))
                    raise IOSTestError('devicectl failed with exit code %s: %s' % (
                        getattr(result, 'returncode', '?'), error))

                if not os.path.exists(json_path) or os.path.getsize(json_path) == 0:
                    return {}
                with open(json_path, 'rb') as f:
                    return json.load(f)
            finally:
                if json_path and os.path.exists(json_path):
                    os.unlink(json_path)
        return None

    def list_devices(self):
        return devices_from_devicectl_json(self._devicectl_json(['list', 'devices']) or {})

    def device_display(self, device):
        return '%s (%s)' % (device.name or device.device_id, device.udid)

    def device_lock_status(self, device, refresh=False):
        key = device.device_id or device.udid
        if not refresh and key in self._device_lock_status_cache:
            return self._device_lock_status_cache[key]

        data = self._devicectl_json(
            ['device', 'info', 'lockState', '--device', device.device_id],
            allow_failure=True)
        if data is None:
            status = (False, None, '')
        else:
            lock_state, raw_value = _parse_device_lock_state(data)
            status = (True, lock_state, raw_value)
        self._device_lock_status_cache[key] = status
        return status

    def _device_is_reachable(self, device):
        reachable, _lock_state, _raw_value = self.device_lock_status(device)
        return reachable

    def device_is_locked(self, device):
        _reachable, lock_state, _raw_value = self.device_lock_status(device)
        return lock_state

    def device_developer_services_status(self, device, refresh=False):
        key = device.device_id or device.udid
        if not refresh and key in self._device_developer_services_cache:
            return self._device_developer_services_cache[key]

        try:
            data = self._devicectl_json(
                ['device', 'info', 'ddiServices', '--device', device.device_id])
        except IOSTestError as e:
            detail = str(e)
            status = (False, _devicectl_error_indicates_locked(detail), detail)
            self._device_developer_services_cache[key] = status
            return status

        result = _as_dict(data.get('result'))
        metadata = _as_dict(result.get('ddiMetadata'))
        if metadata.get('isUsable') is False:
            detail = 'devicectl reported developer disk image services are not usable'
            status = (False, False, detail)
        else:
            status = (True, False, '')
        self._device_developer_services_cache[key] = status
        return status

    def ensure_device_ready(self, device, refresh=False):
        reachable, lock_state, raw_lock_state = self.device_lock_status(device, refresh=refresh)
        if not reachable:
            raise IOSTestError('Could not read lock state for iOS device %s; make sure it is connected, trusted, and unlocked' % (
                self.device_display(device)))
        if lock_state is True:
            raise IOSTestError('iOS device %s is locked; unlock it and retry' % self.device_display(device))

        developer_services_ok, developer_services_locked, developer_services_detail = self.device_developer_services_status(device, refresh=refresh)
        if developer_services_locked:
            raise IOSTestError('iOS device %s is locked; unlock it and retry' % self.device_display(device))
        if developer_services_ok:
            return
        if lock_state is None:
            detail = '; devicectl reported lock state "%s"' % raw_lock_state if raw_lock_state else ''
            service_detail = (': ' + developer_services_detail) if developer_services_detail else ''
            raise IOSTestError('Could not determine whether iOS device %s is locked%s; unlock it and retry%s' % (
                self.device_display(device), detail, service_detail))
        if not developer_services_ok:
            raise IOSTestError('Could not prepare iOS developer disk image services for device %s; unlock it, keep it connected, and retry%s' % (
                self.device_display(device),
                (': ' + developer_services_detail) if developer_services_detail else ''))

    def _select_single_device_by_lock_state(self, devices):
        probed = []
        for device in devices:
            reachable, lock_state, raw_value = self.device_lock_status(device)
            if reachable:
                probed.append((device, lock_state, raw_value))

        for desired_state in (False, True, None):
            matches = [device for device, lock_state, _raw_value in probed if lock_state is desired_state]
            if len(matches) == 1:
                return matches[0]
            if len(matches) > 1:
                return None
        return None

    def select_device(self):
        devices = self.list_devices()
        requested = self._device
        if requested:
            matches = [device for device in devices if device_matches_request(device, requested)]
            if len(matches) == 1:
                return matches[0]
            if len(matches) > 1:
                selected = self._select_single_device_by_lock_state(matches)
                if selected:
                    return selected
                raise IOSTestError('Requested iOS device is ambiguous: %s' % requested)
            raise IOSTestError('Requested iOS device not found: %s' % requested)

        if len(devices) == 1:
            return devices[0]
        if len(devices) == 0:
            raise IOSTestError('No connected iOS devices found')

        selected = self._select_single_device_by_lock_state(devices)
        if selected:
            return selected

        reachable = [device for device in devices if self._device_is_reachable(device)]
        if reachable:
            devices = reachable

        raise IOSTestError('Multiple iOS devices found; set --device or IOS_DEVICE_ID:\n%s' % (
            '\n'.join('  %s (%s)' % (device.name or device.device_id, device.udid) for device in devices)))

    def _select_signing_assets(self, bundle_id, device):
        profile_paths = _collect_mobileprovision_paths([self._mobileprovision] if self._mobileprovision else None)
        if self._mobileprovision:
            profile_paths = [os.path.abspath(os.path.expanduser(self._mobileprovision))]

        profile = select_profile(
            profile_paths,
            device.udid,
            bundle_id,
            team_id=self._team_id,
            command_runner=self._command_runner,
            env=self._env,
            log_fn=self._log_fn)

        identities = find_codesign_identities(command_runner=self._command_runner, env=self._env)
        identity = select_identity(profile, identities, self._identity)
        return profile, identity

    def _write_info_plist(self, app_dir, bundle_id, executable_name):
        _write_test_info_plist(app_dir, bundle_id, executable_name, 'iPhoneOS')

    def _create_app_bundle(self, program, app_dir, bundle_id):
        _create_app_bundle(program, app_dir, bundle_id, 'iPhoneOS')

    def _sign_app_bundle(self, app_dir, profile, identity, bundle_id, work_dir):
        shutil.copy2(profile.path, os.path.join(app_dir, 'embedded.mobileprovision'))

        entitlements_path = os.path.join(work_dir, 'entitlements.xcent')
        with open(entitlements_path, 'wb') as f:
            plistlib.dump(make_entitlements(profile, bundle_id), f)

        sign_env = dict(self._env)
        try:
            result = self._run(['xcrun', '--find', 'codesign_allocate'], capture=True)
            if getattr(result, 'returncode', 1) == 0:
                sign_env['CODESIGN_ALLOCATE'] = _completed_output(result).decode('utf-8', errors='replace').strip()
        except IOSTestError:
            pass

        result = self._run([
            'codesign',
            '-f',
            '-s', identity.sha1,
            '--timestamp=none',
            '--entitlements', entitlements_path,
            app_dir,
        ], capture=True, env=sign_env)
        if getattr(result, 'returncode', 1) != 0:
            raise IOSTestError('codesign failed with exit code %s: %s' % (
                getattr(result, 'returncode', '?'), _completed_error(result).strip()))

    def _copy_stage_source(self, cwd, source, target, stage_root):
        return _copy_stage_source(cwd, source, target, stage_root, log_fn=self._log_fn)

    def _create_stage_root(self, cwd, configfile, folders, stage_root):
        return _create_stage_root(cwd, configfile, folders, stage_root, log_fn=self._log_fn)

    def _install_app(self, device, app_dir):
        self._devicectl_json(['device', 'install', 'app', '--device', device.device_id, app_dir], timeout=120)

    def _uninstall_app(self, device, bundle_id):
        self._devicectl_json(['device', 'uninstall', 'app', '--device', device.device_id, bundle_id], timeout=60, allow_failure=True)

    def _copy_stage_to_device(self, device, bundle_id, stage_transfer_root):
        self._devicectl_json([
            'device', 'copy', 'to',
            '--device', device.device_id,
            '--source', stage_transfer_root,
            '--destination', 'Documents',
            '--domain-type', 'appDataContainer',
            '--domain-identifier', bundle_id,
            '--remove-existing-content', 'true',
        ], timeout=120)

    def _launch_app(self, device, bundle_id, library_name, configfile, folders=None, copied_targets=None, stage_workdir=None):
        if stage_workdir is None:
            stage_workdir = bool(configfile or folders or copied_targets)
        env = {
            IOS_TEST_AUTO_EXIT_ENV: '1',
        }
        if stage_workdir:
            env['DEFOLD_TEST_WORKDIR'] = '%s/%s' % (IOS_TEST_CONTAINER_ROOT, library_name)
            ready_paths = _launch_ready_paths(configfile, folders or {}, copied_targets=copied_targets)
            if ready_paths:
                env[IOS_TEST_READY_PATHS_ENV] = ';'.join(ready_paths)
        args = [
            'xcrun', 'devicectl',
            '--timeout', '600',
            'device', 'process', 'launch',
            '--device', device.device_id,
            '--console',
            '--terminate-existing',
            bundle_id,
        ]
        if env:
            args[9:9] = ['--environment-variables', json.dumps(env, sort_keys=True)]
        if configfile:
            args.append('./unittest.cfg')

        self._log('ios-test: %s' % subprocess.list2cmdline(args))
        try:
            return self._command_runner(args, env=self._env).returncode
        except FileNotFoundError as e:
            raise IOSTestError('xcrun not found') from e

    def run_test(self, program, cwd, configfile=None, folders=None, target_name=None):
        cwd = os.path.abspath(cwd)
        program = os.path.abspath(program)
        if not os.path.exists(program):
            raise IOSTestError('iOS test program not found: %s' % program)

        device = self.select_device()
        self.ensure_device_ready(device, refresh=True)
        library_name = os.path.basename(os.path.normpath(cwd)) or 'test'
        bundle_id = make_bundle_id(target_name=target_name, program=program, bundle_id_prefix=self._bundle_id_prefix, env=self._env)
        profile, identity = self._select_signing_assets(bundle_id, device)

        self._log('ios-test: selected device %s (%s)' % (device.name or device.device_id, device.udid))
        self._log('ios-test: selected profile %s' % profile.display())
        self._log('ios-test: selected identity %s' % identity.display())

        with tempfile.TemporaryDirectory(prefix='defold-ios-test-') as tmp:
            app_dir = os.path.join(tmp, '%s.app' % os.path.splitext(os.path.basename(program))[0])
            self._create_app_bundle(program, app_dir, bundle_id)
            self._sign_app_bundle(app_dir, profile, identity, bundle_id, tmp)
            stage_workdir = bool(configfile or folders)
            copied_targets = []
            stage_transfer_root = None
            if stage_workdir:
                stage_transfer_root = os.path.join(tmp, 'stage')
                stage_container = os.path.join(stage_transfer_root, os.path.basename(IOS_TEST_CONTAINER_ROOT))
                stage_parent = os.path.join(stage_container, library_name)
                copied_targets = self._create_stage_root(cwd, configfile, folders or {}, stage_parent)

            self._uninstall_app(device, bundle_id)
            try:
                self._install_app(device, app_dir)
                if stage_workdir:
                    self._copy_stage_to_device(device, bundle_id, stage_transfer_root)
                return self._launch_app(
                    device,
                    bundle_id,
                    library_name,
                    configfile,
                    folders or {},
                    copied_targets=copied_targets,
                    stage_workdir=stage_workdir)
            finally:
                self._uninstall_app(device, bundle_id)


class IOSSimulatorTestRunner(object):
    def __init__(self, env=None, log_fn=None, device=None, bundle_id_prefix=None, command_runner=subprocess.run):
        self._env = _copy_env(env)
        self._log_fn = log_fn
        self._simulator = device or self._env.get('IOS_SIMULATOR_ID') or self._env.get('IOS_DEVICE_ID')
        self._bundle_id_prefix = bundle_id_prefix or self._env.get('IOS_TEST_BUNDLE_ID_PREFIX')
        self._command_runner = command_runner

    def _log(self, message):
        _log(self._log_fn, message)

    def _run(self, cmd, capture=True):
        self._log('ios-test: %s' % subprocess.list2cmdline(cmd))
        try:
            return _run_command(self._command_runner, cmd, env=self._env, capture=capture)
        except FileNotFoundError as e:
            raise IOSTestError('%s not found' % cmd[0]) from e

    def _simctl(self, subcommand, capture=True, allow_failure=False):
        cmd = ['xcrun', 'simctl']
        cmd.extend(subcommand)
        result = self._run(cmd, capture=capture)
        if getattr(result, 'returncode', 1) != 0 and not allow_failure:
            raise IOSTestError('simctl failed with exit code %s: %s' % (
                getattr(result, 'returncode', '?'), _completed_error(result).strip()))
        return result

    def _simctl_json(self, subcommand):
        result = self._simctl(subcommand, capture=True)
        output = _completed_output(result)
        if not output:
            return {}
        try:
            return json.loads(output.decode('utf-8', errors='replace'))
        except ValueError as e:
            raise IOSTestError('simctl did not return valid JSON for %s' % ' '.join(subcommand)) from e

    def list_simulators(self):
        return simulators_from_simctl_json(self._simctl_json(['list', 'devices', 'available', '-j']))

    def simulator_display(self, simulator):
        return '%s (%s, %s)' % (simulator.name or simulator.udid, simulator.udid, simulator.state or 'unknown')

    def _simulator_candidate_list(self, simulators):
        return '\n'.join('  %s' % self.simulator_display(simulator) for simulator in simulators)

    def select_simulator(self):
        simulators = self.list_simulators()
        requested = self._simulator
        if requested:
            if requested.lower() == 'booted':
                booted = [simulator for simulator in simulators if simulator.state == 'Booted']
                if len(booted) == 1:
                    return booted[0]
                if len(booted) > 1:
                    raise IOSTestError('Multiple booted iOS simulators found; set --device or IOS_SIMULATOR_ID:\n%s' % (
                        self._simulator_candidate_list(booted)))
                raise IOSTestError('No booted iOS simulator found')

            matches = [simulator for simulator in simulators if simulator_matches_request(simulator, requested)]
            if len(matches) == 1:
                return matches[0]
            if len(matches) > 1:
                raise IOSTestError('Requested iOS simulator is ambiguous: %s\n%s' % (
                    requested, self._simulator_candidate_list(matches)))
            raise IOSTestError('Requested iOS simulator not found: %s' % requested)

        booted = [simulator for simulator in simulators if simulator.state == 'Booted']
        if len(booted) == 1:
            return booted[0]
        if len(booted) > 1:
            raise IOSTestError('Multiple booted iOS simulators found; set --device or IOS_SIMULATOR_ID:\n%s' % (
                self._simulator_candidate_list(booted)))
        if not simulators:
            raise IOSTestError(no_available_simulators_message())
        if len(simulators) == 1:
            return simulators[0]
        raise IOSTestError('No booted iOS simulator found; start Simulator or set --device/IOS_SIMULATOR_ID:\n%s' % (
            self._simulator_candidate_list(simulators)))

    def _boot_simulator_if_needed(self, simulator):
        if simulator.state == 'Booted':
            return
        self._simctl(['boot', simulator.udid])
        self._simctl(['bootstatus', simulator.udid, '-b'])

    def _create_app_bundle(self, program, app_dir, bundle_id):
        _create_app_bundle(program, app_dir, bundle_id, 'iPhoneSimulator')

    def _sign_app_bundle(self, app_dir):
        result = self._run([
            'codesign',
            '-f',
            '-s', '-',
            '--timestamp=none',
            app_dir,
        ], capture=True)
        if getattr(result, 'returncode', 1) != 0:
            raise IOSTestError('codesign failed with exit code %s: %s' % (
                getattr(result, 'returncode', '?'), _completed_error(result).strip()))

    def _install_app(self, simulator, app_dir):
        self._simctl(['install', simulator.udid, app_dir])

    def _uninstall_app(self, simulator, bundle_id):
        self._simctl(['uninstall', simulator.udid, bundle_id], allow_failure=True)

    def _data_container(self, simulator, bundle_id):
        result = self._simctl(['get_app_container', simulator.udid, bundle_id, 'data'])
        path = _completed_output(result).decode('utf-8', errors='replace').strip()
        if not path:
            raise IOSTestError('simctl did not return an app data container path for %s' % bundle_id)
        return path

    def _stage_to_simulator(self, simulator, bundle_id, library_name, cwd, configfile, folders):
        data_container = self._data_container(simulator, bundle_id)
        stage_root = os.path.join(data_container, IOS_TEST_CONTAINER_ROOT, library_name)
        return _create_stage_root(cwd, configfile, folders or {}, stage_root, log_fn=self._log_fn)

    def _launch_app(self, simulator, bundle_id, library_name, configfile, folders=None, copied_targets=None, stage_workdir=None):
        if stage_workdir is None:
            stage_workdir = bool(configfile or folders or copied_targets)
        app_env = {
            IOS_TEST_AUTO_EXIT_ENV: '1',
        }
        if stage_workdir:
            app_env['DEFOLD_TEST_WORKDIR'] = '%s/%s' % (IOS_TEST_CONTAINER_ROOT, library_name)
            ready_paths = _launch_ready_paths(configfile, folders or {}, copied_targets=copied_targets)
            if ready_paths:
                app_env[IOS_TEST_READY_PATHS_ENV] = ';'.join(ready_paths)

        launch_env = _copy_env(self._env)
        for key, value in app_env.items():
            launch_env['SIMCTL_CHILD_%s' % key] = value

        args = [
            'xcrun', 'simctl',
            'launch',
            '--console',
            '--terminate-running-process',
        ]
        args.extend([simulator.udid, bundle_id])
        if configfile:
            args.append('./unittest.cfg')

        self._log('ios-test: %s' % subprocess.list2cmdline(args))
        try:
            return self._command_runner(args, env=launch_env).returncode
        except FileNotFoundError as e:
            raise IOSTestError('xcrun not found') from e

    def run_test(self, program, cwd, configfile=None, folders=None, target_name=None):
        cwd = os.path.abspath(cwd)
        program = os.path.abspath(program)
        if not os.path.exists(program):
            raise IOSTestError('iOS simulator test program not found: %s' % program)

        simulator = self.select_simulator()
        library_name = os.path.basename(os.path.normpath(cwd)) or 'test'
        bundle_id = make_bundle_id(target_name=target_name, program=program, bundle_id_prefix=self._bundle_id_prefix, env=self._env)

        self._log('ios-test: selected simulator %s' % self.simulator_display(simulator))

        with tempfile.TemporaryDirectory(prefix='defold-ios-sim-test-') as tmp:
            app_dir = os.path.join(tmp, '%s.app' % os.path.splitext(os.path.basename(program))[0])
            self._create_app_bundle(program, app_dir, bundle_id)
            self._sign_app_bundle(app_dir)

            self._boot_simulator_if_needed(simulator)
            self._uninstall_app(simulator, bundle_id)
            try:
                self._install_app(simulator, app_dir)
                stage_workdir = bool(configfile or folders)
                copied_targets = []
                if stage_workdir:
                    copied_targets = self._stage_to_simulator(simulator, bundle_id, library_name, cwd, configfile, folders or {})
                return self._launch_app(
                    simulator,
                    bundle_id,
                    library_name,
                    configfile,
                    folders or {},
                    copied_targets=copied_targets,
                    stage_workdir=stage_workdir)
            finally:
                self._uninstall_app(simulator, bundle_id)


def _parse_stage_args(stage_args):
    if not stage_args:
        return None
    folders = {}
    for source, target in stage_args:
        folders[source] = target
    return folders


def _ios_preflight_failed(message, strict, log_fn, subject='iOS device tests'):
    if strict:
        raise IOSTestError(message)
    _log(log_fn, '%s\nSkipping %s.' % (message, subject))
    return False


def _missing_profile_message(device, team_id=None, mobileprovision=None, bundle_id_prefix=None, profiles=None, errors=None):
    bundle_id_prefix = bundle_id_prefix or IOS_DEFAULT_BUNDLE_ID_PREFIX
    lines = [
        'No installed iOS provisioning profiles match device %s and generated test bundle ids with prefix %s' % (
            device.udid, bundle_id_prefix.rstrip('.')),
    ]
    if mobileprovision:
        lines.append('Explicit profile path checked: %s.' % os.path.abspath(os.path.expanduser(mobileprovision)))
    if team_id:
        lines.append('Current team filter: %s.' % team_id)

    if profiles:
        lines.extend(['', 'Checked provisioning profiles:'])
        for profile in profiles:
            reasons = profile_preflight_mismatch_reasons(
                profile,
                device.udid,
                team_id=team_id,
                bundle_id_prefix=bundle_id_prefix)
            if reasons:
                lines.append('  %s' % _profile_summary(profile))
                lines.extend('    - %s' % reason for reason in reasons)
            else:
                lines.append('  %s' % _profile_summary(profile))
                lines.append('    - matches device/team/bundle preflight filters')

    if errors:
        lines.extend(['', 'Provisioning profile read/decode errors:'])
        lines.extend('  %s' % error for error in errors)

    lines.extend([
        '',
        'How to fix:',
        '  1. Check the selected device:',
        '     python3 build_tools/build_ios.py list-devices',
        '     If the wrong device was selected, pass --test-device <udid-or-name> to build.py.',
        '  2. Check installed provisioning-profile teams:',
        '     python3 build_tools/build_ios.py list-teams',
        '  3. Download or create an iOS Development provisioning profile in Apple Developer that includes device UDID %s.' % device.udid,
        '  4. Install the .mobileprovision file by opening it in Xcode/Finder, or pass it explicitly:',
        '     ./scripts/build.py --platform=arm64-ios --ios-mobileprovision /path/to/profile.mobileprovision build_engine',
        '     python3 build_tools/build_ios.py can-run-tests --mobileprovision /path/to/profile.mobileprovision --device <udid-or-name>',
        '',
        'The profile must be for iOS, not expired, include the selected device UDID, and match the selected Apple team.',
        'The generated test app bundle ids use the default prefix %s, for example %s.<test-target>.' % (
            IOS_DEFAULT_BUNDLE_ID_PREFIX, IOS_DEFAULT_BUNDLE_ID_PREFIX),
        'If your profile uses another app id prefix, pass --ios-bundle-id-prefix <prefix>.',
        'This runner does not register devices or create/update Apple provisioning profiles.',
    ])
    return '\n'.join(lines)


def _multiple_profile_message(device, bundle_id_prefix, profiles):
    bundle_id_prefix = bundle_id_prefix or IOS_DEFAULT_BUNDLE_ID_PREFIX
    lines = [
        'Multiple iOS provisioning profiles match device %s and generated test bundle ids with prefix %s:' % (
            device.udid, bundle_id_prefix.rstrip('.')),
    ]
    lines.extend('  %s' % _profile_summary(profile) for profile in profiles)
    lines.extend([
        '',
        'Pass --ios-mobileprovision /path/to/profile.mobileprovision or IOS_MOBILEPROVISION to choose one explicitly.',
    ])
    return '\n'.join(lines)


def _has_signing_setup_for_device(env, device, identity=None, mobileprovision=None, team_id=None,
                                  bundle_id_prefix=None, command_runner=subprocess.run, log_fn=None, strict=False):
    bundle_id_prefix = bundle_id_prefix or env.get('IOS_TEST_BUNDLE_ID_PREFIX') or IOS_DEFAULT_BUNDLE_ID_PREFIX
    identities = find_codesign_identities(command_runner=command_runner, env=env)
    if not identities:
        return _ios_preflight_failed('No iOS code signing identities found', strict, log_fn)

    if mobileprovision:
        paths = [os.path.abspath(os.path.expanduser(mobileprovision))]
    else:
        paths = _collect_mobileprovision_paths()
    all_profiles, _errors = load_mobileprovision_profiles(paths, command_runner=command_runner, env=env)
    profiles = [
        profile for profile in all_profiles
        if profile_is_ios(profile) and
        profile_is_not_expired(profile) and
        profile_matches_device(profile, device.udid) and
        profile_matches_team(profile, team_id) and
        profile_matches_generated_test_bundle_ids(profile, bundle_id_prefix)
    ]

    if not profiles:
        return _ios_preflight_failed(
            _missing_profile_message(
                device,
                team_id=team_id,
                mobileprovision=mobileprovision,
                bundle_id_prefix=bundle_id_prefix,
                profiles=all_profiles,
                errors=_errors),
            strict,
            log_fn)

    if len(profiles) > 1:
        return _ios_preflight_failed(
            _multiple_profile_message(device, bundle_id_prefix, profiles),
            strict,
            log_fn)

    try:
        select_identity(profiles[0], identities, identity)
    except IOSTestError as e:
        return _ios_preflight_failed(str(e), strict, log_fn)

    return True


def can_run_tests_ios(log_fn=None, env=None, device=None, identity=None, mobileprovision=None,
                      team_id=None, bundle_id_prefix=None, command_runner=subprocess.run, strict=False):
    env = _copy_env(env)
    if shutil.which('xcrun', path=env.get('PATH')) is None:
        return _ios_preflight_failed('No xcrun found', strict, log_fn)

    runner = IOSTestRunner(
        env=env,
        log_fn=log_fn,
        device=device,
        identity=identity,
        mobileprovision=mobileprovision,
        team_id=team_id,
        bundle_id_prefix=bundle_id_prefix,
        command_runner=command_runner)

    try:
        selected_device = runner.select_device()
    except Exception as e:
        return _ios_preflight_failed(str(e), strict, log_fn)

    try:
        runner.ensure_device_ready(selected_device)
    except IOSTestError as e:
        return _ios_preflight_failed(str(e), strict, log_fn)

    return _has_signing_setup_for_device(
        env,
        selected_device,
        identity=identity or env.get('IOS_CODESIGN_IDENTITY'),
        mobileprovision=mobileprovision or env.get('IOS_MOBILEPROVISION'),
        team_id=team_id or env.get('IOS_DEVELOPMENT_TEAM'),
        bundle_id_prefix=bundle_id_prefix or env.get('IOS_TEST_BUNDLE_ID_PREFIX'),
        command_runner=command_runner,
        log_fn=log_fn,
        strict=strict)


def can_run_tests_ios_simulator(log_fn=None, env=None, device=None, bundle_id_prefix=None,
                                command_runner=subprocess.run, strict=False):
    env = _copy_env(env)
    if shutil.which('xcrun', path=env.get('PATH')) is None:
        return _ios_preflight_failed('No xcrun found', strict, log_fn, subject='iOS simulator tests')

    runner = IOSSimulatorTestRunner(
        env=env,
        log_fn=log_fn,
        device=device,
        bundle_id_prefix=bundle_id_prefix,
        command_runner=command_runner)

    try:
        runner.select_simulator()
    except Exception as e:
        return _ios_preflight_failed(str(e), strict, log_fn, subject='iOS simulator tests')
    return True


def can_run_tests_for_platform(target_platform, log_fn=None, env=None, device=None, identity=None,
                               mobileprovision=None, team_id=None, bundle_id_prefix=None,
                               command_runner=subprocess.run, strict=False):
    if target_platform == IOS_SIMULATOR_PLATFORM:
        return can_run_tests_ios_simulator(
            log_fn=log_fn,
            env=env,
            device=device,
            bundle_id_prefix=bundle_id_prefix,
            command_runner=command_runner,
            strict=strict)

    if target_platform == IOS_DEVICE_PLATFORM:
        return can_run_tests_ios(
            log_fn=log_fn,
            env=env,
            device=device,
            identity=identity,
            mobileprovision=mobileprovision,
            team_id=team_id,
            bundle_id_prefix=bundle_id_prefix,
            command_runner=command_runner,
            strict=strict)

    return False


def _create_argument_parser():
    parser = argparse.ArgumentParser(description='iOS test helper utilities')
    subparsers = parser.add_subparsers(dest='command', required=True)

    parser_can_run = subparsers.add_parser('can-run-tests', help='Check if iOS device tests can run')
    parser_can_run.add_argument('--platform', choices=('device', 'simulator'), default='device',
        help='Run target type. Defaults to physical device.')
    parser_can_run.add_argument('--device', help='iOS device or simulator identifier, name, serial number or UDID')
    parser_can_run.add_argument('--identity', help='iOS code signing identity name or SHA-1')
    parser_can_run.add_argument('--mobileprovision', help='Path to a .mobileprovision file')
    parser_can_run.add_argument('--team-id', help='Apple development team id')
    parser_can_run.add_argument('--bundle-id-prefix', help='Bundle id prefix for generated test apps')

    parser_run = subparsers.add_parser('run-test', help='Package, install and run one iOS test binary on device')
    parser_run.add_argument('--platform', choices=('device', 'simulator'), default='device',
        help='Run target type. Defaults to physical device.')
    parser_run.add_argument('--cwd', required=True, help='Library working directory')
    parser_run.add_argument('--program', required=True, help='Path to the built test program')
    parser_run.add_argument('--target', help='CMake target name used for the generated bundle id')
    parser_run.add_argument('--config', help='Optional test config file path relative to cwd')
    parser_run.add_argument('--device', help='iOS device or simulator identifier, name, serial number or UDID')
    parser_run.add_argument('--identity', help='iOS code signing identity name or SHA-1')
    parser_run.add_argument('--mobileprovision', help='Path to a .mobileprovision file')
    parser_run.add_argument('--team-id', help='Apple development team id')
    parser_run.add_argument('--bundle-id-prefix', help='Bundle id prefix for generated test apps')
    parser_run.add_argument('--stage', action='append', nargs=2, metavar=('SOURCE', 'TARGET'),
        help='Stage SOURCE from cwd to TARGET under the iOS test working directory')

    parser_list_devices = subparsers.add_parser('list-devices', help='List connected iOS devices known to devicectl')
    parser_list_devices.add_argument('--device', help='Optional iOS device identifier/name/UDID filter')

    parser_list_simulators = subparsers.add_parser('list-simulators', help='List available iOS simulators known to simctl')
    parser_list_simulators.add_argument('--device', help='Optional iOS simulator identifier/name/UDID filter')

    subparsers.add_parser('list-identities', help='List installed code signing identities')

    parser_list_teams = subparsers.add_parser('list-teams', help='List Apple development teams found in provisioning profiles')
    parser_list_teams.add_argument('--mobileprovision', action='append', dest='mobileprovisions',
        help='Additional .mobileprovision file to inspect; can be passed more than once')

    return parser


def main(argv=None):
    parser = _create_argument_parser()
    args = parser.parse_args(argv)

    env = dict(os.environ)
    if getattr(args, 'device', None):
        env['IOS_DEVICE_ID'] = args.device
    if getattr(args, 'identity', None):
        env['IOS_CODESIGN_IDENTITY'] = args.identity
    if getattr(args, 'mobileprovision', None):
        env['IOS_MOBILEPROVISION'] = args.mobileprovision
    if getattr(args, 'team_id', None):
        env['IOS_DEVELOPMENT_TEAM'] = args.team_id
    if getattr(args, 'bundle_id_prefix', None):
        env['IOS_TEST_BUNDLE_ID_PREFIX'] = args.bundle_id_prefix

    if args.command == 'can-run-tests':
        if args.platform == 'simulator':
            return 0 if can_run_tests_ios_simulator(
                print,
                env=env,
                device=args.device,
                bundle_id_prefix=args.bundle_id_prefix) else 1
        return 0 if can_run_tests_ios(
            print,
            env=env,
            device=args.device,
            identity=args.identity,
            mobileprovision=args.mobileprovision,
            team_id=args.team_id,
            bundle_id_prefix=args.bundle_id_prefix) else 1

    try:
        if args.command == 'list-devices':
            runner = IOSTestRunner(env=env, log_fn=print, device=getattr(args, 'device', None))
            devices = runner.list_devices()
            if args.device:
                devices = [device for device in devices if device_matches_request(device, args.device)]
            for device in devices:
                print('%s\t%s\t%s' % (device.udid, device.name, device.device_id))
            return 0

        if args.command == 'list-simulators':
            runner = IOSSimulatorTestRunner(env=env, log_fn=print, device=getattr(args, 'device', None))
            simulators = runner.list_simulators()
            if args.device:
                simulators = [simulator for simulator in simulators if simulator_matches_request(simulator, args.device)]
            for simulator in simulators:
                print('%s\t%s\t%s\t%s' % (simulator.udid, simulator.name, simulator.state, simulator.runtime))
            return 0

        if args.command == 'list-identities':
            for identity in find_codesign_identities(env=env):
                print('%s\t%s' % (identity.sha1, identity.name))
            return 0

        if args.command == 'list-teams':
            teams, errors = find_development_teams(extra_paths=args.mobileprovisions, env=env)
            for error in errors:
                print('warning: %s' % error, file=sys.stderr)
            for team in teams:
                print('%s\t%s\t%d\t%d\t%d' % (
                    team.team_id,
                    team.name,
                    team.profile_count,
                    team.ios_profile_count,
                    team.valid_ios_profile_count))
            return 0

        if args.command == 'run-test':
            if args.platform == 'simulator':
                runner = IOSSimulatorTestRunner(
                    env=env,
                    log_fn=print,
                    device=getattr(args, 'device', None),
                    bundle_id_prefix=getattr(args, 'bundle_id_prefix', None))
            else:
                runner = IOSTestRunner(
                    env=env,
                    log_fn=print,
                    device=getattr(args, 'device', None),
                    identity=getattr(args, 'identity', None),
                    mobileprovision=getattr(args, 'mobileprovision', None),
                    team_id=getattr(args, 'team_id', None),
                    bundle_id_prefix=getattr(args, 'bundle_id_prefix', None))
            return runner.run_test(
                args.program,
                args.cwd,
                configfile=args.config,
                folders=_parse_stage_args(args.stage),
                target_name=args.target)
    except IOSTestError as e:
        print(e, file=sys.stderr)
        return 1

    parser.error('Unknown command: %s' % args.command)
    return 2


if __name__ == '__main__':
    sys.exit(main())
