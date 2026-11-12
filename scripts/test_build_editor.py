import contextlib
import io
import os
from pathlib import Path
import shlex
import stat
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ci'))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'editor' / 'scripts'))

import build
import bundle
import ci


class EditorCITests(unittest.TestCase):
    def setUp(self):
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        self.enterContext(mock.patch.dict(os.environ, {'BUILD_BRANCH': 'dev', 'GITHUB_REPOSITORY': 'defold/defold'}))
        self.enterContext(mock.patch.object(ci, 'is_repo_private', return_value=False))

    def run_ci(self, arguments):
        with mock.patch.object(sys, 'argv', ['ci.py'] + arguments), \
                mock.patch.object(ci, 'call') as call:
            ci.main(arguments)
        return [shlex.split(invocation.args[0]) for invocation in call.call_args_list]

    # CI must request all editor targets in one build invocation while retaining Linux as the dependency host.
    def test_build_all_targets_uses_one_invocation(self):
        platforms = ','.join(ci.PLATFORMS_DESKTOP)
        commands = self.run_ci(['build-editor', '--platform=x86_64-linux', '--platforms=' + platforms,
                                '--skip-install-ext', '--skip-tests', '--engine-artifacts=dynamo-home'])
        self.assertEqual(1, len(commands))
        command = commands[0]
        self.assertIn('build_editor2', command)
        self.assertIn('--platform=x86_64-linux', command)
        self.assertIn('--engine-artifacts=dynamo-home', command)
        self.assertIn('--skip-tests', command)
        self.assertNotIn('install_ext', command)
        self.assertEqual(['--editor-platform=' + platform for platform in ci.PLATFORMS_DESKTOP],
                         [option for option in command if option.startswith('--editor-platform=')])

    # The Mac finalization command must not clean prepared artifacts or install build dependencies.
    def test_finalize_reuses_prepared_apps_and_forwards_signing_options(self):
        commands = self.run_ci(['finalize-editor', '--platforms=x86_64-macos,arm64-macos', '--codesign',
                                '--notarization-username=test@example.invalid', '--notarization-password=test password',
                                '--notarization-itc-provider=test-team'])
        self.assertEqual(1, len(commands))
        command = commands[0]
        self.assertIn('finalize_editor2', command)
        self.assertNotIn('distclean', command)
        self.assertNotIn('install_ext', command)
        self.assertIn('--editor-platform=x86_64-macos', command)
        self.assertIn('--editor-platform=arm64-macos', command)
        self.assertIn('--codesign', command)
        self.assertIn('--notarization-password=test password', command)

    # Each job must archive only its completed platforms, leaving macOS publication to finalization.
    def test_archive_platform_selection(self):
        commands = self.run_ci(['archive-editor', '--platforms=x86_64-linux,x86_64-win32', '--skip-install-ext'])
        self.assertEqual(3, len(commands))
        self.assertIn('install_release_dependencies', commands[0])
        self.assertEqual(['--platform=x86_64-linux', '--platform=x86_64-win32'],
                         [option for command in commands for option in command if option.startswith('--platform=')])
        self.assertTrue(all('archive_editor2' in command and 'install_ext' not in command for command in commands[1:]))

    # A bad target list must fail before CI starts a build or signs any files.
    def test_invalid_target_selection_is_rejected(self):
        for platforms in ('x86_64-linux,wasm-web', 'x86_64-linux,x86_64-linux', 'x86_64-linux,'):
            arguments = ['build-editor', '--platforms=' + platforms]
            with self.subTest(platforms=platforms), mock.patch.object(ci, 'call') as call, \
                    mock.patch.object(sys, 'argv', ['ci.py'] + arguments):
                with self.assertRaises(ValueError):
                    ci.main(arguments)
                call.assert_not_called()

    # Existing single-platform callers must continue selecting exactly one editor target.
    def test_single_platform_build_remains_supported(self):
        commands = self.run_ci(['build-editor', '--platform=arm64-macos', '--skip-install-ext'])
        self.assertEqual(['--editor-platform=arm64-macos'],
                         [option for option in commands[0] if option.startswith('--editor-platform=')])


class EditorBuildDriverTests(unittest.TestCase):
    # The build driver must preserve the full target list and route finalization without recompilation.
    def test_build_and_finalize_forward_all_targets(self):
        with tempfile.TemporaryDirectory(prefix='editor build driver ') as directory:
            configuration = build.Configuration(dynamo_home=directory, target_platform='x86_64-linux',
                                                editor_platforms=['x86_64-macos', 'arm64-macos'],
                                                channel='alpha', engine_artifacts='engine-sha', skip_tests=True)
            with mock.patch.object(configuration, 'run_editor_script') as run:
                configuration.build_editor2()
                configuration.finalize_editor2()
            self.assertEqual(2, run.call_count)
            for invocation, expected in zip(run.call_args_list, ('build', 'finalize')):
                command = invocation.args[0]
                self.assertIn(expected, command)
                self.assertEqual(['--platform=x86_64-macos', '--platform=arm64-macos'],
                                 [option for option in command if option.startswith('--platform=')])


