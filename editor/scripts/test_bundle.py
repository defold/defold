import contextlib
import configparser
import io
import os
from pathlib import Path
import stat
import tarfile
import tempfile
from threading import Barrier, Event
from types import SimpleNamespace
import unittest
from unittest import mock
import zipfile

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

    # Consolidation must prepare, test and compile once even when packaging every desktop target.
    def test_all_targets_share_one_compilation(self):
        platforms = list(bundle.platform_to_java)
        for skip_tests in (True, False):
            with self.subTest(skip_tests=skip_tests):
                options = SimpleNamespace(target_platform=platforms, skip_tests=skip_tests,
                                          engine_sha1='engine-sha', archive_domain='archive.example')
                with mock.patch.object(bundle, 'current_platform', return_value='x86_64-linux'), \
                        mock.patch.object(bundle, 'place_release_notes'), \
                        mock.patch.object(bundle, 'get_jdk', return_value='/linux-jdk') as get_jdk, \
                        mock.patch.object(bundle, 'invoke_lein') as lein, \
                        mock.patch.object(bundle, 'run_tests') as run_tests, \
                        mock.patch.object(bundle, 'create_bundle') as create_bundle:
                    self.assertEqual(set(platforms), bundle.supported_build_platforms())
                    self.assertEqual({'x86_64-linux'}, bundle.supported_platforms())
                    bundle.build(options)
                get_jdk.assert_called_once_with('x86_64-linux')
                self.assertEqual([
                    mock.call(['with-profile', '+release', 'init', 'engine-sha', 'archive.example'] + platforms, jdk_path='/linux-jdk'),
                    mock.call(['with-profile', '+ns-batch-builder', 'run', '-m', 'editor.ns-batch-builder', 'resources/sorted_clojure_ns_list.edn'], jdk_path='/linux-jdk'),
                    mock.call(['prerelease-prepared' if skip_tests else 'prerelease'], jdk_path='/linux-jdk'),
                ], lein.call_args_list)
                self.assertEqual(0 if skip_tests else 1, run_tests.call_count)
                self.assertEqual([mock.call('/linux-jdk', platform, options) for platform in platforms],
                                 create_bundle.call_args_list)

    # Windows modules are ZIPs, while both Mac module sets are tarballs; all must link on Linux.
    def test_linux_links_all_foreign_runtimes(self):
        for platform, jdk_platform in bundle.platform_to_java.items():
            if platform == 'x86_64-linux':
                continue
            with self.subTest(platform=platform):
                if platform == 'x86_64-win32':
                    archive = self.directory / 'jmods.zip-00'
                    with zipfile.ZipFile(archive, 'w') as output:
                        output.writestr('jdk-%s-jmods/java.base.jmod' % bundle.java_version, b'jmod\n')
                else:
                    archive = self.jmods_archive('java.base.jmod')
                real_command = bundle.run.command
                with mock.patch.object(bundle, 'current_platform', return_value='x86_64-linux'), \
                        mock.patch.object(bundle, 'download', return_value=str(archive)) as download, \
                        mock.patch.object(bundle.run, 'command') as command:
                    command.side_effect = lambda args: '' if args[0].endswith('/jlink') else real_command(args)
                    bundle.create_runtime('/linux-jdk', platform, 'runtime')
                self.assertIn('-jmods_%s_hotspot_' % jdk_platform, download.call_args.args[1])
                self.assertEqual('/linux-jdk/bin/jlink', command.call_args.args[0][0])
                modules = Path('build/jmods') / platform / ('jdk-%s-jmods' % bundle.java_version)
                self.assertEqual(b'jmod\n', (modules / 'java.base.jmod').read_bytes())


