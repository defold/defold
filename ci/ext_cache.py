"""Keys and staged installations for the external dependency cache in CI.

Toolchains and SDKs are versioned inputs. GitHub Actions owns archive transport;
local builds retain their normal incremental CMake build directories.
"""

import hashlib
from argparse import ArgumentParser
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'scripts'))
import build


SCHEMA = 3
# Source dependencies built by external/CMakeLists.txt, including host tools.
SOURCE_PATHS = (
    'external/CMakeLists.txt', 'external/basisu', 'external/box2d',
    'external/box2d_v2', 'external/bullet3d', 'external/harfbuzz',
    'external/libunibreak', 'external/luajit', 'external/lz4', 'external/opus',
    'external/protobuf', 'external/sheenbidi', 'external/skribidi',
    'scripts/cmake/*.cmake', 'scripts/cmake/*.in', 'ci/ext_cache.py',
    ':(exclude,icase,glob)external/**/readme*', ':(exclude,glob)**/AGENTS.md',
)
# These compiler inputs are implicit, so Ninja's command lines do not contain them.
ENVIRONMENT_INPUTS = (
    'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
    'INCLUDE', 'LIB', 'CL', '_CL_', 'EMCC_CFLAGS', 'SOURCE_DATE_EPOCH',
)


def digest_json(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def sdk_identity(sdkfolder, platform):
    info = build.sdk.get_sdk_info(sdkfolder, platform)
    if not info:
        raise ValueError('Missing SDK information for %s' % platform)
    # SDK validation mutates this dictionary; select only versioned build inputs.
    identity = {name: info[name] for name in ('clang-version', 'ndk', 'api') if name in info}
    for name in (platform, 'xcode', 'msvc', 'emsdk'):
        if name in info:
            identity[name] = {field: info[name][field] for field in ('version', 'path') if field in info[name]}
    return identity


def source_identity(root):
    names = subprocess.check_output([
        'git', '-C', str(root), 'ls-files', '-z', '--cached', '--others',
        '--exclude-standard', '--', *SOURCE_PATHS])
    files = {}
    for name in sorted(set(names.split(b'\0')) - {b''}):
        path = Path(root) / os.fsdecode(name)
        if path.is_file():
            # Use one content representation regardless of timestamps or index state.
            with path.open('rb') as source:
                files[os.fsdecode(name)] = hashlib.file_digest(source, 'sha256').hexdigest()
    return digest_json(files)


def prepare_query(build_dir):
    query = Path(build_dir) / '.cmake/api/v1/query/client-defold-ext-cache'
    query.mkdir(parents=True, exist_ok=True)
    (query / 'toolchains-v1').touch()


def read_cmake_cache(build_dir):
    result = {}
    for line in (Path(build_dir) / 'CMakeCache.txt').read_text().splitlines():
        if not line or line.startswith(('#', '//')):
            continue
        name, separator, value = line.partition('=')
        if separator:
            result[name.rsplit(':', 1)[0]] = value
    return result


def fingerprint(build_dir, host, target, sources, env, sdk_info):
    build_dir = Path(build_dir).resolve()
    directory = build_dir / '.cmake/api/v1/reply'
    index = json.loads(sorted(directory.glob('index-*.json'))[-1].read_text())
    reply = index['reply']['client-defold-ext-cache']['toolchains-v1']['jsonFile']
    toolchains = json.loads((directory / reply).read_text())['toolchains']
    cache = read_cmake_cache(build_dir)
    # Unlike raw build.ninja, these commands are stable across cold/warm configure.
    commands = subprocess.check_output([
        cache['CMAKE_MAKE_PROGRAM'], '-C', str(build_dir), '-t', 'commands', 'all'], env=env)
    inputs = {
        'sources': sources,
        'sdk': sdk_info,
        'image': {name: env.get(name, '') for name in ('ImageOS', 'ImageVersion')},
        'cmake': index['cmake']['version']['string'],
        'compilers': {chain['language']: {name: chain['compiler'].get(name, '')
                     for name in ('id', 'version', 'path', 'target')} for chain in toolchains},
        'configuration': {name: cache.get(name, '') for name in (
            'CMAKE_BUILD_TYPE', 'CMAKE_INSTALL_PREFIX', 'DEFOLD_SDK_ROOT', 'DEFOLD_BUILD_HOME')},
        'environment': {name: env.get(name, '') for name in ENVIRONMENT_INPUTS},
        # Keep paths: debug information and installed package metadata can embed them.
        'commands': hashlib.sha256(commands).hexdigest(),
    }
    return 'ext-v%d-%s-%s-%s' % (SCHEMA, host, target, digest_json(inputs))


def stage_install(install_manifest, prefix, destination):
    """Publish only a complete installation for actions/cache to save."""
    prefix, destination = Path(prefix).resolve(), Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
        staging = Path(temporary) / 'files'
        for line in Path(install_manifest).read_text().splitlines():
            path = Path(line)
            # Resolve directory aliases, retaining installed names such as protoc.
            path = path.parent.resolve() / path.name
            name = path.relative_to(prefix)
            # Engine headers must always come from the current checkout.
            if name.parts[:3] == ('sdk', 'include', 'dmsdk'):
                continue
            target = staging / name
            target.parent.mkdir(parents=True, exist_ok=True)
            # Dereference file aliases so Windows restore needs no symlink privileges.
            shutil.copy2(path, target)
        if not staging.is_dir():
            raise ValueError('Empty external installation manifest')
        os.replace(staging, destination)


def platforms(configuration):
    yield 'host', configuration.host
    if configuration.target_platform != configuration.host:
        yield 'target', configuration.target_platform


def prepare(configuration):
    directory = os.environ.get('DEFOLD_EXT_CACHE_DIR')
    if not directory or os.environ.get('GITHUB_ACTIONS') != 'true':
        return
    if build.get_configured_platforms() or build.get_platform_root(configuration.target_platform):
        configuration._log('External cache disabled for private platform configuration')
        return
    configuration.check_sdk(require_protoc=False)
    try:
        sources = source_identity(configuration.defold_root)
        sdkfolder = str(Path(configuration.ext) / 'SDKs')
        host_sdk = sdk_identity(sdkfolder, configuration.host)
        for role, platform in platforms(configuration):
            directory_for_platform = Path(configuration.defold_root) / 'external/build' / platform
            prepare_query(directory_for_platform)
            build_dir, _ = configuration._configure_ext_platform(platform)
            # LuaJIT's cross-build also uses the native host toolchain.
            sdk_info = {'host': host_sdk, 'target': sdk_identity(sdkfolder, platform)}
            key = fingerprint(build_dir, configuration.host, platform, sources,
                              configuration._form_env(), sdk_info)
            path = (Path(directory) / key).resolve()
            with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
                output.write('%s-key=%s\n%s-path=%s\n' % (role, key, role, path))
    except (OSError, ValueError, KeyError, IndexError, subprocess.SubprocessError) as error:
        configuration._log('External cache unavailable; building from source: %s' % error)


def build_dependencies(configuration):
    configuration.check_sdk(require_protoc=False)
    for role, platform in platforms(configuration):
        cache_path = None
        if os.environ.get('GITHUB_ACTIONS') == 'true':
            cache_path = os.environ.get('DEFOLD_EXT_%s_CACHE' % role.upper())
        build_dir = Path(configuration.defold_root) / 'external/build' / platform
        if cache_path:
            if os.environ.get('DEFOLD_EXT_%s_CACHE_HIT' % role.upper()) == 'true':
                try:
                    shutil.copytree(cache_path, configuration.ext, dirs_exist_ok=True)
                except OSError as error:
                    configuration._log('Could not restore external cache; building from source: %s' % error)
                else:
                    configuration._log('External cache hit: %s' % platform)
                    build_type = configuration._find_cmake_build_type(configuration.build_options)
                    build.run.env_command(configuration._form_env(), [
                        'cmake', '--install', str(build_dir), '--config', build_type,
                        '--component', 'defold_sdk_headers'], cwd=configuration.defold_root)
                    continue
            # Discard an incomplete download before staging a successful build.
            configuration._remove_tree(cache_path)
        configuration._build_ext_platform(platform, configure=not cache_path)
        if cache_path:
            try:
                stage_install(build_dir / 'install_manifest.txt', configuration.ext, cache_path)
            except (OSError, ValueError) as error:
                configuration._log('Could not stage external cache: %s' % error)


if __name__ == '__main__':
    parser = ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('prepare', 'build'))
    parser.add_argument('--platform', required=True, choices=build.get_target_platforms())
    args = parser.parse_args()
    build.build_private.set_target_platform(args.platform)
    configuration = build.Configuration(dynamo_home=os.environ['DYNAMO_HOME'], target_platform=args.platform)
    if args.command == 'prepare':
        prepare(configuration)
    else:
        build_dependencies(configuration)
    configuration.build_tracker.print_summary()
    configuration.build_tracker.save_times()
