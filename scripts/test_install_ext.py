# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import contextlib
import io
import os
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

import build


class InstallExtTests(unittest.TestCase):
    # Every host/target combination must install the shared Android JAR once,
    # without requesting any of the removed architecture-specific archives.
    def test_shared_android_package_is_installed_once(self):
        root = Path(__file__).resolve().parents[1]
        android_package = root / 'packages' / (build.sdk.ANDROID_PACKAGE + '-common.tar.gz')
        with tempfile.TemporaryDirectory() as directory:
            for host in build.BOB_TOOL_PLATFORMS:
                for target in build.PLATFORM_PACKAGES:
                    with self.subTest(host=host, target=target):
                        configuration = build.Configuration.__new__(build.Configuration)
                        configuration.defold_root = str(root)
                        configuration.dynamo_home = directory
                        configuration.ext = os.path.join(directory, 'ext')
                        configuration.dmsdk = os.path.join(directory, 'sdk')
                        configuration.host = host
                        configuration.target_platform = target
                        configuration._build_engine_with_waf = mock.Mock(return_value=False)
                        configuration._extract_tgz = mock.Mock()
                        configuration._install_python_packages = mock.Mock()
                        configuration._copy = mock.Mock()

                        with contextlib.redirect_stdout(io.StringIO()):
                            configuration.install_ext()

                        archives = [Path(call.args[0]) for call in configuration._extract_tgz.call_args_list]
                        android_archives = [path for path in archives if path.name.startswith(build.sdk.ANDROID_PACKAGE + '-')]
                        self.assertEqual([android_package], android_archives)
                        self.assertTrue(all(path.is_file() for path in archives))

    # Extract through the real installer and verify the platform API classes are
    # available at the path used by Bob and Android builds after consolidation.
    def test_shared_android_package_extracts_platform_jar(self):
        root = Path(__file__).resolve().parents[1]
        archive = root / 'packages' / (build.sdk.ANDROID_PACKAGE + '-common.tar.gz')
        with tempfile.TemporaryDirectory() as directory:
            configuration = build.Configuration.__new__(build.Configuration)
            configuration._log = mock.Mock()
            configuration._form_env = mock.Mock(return_value=dict(os.environ))
            configuration._extract_tgz(str(archive), directory)

            with zipfile.ZipFile(Path(directory) / 'share/java/android.jar') as jar:
                self.assertIn('android/app/Activity.class', jar.namelist())


if __name__ == '__main__':
    unittest.main()
