# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import build


class BobDependencyTests(unittest.TestCase):
    def test_install_ext_covers_bob_tools_for_every_public_host_and_target(self):
        # Bob packages these tools for all desktop hosts, regardless of the
        # host/target selected when install_ext prepares the dependencies.
        prefixes = ('aapt2-', 'apkc-', 'glslang-', 'gltf-validator-', 'lipo-',
                    'luajit-', 'ogg-', 'spirv-tools-', 'strip_android-', 'tint-')
        names = ('codesign_allocate', 'strip', 'zipalign')
        required = {
            f'{package}-{platform}.tar.gz'
            for platform in build.BOB_TOOL_PLATFORMS
            for package in build.PLATFORM_PACKAGES[platform]
            if package.startswith(prefixes) or package in names
        }
        required.update(
            f'{package}-{platform}.tar.gz'
            for platform in ('armv7-android', 'arm64-android', 'x86_64-android')
            for package in build.PLATFORM_PACKAGES[platform]
            if package.startswith('vkquality-'))
        required.add(f'{build.sdk.ANDROID_PACKAGE}-arm64-android.tar.gz')

        with tempfile.TemporaryDirectory() as temporary_directory:
            configuration = build.Configuration.__new__(build.Configuration)
            configuration.defold_root = str(Path(__file__).resolve().parents[1])
            configuration.dynamo_home = temporary_directory
            configuration.ext = str(Path(temporary_directory) / 'ext')
            configuration.dmsdk = str(Path(temporary_directory) / 'sdk')
            (Path(temporary_directory) / 'share/proto').mkdir(parents=True)
            configuration._build_engine_with_waf = lambda: False
            configuration._install_python_packages = mock.Mock()
            configuration._copy = mock.Mock()
            with mock.patch.object(build.build_private, 'get_install_host_packages', return_value=[]), \
                    mock.patch.object(build.build_private, 'get_install_target_packages', return_value=[]), \
                    contextlib.redirect_stdout(io.StringIO()):
                for host in build.BOB_TOOL_PLATFORMS:
                    for target in build.BASE_PLATFORMS:
                        with self.subTest(host=host, target=target):
                            configuration.host = host
                            configuration.target_platform = target
                            configuration._extract_tgz = mock.Mock()
                            configuration.install_ext()
                            installed = {
                                Path(call.args[0]).name
                                for call in configuration._extract_tgz.call_args_list
                                if call.args[1] == configuration.ext
                            }
                            self.assertFalse(required - installed, sorted(required - installed))


class BobArchiveTests(unittest.TestCase):
    def setUp(self):
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        self.root = Path(temporary_directory.name)
        self.configuration = build.Configuration.__new__(build.Configuration)
        self.configuration.defold_root = str(Path(__file__).resolve().parents[1])
        self.configuration.dynamo_home = str(self.root)
        self.configuration.thread_pool = object()
        self.configuration._git_sha1 = lambda: 'revision'
        self.configuration.get_archive_path = lambda: 's3://bucket/archive/dev'
        self.configuration._mkdirs = lambda path: Path(path).mkdir(parents=True, exist_ok=True)
        self.configuration._log = lambda message: None

    def sync(self, paths):
        prefix = 'archive/dev/revision/'
        keys = [prefix + path for path in paths]
        bucket = mock.Mock()
        bucket.objects.filter.side_effect = lambda Prefix: [
            SimpleNamespace(key=key) for key in keys if key.startswith(Prefix)]
        downloaded = {}

        def object_for_key(key):
            def download_file(path):
                downloaded[key[len(prefix):]] = Path(path).relative_to(self.root).as_posix()
            return SimpleNamespace(key=key, download_file=download_file)

        bucket.Object.side_effect = object_for_key
        with mock.patch.object(build.s3, 'get_bucket', return_value=bucket), \
                mock.patch.object(build, 'Future', side_effect=lambda pool, fn, *args: lambda: fn(*args)):
            self.configuration.sync_archive()
        bucket.objects.filter.assert_called_once_with(Prefix=prefix + 'engine/')
        return downloaded

    def test_downloads_public_bob_inputs_without_other_engine_outputs(self):
        manifest = Path(self.configuration.defold_root) / 'com.dynamo.cr/com.dynamo.cr.bob/archive-artifacts.json'
        required = {'engine/' + path for path in json.loads(manifest.read_text())}
        unused = {
            'engine/x86_64-linux/dmengine',
            'engine/x86_64-linux/dmengine_headless',
            'engine/x86_64-linux/gdc_x86_64_linux',
            'engine/x86_64-linux/libparticle_shared.so',
            'engine/x86_64-win32/dmengine.pdb',
            'engine/arm64-linux/stripped/dmengine',
            'engine/armv7-android/stripped/libdmengine.so',
            'engine/wasm-web/dmengine.js.symbols',
            'engine/wasm_pthread-web/dmengine.wasm',
            'engine/arm64-android/android.jar',
            'engine/arm64-android/android-resources.zip',
            'engine/arm64-android/defoldsdk.sha256',
            'engine/defoldsdk_headers.zip',
            'engine/platform.sdks.json',
            'engine/share/ref-doc.zip',
            'bob/bob.jar',
            'editor2/Defold.zip',
        }
        self.assertFalse(required & unused)
        self.assertEqual(
            {path: 'archive/revision/' + path for path in required},
            self.sync(required | unused))

    def test_preserves_private_platform_inputs_and_their_existing_exclusions(self):
        required = {
            'engine/arm64-nx64/dmengine.nss',
            'engine/x86_64-private/stripped/dmengine',
            'engine/x86_64-private/compiler.dll',
            'engine/x86_64-private/dmengine.pdb',
        }
        excluded = {
            'engine/x86_64-private/',
            'engine/x86_64-private/dmengine_headless',
            'engine/x86_64-private/stripped/dmengine_headless',
            'engine/x86_64-private/launcher.exe',
            'engine/x86_64-private/defoldsdk.zip',
            'engine/x86_64-private/editor2/resources.zip',
            'engine/x86_64-private/armv7-android/libdmengine.so',
            'engine/x86_64-private/arm64-linux/stripped/dmengine',
        }
        self.assertEqual(
            {path: 'archive/revision/' + path for path in required},
            self.sync(required | excluded))


if __name__ == '__main__':
    unittest.main()
