import contextlib
import io
import json
import os
import runpy
import shlex
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ci'))

import build
import ci
import cross_build


class BuildStartupTests(unittest.TestCase):
    # SDK timing callbacks must receive the same redacted output as the command log.
    def test_streamed_command_output_is_redacted(self):
        lines = []
        output = build.run.env_command(
            os.environ, [sys.executable, '-c', 'import sys; print(sys.argv[1])', 'test-secret'],
            silent=True, redacted_values=['test-secret'], on_output=lines.append)
        self.assertEqual(['***'], lines)
        self.assertEqual('***', output)

    # Loading build.py for SDK and build commands must not resolve release settings or run Git.
    def test_import_does_not_query_git(self):
        with mock.patch.dict(os.environ), \
                mock.patch.object(sys, 'path', sys.path.copy()), \
                mock.patch.object(build.run, 'shell_command', side_effect=AssertionError('Unexpected Git command')) as command, \
                contextlib.redirect_stdout(io.StringIO()) as output:
            os.environ.pop('DM_RELEASE_REPOSITORY', None)
            os.environ.pop('GITHUB_REPOSITORY', None)
            runpy.run_path(build.__file__)
        command.assert_not_called()
        self.assertEqual('', output.getvalue())

    # Deferring repository detection must preserve explicit, environment, and origin release destinations.
    def test_release_resolves_target_repository_only_when_needed(self):
        cases = (
            ('explicit/repo', {'DM_RELEASE_REPOSITORY': 'env/repo', 'GITHUB_REPOSITORY': 'ci/repo'}, 'origin/repo', 'explicit/repo', False),
            (None, {'DM_RELEASE_REPOSITORY': 'env/repo', 'GITHUB_REPOSITORY': 'ci/repo'}, 'origin/repo', 'env/repo', False),
            (None, {'GITHUB_REPOSITORY': 'ci/repo'}, 'origin/repo', 'ci/repo', False),
            (None, {}, 'origin/repo', 'origin/repo', True),
            (None, {}, None, 'defold/defold', True),
        )
        for explicit, environment, origin, expected, detect_origin in cases:
            with self.subTest(explicit=explicit, environment=environment, origin=origin), \
                    mock.patch.dict(os.environ), \
                    mock.patch.object(build.release_to_github, 'get_current_repo', return_value=origin) as get_current_repo, \
                    mock.patch.object(build.release_to_github, 'github') as github, \
                    mock.patch.object(build.release_to_github, 'log'):
                os.environ.pop('DM_RELEASE_REPOSITORY', None)
                os.environ.pop('GITHUB_REPOSITORY', None)
                os.environ.update(environment)
                configuration = mock.Mock(github_target_repo=explicit, github_token='test-token',
                                          channel='alpha', version='1.0.0', archive_path='s3://test-bucket/archive')
                release = {'id': 1, 'assets': []}
                github.get.return_value = release
                github.patch.return_value = release
                build.release_to_github.release(configuration, 'test-tag', 'test-sha',
                                                {'files': [{'path': '/unrelated.txt'}]})
                github.get.assert_called_once_with(f'/repos/{expected}/releases/tags/test-tag', 'test-token')
                self.assertEqual(f'/repos/{expected}/releases/1', github.patch.call_args.args[0])
                github.upload_release_asset.assert_not_called()
                if detect_origin:
                    get_current_repo.assert_called_once_with()
                else:
                    get_current_repo.assert_not_called()


