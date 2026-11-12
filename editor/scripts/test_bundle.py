import contextlib
import io
from pathlib import Path
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import bundle


class CrossBundleTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        directory = self.enterContext(tempfile.TemporaryDirectory(prefix='editor cross build '))
        self.directory = Path(directory)
        self.enterContext(contextlib.chdir(directory))

    def jmods_archive(self, module):
        archive = self.directory / 'jmods.tar.gz-00'
        with tarfile.open(archive, 'w:gz') as tar:
            entry = tarfile.TarInfo('jdk-%s-jmods/%s' % (bundle.java_version, module))
            entry.size = 5
            tar.addfile(entry, io.BytesIO(b'jmod\n'))
        return str(archive)

    # Cross-linking must use the native JDK and the pinned target modules, never the host modules.
    def test_cross_runtime_uses_target_jmods(self):
        archive = self.jmods_archive('java.base.jmod')
        real_command = bundle.run.command
        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'download', return_value=archive) as download, \
                mock.patch.object(bundle.run, 'command', wraps=bundle.run.command) as command:
            # Run real archive extraction; capture only the jlink process.
            command.side_effect = lambda args: '' if args[0].endswith('/jlink') else real_command(args)
            bundle.create_runtime('/arm-jdk', 'x86_64-macos', 'intel-runtime')
        url = download.call_args.args[1]
        self.assertIn('-jmods_x64_mac_hotspot_', url)
        self.assertIn(bundle.java_version.replace('+', '_'), url)
        jlink = command.call_args.args[0]
        self.assertEqual('/arm-jdk/bin/jlink', jlink[0])
        module_path = next(arg.split('=', 1)[1] for arg in jlink if arg.startswith('--module-path='))
        self.assertEqual(b'jmod\n', (Path(module_path) / 'java.base.jmod').read_bytes())

    # Incomplete target archives must fail before jlink can silently produce a host runtime.
    def test_missing_target_base_module_is_rejected(self):
        archive = self.jmods_archive('java.desktop.jmod')
        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'download', return_value=archive), \
                mock.patch.object(bundle.run, 'command', wraps=bundle.run.command) as command:
            with self.assertRaisesRegex(Exception, 'missing java.base.jmod'):
                bundle.create_runtime('/arm-jdk', 'x86_64-macos', 'intel-runtime')
        self.assertFalse(any(call.args[0][0].endswith('/jlink') for call in command.call_args_list))

    # Same-platform builds retain runtime-image linking without requiring separate JMOD downloads.
    def test_native_runtime_does_not_download_jmods(self):
        for platform in bundle.platform_to_java:
            with self.subTest(platform=platform), \
                    mock.patch.object(bundle, 'current_platform', return_value=platform), \
                    mock.patch.object(bundle, 'download') as download, \
                    mock.patch.object(bundle.run, 'command') as command:
                bundle.create_runtime('/native-jdk', platform, 'runtime')
                download.assert_not_called()
                self.assertFalse(any(arg.startswith('--module-path=') for arg in command.call_args.args[0]))

    # ARM-to-Intel builds must compile on ARM with both sets of natives and still package an Intel bundle.
    def test_cross_build_keeps_host_tools_and_target_bundle_separate(self):
        options = SimpleNamespace(target_platform=['x86_64-macos'], skip_tests=True,
                                  engine_sha1='engine-sha', archive_domain='archive.example')
        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'has_rosetta', return_value=False), \
                mock.patch.object(bundle, 'place_release_notes'), \
                mock.patch.object(bundle, 'get_jdk', return_value='/arm-jdk') as get_jdk, \
                mock.patch.object(bundle, 'invoke_lein') as lein, \
                mock.patch.object(bundle, 'create_bundle') as create_bundle:
            self.assertIn('x86_64-macos', bundle.supported_platforms())
            bundle.build(options)
        get_jdk.assert_called_once_with('arm64-macos')
        self.assertEqual(['x86_64-macos', 'arm64-macos'], lein.call_args_list[0].args[0][-2:])
        self.assertTrue(all(call.kwargs['jdk_path'] == '/arm-jdk' for call in lein.call_args_list))
        create_bundle.assert_called_once_with('/arm-jdk', 'x86_64-macos', options)


if __name__ == '__main__':
    unittest.main()