@unittest.skipIf(os.name == 'nt', 'Prepared macOS bundles are built on Linux and finalized on macOS')
class PreparedBundleTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        self.directory = Path(self.enterContext(tempfile.TemporaryDirectory(prefix='editor prepared bundle ')))
        self.enterContext(contextlib.chdir(self.directory))
        resources = Path('bundle-resources')
        resources.mkdir()
        for name in ('Info.plist', 'Assets.car', 'document_legacy.icns', 'logo.icns', 'logo_blue.png'):
            (resources / name).write_text(name)
        (resources / 'config').write_text('[build]\n')
        self.jar = self.directory / 'editor.jar'
        with zipfile.ZipFile(self.jar, 'w') as jar:
            for name in bundle.REQUIRED_ANDROID_VKQUALITY_FILES:
                jar.writestr(name, name)
        self.launcher = self.directory / 'launcher'
        self.launcher.write_bytes(b'launcher')
        self.platforms = ['x86_64-macos', 'arm64-macos']
        self.options = SimpleNamespace(target_platform=self.platforms, codesign=True,
                                       editor_sha1='editor-sha', engine_sha1='engine-sha',
                                       version='1.0.0', channel='alpha', archive_domain='archive.example')
        self.enterContext(mock.patch.object(bundle, 'create_standalone_editor_jar', return_value=str(self.jar)))
        self.enterContext(mock.patch.object(bundle, 'launcher_path', return_value=str(self.launcher)))
        self.enterContext(mock.patch.object(bundle, 'create_runtime', side_effect=self.create_runtime))

    def create_runtime(self, jdk, platform, output):
        runtime = Path(output)
        (runtime / 'bin').mkdir(parents=True)
        java = runtime / 'bin' / ('java.exe' if platform == 'x86_64-win32' else 'java')
        java.write_text(platform)
        java.chmod(0o755)
        (runtime / 'bin' / 'java-link').symlink_to(java.name)
        (runtime / 'bin-link').symlink_to('bin', target_is_directory=True)

    def prepare_macos(self):
        with mock.patch.object(bundle, 'current_platform', return_value='x86_64-linux'), \
                mock.patch.object(bundle, 'sign') as sign, \
                mock.patch.object(bundle, 'create_dmg') as create_dmg:
            for platform in self.platforms:
                bundle.create_bundle('/linux-jdk', platform, self.options)
            sign.assert_not_called()
            create_dmg.assert_not_called()

    # The handoff must retain both apps, target runtimes, symlinks and executable bits without signing on Linux.
    def test_macos_preparation_preserves_app_contents(self):
        self.prepare_macos()
        for platform in self.platforms:
            self.assertFalse(Path('target/editor/Defold-%s.zip' % platform).exists())
            with tarfile.open('target/editor/prepared/Defold-%s.tar' % platform) as archive:
                base = 'Defold.app/Contents/Resources/packages/jdk-%s' % bundle.java_version
                self.assertEqual(platform.encode(), archive.extractfile(base + '/bin/java').read())
                self.assertTrue(archive.getmember(base + '/bin/java').mode & stat.S_IXUSR)
                self.assertTrue(archive.getmember('Defold.app/Contents/MacOS/Defold').mode & stat.S_IXUSR)
                self.assertTrue(archive.getmember(base + '/bin/java-link').issym())
                self.assertEqual('bin', archive.getmember(base + '/bin-link').linkname)

    # Both apps must sign concurrently, and their signed contents must reach both final distributables.
    def test_finalization_signs_before_archiving_without_rebuilding(self):
        self.prepare_macos()
        signing_started = Barrier(2, timeout=10)

        def sign(directory, platform, options):
            app = Path(directory)
            config = configparser.ConfigParser()
            config.read(app / 'Contents/Resources/config')
            self.assertEqual('engine-sha', config.get('build', 'engine_sha1'))
            self.assertEqual('1.0.0', config.get('build', 'version'))
            java = app / ('Contents/Resources/packages/jdk-%s/bin/java' % bundle.java_version)
            self.assertEqual(platform, java.read_text())
            self.assertTrue(java.stat().st_mode & stat.S_IXUSR)
            self.assertTrue(java.with_name('java-link').is_symlink())
            signing_started.wait()
            (app / 'signed').write_text(platform)

        def create_dmg(directory, options, platform):
            self.assertEqual(platform, (Path(directory) / 'signed').read_text())
            Path('target/editor/Defold-%s.dmg' % platform).write_text(platform)

        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'get_jdk', side_effect=AssertionError('Finalization must not download a JDK')), \
                mock.patch.object(bundle, 'invoke_lein', side_effect=AssertionError('Finalization must not compile')), \
                mock.patch.object(bundle, 'sign', side_effect=sign) as signing, \
                mock.patch.object(bundle, 'create_dmg', side_effect=create_dmg):
            bundle.finalize(self.options)
        self.assertEqual(2, signing.call_count)
        for platform in self.platforms:
            with zipfile.ZipFile('target/editor/Defold-%s.zip' % platform) as archive:
                self.assertEqual(platform.encode(), archive.read('Defold.app/signed'))
                base = 'Defold.app/Contents/Resources/packages/jdk-%s' % bundle.java_version
                self.assertEqual(b'java', archive.read(base + '/bin/java-link'))
                self.assertTrue(stat.S_ISLNK(archive.getinfo(base + '/bin/java-link').external_attr >> 16))
                self.assertEqual(b'bin', archive.read(base + '/bin-link'))
            with tarfile.open('target/editor/prepared/Defold-%s.tar' % platform) as archive:
                self.assertNotIn('Defold.app/signed', archive.getnames())

    # Packing the second DMG must proceed while the first awaits notarization, using separate staging trees.
    def test_dmg_packaging_overlaps_notarization_without_sharing_staging(self):
        self.prepare_macos()
        Path('bundle-resources/dmg_ds_store').write_bytes(b'layout')
        Path('bundle-resources/dmg_background').mkdir()
        Path('bundle-resources/dmg_background/background.png').write_bytes(b'background')
        first_notarizing = Event()
        second_packed = Event()
        real_command = bundle.run.command

        def command(args):
            if args[0] != 'hdiutil':
                return real_command(args)
            output = Path(args[-1])
            platform = output.stem.removeprefix('Defold-')
            staging = Path(args[args.index('-srcfolder') + 1])
            self.assertEqual(Path('build/dmg') / platform, staging)
            if platform == 'arm64-macos':
                self.assertTrue(first_notarizing.wait(10), 'The first target did not reach notarization')
            java = staging / ('Defold.app/Contents/Resources/packages/jdk-%s/bin/java' % bundle.java_version)
            self.assertEqual(platform, java.read_text())
            output.write_text(platform)
            if platform == 'arm64-macos':
                second_packed.set()
            return ''

        def notarize(dmg, options):
            if dmg.endswith('x86_64-macos.dmg'):
                first_notarizing.set()
                self.assertTrue(second_packed.wait(10), 'The second target waited for the first notarization')

        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'sign'), \
                mock.patch.object(bundle.codesigning, 'sign_macos_dmg') as sign_dmg, \
                mock.patch.object(bundle.run, 'command', side_effect=command), \
                mock.patch.object(bundle, 'notarize_dmg', side_effect=notarize) as notarization:
            bundle.finalize(self.options)
        self.assertEqual(2, sign_dmg.call_count)
        self.assertEqual(2, notarization.call_count)
        for platform in self.platforms:
            self.assertEqual(platform, Path('target/editor/Defold-%s.dmg' % platform).read_text())

    # Failures, including sys.exit from signing commands, must fail finalization even if the other target succeeds.
    def test_parallel_finalization_propagates_worker_failures(self):
        self.prepare_macos()
        for error in (RuntimeError('signing failed'), SystemExit(7)):
            with self.subTest(error=type(error).__name__):
                other_finished = Event()

                def finish(directory, platform, options):
                    if platform == 'x86_64-macos':
                        raise error
                    other_finished.set()

                with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                        mock.patch.object(bundle, 'finish_bundle', side_effect=finish):
                    with self.assertRaises(type(error)) as raised:
                        bundle.finalize(self.options)
                self.assertIs(error, raised.exception)
                self.assertTrue(other_finished.is_set())

    # Duplicate targets must be rejected before parallel workers can overwrite the same output files.
    def test_parallel_finalization_rejects_duplicate_targets(self):
        self.options.target_platform = ['arm64-macos', 'arm64-macos']
        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'finish_bundle') as finish:
            with self.assertRaisesRegex(ValueError, 'Duplicate macOS'):
                bundle.finalize(self.options)
        finish.assert_not_called()

    # Contrib and skip-sign builds must still produce macOS distributables without accessing signing credentials.
    def test_unsigned_finalization(self):
        self.prepare_macos()
        self.options.codesign = False
        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'sign') as sign, \
                mock.patch.object(bundle, 'create_dmg') as dmg:
            bundle.finalize(self.options)
        sign.assert_not_called()
        self.assertEqual(2, dmg.call_count)
        for platform in self.platforms:
            self.assertTrue(Path('target/editor/Defold-%s.zip' % platform).is_file())

    # A stale or wrong-run artifact must never be signed under the current checkout's identity.
    def test_finalization_rejects_a_different_commit(self):
        self.prepare_macos()
        self.options.editor_sha1 = 'different-commit'
        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'), \
                mock.patch.object(bundle, 'sign') as sign:
            with self.assertRaisesRegex(ValueError, 'does not match'):
                bundle.finalize(self.options)
        sign.assert_not_called()

    # Linux and Windows builds must remain distributable immediately, including Windows signing on Linux.
    def test_linux_and_windows_bundles_are_finished_on_linux(self):
        with mock.patch.object(bundle, 'current_platform', return_value='x86_64-linux'), \
                mock.patch.object(bundle.codesigning, 'sign_file') as sign, \
                mock.patch.object(bundle, 'create_dmg') as dmg:
            for platform in ('x86_64-linux', 'x86_64-win32'):
                bundle.create_bundle('/linux-jdk', platform, self.options)
                with zipfile.ZipFile('target/editor/Defold-%s.zip' % platform) as archive:
                    self.assertIn('Defold/Defold' + bundle.get_exe_suffix(platform), archive.namelist())
        sign.assert_called_once_with('x86_64-win32', self.options, 'tmp/Defold/Defold.exe')
        dmg.assert_not_called()
        self.assertTrue(Path('target/editor/Defold-x86_64-linux.tar.gz').is_file())

    # Finalization must fail before touching artifacts when the host or requested targets are unsupported.
    def test_finalization_requires_macos_host_and_targets(self):
        with mock.patch.object(bundle, 'current_platform', return_value='x86_64-linux'):
            with self.assertRaisesRegex(ValueError, 'requires a Mac'):
                bundle.finalize(self.options)
        self.options.target_platform.append('x86_64-win32')
        with mock.patch.object(bundle, 'current_platform', return_value='arm64-macos'):
            with self.assertRaisesRegex(ValueError, 'Only macOS'):
                bundle.finalize(self.options)


if __name__ == '__main__':
    unittest.main()