class BuildTimeTrackerTests(unittest.TestCase):
    # Nested build-tool commands remain visible without adding their durations again to the session total.
    def test_nested_command_times_do_not_inflate_session_duration(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(build.time, 'time', side_effect=[10, 11, 12, 13, 14, 16, 20, 23]):
            path = Path(directory) / 'build_times.json'
            messages = []
            tracker = build.BuildTimeTracker(output_file=str(path), logger=messages.append)
            tracker.start_command('build_bob')
            tracker.start_command('Gradle bob', quiet=True)
            tracker.end_command('Gradle bob', quiet=True)
            tracker.start_command('Gradle bob-plugin-xbox', quiet=True)
            tracker.end_command('Gradle bob-plugin-xbox', quiet=True)
            tracker.end_command('build_bob')
            tracker.start_command('check_sdk')
            tracker.end_command('check_sdk')
            tracker.print_summary()
            tracker.save_times()
            summary = json.loads(path.read_text())

        self.assertEqual(9, summary['build_session']['total_duration'])
        self.assertEqual(10, summary['build_session']['start_time'])
        self.assertEqual(23, summary['build_session']['end_time'])
        self.assertEqual(1, summary['commands']['Gradle bob']['duration'])
        self.assertEqual(1, summary['commands']['Gradle bob-plugin-xbox']['duration'])
        output = '\n'.join(messages)
        self.assertIn('Gradle bob-plugin-xbox', output)
        self.assertNotIn("Starting 'Gradle", output)
        self.assertNotIn("'Gradle bob' completed", output)

    # Standalone SDK timings remain visible; the engine aggregate and preflight checks stay only in JSON.
    def test_check_timings_are_included_in_summary_and_json(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(build.time, 'time', side_effect=[10, 11, 11, 12, 12.25, 13]):
            path = Path(directory) / 'build_times.json'
            messages = []
            tracker = build.BuildTimeTracker(output_file=str(path), logger=messages.append)
            tracker.record_check_time('Untracked', 99)
            tracker.start_command('check_sdk')
            tracker.record_check_time('Python', 0.125)
            tracker.record_check_time('Java', 0.25)
            tracker.end_command('check_sdk')
            tracker.start_command('build_engine')
            tracker.record_check_time('SDK', 0.1)
            tracker.start_command('CMake build engine_libs')
            tracker.end_command('CMake build engine_libs')
            tracker.end_command('build_engine')
            tracker.print_summary()
            tracker.save_times()
            summary = json.loads(path.read_text())

        checks = summary['commands']['check_sdk']['checks']
        self.assertEqual({'Python': {'duration': 0.125}, 'Java': {'duration': 0.25}}, checks)
        self.assertEqual({'SDK': {'duration': 0.1}}, summary['commands']['build_engine']['checks'])
        self.assertEqual(3, summary['build_session']['total_duration'])
        output = '\n'.join(messages)
        self.assertIn('    Checks', output)
        self.assertRegex(output, r'(?m)^\s+Python\s+0\.12 s$')
        self.assertRegex(output, r'(?m)^\s+Java\s+0\.25 s$')
        self.assertNotRegex(output, r'(?m)^\s+build_engine\s+\d')
        self.assertNotRegex(output, r'(?m)^\s+SDK\s+0\.10 s$')
        self.assertRegex(output, r'(?m)^\s+CMake build engine_libs\s+0\.25 s$')


class BuildSdkTests(unittest.TestCase):
    def setUp(self):
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        self.root = Path(temporary_directory.name)
        self.public_sdks = {'x86_64-linux': ['linux', 'latest']}
        self.private_sdks = {
            'arm64-private': ['private-arm', '1'],
            'x86_64-private': ['private-x86', '2'],
        }
        self.write_json('share/platform.sdks.json', self.public_sdks)
        self.write_json('private-a/share/platform.sdks.json', {
            'arm64-private': self.private_sdks['arm64-private'],
            'arm64-unselected': ['unselected', '3'],
            'x86_64-linux': ['do-not-override', '4'],
        })
        self.write_json('private-b/share/platform.sdks.json', {
            'x86_64-private': self.private_sdks['x86_64-private'],
        })
        config_path = self.write_json('.defold-platforms', {
            'arm64-private': {'root': str(self.root / 'private-a')},
            'arm64-unselected': {'root': str(self.root / 'private-a')},
            'x86_64-private': {'root': str(self.root / 'private-b')},
        })
        config_patch = mock.patch.object(cross_build, 'get_platforms_config_path', return_value=config_path)
        config_patch.start()
        self.addCleanup(config_patch.stop)

    def write_json(self, relative_path, data):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data))
        return path

    def test_merge_selected_platforms_from_multiple_private_repositories(self):
        platform_sdks = cross_build.merge_platform_sdks(
            self.root, ['x86_64-linux', 'arm64-private', 'x86_64-private'])
        self.assertEqual(self.public_sdks | self.private_sdks, platform_sdks)

    def test_single_target_keeps_other_private_mappings_out(self):
        output_path = self.root / 'platform.sdks.json'
        cross_build.write_merged_platform_sdks(self.root, 'arm64-private', output_path)
        self.assertEqual(
            self.public_sdks | {'arm64-private': self.private_sdks['arm64-private']},
            json.loads(output_path.read_text()))

    def test_no_private_targets_keeps_public_mappings(self):
        for platforms in (None, [], ['x86_64-linux']):
            with self.subTest(platforms=platforms):
                self.assertEqual(self.public_sdks, cross_build.merge_platform_sdks(self.root, platforms))

    def test_combined_sdk_uploads_metadata_for_the_archived_platforms(self):
        cases = (
            (['arm64-private', 'x86_64-private'], 'x86_64-linux', False, list(self.private_sdks)),
            (None, 'x86_64-linux', True, list(self.private_sdks)),
            (None, 'arm64-private', False, ['arm64-private']),
            (None, 'x86_64-linux', False, ['x86_64-linux'] + list(self.private_sdks)),
        )
        for selected, target, private_repo, expected_platforms in cases:
            for zipmerge_path in (None, '/test/zipmerge'):
                with self.subTest(selected=selected, target=target, private_repo=private_repo, zipmerge=zipmerge_path):
                    configuration = build.Configuration.__new__(build.Configuration)
                    configuration.defold_root = str(self.root)
                    configuration.sdk_platforms = selected
                    configuration.target_platform = target
                    configuration._git_sha1 = lambda: 'test-sha1'
                    configuration.get_archive_path = lambda: 's3://test-bucket/archive'
                    configuration._ziptree = lambda path, directory: path + '.zip'
                    configuration._create_sha256_signature_file = lambda path: 'defoldsdk.sha256'
                    configuration.wait_uploads = lambda: None
                    uploads = {}

                    def upload(path, key):
                        if key.endswith('/platform.sdks.json'):
                            uploads[key] = json.loads(Path(path).read_text())

                    configuration.upload_to_archive = upload
                    with contextlib.ExitStack() as stack:
                        stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                        stack.enter_context(mock.patch.object(build.shutil, 'which', return_value=zipmerge_path))
                        stack.enter_context(mock.patch.object(build.build_private, 'is_repo_private', return_value=private_repo))
                        stack.enter_context(mock.patch.object(build.build_private, 'get_target_platforms', return_value=list(self.private_sdks)))
                        stack.enter_context(mock.patch.object(build, 'get_target_platforms', return_value=['x86_64-linux'] + list(self.private_sdks)))
                        merge_tree = stack.enter_context(mock.patch.object(build.sdk_merge, 'build_combined_sdk_tree'))
                        merge_zip = stack.enter_context(mock.patch.object(build.sdk_merge, 'build_combined_sdk_zip'))
                        configuration.build_sdk()

                    merge = merge_zip if zipmerge_path else merge_tree
                    self.assertEqual(expected_platforms, merge.call_args.kwargs['platforms'])
                    expected_sdks = self.public_sdks | {
                        platform: self.private_sdks[platform]
                        for platform in expected_platforms if platform in self.private_sdks
                    }
                    self.assertEqual({'test-sha1/engine/platform.sdks.json': expected_sdks}, uploads)


