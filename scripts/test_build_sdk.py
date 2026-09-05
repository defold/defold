# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import contextlib
import io
import json
import shlex
import sys
import tempfile
import unittest
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
