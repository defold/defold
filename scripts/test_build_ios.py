import contextlib
import io
import json
import plistlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'build_tools'))
import build_ios


class IOSTestBundleTests(unittest.TestCase):
    # Custom orientation fixtures must not change the executable, bundle id or
    # platform selected by the runner, including a runtime signing-prefix override.
    def test_custom_plist_preserves_runner_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            program = root / 'test_program'
            program.write_bytes(b'test')
            fixture = root / 'fixture.plist'
            with fixture.open('wb') as f:
                plistlib.dump({
                    'CFBundleExecutable': 'configured_executable',
                    'CFBundleIdentifier': 'com.configured.fixture',
                    'CFBundleSupportedPlatforms': ['configured_platform'],
                    'UIRequiresFullScreen': True,
                    'DefoldSupportedInterfaceOrientations': ['UIInterfaceOrientationPortrait'],
                }, f)
            for platform in ('iPhoneOS', 'iPhoneSimulator'):
                with self.subTest(platform=platform):
                    app_dir = root / (platform + '.app')
                    build_ios._create_app_bundle(str(program), str(app_dir), 'com.runtime.fixture', platform)
                    build_ios._apply_test_info_plist(str(app_dir), str(fixture))
                    with (app_dir / 'Info.plist').open('rb') as f:
                        info = plistlib.load(f)
                    self.assertEqual('test_program', info['CFBundleExecutable'])
                    self.assertEqual('com.runtime.fixture', info['CFBundleIdentifier'])
                    self.assertEqual([platform], info['CFBundleSupportedPlatforms'])
                    self.assertTrue(info['UIRequiresFullScreen'])
                    self.assertEqual(['UIInterfaceOrientationPortrait'], info['DefoldSupportedInterfaceOrientations'])

    # Storyboard directories and other bundle resources must be present when
    # signing runs, for both physical devices and simulators.
    def test_bundle_resources_are_copied_before_signing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            program = root / 'test_program'
            program.write_bytes(b'test')
            storyboard = root / 'LaunchScreen.storyboardc'
            storyboard.mkdir()
            (storyboard / 'view.nib').write_bytes(b'compiled view')
            resource = root / 'resource.txt'
            resource.write_text('resource')

            def check_signed_bundle(app_dir, *args):
                self.assertEqual(b'compiled view', (Path(app_dir) / 'LaunchScreen.storyboardc' / 'view.nib').read_bytes())
                self.assertEqual('resource', (Path(app_dir) / 'resource.txt').read_text())

            for runner_type in (build_ios.IOSTestRunner, build_ios.IOSSimulatorTestRunner):
                with self.subTest(runner=runner_type.__name__), contextlib.ExitStack() as mocks:
                    runner = runner_type(env={})
                    setup = ('select_device', 'ensure_device_ready', '_select_signing_assets') if runner_type is build_ios.IOSTestRunner else ('select_simulator', '_boot_simulator_if_needed')
                    for method in setup:
                        patched = mocks.enter_context(mock.patch.object(runner, method))
                        if method == '_select_signing_assets':
                            patched.return_value = (mock.Mock(), mock.Mock())
                    for method in ('_uninstall_app', '_install_app', '_launch_app'):
                        mocks.enter_context(mock.patch.object(runner, method, return_value=0))
                    signed = mocks.enter_context(mock.patch.object(runner, '_sign_app_bundle', side_effect=check_signed_bundle))
                    self.assertEqual(0, runner.run_test(str(program), str(root), bundle_resources=[str(storyboard), str(resource)]))
                    signed.assert_called_once()