class SdkInstallationTests(unittest.TestCase):
    # Execute the host launchers, including SDK paths with spaces, to guard against WinError 193.
    def test_web_sdk_activation_and_cache_warmup_use_runnable_launchers(self):
        with tempfile.TemporaryDirectory(prefix='emsdk test ') as directory:
            emsdk = Path(directory)
            bin_dir = emsdk / 'upstream' / 'emscripten'
            bin_dir.mkdir(parents=True)
            launchers = {
                emsdk / 'emsdk': '#!/bin/sh\nprintf "%s\\n" "$*" > "$0.args"\n',
                emsdk / 'emsdk.bat': '@echo off\necho %* > "%~dp0emsdk.args"\n',
                bin_dir / 'emcc': '#!/bin/sh\ncp "$1" "$0.input"\ncp "$1" "$3"\n',
                bin_dir / 'emcc.bat': '@echo off\ncopy /Y "%~1" "%~dp0emcc.input" >nul\ncopy /Y "%~1" "%~3" >nul\n',
            }
            for path, content in launchers.items():
                path.write_text(content)
                path.chmod(0o755)
            configuration = build.Configuration.__new__(build.Configuration)
            configuration.host = build.get_host_platform()
            configuration._form_env = lambda: os.environ.copy()
            version = build.sdk.EMSCRIPTEN_VERSION_STR
            with mock.patch.object(build.tempfile, 'tempdir', directory):
                configuration._activate_ems(str(emsdk), str(bin_dir), version)
            self.assertEqual(f'activate {version} --embedded', (emsdk / 'emsdk.args').read_text().strip())
            self.assertEqual('int main() { return 0; }', (bin_dir / 'emcc.input').read_text())
            outputs = list(emsdk.glob('*.js'))
            self.assertEqual(1, len(outputs))
            self.assertEqual('int main() { return 0; }', outputs[0].read_text())

    # macOS ARMv7 source builds need the Web SDK for LuaJIT's 32-bit generators; other Android builds do not.
    def test_android_installs_web_sdk_only_for_macos_armv7(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            emsdk = root / 'emsdk'
            for host in ('arm64-macos', 'x86_64-macos', 'x86_64-linux', 'x86_64-win32'):
                for target in ('armv7-android', 'arm64-android', 'x86_64-android'):
                    with self.subTest(host=host, target=target):
                        configuration = build.Configuration.__new__(build.Configuration)
                        configuration.ext = str(root)
                        configuration.host = host
                        configuration.target_platform = target
                        configuration.package_path = 'https://packages.example'
                        configuration.has_sdk = lambda folder, platform: True
                        with mock.patch.object(build.sdk, 'get_host_platform', return_value=host), \
                                mock.patch.object(build.sdk, 'get_defold_emsdk', return_value=str(emsdk)), \
                                mock.patch.object(build.sdk, 'get_defold_emsdk_config', return_value=str(emsdk / '.emscripten')), \
                                mock.patch.object(build.os.path, 'isfile', return_value=True), \
                                mock.patch.object(build.build_private, 'install_sdk'), \
                                mock.patch.object(build, 'download_sdk') as download:
                            configuration.install_sdk()
                        web_sdk = f'https://packages.example/{build.sdk.PACKAGES_EMSCRIPTEN_SDK}-{host}.tar.gz'
                        downloads = [call.args[1] for call in download.call_args_list]
                        self.assertEqual(target == 'armv7-android' and host.endswith('-macos'), web_sdk in downloads)


class SdkCheckTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.defold_root = str(Path(__file__).resolve().parents[1])
        self.configuration.dynamo_home = '/test-sdk'
        self.configuration.ext = '/test-sdk/ext'
        self.configuration.verbose = False
        self.configuration.no_colors = False
        self.configuration.host = 'arm64-macos'
        self.configuration.target_platform = 'arm64-macos'
        self.configuration.skip_tests = False
        self.configuration.build_tracker = build.BuildTimeTracker(logger=lambda message: None)
        self.configuration._form_env = lambda: os.environ.copy()
        self.protobuf_output = json.dumps({
            'version': build.sdk.VERSION_PYTHON_PROTOBUF,
            'path': '/test-sdk/ext/lib/python/google/protobuf/__init__.py',
        })
        self.cmake_output = '\n'.join((
            '-- check_install: diagnostic noise',
            'DEFOLD_SDK_CHECK\tJava',
            'DEFOLD_SDK_CHECK\tJava\tJava 25.0.1\t/tools/java',
            'DEFOLD_SDK_CHECK\tNinja',
            'DEFOLD_SDK_CHECK\tNinja\tNinja 1.12.1\t/tools/ninja',
            'DEFOLD_SDK_CHECK\tProtoc',
            'DEFOLD_SDK_CHECK\tProtoc\tlibprotoc 30.2\t/tools/protoc',
            'DEFOLD_SDK_CHECK\tToolchain',
            'DEFOLD_SDK_CHECK\tToolchain\tarm64-macos\t/tools/toolchain.cmake',
        ))
        for patch in (
                mock.patch.object(self.configuration, 'check_python'),
                mock.patch.object(self.configuration, '_log'),
                mock.patch.object(build.sdk, 'get_sdk_info', return_value={
                    'arm64-macos': {'version': '27.0', 'path': '/tools/MacOSX27.0.sdk'},
                    'clang-version': '21.0.0', 'clang': '/tools/clang++',
                }),
                mock.patch.object(build.sdk, 'test_sdk', return_value=True),
                mock.patch.object(build.run, 'env_command', side_effect=self.run_command),
                mock.patch.object(build.shutil, 'which', return_value='/tools/tool')):
            patch.start()
            self.addCleanup(patch.stop)

    def run_command(self, env, args, **kwargs):
        if '-S' in args and '-c' in args:
            result = self.protobuf_output
        else:
            result = 'cmake version 4.4.0\nCMake suite' if args[-1] == '--version' else self.cmake_output
        return self.command_result(result, **kwargs)

    def command_result(self, result, **kwargs):
        output = result.output if isinstance(result, build.run.ExecException) else result
        if kwargs.get('on_output'):
            for line in output.splitlines():
                kwargs['on_output'](line)
        if isinstance(result, build.run.ExecException):
            raise result
        return output

    def messages(self):
        return '\n'.join(call.args[0] for call in self.configuration._log.call_args_list)

    # A standalone SDK check stays concise; verbose and multi-command runs retain overall build reporting.
    def test_cli_summary_requires_verbose_for_standalone_check(self):
        repository = str(Path(__file__).resolve().parents[1])
        cases = ((False, ['check_sdk'], False), (True, ['check_sdk'], True),
                 (False, ['check_sdk', 'check_python'], True))
        for verbose, commands, report in cases:
            with self.subTest(verbose=verbose, commands=commands), tempfile.TemporaryDirectory() as directory, \
                    mock.patch.dict(os.environ, {'DEFOLD_HOME': repository, 'DYNAMO_HOME': directory,
                                                 'PYTHONPATH': repository, 'JAVA_HOME': directory}), \
                    mock.patch.object(sys, 'path', sys.path.copy()), \
                    mock.patch.object(sys, 'argv', [build.__file__, '--platform=arm64-macos',
                                                   *(['--verbose'] if verbose else []), *commands]), \
                    mock.patch('BuildTimeTracker.BuildTimeTracker') as tracker_type, \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                timing_file = Path(directory) / 'build_times.json'
                tracker = build.BuildTimeTracker(output_file=str(timing_file))
                tracker_type.return_value = tracker
                runpy.run_path(build.__file__, run_name='__main__')
                saved = json.loads(timing_file.read_text()) if timing_file.exists() else None
            messages = output.getvalue()
            self.assertEqual(report, 'BUILD TIME SUMMARY' in messages)
            self.assertEqual(report, 'Build times saved' in messages)
            self.assertEqual(report, messages.endswith('Done\n'))
            self.assertEqual(report, bool(tracker.command_times))
            self.assertEqual(report, saved is not None)
            if report:
                checks = saved['commands']['check_sdk']['checks']
                self.assertEqual({'Python', 'SDK', 'Compiler', 'CMake', 'Java', 'Ninja', 'Protoc',
                                  'Toolchain', 'Python Protobuf'}, set(checks))
                summary = messages.split('BUILD TIME SUMMARY', 1)[1]
                for name, timing in checks.items():
                    self.assertRegex(summary, rf'(?m)^\s+{name}\s+{timing["duration"]:.2f} s$')
            else:
                self.assertTrue(messages.rstrip().endswith('All checks passed: arm64-macos'))

    # Success and failure timings use a monotonic clock, stay recorded, and appear only in verbose output.
    def test_check_timing_includes_successes_and_failures(self):
        for verbose in (False, True):
            for passed in (True, False):
                with self.subTest(verbose=verbose, passed=passed), \
                        mock.patch.object(build.time, 'perf_counter', side_effect=[10, 10.25]):
                    self.configuration.verbose = verbose
                    self.configuration._log.reset_mock()
                    self.configuration.build_tracker.start_command('check_sdk', quiet=True)
                    if passed:
                        result, elapsed = self.configuration._run_sdk_check('Python', lambda: True)
                        self.assertTrue(result)
                        self.assertEqual(0.25, elapsed)
                        self.configuration._log_sdk_check('Python', 'OK', elapsed=elapsed)
                    else:
                        with self.assertRaises(SystemExit):
                            self.configuration._run_sdk_check('Python', lambda: False)
                    self.assertIn(f'Python: {"OK" if passed else "Failed"}', self.messages())
                    self.assertEqual(verbose, '[0.25 s]' in self.messages())
                    self.assertEqual({'Python': {'duration': 0.25}},
                                     self.configuration.build_tracker.check_times['check_sdk'])

    # Successful checks stay on one line without paths, raw SDK logs, or duplicate CMake output.
    def test_concise_output_for_redirected_stdout(self):
        def find_sdk(*args):
            build.sdk.log.log('SDK discovery noise')
            return {'arm64-macos': {'version': '27.0', 'path': '/tools/MacOSX27.0.sdk'}}

        def test_sdk(*args, **kwargs):
            build.sdk.log.log('Compiler command noise')
            print('Compiler output noise')
            return True

        with contextlib.redirect_stdout(io.StringIO()) as output, \
                mock.patch.object(build.sdk, 'get_sdk_info', side_effect=find_sdk), \
                mock.patch.object(build.sdk, 'test_sdk', side_effect=test_sdk):
            self.configuration.check_sdk()
        messages = self.messages()
        self.assertEqual('', output.getvalue())
        self.assertRegex(messages, r'(?m)^  ✓ Java: Java 25\.0\.1$')
        self.assertRegex(messages, r'(?m)^  ✓ SDK: arm64-macos 27\.0$')
        rows = messages.splitlines()[:-1]
        self.assertEqual(9, len(rows))
        for row in rows:
            self.assertNotRegex(row, r'\[\d+\.\d{2} s\]$')
        self.assertNotIn('/tools/', messages)
        self.assertNotIn('noise', messages)
        self.assertNotIn('\033[', messages)
        self.assertEqual(1, messages.count('Java:'))
        self.assertTrue(messages.endswith('All checks passed: arm64-macos'))

    # Verbose output adds paths and per-check timings to the concise rows without dumping SDK dictionaries.
    def test_verbose_output_includes_paths(self):
        self.configuration.verbose = True
        self.configuration.check_sdk()
        messages = self.messages()
        self.assertIn('Java: Java 25.0.1 (/tools/java)', messages)
        self.assertIn('SDK: arm64-macos 27.0 (/tools/MacOSX27.0.sdk)', messages)
        self.assertIn('Compiler: Clang 21.0.0 (/tools/clang++)', messages)
        for row in messages.splitlines()[:-1]:
            self.assertRegex(row, r'\[\d+\.\d{2} s\]$')
        self.assertNotIn('SDK info:', messages)

    # A terminal uses green checks and orange failures with red crosses.
    def test_terminal_colors_distinguish_passes_and_failures(self):
        with mock.patch.object(build.sys.stdout, 'isatty', return_value=True), \
                mock.patch.dict(os.environ, {'NOCOLOR': ''}):
            os.environ.pop('NO_COLOR', None)
            self.configuration._log_sdk_check('Java', 'Java 25.0.1')
            self.configuration._log_sdk_check('Ninja', 'Not found', passed=False)
        messages = self.messages()
        self.assertIn('\033[32m✓\033[0m \033[32mJava: Java 25.0.1\033[0m', messages)
        self.assertIn('\033[31m✗\033[0m \033[38;5;208mNinja: Not found\033[0m', messages)

    # Existing color opt-outs and NO_COLOR must also apply to the SDK report on a terminal.
    def test_terminal_respects_color_opt_outs(self):
        for no_colors, environment in ((True, {}), (False, {'NOCOLOR': '1'}), (False, {'NO_COLOR': ''})):
            with self.subTest(no_colors=no_colors, environment=environment), \
                    mock.patch.object(build.sys.stdout, 'isatty', return_value=True), \
                    mock.patch.dict(os.environ):
                os.environ.pop('NOCOLOR', None)
                os.environ.pop('NO_COLOR', None)
                os.environ.update(environment)
                self.configuration.no_colors = no_colors
                self.configuration._log_sdk_check('Java', 'Java 25.0.1')
        self.assertNotIn('\033[', self.messages())

    # A failed compiler test must reveal its diagnostics and suppress the success summary.
    def test_compiler_failure_preserves_diagnostics(self):
        def test_sdk(*args, **kwargs):
            build.sdk.log.log('Clang is too old')
            return False

        with mock.patch.object(build.sdk, 'test_sdk', side_effect=test_sdk), self.assertRaises(SystemExit):
            self.configuration.check_sdk()
        messages = self.messages()
        self.assertIn('✗ Compiler: Failed', messages)
        self.assertIn('Clang is too old', messages)
        self.assertNotIn('All checks passed', messages)

    # Failed SDK discovery runs only once and retains the setup instructions.
    def test_missing_sdk_reports_failure(self):
        with mock.patch.object(build.sdk, 'get_sdk_info', return_value=None) as find_sdk, self.assertRaises(SystemExit):
            self.configuration.check_sdk()
        find_sdk.assert_called_once()
        self.assertIn('✗ SDK: Failed', self.messages())
        self.assertIn('run install_sdk', self.messages())
        self.assertNotIn('All checks passed', self.messages())

    # A CMake failure must put the actual reason on the failed row and preserve the exit status.
    def test_cmake_failure_names_the_failed_check(self):
        error = build.run.ExecException(2)
        error.output = 'DEFOLD_SDK_CHECK\tJava\nCMake Error: Java 25 or newer is required'
        self.cmake_output = error
        with self.assertRaises(SystemExit) as raised:
            self.configuration.check_sdk()
        self.assertEqual(2, raised.exception.code)
        self.assertIn('✗ Java: Java 25 or newer is required', self.messages())
        self.assertNotIn('All checks passed', self.messages())

    # Verbose failures retain the failing tool's diagnostics without replaying setup or earlier checks.
    def test_cmake_failure_summarizes_wrapped_errors(self):
        error = build.run.ExecException(1)
        error.output = '''-- check_install: DEFOLD_SDK_ROOT=/test-sdk
-- HOST_PLATFORM: arm64-macos
DEFOLD_SDK_CHECK\tJava
-- tools_java.cmake:
-- Found Java: /tools/java (found suitable version "25.0.1")
DEFOLD_SDK_CHECK\tJava\tJava 25.0.1\t/tools/java
DEFOLD_SDK_CHECK\tNinja
-- tools_ninja.cmake:
-- tools_ninja: ninja 1.12.1 at /tools/ninja
DEFOLD_SDK_CHECK\tNinja\tNinja 1.12.1\t/tools/ninja
DEFOLD_SDK_CHECK\tProtoc
-- tools_protoc.cmake:
CMake Error at scripts/cmake/tools_protoc.cmake:53 (message):
  Could not find protoc. Run ./scripts/build.py install_ext to build and
  install it.

  Expected in /test-sdk/ext/bin/arm64-macos/ or on PATH.
Call Stack (most recent call first):
  scripts/cmake/check_install.cmake:63 (include)
'''
        self.cmake_output = error
        for verbose in (False, True):
            with self.subTest(verbose=verbose), \
                    mock.patch.object(build.time, 'perf_counter', side_effect=[100, 101, 101.25, 102, 102.125, 103, 103.5]), \
                    self.assertRaises(SystemExit):
                self.configuration.verbose = verbose
                self.configuration._log.reset_mock()
                self.configuration._check_sdk_cmake(['cmake'])
            messages = self.messages()
            self.assertIn('✓ Java: Java 25.0.1', messages)
            self.assertIn('✓ Ninja: Ninja 1.12.1', messages)
            self.assertIn('✗ Protoc: Could not find protoc. Run ./scripts/build.py install_ext to build and install it.', messages)
            self.assertEqual(verbose, '[0.50 s]' in messages)
            self.assertEqual(verbose, '[0.25 s]' in messages)
            self.assertEqual(verbose, 'Call Stack' in messages)
            self.assertEqual(verbose, '-- tools_protoc.cmake:' in messages)
            self.assertEqual(verbose, '/test-sdk/' in messages)
            for unrelated in ('DEFOLD_SDK_ROOT', 'HOST_PLATFORM', 'Found Java', 'tools_java', 'tools_ninja'):
                self.assertNotIn(unrelated, messages)
            self.assertNotIn('DEFOLD_SDK_CHECK', messages)

    # Missing web runners fail required tests while --skip-tests still permits SDK checks to finish.
    def test_missing_web_runner_respects_skip_tests(self):
        self.configuration.target_platform = 'wasm-web'
        for skip_tests in (False, True):
            with self.subTest(skip_tests=skip_tests), \
                    mock.patch.object(self.configuration, '_find_wasm_test_runner', return_value=(None, ['node failed'])):
                self.configuration.skip_tests = skip_tests
                self.configuration._log.reset_mock()
                if skip_tests:
                    self.configuration.check_sdk()
                    self.assertIn('Wasm test runner: Not required (--skip-tests)', self.messages())
                    self.assertIn('All checks passed: wasm-web', self.messages())
                else:
                    with self.assertRaises(SystemExit):
                        self.configuration.check_sdk()
                    self.assertIn('✗ Wasm test runner: Not found', self.messages())
                    self.assertNotIn('All checks passed', self.messages())

    # ARMv7 on macOS must check its generator SDK as well as Android; other Android builds need only Android.
    def test_android_checks_generator_sdk_only_for_macos_armv7(self):
        for host in ('arm64-macos', 'x86_64-macos', 'x86_64-linux', 'x86_64-win32'):
            for target in ('armv7-android', 'arm64-android', 'x86_64-android'):
                with self.subTest(host=host, target=target), \
                        mock.patch.object(build.run, 'env_command', side_effect=self.run_command) as command:
                    self.configuration.host = host
                    self.configuration.target_platform = target
                    self.configuration.check_sdk()
                    checks = [call.args[1] for call in command.call_args_list if '-P' in call.args[1]]
                    self.assertTrue(checks[0][-1].endswith('scripts/cmake/check_install.cmake'))
                    if target == 'armv7-android' and host.endswith('-macos'):
                        self.assertEqual(2, len(checks))
                        self.assertTrue(checks[1][-1].endswith('external/luajit/host/emscripten.cmake'))
                        self.assertIn('-DDEFOLD_SDK_ROOT:PATH=/test-sdk', checks[1])
                        self.assertIn('-DLUAJIT_CHECK_SDK=ON', checks[1])
                    else:
                        self.assertEqual(1, len(checks))

    # A missing or unusable generator SDK must fail check_sdk even when the Android toolchain passes.
    def test_macos_armv7_generator_sdk_failure_is_not_ignored(self):
        self.configuration.host = 'arm64-macos'
        self.configuration.target_platform = 'armv7-android'
        results = iter(['cmake version 4.4.0', self.cmake_output, self.protobuf_output, build.run.ExecException(7)])
        with mock.patch.object(build.run, 'env_command',
                               side_effect=lambda env, args, **kwargs: self.command_result(next(results), **kwargs)):
            with self.assertRaises(SystemExit) as error:
                self.configuration.check_sdk()
            self.assertEqual(7, error.exception.code)
        self.assertIn('✗ LuaJIT generator: Failed', self.messages())
        self.assertNotIn('All checks passed', self.messages())


@unittest.skipUnless(build.shutil.which('cmake'), 'CMake is required')
class CmakeSdkCheckTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))

    # Real CMake execution must report individual successes and the failed check, including optional protoc.
    def test_check_install_reports_results_and_failures(self):
        repository = Path(__file__).resolve().parents[1]
        for require_protoc, fail_java in ((True, False), (False, False), (True, True)):
            with self.subTest(require_protoc=require_protoc, fail_java=fail_java), \
                    tempfile.TemporaryDirectory(prefix='sdk check ') as directory:
                root = Path(directory)
                modules = {
                    'functions': '',
                    'platform_host': 'set(HOST_PLATFORM arm64-macos)',
                    'tools_clang': '',
                    'tools_java': '''
if(FAIL_JAVA)
  message(FATAL_ERROR "Java 25 or newer is required")
endif()
set(Java_VERSION 25.0.1)
set(Java_JAVA_EXECUTABLE "${TOOLS_ROOT}/Java (test)/java")
''',
                    'tools_ninja': 'set(_ninja_ver 1.12.1)\nset(_DEFOLD_NINJA_EXECUTABLE "${TOOLS_ROOT}/ninja")',
                    'tools_protoc': 'set(_protoc_ver "libprotoc 30.2")\nset(_DEFOLD_PROTOC_REAL "${TOOLS_ROOT}/protoc")',
                    'sdk': 'set(CMAKE_TOOLCHAIN_FILE "${TOOLS_ROOT}/toolchain.cmake")',
                }
                for name, source in modules.items():
                    (root / f'{name}.cmake').write_text(source)
                configuration = build.Configuration.__new__(build.Configuration)
                configuration.verbose = True
                configuration.no_colors = True
                configuration.build_tracker = build.BuildTimeTracker(logger=lambda message: None)
                configuration._log = mock.Mock()
                args = [
                    build.shutil.which('cmake'), '-DDEFOLD_SDK_CHECK=ON',
                    f'-DCMAKE_MODULE_PATH={root.as_posix()}', f'-DTOOLS_ROOT={root.as_posix()}',
                    '-DTARGET_PLATFORM=arm64-macos',
                    f'-DDEFOLD_REQUIRE_PROTOC={"ON" if require_protoc else "OFF"}',
                    f'-DFAIL_JAVA={"ON" if fail_java else "OFF"}',
                    '-P', str(repository / 'scripts/cmake/check_install.cmake'),
                ]
                if fail_java:
                    with self.assertRaises(SystemExit) as error:
                        configuration._check_sdk_cmake(args)
                    self.assertNotEqual(0, error.exception.code)
                else:
                    configuration._check_sdk_cmake(args)
                messages = '\n'.join(call.args[0] for call in configuration._log.call_args_list)
                self.assertNotIn('DEFOLD_SDK_CHECK', messages)
                if fail_java:
                    self.assertIn('✗ Java: Java 25 or newer is required', messages)
                    self.assertNotIn('✓ Java:', messages)
                    self.assertNotIn('Ninja:', messages)
                else:
                    self.assertIn(f'✓ Java: Java 25.0.1 ({root.as_posix()}/Java (test)/java)', messages)
                    self.assertIn('✓ Ninja: Ninja 1.12.1', messages)
                    self.assertEqual(require_protoc, '✓ Protoc: libprotoc 30.2' in messages)
                    self.assertIn('✓ Toolchain: arm64-macos', messages)

    # The real protoc module must give a repair command when missing and find a compiler supplied on PATH.
    @unittest.skipIf(os.name == 'nt', 'The protoc fixture uses a POSIX launcher')
    def test_protoc_failure_and_path_fallback(self):
        repository = Path(__file__).resolve().parents[1]
        cmake = build.shutil.which('cmake')
        for on_path, verbose in ((False, False), (False, True), (True, False)):
            with self.subTest(on_path=on_path, verbose=verbose), tempfile.TemporaryDirectory(prefix='protoc check ') as directory:
                root = Path(directory)
                programs = root / 'bin'
                programs.mkdir()
                if on_path:
                    protoc = programs / 'protoc'
                    protoc.write_text("#!/bin/sh\nprintf 'libprotoc 35.1\\n'\n")
                    protoc.chmod(0o755)
                script = root / 'check_protoc.cmake'
                script.write_text('''
cmake_minimum_required(VERSION 4.0)
function(defold_log)
endfunction()
set(HOST_PLATFORM arm64-macos)
set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH OFF)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
set(CMAKE_FIND_USE_INSTALL_PREFIX OFF)
message("DEFOLD_SDK_CHECK\\tProtoc")
include("${DEFOLD_HOME}/scripts/cmake/functions_sdk.cmake")
include("${DEFOLD_HOME}/scripts/cmake/tools_protoc.cmake")
message("DEFOLD_SDK_CHECK\\tProtoc\\t${_protoc_ver}\\t${_DEFOLD_PROTOC_REAL}")
''')
                configuration = build.Configuration.__new__(build.Configuration)
                configuration.verbose = verbose
                configuration.no_colors = True
                configuration.build_tracker = build.BuildTimeTracker(logger=lambda message: None)
                configuration._log = mock.Mock()
                args = [cmake, f'-DDEFOLD_HOME={repository.as_posix()}',
                        f'-DDEFOLD_SDK_ROOT={root.as_posix()}', '-P', str(script)]
                with mock.patch.dict(os.environ, {'PATH': str(programs)}):
                    if on_path:
                        configuration._check_sdk_cmake(args)
                    else:
                        with self.assertRaises(SystemExit):
                            configuration._check_sdk_cmake(args)
                messages = '\n'.join(call.args[0] for call in configuration._log.call_args_list)
                if on_path:
                    self.assertIn('✓ Protoc: libprotoc 35.1', messages)
                    self.assertNotIn('install_ext', messages)
                else:
                    self.assertIn('✗ Protoc: Could not find protoc. Run ./scripts/build.py install_ext to build and install it.', messages)
                    self.assertEqual(verbose, 'CMake Error' in messages)
                    self.assertEqual(verbose, 'Expected in' in messages)


