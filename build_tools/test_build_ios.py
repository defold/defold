import datetime
import json
import os
import plistlib
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import build_ios


class Completed(object):
    def __init__(self, returncode=0, stdout=b'', stderr=b''):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def make_profile(**kwargs):
    values = {
        'path': '/tmp/profile.mobileprovision',
        'name': 'Profile',
        'uuid': 'PROFILE-UUID',
        'team_ids': ['TEAMID'],
        'platforms': ['iOS'],
        'expiration': datetime.datetime.now() + datetime.timedelta(days=1),
        'provisioned_devices': ['DEVICEUDID'],
        'entitlements': {
            'application-identifier': 'TEAMID.com.defold.tests.*',
            'keychain-access-groups': ['TEAMID.*'],
        },
        'developer_certificate_sha1s': {'A' * 40},
        'team_name': 'Defold Team',
    }
    values.update(kwargs)
    return build_ios.IOSProvisioningProfile(**values)


def make_devicectl_runner(lock_state=False, identity_output=None, developer_services=True):
    def runner(cmd, **kwargs):
        if len(cmd) >= 6 and cmd[0:3] == ['xcrun', 'devicectl', '--timeout'] and cmd[4] == '--json-output':
            json_path = cmd[5]
            subcommand = cmd[6:]
            if subcommand == ['list', 'devices']:
                data = {
                    'result': {
                        'devices': [{
                            'identifier': 'core-device-id',
                            'deviceProperties': {'name': 'Phone'},
                            'hardwareProperties': {
                                'platform': 'iOS',
                                'udid': 'DEVICEUDID',
                            },
                            'connectionProperties': {'transportType': 'wired'},
                        }]
                    }
                }
                with open(json_path, 'w') as f:
                    json.dump(data, f)
                return Completed(0)

            if subcommand == ['device', 'info', 'lockState', '--device', 'core-device-id']:
                if lock_state == 'unavailable':
                    return Completed(1, b'', b'could not read lock state')
                with open(json_path, 'w') as f:
                    if lock_state in ('unknown', 'undecided'):
                        json.dump({'result': {'lockState': lock_state}}, f)
                    else:
                        json.dump({'result': {'locked': lock_state}}, f)
                return Completed(0)

            if subcommand == ['device', 'info', 'ddiServices', '--device', 'core-device-id']:
                if developer_services == 'locked':
                    return Completed(1, b'', b'kAMDMobileImageMounterDeviceLocked: The device is locked.')
                if developer_services == 'unavailable':
                    return Completed(1, b'', b'developer disk image services are unavailable')
                with open(json_path, 'w') as f:
                    json.dump({'result': {'ddiMetadata': {'isUsable': developer_services is not False}}}, f)
                return Completed(0)

        if cmd == ['security', 'find-identity', '-v', '-p', 'codesigning']:
            return Completed(0, identity_output or b'', b'')

        return Completed(1, b'', b'unsupported command')

    return runner


