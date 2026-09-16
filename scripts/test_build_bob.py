# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import build


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
