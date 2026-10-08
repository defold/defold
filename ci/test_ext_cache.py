import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / 'scripts'))

import build
import ci
import ext_cache


class RepositoryFixture(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.git('init', '-q')
        self.write('.gitignore', 'external/build/\n')
        self.source = self.write('external/lz4/main.c', 'int main(void) { return 0; }\n')
        self.git('add', '.')

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.STDOUT)

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path


class SourceIdentityTests(RepositoryFixture):
    # Only file contents should matter; touching, staging, or refreshing the index must preserve a key.
    def test_contents_are_independent_of_index_state(self):
        key = ext_cache.source_identity(self.root)
        os.utime(self.source, (1700000000, 1700000000))
        self.assertEqual(key, ext_cache.source_identity(self.root))
        self.git('update-index', '--refresh')
        self.assertEqual(key, ext_cache.source_identity(self.root))
        self.source.write_text('int main(void) { return 1; }\n')
        changed = ext_cache.source_identity(self.root)
        self.assertNotEqual(key, changed)
        self.git('add', '.')
        self.assertEqual(changed, ext_cache.source_identity(self.root))
        self.source.rename(self.source.with_name('renamed.c'))
        self.assertNotEqual(changed, ext_cache.source_identity(self.root))

    # Engine edits, documentation, generated files, packages and unrelated tools must not discard this cache.
    def test_only_dependency_inputs_invalidate(self):
        key = ext_cache.source_identity(self.root)
        for name in ('engine/current.h', 'external/README.md', 'external/lz4/readme.md',
                     'external/AGENTS.md', 'external/build/host/build.ninja', 'scripts/cmake/README.md',
                     'external/dawn/CMakeLists.txt', 'external/vkquality/source.cpp',
                     'external/fir_filter/source.cpp', 'external/rebuild.sh',
                     'scripts/build.py', 'build_tools/sdk.py',
                     'build_tools/release_to_github.py', 'packages/unrelated.tar.gz'):
            self.write(name, 'unrelated')
        self.assertEqual(key, ext_cache.source_identity(self.root))
        added = self.write('external/lz4/new.h', '#define NEW 1\n')
        self.assertNotEqual(key, ext_cache.source_identity(self.root))
        added.unlink()
        self.assertEqual(key, ext_cache.source_identity(self.root))
        self.source.unlink()
        self.assertNotEqual(key, ext_cache.source_identity(self.root))

    # Adding a dependency to the CMake build must also add its sources to the cache identity.
    def test_all_cmake_dependencies_invalidate(self):
        cmake = (REPO_ROOT / 'external/CMakeLists.txt').read_text()
        dependencies = re.findall(r'^\s*add_subdirectory\(([^)]+)\)', cmake, re.MULTILINE)
        self.assertTrue(dependencies)
        key = ext_cache.source_identity(self.root)
        for dependency in dependencies:
            with self.subTest(dependency=dependency):
                source = self.write('external/%s/cache_input.h' % dependency, 'changed')
                self.assertNotEqual(key, ext_cache.source_identity(self.root))
                source.unlink()