@unittest.skipIf(os.name == 'nt', 'Symlink creation requires additional privileges on Windows')
class EditorFinalizationTests(unittest.TestCase):
    # Existing updaters need full file contents, and signing must see the same materialized files as ZIP/DMG packaging.
    def test_symlinks_are_materialized_before_signing_and_packaging(self):
        for codesign in (False, True):
            with self.subTest(codesign=codesign), tempfile.TemporaryDirectory() as directory, \
                    contextlib.chdir(directory), contextlib.redirect_stdout(io.StringIO()):
                app = Path('Defold.app')
                resources = app / 'Contents' / 'Resources'
                legal = resources / 'packages' / 'jdk-new' / 'legal'
                (legal / 'java.base').mkdir(parents=True)
                (legal / 'java.desktop').mkdir()
                license_text = b'The complete license text must survive an editor update.\n'
                (legal / 'java.base' / 'LICENSE').write_bytes(license_text)
                (legal / 'java.desktop' / 'LICENSE').symlink_to('../java.base/LICENSE')
                (legal / 'LICENSE').symlink_to('java.desktop/LICENSE')
                shared = resources / 'shared'
                shared.mkdir()
                (shared / 'asset').write_bytes(b'shared asset')
                (shared / 'license').symlink_to('../packages/jdk-new/legal/java.base/LICENSE')
                (resources / 'shared-alias').symlink_to('shared', target_is_directory=True)
                executables = app / 'Contents' / 'MacOS'
                executables.mkdir()
                (executables / 'launcher').write_bytes(b'launcher')
                (executables / 'launcher').chmod(0o755)
                (executables / 'Defold').symlink_to('launcher')
                Path('target/editor').mkdir(parents=True)
                options = SimpleNamespace(codesign=codesign)

                def snapshot():
                    files = {}
                    for root, dirs, names in os.walk(app):
                        for name in dirs + names:
                            self.assertFalse((Path(root) / name).is_symlink())
                        for name in names:
                            file = Path(root) / name
                            files[file.as_posix()] = (file.read_bytes(), stat.S_IMODE(file.stat().st_mode))
                    return files

                signed_files = []
                with mock.patch.object(bundle, 'sign', side_effect=lambda *_: signed_files.append(snapshot())) as sign, \
                        mock.patch.object(bundle, 'create_dmg', side_effect=lambda *_: snapshot()) as create_dmg:
                    bundle.finish_bundle(str(app), 'arm64-macos', options)
                self.assertEqual(int(codesign), sign.call_count)
                create_dmg.assert_called_once_with(str(app), options, 'arm64-macos')
                files = snapshot()
                if codesign:
                    self.assertEqual(files, signed_files[0])
                with zipfile.ZipFile('target/editor/Defold-arm64-macos.zip') as archive:
                    self.assertEqual(set(files), set(archive.namelist()))
                    for name, (contents, mode) in files.items():
                        entry = archive.getinfo(name)
                        self.assertTrue(stat.S_ISREG(entry.external_attr >> 16))
                        self.assertEqual(mode, stat.S_IMODE(entry.external_attr >> 16))
                        self.assertEqual(contents, archive.read(entry))
                    self.assertEqual(license_text, archive.read((legal / 'java.desktop' / 'LICENSE').as_posix()))
                    self.assertEqual(license_text, archive.read((resources / 'shared-alias' / 'license').as_posix()))
                    self.assertEqual(b'launcher', archive.read((executables / 'Defold').as_posix()))

    # Reject missing symlink targets before signing instead of shipping link paths as updater file contents.
    def test_broken_symlink_stops_finalization_before_signing(self):
        with tempfile.TemporaryDirectory() as directory, contextlib.chdir(directory):
            app = Path('Defold.app')
            app.mkdir()
            link = app / 'LICENSE'
            link.symlink_to('missing-license')
            Path('target/editor').mkdir(parents=True)
            with mock.patch.object(bundle, 'sign') as sign, mock.patch.object(bundle, 'create_dmg') as create_dmg:
                with self.assertRaises(FileNotFoundError):
                    bundle.finish_bundle(str(app), 'arm64-macos', SimpleNamespace(codesign=True))
            sign.assert_not_called()
            create_dmg.assert_not_called()
            self.assertTrue(link.is_symlink())


if __name__ == '__main__':
    unittest.main()