class BuildIOSTest(unittest.TestCase):
    def test_make_bundle_id_uses_defold_tests_default_prefix(self):
        self.assertEqual(
            'com.defold.tests.test-engine',
            build_ios.make_bundle_id(target_name='test_engine', env={}))

    def test_ios_build_options_are_applied_to_environment(self):
        env = {}

        result = build_ios.apply_build_options_to_env(
            env,
            build_ios.IOS_SIMULATOR_PLATFORM,
            test_device='SIM-UDID',
            identity='Apple Development: Test',
            mobileprovision='/tmp/profile.mobileprovision',
            team_id='TEAMID',
            bundle_id_prefix='com.example.tests')

        self.assertIs(env, result)
        self.assertEqual('SIM-UDID', env['IOS_DEVICE_ID'])
        self.assertEqual('SIM-UDID', env['IOS_SIMULATOR_ID'])
        self.assertEqual('Apple Development: Test', env['IOS_CODESIGN_IDENTITY'])
        self.assertEqual('/tmp/profile.mobileprovision', env['IOS_MOBILEPROVISION'])
        self.assertEqual('TEAMID', env['IOS_DEVELOPMENT_TEAM'])
        self.assertEqual('com.example.tests', env['IOS_TEST_BUNDLE_ID_PREFIX'])

    def test_ios_test_cmake_args_only_emit_explicit_prefix(self):
        self.assertEqual([], build_ios.ios_test_cmake_args('arm64-macos', bundle_id_prefix='com.example.tests'))
        self.assertEqual([], build_ios.ios_test_cmake_args(build_ios.IOS_DEVICE_PLATFORM, env={}))
        self.assertEqual(
            ['-DDEFOLD_IOS_TEST_BUNDLE_ID_PREFIX:STRING=com.example.tests'],
            build_ios.ios_test_cmake_args(build_ios.IOS_DEVICE_PLATFORM, bundle_id_prefix='com.example.tests'))
        self.assertEqual(
            ['-DDEFOLD_IOS_TEST_BUNDLE_ID_PREFIX:STRING=com.env.tests'],
            build_ios.ios_test_cmake_args(build_ios.IOS_SIMULATOR_PLATFORM, env={'IOS_TEST_BUNDLE_ID_PREFIX': 'com.env.tests'}))

    def test_xcode_solution_bundle_id_uses_profile_pattern(self):
        self.assertEqual(
            'com.example.dmengine',
            build_ios.xcode_solution_bundle_id(make_profile(entitlements={'application-identifier': 'TEAMID.com.example.*'})))
        self.assertEqual(
            'com.example.exact',
            build_ios.xcode_solution_bundle_id(make_profile(entitlements={'application-identifier': 'TEAMID.com.example.exact'})))
        self.assertEqual(
            build_ios.IOS_XCODE_SOLUTION_DEFAULT_BUNDLE_ID,
            build_ios.xcode_solution_bundle_id(make_profile(entitlements={'application-identifier': 'TEAMID.*'})))

    def test_xcode_solution_signing_cmake_args_installs_profile(self):
        original_profile_dir = build_ios.IOS_PROVISIONING_PROFILE_DIR
        original_parse = build_ios.parse_mobileprovision
        original_find = build_ios.find_codesign_identities

        try:
            with tempfile.TemporaryDirectory() as tmp:
                profile_path = os.path.join(tmp, 'Profile.mobileprovision')
                install_dir = os.path.join(tmp, 'Provisioning Profiles')
                with open(profile_path, 'wb') as f:
                    f.write(b'profile')

                profile = make_profile(
                    path=profile_path,
                    name='Dev Profile',
                    uuid='PROFILE-UUID',
                    entitlements={'application-identifier': 'TEAMID.com.example.*'})
                build_ios.IOS_PROVISIONING_PROFILE_DIR = install_dir
                build_ios.parse_mobileprovision = lambda *_args, **_kwargs: profile
                build_ios.find_codesign_identities = lambda **_kwargs: [
                    build_ios.IOSCodeSignIdentity('A' * 40, 'Apple Development: Test')
                ]

                logs = []
                args = build_ios.xcode_solution_signing_cmake_args(
                    mobileprovision=profile_path,
                    env={},
                    log_fn=logs.append)

                installed_profile = os.path.join(install_dir, 'PROFILE-UUID.mobileprovision')
                self.assertTrue(os.path.exists(installed_profile))
                self.assertIn('-DDEFOLD_IOS_MOBILEPROVISION:PATH=%s' % installed_profile, args)
                self.assertIn('-DDEFOLD_IOS_DEVELOPMENT_TEAM:STRING=TEAMID', args)
                self.assertIn('-DDEFOLD_IOS_PROVISIONING_PROFILE_UUID:STRING=PROFILE-UUID', args)
                self.assertIn('-DDEFOLD_IOS_PROVISIONING_PROFILE_SPECIFIER:STRING=Dev Profile', args)
                self.assertIn('-DDEFOLD_IOS_CODESIGN_IDENTITY:STRING=Apple Development: Test', args)
                self.assertIn('-DDEFOLD_IOS_BUNDLE_IDENTIFIER:STRING=com.example.dmengine', args)
                self.assertTrue(any('Using iOS Xcode signing profile' in log for log in logs))
        finally:
            build_ios.IOS_PROVISIONING_PROFILE_DIR = original_profile_dir
            build_ios.parse_mobileprovision = original_parse
            build_ios.find_codesign_identities = original_find

    def test_decode_mobileprovision_falls_back_to_openssl(self):
        plist = plistlib.dumps({'Name': 'Fallback'})
        commands = []

        def runner(cmd, **kwargs):
            commands.append(cmd)
            if cmd[0] == 'security':
                return Completed(1, b'', b'problem decoding')
            return Completed(0, plist, b'Verification successful')

        decoded = build_ios.decode_mobileprovision('/tmp/profile.mobileprovision', command_runner=runner)

        self.assertEqual('Fallback', decoded['Name'])
        self.assertEqual('security', commands[0][0])
        self.assertEqual(['openssl', 'smime'], commands[1][0:2])

    def test_profile_filtering_by_platform_expiry_device_team_and_bundle(self):
        profile = make_profile()

        self.assertTrue(build_ios.profile_matches(profile, 'DEVICEUDID', 'com.defold.tests.test-engine', 'TEAMID'))
        self.assertFalse(build_ios.profile_matches(profile, 'OTHERDEVICE', 'com.defold.tests.test-engine', 'TEAMID'))
        self.assertFalse(build_ios.profile_matches(profile, 'DEVICEUDID', 'com.other.test', 'TEAMID'))
        self.assertFalse(build_ios.profile_matches(profile, 'DEVICEUDID', 'com.defold.tests.test-engine', 'OTHERTEAM'))
        self.assertFalse(build_ios.profile_matches(
            make_profile(platforms=['tvOS']), 'DEVICEUDID', 'com.defold.tests.test-engine', 'TEAMID'))
        self.assertFalse(build_ios.profile_matches(
            make_profile(expiration=datetime.datetime.now() - datetime.timedelta(days=1)),
            'DEVICEUDID', 'com.defold.tests.test-engine', 'TEAMID'))

    def test_generated_bundle_id_preflight_rejects_exact_profile(self):
        profile = make_profile(
            entitlements={'application-identifier': 'TEAMID.com.defold.tests.test-testmain'})

        self.assertFalse(build_ios.profile_matches_generated_test_bundle_ids(profile, 'com.defold.tests'))
        reasons = build_ios.profile_preflight_mismatch_reasons(
            profile,
            'DEVICEUDID',
            'TEAMID',
            bundle_id_prefix='com.defold.tests')

        self.assertTrue(any('does not match generated test bundle ids' in reason for reason in reasons))

    def test_profile_preflight_mismatch_reasons_explain_device_team_platform_and_expiry(self):
        profile = make_profile(
            platforms=['tvOS'],
            expiration=datetime.datetime.now() - datetime.timedelta(days=1),
            provisioned_devices=['OTHERDEVICE'],
            team_ids=['OTHERTEAM'],
            entitlements={'application-identifier': 'OTHERTEAM.com.defold.tests.*'})

        reasons = build_ios.profile_preflight_mismatch_reasons(profile, 'DEVICEUDID', 'TEAMID')

        self.assertTrue(any('expected iOS' in reason for reason in reasons))
        self.assertTrue(any('expired at' in reason for reason in reasons))
        self.assertTrue(any('does not include selected device UDID DEVICEUDID' in reason for reason in reasons))
        self.assertTrue(any('team ids are OTHERTEAM, expected TEAMID' in reason for reason in reasons))

    def test_identity_selection_matches_profile_certificate(self):
        profile = make_profile(developer_certificate_sha1s={'B' * 40})
        identities = [
            build_ios.IOSCodeSignIdentity('A' * 40, 'Apple Development: Other'),
            build_ios.IOSCodeSignIdentity('B' * 40, 'Apple Development: Match'),
        ]

        self.assertEqual('Apple Development: Match', build_ios.select_identity(profile, identities).name)
        self.assertEqual('Apple Development: Match', build_ios.select_identity(profile, identities, 'Match').name)
        with self.assertRaises(build_ios.IOSTestError):
            build_ios.select_identity(profile, identities, 'Other')

    def test_find_codesign_identities_parses_security_output(self):
        output = (
            '  1) AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA "Apple Development: One (TEAMID)"\n'
            '  2) BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB "Apple Distribution: Two (OTHER)"\n'
            '     2 valid identities found\n'
        ).encode('utf-8')

        def runner(cmd, **kwargs):
            self.assertEqual(['security', 'find-identity', '-v', '-p', 'codesigning'], cmd)
            return Completed(0, output, b'')

        identities = build_ios.find_codesign_identities(command_runner=runner)

        self.assertEqual(2, len(identities))
        self.assertEqual('A' * 40, identities[0].sha1)
        self.assertEqual('Apple Development: One (TEAMID)', identities[0].name)

    def test_development_teams_are_collected_from_profiles(self):
        teams = build_ios.development_teams_from_profiles([
            make_profile(team_ids=['TEAMID'], team_name='Defold Team'),
            make_profile(team_ids=['TEAMID'], team_name='Defold Team', platforms=['tvOS']),
            make_profile(team_ids=['OTHERTEAM'], team_name='Other Team'),
        ])

        self.assertEqual(['OTHERTEAM', 'TEAMID'], [team.team_id for team in teams])
        team = teams[1]
        self.assertEqual('Defold Team', team.name)
        self.assertEqual(2, team.profile_count)
        self.assertEqual(1, team.ios_profile_count)
        self.assertEqual(1, team.valid_ios_profile_count)

    def test_device_matches_request_uses_raw_identifiers(self):
        device = build_ios.IOSDevice(
            'device-id',
            'DEVICEUDID',
            'Phone',
            'iOS',
            {'serialNumber': 'SERIAL123', 'connectionProperties': {'dnsName': 'phone.local'}})

        self.assertTrue(build_ios.device_matches_request(device, 'Phone'))
        self.assertTrue(build_ios.device_matches_request(device, 'SERIAL123'))
        self.assertFalse(build_ios.device_matches_request(device, 'Other Phone'))

    def test_devices_from_devicectl_json_deduplicates_nested_hardware_records(self):
        data = {
            'result': {
                'devices': [{
                    'identifier': '220A3F37-CA47-59AE-89C9-08D6D2E3DAA5',
                    'deviceProperties': {'name': 'Mathias iPhone'},
                    'hardwareProperties': {
                        'platform': 'iOS',
                        'udid': '00008020-001954A63A88002E',
                    },
                    'connectionProperties': {'transportType': 'wired'},
                }]
            }
        }

        devices = build_ios.devices_from_devicectl_json(data)

        self.assertEqual(1, len(devices))
        self.assertEqual('220A3F37-CA47-59AE-89C9-08D6D2E3DAA5', devices[0].device_id)
        self.assertEqual('00008020-001954A63A88002E', devices[0].udid)
        self.assertEqual('Mathias iPhone', devices[0].name)

    def test_devices_from_devicectl_json_prefers_connected_devices(self):
        data = {
            'result': {
                'devices': [
                    {
                        'identifier': '220A3F37-CA47-59AE-89C9-08D6D2E3DAA5',
                        'deviceProperties': {'name': 'Connected iPhone'},
                        'hardwareProperties': {
                            'platform': 'iOS',
                            'udid': '00008020-001954A63A88002E',
                        },
                        'connectionProperties': {'transportType': 'wired'},
                    },
                    {
                        'identifier': '30BEF47E-5833-5203-B40C-74CD05BDD26D',
                        'deviceProperties': {'name': 'Remembered iPhone'},
                        'hardwareProperties': {
                            'platform': 'iOS',
                            'udid': '00008150-000209E43647401C',
                        },
                        'connectionProperties': {'connectionState': 'unavailable'},
                    },
                ]
            }
        }

        devices = build_ios.devices_from_devicectl_json(data)

        self.assertEqual(1, len(devices))
        self.assertEqual('Connected iPhone', devices[0].name)
        self.assertEqual('00008020-001954A63A88002E', devices[0].udid)

    def test_simulators_from_simctl_json_keeps_available_ios_simulators(self):
        data = {
            'devices': {
                'com.apple.CoreSimulator.SimRuntime.iOS-18-0': [
                    {'udid': 'SIM-1', 'name': 'iPhone 16', 'state': 'Booted', 'isAvailable': True},
                    {'udid': 'SIM-2', 'name': 'iPhone 15', 'state': 'Shutdown', 'isAvailable': False},
                ],
                'com.apple.CoreSimulator.SimRuntime.tvOS-18-0': [
                    {'udid': 'TV-1', 'name': 'Apple TV', 'state': 'Booted', 'isAvailable': True},
                ],
            }
        }

        simulators = build_ios.simulators_from_simctl_json(data)

        self.assertEqual(1, len(simulators))
        self.assertEqual('SIM-1', simulators[0].udid)
        self.assertEqual('iPhone 16', simulators[0].name)

    def test_select_simulator_has_actionable_message_when_none_are_available(self):
        runner = build_ios.IOSSimulatorTestRunner()
        runner.list_simulators = lambda: []

        with self.assertRaisesRegex(build_ios.IOSTestError, 'xcodebuild -downloadPlatform iOS -architectureVariant universal') as cm:
            runner.select_simulator()

        message = str(cm.exception)
        self.assertIn('Xcode Settings > Platforms', message)
        self.assertIn('python3 build_tools/build_ios.py list-simulators', message)

    def test_select_simulator_uses_single_available_simulator(self):
        runner = build_ios.IOSSimulatorTestRunner()
        simulator = build_ios.IOSSimulator('SIM-1', 'iPhone 16', 'com.apple.CoreSimulator.SimRuntime.iOS-18-0', 'Shutdown', {})
        runner.list_simulators = lambda: [simulator]

        self.assertEqual(simulator, runner.select_simulator())

    def test_select_simulator_lists_candidates_when_multiple_are_available_but_none_booted(self):
        runner = build_ios.IOSSimulatorTestRunner()
        runner.list_simulators = lambda: [
            build_ios.IOSSimulator('SIM-1', 'iPhone 16', 'com.apple.CoreSimulator.SimRuntime.iOS-18-0', 'Shutdown', {}),
            build_ios.IOSSimulator('SIM-2', 'iPhone 15', 'com.apple.CoreSimulator.SimRuntime.iOS-18-0', 'Shutdown', {}),
        ]

        with self.assertRaisesRegex(build_ios.IOSTestError, 'No booted iOS simulator found') as cm:
            runner.select_simulator()

        message = str(cm.exception)
        self.assertIn('iPhone 16 (SIM-1, Shutdown)', message)
        self.assertIn('iPhone 15 (SIM-2, Shutdown)', message)

    def test_devicectl_timeout_can_be_overridden_by_environment(self):
        commands = []

        def runner(cmd, **kwargs):
            commands.append(cmd)
            json_path = cmd[5]
            with open(json_path, 'w') as f:
                json.dump({'result': {'devices': []}}, f)
            return Completed(0)

        runner = build_ios.IOSTestRunner(env={'IOS_DEVICECTL_TIMEOUT': '123'}, command_runner=runner)

        self.assertEqual([], runner.list_devices())
        self.assertEqual('123', commands[0][3])

    def test_devicectl_has_python_subprocess_timeout(self):
        timeouts = []

        def runner(cmd, **kwargs):
            timeout = kwargs.get('timeout')
            timeouts.append(timeout)
            raise subprocess.TimeoutExpired(cmd, timeout)

        runner = build_ios.IOSTestRunner(
            env={'IOS_DEVICECTL_TIMEOUT': '7', 'IOS_DEVICECTL_RETRIES': '0'},
            command_runner=runner)

        with self.assertRaisesRegex(build_ios.IOSTestError, 'timed out after 22 seconds'):
            runner.list_devices()

        self.assertEqual([22], timeouts)

    def test_devicectl_retries_transient_coredevice_startup_error(self):
        calls = []
        original_sleep = build_ios.time.sleep
        try:
            build_ios.time.sleep = lambda _seconds: None

            def runner(cmd, **kwargs):
                calls.append(cmd)
                if len(calls) == 1:
                    return Completed(1, b'', b'Timed out waiting for CoreDeviceService to fully initialize')
                json_path = cmd[5]
                with open(json_path, 'w') as f:
                    json.dump({
                        'result': {
                            'devices': [{
                                'identifier': 'core-device-id',
                                'deviceProperties': {'name': 'Phone'},
                                'hardwareProperties': {'platform': 'iOS', 'udid': 'DEVICEUDID'},
                                'connectionProperties': {'transportType': 'wired'},
                            }]
                        }
                    }, f)
                return Completed(0)

            runner = build_ios.IOSTestRunner(env={'IOS_DEVICECTL_RETRIES': '1'}, command_runner=runner)

            devices = runner.list_devices()

            self.assertEqual(2, len(calls))
            self.assertEqual(1, len(devices))
            self.assertEqual('DEVICEUDID', devices[0].udid)
        finally:
            build_ios.time.sleep = original_sleep

    def test_devicectl_exhausted_coredevice_retries_have_actionable_message(self):
        original_sleep = build_ios.time.sleep
        try:
            build_ios.time.sleep = lambda _seconds: None

            def runner(cmd, **kwargs):
                return Completed(1, b'', b'Timed out waiting for CoreDeviceService to fully initialize')

            runner = build_ios.IOSTestRunner(env={'IOS_DEVICECTL_RETRIES': '1'}, command_runner=runner)

            with self.assertRaisesRegex(build_ios.IOSTestError, 'restart Apple CoreDeviceService'):
                runner.list_devices()
        finally:
            build_ios.time.sleep = original_sleep

    def test_select_device_uses_single_reachable_device_when_multiple_are_known(self):
        runner = build_ios.IOSTestRunner()
        devices = [
            build_ios.IOSDevice('core-device-a', 'UDID-A', 'MaWe iPhone', 'iOS', {}),
            build_ios.IOSDevice('core-device-b', 'UDID-B', 'MaWe iPhone', 'iOS', {}),
        ]
        probes = []

        runner.list_devices = lambda: devices

        def fake_devicectl_json(subcommand, timeout=60, allow_failure=False):
            probes.append(subcommand)
            if subcommand[-1] == 'core-device-a':
                return {'result': {'locked': False}}
            return None

        runner._devicectl_json = fake_devicectl_json

        selected = runner.select_device()

        self.assertEqual('core-device-a', selected.device_id)
        self.assertEqual([
            ['device', 'info', 'lockState', '--device', 'core-device-a'],
            ['device', 'info', 'lockState', '--device', 'core-device-b'],
        ], probes)

    def test_select_device_prefers_unlocked_over_undecided_device_with_same_name(self):
        runner = build_ios.IOSTestRunner()
        devices = [
            build_ios.IOSDevice('core-device-a', 'UDID-A', 'MaWe iPhone', 'iOS', {}),
            build_ios.IOSDevice('core-device-b', 'UDID-B', 'MaWe iPhone', 'iOS', {}),
        ]

        runner.list_devices = lambda: devices

        def fake_devicectl_json(subcommand, timeout=60, allow_failure=False):
            if subcommand[-1] == 'core-device-a':
                return {'result': {'locked': False}}
            return {'result': {'lockState': 'undecided'}}

        runner._devicectl_json = fake_devicectl_json

        selected = runner.select_device()

        self.assertEqual('core-device-a', selected.device_id)

    def test_device_lock_state_from_json_detects_locked_and_unlocked(self):
        self.assertTrue(build_ios.device_lock_state_from_json({'result': {'locked': True}}))
        self.assertFalse(build_ios.device_lock_state_from_json({'result': {'lockState': 'unlocked'}}))
        self.assertIsNone(build_ios.device_lock_state_from_json({'result': {'lockState': 'undecided'}}))

    def test_can_run_tests_ios_strict_fails_on_locked_device(self):
        original_which = build_ios.shutil.which
        try:
            build_ios.shutil.which = lambda tool, path=None: '/usr/bin/xcrun' if tool == 'xcrun' else None

            with self.assertRaisesRegex(build_ios.IOSTestError, 'is locked'):
                build_ios.can_run_tests_ios(
                    env={},
                    command_runner=make_devicectl_runner(lock_state=True),
                    strict=True)
        finally:
            build_ios.shutil.which = original_which

    def test_can_run_tests_ios_strict_fails_when_lock_state_is_unreadable(self):
        original_which = build_ios.shutil.which
        try:
            build_ios.shutil.which = lambda tool, path=None: '/usr/bin/xcrun' if tool == 'xcrun' else None

            with self.assertRaisesRegex(build_ios.IOSTestError, 'Could not read lock state'):
                build_ios.can_run_tests_ios(
                    env={},
                    command_runner=make_devicectl_runner(lock_state='unavailable'),
                    strict=True)
        finally:
            build_ios.shutil.which = original_which

    def test_can_run_tests_ios_strict_continues_quietly_when_lock_state_is_undecided_but_services_work(self):
        original_which = build_ios.shutil.which
        logs = []
        try:
            build_ios.shutil.which = lambda tool, path=None: '/usr/bin/xcrun' if tool == 'xcrun' else None

            with self.assertRaisesRegex(build_ios.IOSTestError, 'No iOS code signing identities found'):
                build_ios.can_run_tests_ios(
                    log_fn=logs.append,
                    env={},
                    command_runner=make_devicectl_runner(lock_state='undecided'),
                    strict=True)
            self.assertFalse(any('devicectl reported lock state "undecided"' in log for log in logs))
        finally:
            build_ios.shutil.which = original_which

    def test_can_run_tests_ios_strict_fails_when_developer_services_report_locked(self):
        original_which = build_ios.shutil.which
        try:
            build_ios.shutil.which = lambda tool, path=None: '/usr/bin/xcrun' if tool == 'xcrun' else None

            with self.assertRaisesRegex(build_ios.IOSTestError, 'unlock it and retry'):
                build_ios.can_run_tests_ios(
                    env={},
                    command_runner=make_devicectl_runner(
                        lock_state='undecided',
                        developer_services='locked'),
                    strict=True)
        finally:
            build_ios.shutil.which = original_which

    def test_run_test_device_readiness_reports_locked_device(self):
        runner = build_ios.IOSTestRunner(command_runner=make_devicectl_runner(lock_state=True))
        device = build_ios.IOSDevice('core-device-id', 'DEVICEUDID', 'Phone', 'iOS', {})

        with self.assertRaisesRegex(build_ios.IOSTestError, 'unlock it and retry'):
            runner.ensure_device_ready(device, refresh=True)

    def test_can_run_tests_ios_strict_fails_on_missing_profile(self):
        original_which = build_ios.shutil.which
        identity_output = (
            '  1) AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA "Apple Development: One (TEAMID)"\n'
            '     1 valid identities found\n'
        ).encode('utf-8')
        try:
            build_ios.shutil.which = lambda tool, path=None: '/usr/bin/xcrun' if tool == 'xcrun' else None

            with self.assertRaisesRegex(build_ios.IOSTestError, 'No installed iOS provisioning profiles match device DEVICEUDID') as cm:
                build_ios.can_run_tests_ios(
                    env={},
                    mobileprovision='/tmp/does-not-exist.mobileprovision',
                    command_runner=make_devicectl_runner(lock_state=False, identity_output=identity_output),
                    strict=True)

            message = str(cm.exception)
            self.assertIn('How to fix:', message)
            self.assertIn('python3 build_tools/build_ios.py list-teams', message)
            self.assertIn('includes device UDID DEVICEUDID', message)
            self.assertIn('--ios-mobileprovision /path/to/profile.mobileprovision', message)
            self.assertIn('--ios-bundle-id-prefix <prefix>', message)
            self.assertIn('Explicit profile path checked: /tmp/does-not-exist.mobileprovision.', message)
            self.assertIn('Provisioning profile read/decode errors:', message)
        finally:
            build_ios.shutil.which = original_which

    def test_signing_preflight_filters_profile_by_bundle_prefix(self):
        original_find = build_ios.find_codesign_identities
        original_load = build_ios.load_mobileprovision_profiles
        try:
            build_ios.find_codesign_identities = lambda **_kwargs: [
                build_ios.IOSCodeSignIdentity('A' * 40, 'Apple Development: Test')
            ]
            build_ios.load_mobileprovision_profiles = lambda *_args, **_kwargs: ([
                make_profile(entitlements={'application-identifier': 'TEAMID.com.other.tests.*'})
            ], [])

            device = build_ios.IOSDevice('device-id', 'DEVICEUDID', 'Phone', 'iOS', {})
            with self.assertRaisesRegex(build_ios.IOSTestError, 'generated test bundle ids with prefix com.defold.tests') as cm:
                build_ios._has_signing_setup_for_device(
                    {},
                    device,
                    bundle_id_prefix='com.defold.tests',
                    strict=True)

            self.assertIn('app id com.other.tests.* does not match generated test bundle ids', str(cm.exception))
        finally:
            build_ios.find_codesign_identities = original_find
            build_ios.load_mobileprovision_profiles = original_load

    def test_signing_preflight_requires_identity_matching_profile(self):
        original_find = build_ios.find_codesign_identities
        original_load = build_ios.load_mobileprovision_profiles
        try:
            build_ios.find_codesign_identities = lambda **_kwargs: [
                build_ios.IOSCodeSignIdentity('A' * 40, 'Apple Development: Test')
            ]
            build_ios.load_mobileprovision_profiles = lambda *_args, **_kwargs: ([
                make_profile(developer_certificate_sha1s={'B' * 40})
            ], [])

            device = build_ios.IOSDevice('device-id', 'DEVICEUDID', 'Phone', 'iOS', {})
            with self.assertRaisesRegex(build_ios.IOSTestError, 'No installed code signing identity matches provisioning profile'):
                build_ios._has_signing_setup_for_device(
                    {},
                    device,
                    bundle_id_prefix='com.defold.tests',
                    strict=True)
        finally:
            build_ios.find_codesign_identities = original_find
            build_ios.load_mobileprovision_profiles = original_load

    def test_signing_preflight_fails_on_ambiguous_profiles(self):
        original_find = build_ios.find_codesign_identities
        original_load = build_ios.load_mobileprovision_profiles
        try:
            build_ios.find_codesign_identities = lambda **_kwargs: [
                build_ios.IOSCodeSignIdentity('A' * 40, 'Apple Development: Test')
            ]
            build_ios.load_mobileprovision_profiles = lambda *_args, **_kwargs: ([
                make_profile(name='Profile One', uuid='UUID-1'),
                make_profile(name='Profile Two', uuid='UUID-2'),
            ], [])

            device = build_ios.IOSDevice('device-id', 'DEVICEUDID', 'Phone', 'iOS', {})
            with self.assertRaisesRegex(build_ios.IOSTestError, 'Multiple iOS provisioning profiles match') as cm:
                build_ios._has_signing_setup_for_device(
                    {},
                    device,
                    bundle_id_prefix='com.defold.tests',
                    strict=True)

            self.assertIn('--ios-mobileprovision /path/to/profile.mobileprovision', str(cm.exception))
        finally:
            build_ios.find_codesign_identities = original_find
            build_ios.load_mobileprovision_profiles = original_load

    def test_entitlements_resolve_wildcards_to_bundle_id(self):
        entitlements = build_ios.make_entitlements(make_profile(), 'com.defold.tests.test-engine')

        self.assertEqual('TEAMID.com.defold.tests.test-engine', entitlements['application-identifier'])
        self.assertEqual(['TEAMID.com.defold.tests.test-engine'], entitlements['keychain-access-groups'])
        self.assertEqual('TEAMID', entitlements['com.apple.developer.team-identifier'])

    def test_devicectl_stage_command_targets_app_data_container(self):
        commands = []
        runner = build_ios.IOSTestRunner()
        runner._devicectl_json = lambda subcommand, timeout=60, allow_failure=False: commands.append(subcommand) or {}
        device = build_ios.IOSDevice('device-id', 'DEVICEUDID', 'Phone', 'iOS', {})

        runner._copy_stage_to_device(device, 'com.defold.tests.test-engine', '/tmp/stage')

        command = commands[0]
        self.assertIn('--domain-type', command)
        self.assertIn('appDataContainer', command)
        self.assertIn('--domain-identifier', command)
        self.assertIn('com.defold.tests.test-engine', command)
        self.assertIn('--destination', command)
        self.assertIn('Documents', command)
        self.assertIn('--source', command)
        self.assertIn('/tmp/stage', command)

    def test_device_run_test_without_runtime_files_skips_stage_copy(self):
        launch_commands = []
        devicectl_commands = []
        with tempfile.TemporaryDirectory() as tmp:
            program = os.path.join(tmp, 'test_plain')
            cwd = os.path.join(tmp, 'liveupdate')
            os.makedirs(cwd)
            with open(program, 'w') as f:
                f.write('binary')

            def command_runner(cmd, **kwargs):
                launch_commands.append(cmd)
                return Completed(0, b'', b'')

            runner = build_ios.IOSTestRunner(command_runner=command_runner)
            device = build_ios.IOSDevice('device-id', 'DEVICEUDID', 'Phone', 'iOS', {})
            runner.select_device = lambda: device
            runner.ensure_device_ready = lambda _device, refresh=True: None
            runner._select_signing_assets = lambda _bundle_id, _device: (
                make_profile(),
                build_ios.IOSCodeSignIdentity('A' * 40, 'Apple Development: Test'))
            runner._sign_app_bundle = lambda _app_dir, _profile, _identity, _bundle_id, _work_dir: None
            runner._devicectl_json = lambda subcommand, timeout=60, allow_failure=False: devicectl_commands.append(subcommand) or {}

            result = runner.run_test(program, cwd, target_name='test_plain')

        self.assertEqual(0, result)
        self.assertFalse(any(command[0:3] == ['device', 'copy', 'to'] for command in devicectl_commands))
        self.assertEqual(['xcrun', 'devicectl', '--timeout', '600', 'device', 'process', 'launch'], launch_commands[-1][0:7])
        env_json = launch_commands[-1][launch_commands[-1].index('--environment-variables') + 1]
        launch_env = json.loads(env_json)
        self.assertEqual({'DEFOLD_TEST_AUTO_EXIT': '1'}, launch_env)

    def test_generated_info_plist_allows_local_network_test_server(self):
        runner = build_ios.IOSTestRunner()
        with tempfile.TemporaryDirectory() as tmp:
            runner._write_info_plist(tmp, 'com.defold.tests.test-http', 'test_http')
            with open(os.path.join(tmp, 'Info.plist'), 'rb') as f:
                info = plistlib.load(f)

        self.assertEqual('com.defold.tests.test-http', info['CFBundleIdentifier'])
        self.assertIn('NSLocalNetworkUsageDescription', info)
        self.assertIn('_defold._tcp', info['NSBonjourServices'])
        self.assertIn('_services._dns-sd._udp', info['NSBonjourServices'])
        self.assertTrue(info['NSAppTransportSecurity']['NSAllowsLocalNetworking'])

    def test_simulator_app_bundle_uses_simulator_platform(self):
        runner = build_ios.IOSSimulatorTestRunner()
        with tempfile.TemporaryDirectory() as tmp:
            program = os.path.join(tmp, 'test_sim')
            with open(program, 'w') as f:
                f.write('binary')
            app_dir = os.path.join(tmp, 'test_sim.app')

            runner._create_app_bundle(program, app_dir, 'com.defold.tests.test-sim')

            with open(os.path.join(app_dir, 'Info.plist'), 'rb') as f:
                info = plistlib.load(f)

        self.assertEqual(['iPhoneSimulator'], info['CFBundleSupportedPlatforms'])

    def test_simulator_run_test_installs_stages_and_launches(self):
        commands = []
        command_envs = []
        with tempfile.TemporaryDirectory() as tmp:
            program = os.path.join(tmp, 'test_sim')
            cwd = os.path.join(tmp, 'dlib')
            data_container = os.path.join(tmp, 'sim-data')
            os.makedirs(cwd)
            os.makedirs(data_container)
            with open(program, 'w') as f:
                f.write('binary')
            with open(os.path.join(cwd, 'unittest.cfg'), 'w') as f:
                f.write('[test]\n')

            def command_runner(cmd, **kwargs):
                commands.append(cmd)
                command_envs.append(kwargs.get('env') or {})
                if cmd == ['xcrun', 'simctl', 'list', 'devices', 'available', '-j']:
                    return Completed(0, json.dumps({
                        'devices': {
                            'com.apple.CoreSimulator.SimRuntime.iOS-18-0': [{
                                'udid': 'SIM-UDID',
                                'name': 'iPhone 16',
                                'state': 'Booted',
                                'isAvailable': True,
                            }]
                        }
                    }).encode('utf-8'), b'')
                if cmd == ['xcrun', 'simctl', 'get_app_container', 'SIM-UDID', 'com.defold.tests.test-sim', 'data']:
                    return Completed(0, ('%s\n' % data_container).encode('utf-8'), b'')
                return Completed(0, b'', b'')

            runner = build_ios.IOSSimulatorTestRunner(command_runner=command_runner)

            result = runner.run_test(program, cwd, configfile='unittest.cfg', target_name='test_sim')

            staged_cfg = os.path.join(data_container, 'Documents', 'defold-tests', 'dlib', 'unittest.cfg')
            self.assertEqual(0, result)
            self.assertTrue(os.path.exists(staged_cfg))

        self.assertIn(['codesign', '-f', '-s', '-', '--timestamp=none', commands[1][-1]], commands)
        self.assertIn(['xcrun', 'simctl', 'install', 'SIM-UDID', commands[3][-1]], commands)
        launch = commands[-2]
        self.assertEqual(['xcrun', 'simctl', 'launch'], launch[0:3])
        self.assertIn('--console', launch)
        self.assertIn('--terminate-running-process', launch)
        self.assertNotIn('--env', launch)
        self.assertIn('com.defold.tests.test-sim', launch)
        self.assertEqual('1', command_envs[-2]['SIMCTL_CHILD_DEFOLD_TEST_AUTO_EXIT'])
        self.assertEqual('Documents/defold-tests/dlib', command_envs[-2]['SIMCTL_CHILD_DEFOLD_TEST_WORKDIR'])
        self.assertEqual('unittest.cfg', command_envs[-2]['SIMCTL_CHILD_DEFOLD_TEST_READY_PATHS'])

    def test_simulator_run_test_without_runtime_files_skips_stage_lookup(self):
        commands = []
        command_envs = []
        with tempfile.TemporaryDirectory() as tmp:
            program = os.path.join(tmp, 'test_sim')
            cwd = os.path.join(tmp, 'liveupdate')
            os.makedirs(cwd)
            with open(program, 'w') as f:
                f.write('binary')

            def command_runner(cmd, **kwargs):
                commands.append(cmd)
                command_envs.append(kwargs.get('env') or {})
                if cmd == ['xcrun', 'simctl', 'list', 'devices', 'available', '-j']:
                    return Completed(0, json.dumps({
                        'devices': {
                            'com.apple.CoreSimulator.SimRuntime.iOS-18-0': [{
                                'udid': 'SIM-UDID',
                                'name': 'iPhone 16',
                                'state': 'Booted',
                                'isAvailable': True,
                            }]
                        }
                    }).encode('utf-8'), b'')
                return Completed(0, b'', b'')

            runner = build_ios.IOSSimulatorTestRunner(command_runner=command_runner)

            result = runner.run_test(program, cwd, target_name='test_sim')

        self.assertEqual(0, result)
        self.assertNotIn(['xcrun', 'simctl', 'get_app_container', 'SIM-UDID', 'com.defold.tests.test-sim', 'data'], commands)
        launch = commands[-2]
        self.assertEqual(['xcrun', 'simctl', 'launch'], launch[0:3])
        self.assertEqual('1', command_envs[-2]['SIMCTL_CHILD_DEFOLD_TEST_AUTO_EXIT'])
        self.assertNotIn('SIMCTL_CHILD_DEFOLD_TEST_WORKDIR', command_envs[-2])

    def test_launch_env_includes_staged_ready_paths(self):
        commands = []

        def command_runner(cmd, env=None):
            commands.append(cmd)
            return Completed(0, b'', b'')

        runner = build_ios.IOSTestRunner(command_runner=command_runner)
        device = build_ios.IOSDevice('device-id', 'DEVICEUDID', 'Phone', 'iOS', {})

        result = runner._launch_app(
            device,
            'com.defold.tests.test-engine',
            'dlib',
            'unittest.cfg',
            {'/tmp/source-data': 'src/test/data'})

        self.assertEqual(0, result)
        command = commands[0]
        env_json = command[command.index('--environment-variables') + 1]
        launch_env = json.loads(env_json)
        self.assertEqual('1', launch_env['DEFOLD_TEST_AUTO_EXIT'])
        self.assertEqual('Documents/defold-tests/dlib', launch_env['DEFOLD_TEST_WORKDIR'])
        self.assertEqual('src;src/test;src/test/data;unittest.cfg', launch_env[build_ios.IOS_TEST_READY_PATHS_ENV])

    def test_codesign_command_uses_timestamp_none_and_entitlements(self):
        commands = []

        with tempfile.TemporaryDirectory() as tmp:
            app_dir = os.path.join(tmp, 'Test.app')
            os.makedirs(app_dir)
            profile_path = os.path.join(tmp, 'profile.mobileprovision')
            with open(profile_path, 'wb') as f:
                f.write(b'profile')

            profile = make_profile(path=profile_path)
            identity = build_ios.IOSCodeSignIdentity('A' * 40, 'Apple Development: Test')
            runner = build_ios.IOSTestRunner()

            def fake_run(cmd, capture=True, env=None):
                commands.append(cmd)
                if cmd[0:3] == ['xcrun', '--find', 'codesign_allocate']:
                    return Completed(0, b'/usr/bin/codesign_allocate\n', b'')
                return Completed(0, b'', b'')

            runner._run = fake_run
            runner._sign_app_bundle(app_dir, profile, identity, 'com.defold.tests.test-engine', tmp)

        codesign = [cmd for cmd in commands if cmd and cmd[0] == 'codesign'][0]
        self.assertIn('--timestamp=none', codesign)
        self.assertIn('--entitlements', codesign)
        self.assertIn('A' * 40, codesign)


if __name__ == '__main__':
    unittest.main()
