import contextlib
import io
import json
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