class PlatformSdkPackagingTests(unittest.TestCase):
    # Both SDK archive formats must include the header used by Extender's Android entry point.
    def test_android_sdk_includes_platform_application_header(self):
        for platform in ('armv7-android', 'arm64-android', 'x86_64-android'):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                sdk_root = Path(directory)
                files = {
                    'extender/build.yml': 'context: {}',
                    f'lib/{platform}/libplatform.a': 'platform archive',
                    f'ext/lib/{platform}/libdependency.a': 'external archive',
                    'share/java/platform_android.jar': 'platform Java classes',
                    'include/platform/platform_app.h': 'platform application header',
                    'sdk/include/dmsdk/dlib/android.h': 'Android SDK header',
                }
                for relative_path, contents in files.items():
                    path = sdk_root / relative_path
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(contents)

                configuration = build.Configuration.__new__(build.Configuration)
                configuration.dynamo_home = str(sdk_root)
                headers_path = sdk_root / 'defoldsdk_headers.zip'
                with contextlib.redirect_stdout(io.StringIO()):
                    archive_path, _ = configuration._package_platform_sdk(platform)
                    configuration._package_platform_sdk_headers(headers_path)

                for sdk_path in (archive_path, headers_path):
                    with self.subTest(archive=Path(sdk_path).name), zipfile.ZipFile(sdk_path) as archive:
                        self.assertEqual(b'platform application header', archive.read('defoldsdk/include/platform/platform_app.h'))
                        self.assertEqual(b'Android SDK header', archive.read('defoldsdk/sdk/include/dmsdk/dlib/android.h'))

    def package_web_sdk(self, platform, external_javascript):
        with tempfile.TemporaryDirectory() as directory:
            sdk_root = Path(directory)
            files = {
                'extender/build.yml': 'context: {}',
                f'lib/{platform}/libplatform.a': 'platform archive',
                f'lib/{platform}/js/library_platform.js': 'platform JavaScript',
                f'lib/{platform}/js/library_sys.js': 'system JavaScript',
                f'ext/lib/{platform}/libdependency.a': 'external archive',
                'ext/wagyu-port/wagyu.py': 'port source',
            }
            if external_javascript:
                files[f'ext/lib/{platform}/js/library_external.js'] = 'external JavaScript'
            for relative_path, contents in files.items():
                path = sdk_root / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents)

            configuration = build.Configuration.__new__(build.Configuration)
            configuration.dynamo_home = str(sdk_root)
            with contextlib.redirect_stdout(io.StringIO()):
                archive_path, signature_path = configuration._package_platform_sdk(platform)
            self.assertTrue(Path(signature_path).is_file())
            with zipfile.ZipFile(archive_path) as archive:
                return {name: archive.read(name).decode() for name in archive.namelist()}

    # Web SDKs retain native platform JS when the removed GLFW2 package leaves no external JS directory.
    def test_web_sdk_without_external_javascript(self):
        for platform in ('wasm-web', 'wasm_pthread-web'):
            with self.subTest(platform=platform):
                files = self.package_web_sdk(platform, external_javascript=False)
                self.assertEqual('platform JavaScript', files[f'defoldsdk/lib/{platform}/js/library_platform.js'])
                self.assertEqual('system JavaScript', files[f'defoldsdk/lib/{platform}/js/library_sys.js'])
                self.assertFalse(any(name.startswith(f'defoldsdk/ext/lib/{platform}/js/') for name in files))

    # Optional external JS libraries are still archived alongside the engine JS libraries when present.
    def test_web_sdk_preserves_external_javascript(self):
        for platform in ('wasm-web', 'wasm_pthread-web'):
            with self.subTest(platform=platform):
                files = self.package_web_sdk(platform, external_javascript=True)
                self.assertEqual('platform JavaScript', files[f'defoldsdk/lib/{platform}/js/library_platform.js'])
                self.assertEqual('external JavaScript', files[f'defoldsdk/ext/lib/{platform}/js/library_external.js'])


class CiSdkTests(unittest.TestCase):
    def test_platform_list_is_forwarded_as_one_shell_argument(self):
        for platforms in ('x86_64-linux,arm64-linux', 'x86_64-linux, arm64-linux', ' x86_64-linux ,\tarm64-linux\n'):
            with self.subTest(platforms=platforms), mock.patch.object(ci, 'call') as call:
                ci.build_sdk('dev', platforms)
                self.assertEqual([
                    sys.executable, 'scripts/build.py', 'install_release_dependencies', 'build_sdk',
                    '--channel=dev', '--platforms=x86_64-linux,arm64-linux',
                ], shlex.split(call.call_args.args[0]))

    def test_omitted_platforms_keep_default_selection(self):
        with mock.patch.object(ci, 'call') as call:
            ci.build_sdk('dev')
        self.assertEqual([
            sys.executable, 'scripts/build.py', 'install_release_dependencies', 'build_sdk', '--channel=dev',
        ], shlex.split(call.call_args.args[0]))


if __name__ == '__main__':
    unittest.main()