class StagingTests(RepositoryFixture):
    # Missing files and paths outside the install prefix must never publish a partial cache directory.
    def test_failed_staging_is_not_published(self):
        prefix = self.root / 'install'
        installed = self.write('install/lib/library.a', 'library')
        destination = self.root / 'cache/key'
        for invalid in (prefix / 'missing.a', self.source):
            with self.subTest(invalid=invalid):
                manifest = self.write('install_manifest.txt', '%s\n%s\n' % (installed, invalid))
                with self.assertRaises((OSError, ValueError)):
                    ext_cache.stage_install(manifest, prefix, destination)
                self.assertFalse(destination.exists())

    # Dereferenced executable aliases must remain usable without Windows symlink privileges.
    @unittest.skipIf(os.name == 'nt', 'Symlink creation can require administrator privileges')
    def test_staging_preserves_executable_aliases(self):
        executable = self.write('install/bin/protoc-version', 'executable')
        executable.chmod(0o755)
        alias = executable.with_name('protoc')
        alias.symlink_to(executable.name)
        manifest = self.write('install_manifest.txt', str(alias))
        destination = self.root / 'cache/key'
        ext_cache.stage_install(manifest, self.root / 'install', destination)
        restored = destination / 'bin/protoc'
        self.assertFalse(restored.is_symlink())
        self.assertEqual('executable', restored.read_text())
        self.assertEqual(0o755, restored.stat().st_mode & 0o777)


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake and Ninja required')
class CMakeCacheTests(RepositoryFixture):
    def setUp(self):
        super().setUp()
        version = subprocess.check_output(['cmake', '--version'], text=True).split()[2]
        if int(version.split('.')[0]) < 4:
            self.skipTest('CMake 4 required')
        self.write('engine/current.h', '#define CURRENT 1\n')
        self.write('external/CMakeLists.txt', '''cmake_minimum_required(VERSION 4.0)
project(cache_smoke C)
file(APPEND "${CMAKE_BINARY_DIR}/configured.txt" "configure\\n")
set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext")
add_executable(cache_smoke lz4/main.c)
install(TARGETS cache_smoke RUNTIME DESTINATION bin/${TARGET_PLATFORM})
install(FILES ../engine/current.h DESTINATION sdk/include/dmsdk COMPONENT defold_sdk_headers)
''')
        self.env = dict(os.environ, GITHUB_ACTIONS='true',
                        DEFOLD_EXT_CACHE_DIR=str(self.root / 'cache'),
                        GITHUB_OUTPUT=str(self.root / 'outputs'))
        for name in ('CC', 'CXX', 'CMAKE_TOOLCHAIN_FILE', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS',
                     'DEFOLD_EXT_HOST_CACHE', 'DEFOLD_EXT_TARGET_CACHE'):
            self.env.pop(name, None)
        self.enterContext(mock.patch.dict(os.environ, self.env, clear=True))
        self.enterContext(mock.patch.object(build, 'get_configured_platforms', return_value=[]))
        self.enterContext(mock.patch.object(build, 'get_platform_root', return_value=None))
        self.enterContext(mock.patch.object(build.sdk, 'get_sdk_info', side_effect=lambda folder, platform: {
            platform: {'version': '1', 'path': '/sdk'}, 'clang-version': '1'}))
        self.config = build.Configuration.__new__(build.Configuration)
        self.config.defold_root = str(self.root)
        self.config.dynamo_home = str(self.root / 'install')
        self.config.ext = str(self.root / 'install/ext')
        self.config.host = 'host'
        self.config.target_platform = 'target'
        self.config.build_options = []
        self.config.verbose = False
        self.config.build_tracker = mock.Mock()
        self.config._form_env = lambda: dict(os.environ)
        self.config._log = lambda message: None
        self.config.check_sdk = mock.Mock()

    def prepare(self, hit=False):
        (self.root / 'outputs').write_text('')
        ext_cache.prepare(self.config)
        outputs = dict(line.split('=', 1) for line in (self.root / 'outputs').read_text().splitlines())
        for role, _ in ext_cache.platforms(self.config):
            os.environ['DEFOLD_EXT_%s_CACHE' % role.upper()] = outputs[role + '-path']
            os.environ['DEFOLD_EXT_%s_CACHE_HIT' % role.upper()] = str(hit).lower()
        return outputs

    # Exercise real host/target builds, an incomplete download, cleanup, and hits without recompiling/reconfiguring.
    def test_ci_round_trip_reuses_prepared_configuration(self):
        outputs = self.prepare()
        self.assertNotEqual(outputs['host-key'], outputs['target-key'])
        partial = Path(outputs['host-path']) / 'incomplete'
        partial.parent.mkdir(parents=True)
        partial.write_text('failed download')
        ext_cache.build_dependencies(self.config)
        for role in ('host', 'target'):
            directory = self.root / 'external/build' / role
            self.assertEqual('configure\n', (directory / 'configured.txt').read_text())
            staged = Path(outputs[role + '-path'])
            self.assertFalse((staged / 'sdk').exists())
            self.assertFalse((staged / 'incomplete').exists())
        shutil.rmtree(self.root / 'external/build')
        shutil.rmtree(self.root / 'install')
        self.write('engine/current.h', '#define CURRENT 2\n')
        self.assertEqual(outputs, self.prepare(hit=True))
        ext_cache.build_dependencies(self.config)
        executable = 'cache_smoke.exe' if os.name == 'nt' else 'cache_smoke'
        for role in ('host', 'target'):
            directory = self.root / 'external/build' / role
            self.assertEqual('configure\n', (directory / 'configured.txt').read_text())
            self.assertFalse((directory / executable).exists())
            subprocess.check_call([str(Path(self.config.ext) / 'bin' / role / executable)])
        self.assertEqual('#define CURRENT 2\n',
                         (Path(self.config.ext) / 'sdk/include/dmsdk/current.h').read_text())

    # Compiler or SDK version, effective flags and runner image changes must select a fresh installation.
    def test_key_tracks_versioned_build_inputs(self):
        self.prepare()
        directory = self.root / 'external/build/host'
        def key(env=None, sdk_version='1'):
            return ext_cache.fingerprint(directory, 'host', 'host', ext_cache.source_identity(self.root),
                                         env or dict(os.environ), {'version': sdk_version})
        original = key()
        self.assertNotEqual(original, key(sdk_version='2'))
        self.assertNotEqual(original, key(dict(os.environ, ImageVersion='next')))
        self.assertEqual(key(dict(os.environ, GITHUB_WORKFLOW='Main')),
                         key(dict(os.environ, GITHUB_WORKFLOW='Nightly')))
        replies = directory / '.cmake/api/v1/reply'
        toolchains = next(replies.glob('toolchains-v1-*.json'))
        data = json.loads(toolchains.read_text())
        data['toolchains'][0]['compiler']['version'] += '.changed'
        toolchains.write_text(json.dumps(data))
        self.assertNotEqual(original, key())
        original = key()
        ninja = directory / 'build.ninja'
        ninja.write_text(ninja.read_text().replace('-O2', '-O0'))
        self.assertNotEqual(original, key())

    # Checking the target SDK must not split identical host cache keys between native and cross-build jobs.
    def test_host_key_is_shared_by_native_and_cross_builds(self):
        sdks = {platform: {platform: {'version': '1', 'path': '/sdk'}, 'clang-version': '1'}
                for platform in ('host', 'target')}
        build.sdk.get_sdk_info.side_effect = lambda folder, platform: sdks[platform]
        def check_sdk(**kwargs):
            with mock.patch.object(build.sdk.shutil, 'which', return_value='/sdk/bin/clang++'):
                build.sdk._get_clang_from_info(sdks[self.config.target_platform])
        self.config.check_sdk.side_effect = check_sdk
        cross = self.prepare()['host-key']
        self.assertNotIn('clang', sdks['host'])
        self.config.target_platform = 'host'
        native = self.prepare()['host-key']
        self.assertIn('clang', sdks['host'])
        self.assertEqual(cross, native)
        sdks['host']['host']['version'] = '2'
        self.assertNotEqual(native, self.prepare()['host-key'])

    # Build failures must leave no directory for actions/cache to publish.
    def test_failed_build_is_not_staged(self):
        self.source.write_text('#error intentional test failure\n')
        outputs = self.prepare()
        with self.assertRaises(build.run.ExecException):
            ext_cache.build_dependencies(self.config)
        self.assertFalse(Path(outputs['host-path']).exists())

    # An installation error must not publish even the files installed before the error.
    def test_failed_install_is_not_staged(self):
        cmake = self.root / 'external/CMakeLists.txt'
        cmake.write_text(cmake.read_text() + '\ninstall(CODE "message(FATAL_ERROR intentional)")\n')
        outputs = self.prepare()
        with self.assertRaises(build.run.ExecException):
            ext_cache.build_dependencies(self.config)
        self.assertFalse(Path(outputs['host-path']).exists())

    # Failure to compute an optional key must still allow a normal source build.
    def test_key_failure_falls_back_to_source_build(self):
        self.config.target_platform = self.config.host
        with mock.patch.object(ext_cache, 'source_identity', side_effect=OSError('unavailable')):
            ext_cache.prepare(self.config)
        self.assertFalse((self.root / 'outputs').exists())
        ext_cache.build_dependencies(self.config)
        self.assertTrue((self.root / 'external/build/host/install_manifest.txt').exists())

    # The build script must ignore CI cache settings even when invoked inside a GitHub Actions job.
    def test_build_script_ignores_ci_cache_settings(self):
        os.environ['DEFOLD_EXT_HOST_CACHE'] = str(self.root / 'unused-cache')
        os.environ['DEFOLD_EXT_HOST_CACHE_HIT'] = 'true'
        self.config.target_platform = self.config.host
        self.config.build_ext()
        self.assertFalse((self.root / 'unused-cache').exists())
        self.assertTrue((self.root / 'external/build/host/install_manifest.txt').exists())