class IOSSimulatorResultTests(unittest.TestCase):
    def launch(self, output, returncode=0):
        command_runner = mock.Mock(return_value=subprocess.CompletedProcess([], returncode, stdout=output))
        runner = build_ios.IOSSimulatorTestRunner(env={}, command_runner=command_runner)
        simulator = build_ios.IOSSimulator('simulator-id', 'Test Simulator', 'runtime', 'Booted', {})
        with contextlib.redirect_stdout(io.StringIO()) as captured:
            result = runner._launch_app(simulator, 'com.defold.tests.fixture', 'font', None)
        self.assertEqual(subprocess.PIPE, command_runner.call_args.kwargs['stdout'])
        self.assertEqual(subprocess.STDOUT, command_runner.call_args.kwargs['stderr'])
        return result, captured.getvalue()

    # Accept and preserve colored C++ results while allowing platform-specific skips.
    def test_colored_cpp_result_passes(self):
        output = b'4 tests \x1b[32mPASSED\x1b[0m and 1 skipped\n'
        result, captured = self.launch(output)
        self.assertEqual(0, result)
        self.assertEqual(output.decode(), captured)

    # Accept the distinct success footer emitted by the C-only jc_test runner.
    def test_c_result_passes(self):
        result, _ = self.launch(b'4 tests passed, 0 skipped and 0 tests failed\n')
        self.assertEqual(0, result)

    # A successful simctl launch must not hide a failing test application.
    def test_failed_test_returns_failure_after_successful_launch(self):
        result, _ = self.launch(b'3 tests passed, 0 skipped and 1 tests \x1b[31mFAILED\x1b[0m\n')
        self.assertEqual(1, result)

    # A later successful suite must not mask a previous failure in the same application.
    def test_earlier_failure_is_preserved(self):
        result, _ = self.launch(b'3 tests passed, 0 skipped and 1 tests FAILED\n4 tests PASSED and 0 skipped\n')
        self.assertEqual(1, result)

    # Missing final results indicate an interrupted/crashed app, not a passing suite.
    def test_missing_result_fails(self):
        with self.assertRaisesRegex(build_ios.IOSTestError, 'without a completed jc_test result'):
            self.launch(b'Fixture.FirstCase PASS\n')

    # Preserve launch errors even when partial output contains a successful result.
    def test_launcher_error_is_preserved(self):
        result, _ = self.launch(b'4 tests PASSED and 0 skipped\n', returncode=9)
        self.assertEqual(9, result)


class IOSTestEnvironmentTests(unittest.TestCase):
    # The opt-in network requirement of the connectivity test must reach the application on a device.
    def test_require_network_is_forwarded_to_device_app(self):
        command_runner = mock.Mock(return_value=subprocess.CompletedProcess([], 0))
        runner = build_ios.IOSTestRunner(env={build_ios.IOS_TEST_REQUIRE_NETWORK_ENV: '1'}, command_runner=command_runner)
        runner._launch_app(mock.Mock(device_id='device-id'), 'com.defold.tests.fixture', 'dlib', None)
        args = command_runner.call_args.args[0]
        app_env = json.loads(args[args.index('--environment-variables') + 1])
        self.assertEqual('1', app_env[build_ios.IOS_TEST_REQUIRE_NETWORK_ENV])

    # The opt-in network requirement of the connectivity test must reach the application on a simulator.
    def test_require_network_is_forwarded_to_simulator_app(self):
        command_runner = mock.Mock(return_value=subprocess.CompletedProcess([], 0, stdout=b'1 tests PASSED and 0 skipped\n'))
        runner = build_ios.IOSSimulatorTestRunner(env={build_ios.IOS_TEST_REQUIRE_NETWORK_ENV: '1'}, command_runner=command_runner)
        simulator = build_ios.IOSSimulator('simulator-id', 'Test Simulator', 'runtime', 'Booted', {})
        with contextlib.redirect_stdout(io.StringIO()):
            runner._launch_app(simulator, 'com.defold.tests.fixture', 'dlib', None)
        self.assertEqual('1', command_runner.call_args.kwargs['env']['SIMCTL_CHILD_%s' % build_ios.IOS_TEST_REQUIRE_NETWORK_ENV])

    # Without the opt-in variable, the application runs without requiring a network.
    def test_require_network_is_not_set_by_default(self):
        command_runner = mock.Mock(return_value=subprocess.CompletedProcess([], 0))
        runner = build_ios.IOSTestRunner(env={}, command_runner=command_runner)
        runner._launch_app(mock.Mock(device_id='device-id'), 'com.defold.tests.fixture', 'dlib', None)
        args = command_runner.call_args.args[0]
        app_env = json.loads(args[args.index('--environment-variables') + 1])
        self.assertNotIn(build_ios.IOS_TEST_REQUIRE_NETWORK_ENV, app_env)


if __name__ == '__main__':
    unittest.main()