class CITests(RepositoryFixture):
    # CI preparation keeps packaged SDK selection; prepared engine passes do no implicit cleanup.
    def test_ci_dependency_commands(self):
        options = SimpleNamespace(**dict.fromkeys((
            'verbose', 'archive', 'codesign', 'skip_docs', 'skip_builtins', 'skip_tests',
            'skip_build_tests', 'with_valgrind', 'with_asan', 'with_ubsan', 'with_tsan',
            'with_vanilla_lua', 'skip_install_ext'), False))
        with mock.patch.object(ci, 'call') as call:
            ci.prepare_engine('x86_64-win32')
            self.assertIn('distclean install_sdk install_ext_packages', call.call_args_list[-2].args[0])
            self.assertIn('ci/ext_cache.py prepare --platform=x86_64-win32', call.call_args.args[0])
            ci.prepare_engine('arm64-android')
            self.assertIn('distclean install_ext_packages', call.call_args_list[-2].args[0])
            self.assertIn('ci/ext_cache.py prepare --platform=arm64-android', call.call_args.args[0])
            ci.build_engine(None, 'x86_64-linux', options)
            self.assertIn('distclean install_ext build_engine', call.call_args.args[0])
            options.skip_install_ext = options.with_asan = True
            ci.build_engine(None, 'x86_64-linux', options)
            command = call.call_args.args[0]
            self.assertIn('scripts/build.py build_engine', command)
            self.assertNotIn(' clean ', command)
            self.assertNotIn('install_ext', command)
            self.assertNotIn('distclean', command)
            self.assertIn('--with-asan', command)

    # The shared CI entry point dispatches dependency builds and cleans explicitly before another sanitizer pass.
    def test_ci_build_and_clean_commands(self):
        argv = ['ci.py', 'build-ext', 'clean-engine', 'engine', '--platform=x86_64-linux',
                '--skip-install-ext', '--with-ubsan']
        with mock.patch.object(sys, 'argv', argv), \
             mock.patch.object(ci, 'get_branch', return_value='dev'), \
             mock.patch.object(ci, 'is_repo_private', return_value=False), \
             mock.patch.object(ci, 'call') as call:
            ci.main(argv[1:])
        commands = [args.args[0] for args in call.call_args_list]
        self.assertEqual(3, len(commands))
        self.assertIn('ci/ext_cache.py build --platform=x86_64-linux', commands[0])
        self.assertTrue(commands[1].endswith('scripts/build.py clean'))
        self.assertIn('scripts/build.py build_engine', commands[2])
        self.assertIn('--with-ubsan', commands[2])
        self.assertNotIn('distclean', ' '.join(commands))


if __name__ == '__main__':
    unittest.main()
