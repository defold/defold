#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

"""Prepare an ARM64 APK for HWASan on Android 14 or newer."""

import argparse
import copy
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'build_tools'))
import sdk


ARM64_LIB_DIR = 'lib/arm64-v8a/'
HWASAN_RUNTIME = 'libclang_rt.hwasan-aarch64-android.so'
ANDROID_NAMESPACE = r'(?:android|http://schemas\.android\.com/apk/res/android):'


def run(*command, check=True):
    return subprocess.run([str(arg) for arg in command], check=check,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True).stdout


def manifest_boolean(manifest, name):
    values = re.findall(r'A: ' + ANDROID_NAMESPACE + name + r'(?:\([^)]*\))?=([^\n]+)', manifest)
    if not values:
        return None
    return any(value.strip() in ('true', '0xffffffff', '(type 0x12)0xffffffff') for value in values)


def validate_manifest(manifest):
    if manifest_boolean(manifest, 'debuggable') is not True:
        raise ValueError('Set android.debuggable=1 in game.project and bundle the APK again.')
    if manifest_boolean(manifest, 'extractNativeLibs') is not True:
        raise ValueError('Set android.extract_native_libs=1 in game.project and bundle the APK again so Android extracts wrap.sh.')
    if manifest_boolean(manifest, 'useAppZygote') is True:
        raise ValueError('Remove android:useAppZygote from the Android manifest for HWASan testing.')


def engine_library_name(manifest):
    for element in re.split(r'\n\s*E: ', manifest):
        if not element.startswith('meta-data '):
            continue
        if re.search(ANDROID_NAMESPACE + r'name\([^)]*\)="android\.app\.lib_name"', element):
            match = re.search(ANDROID_NAMESPACE + r'value\([^)]*\)="([A-Za-z0-9_.-]+)"', element)
            if match:
                return ARM64_LIB_DIR + 'lib' + match.group(1) + '.so'
    raise ValueError('Could not find android.app.lib_name in the APK manifest.')


def prepare_apk(source, target, manifest, readelf, libcxx, work_dir, engine=None):
    validate_manifest(manifest)
    replacements = {}
    with zipfile.ZipFile(source) as apk:
        names = apk.namelist()
        if len(names) != len(set(names)):
            raise ValueError('The APK contains duplicate ZIP entries.')
        libraries = [name for name in names if name.startswith('lib/') and name.endswith('.so')]
        if not libraries or any(not name.startswith(ARM64_LIB_DIR) for name in libraries):
            raise ValueError('Bundle an ARM64-only APK (--architectures=arm64-android).')
        if engine:
            name = engine_library_name(manifest)
            if name not in libraries:
                raise ValueError('Engine library is missing from the APK: ' + name)
            replacements[name] = engine.read_bytes()

        dependencies = set()
        for index, name in enumerate(libraries):
            if name.startswith(ARM64_LIB_DIR + 'libclang_rt.'):
                raise ValueError('Start with an APK without bundled sanitizer runtimes; Android supplies the HWASan runtime.')
            library = work_dir / ('library-%d.so' % index)
            library.write_bytes(replacements.get(name, apk.read(name)))
            elf = run(readelf, '--file-header', '--dynamic', library)
            if not re.search(r'Machine:\s+AArch64\b', elf):
                raise ValueError('Expected an AArch64 ELF library: ' + name)
            needed = set(re.findall(r'\(NEEDED\).*?\[([^]]+)\]', elf))
            if any('libclang_rt.asan' in dependency for dependency in needed):
                raise ValueError('The APK contains an ASan library; rebuild it with HWASan: ' + name)
            dependencies.update(needed)

        if HWASAN_RUNTIME not in dependencies:
            raise ValueError('No shared HWASan runtime dependency found. Rebuild the native code with --with-hwasan before repacking.')
        if 'libc++_shared.so' in dependencies and ARM64_LIB_DIR + 'libc++_shared.so' not in names:
            replacements[ARM64_LIB_DIR + 'libc++_shared.so'] = libcxx.read_bytes()
        replacements[ARM64_LIB_DIR + 'wrap.sh'] = Path(__file__).with_name('android-wrap-hwasan.sh').read_bytes()

        with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as output:
            for source_info in apk.infolist():
                info = copy.copy(source_info)
                if re.fullmatch(r'META-INF/(MANIFEST\.MF|[^/]+\.(SF|RSA|DSA|EC))', info.filename, re.IGNORECASE) or info.filename in replacements:
                    continue
                if info.filename == 'resources.arsc':
                    info.compress_type = zipfile.ZIP_STORED
                output.writestr(info, apk.read(info.filename))
            for name, data in replacements.items():
                info = zipfile.ZipInfo(name)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100755 << 16
                output.writestr(info, data)


def reinstall_apk(adb, aapt2, target):
    # Match the existing repackers: this explicit option removes app data.
    device_api = run(adb, 'shell', 'getprop', 'ro.build.version.sdk').strip()
    if not device_api.isdigit() or int(device_api) < 34:
        raise ValueError('--reinstall requires a device running Android 14 or newer.')
    if 'arm64-v8a' not in run(adb, 'shell', 'getprop', 'ro.product.cpu.abilist').strip().split(','):
        raise ValueError('--reinstall requires a device supporting arm64-v8a.')
    match = re.search(r"^package: name='([^']+)'", run(aapt2, 'dump', 'badging', target), re.MULTILINE)
    if not match:
        raise ValueError('Could not determine the APK package name.')
    package = match.group(1)
    installed = run(adb, 'shell', 'pm', 'path', package, check=False)
    if installed.startswith('package:'):
        run(adb, 'uninstall', package)
    run(adb, 'install', target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='ARM64 debuggable APK')
    parser.add_argument('keystore', type=Path, nargs='?', help='keystore (default: temporary debug key)')
    parser.add_argument('keystore_pass', type=Path, nargs='?', help='file containing the keystore password')
    parser.add_argument('--engine', type=Path, help='replace the APK engine with this HWASan libdmengine.so')
    parser.add_argument('--output', type=Path, help='output APK (default: <source>.hwasan.apk)')
    parser.add_argument('--ndk', type=Path, default=os.environ.get('ANDROID_NDK_ROOT'), help='NDK used to build the engine')
    parser.add_argument('--sdk', type=Path, default=os.environ.get('ANDROID_SDK_ROOT') or os.environ.get('ANDROID_HOME'), help='Android SDK root')
    parser.add_argument('--reinstall', action='store_true', help='uninstall the existing app (deletes its data) and install this APK')
    args = parser.parse_args()
    if bool(args.keystore) != bool(args.keystore_pass):
        parser.error('Supply both the keystore and its password file, or neither.')
    source = args.source.resolve()
    target = (args.output or source.with_suffix('.hwasan.apk')).resolve()
    if source == target:
        parser.error('The output must be different from the source APK.')

    dynamo_home = Path(os.environ.get('DYNAMO_HOME', Path(__file__).resolve().parents[2] / 'tmp/dynamo_home'))
    ndk = args.ndk or dynamo_home / 'ext/SDKs' / ('android-ndk-r' + sdk.ANDROID_NDK_VERSION)
    android_sdk = args.sdk or dynamo_home / 'ext/SDKs/android-sdk'
    hosts = {'Darwin': 'darwin-x86_64', 'Linux': 'linux-x86_64'}
    if platform.system() not in hosts:
        parser.error('This repacker supports macOS and Linux hosts.')
    toolchain = ndk / 'toolchains/llvm/prebuilt' / hosts[platform.system()]
    readelf = toolchain / 'bin/llvm-readelf'
    libcxx = toolchain / 'sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'
    build_tools = android_sdk / 'build-tools' / sdk.ANDROID_BUILD_TOOLS_VERSION
    aapt2 = build_tools / 'aapt2'
    zipalign = build_tools / 'zipalign'
    apksigner = build_tools / 'apksigner'
    for tool in (readelf, aapt2, zipalign, apksigner):
        if not os.access(tool, os.X_OK):
            parser.error('Tool is missing or not executable: ' + str(tool))

    manifest = run(aapt2, 'dump', 'xmltree', '--file', 'AndroidManifest.xml', source)
    with tempfile.TemporaryDirectory(prefix='defold-hwasan-') as temporary:
        work_dir = Path(temporary)
        repacked = work_dir / 'repacked.apk'
        aligned = work_dir / 'aligned.apk'
        signed = work_dir / 'signed.apk'
        prepare_apk(source, repacked, manifest, readelf, libcxx, work_dir, args.engine)
        keystore = args.keystore
        keystore_pass = args.keystore_pass
        if not keystore:
            keystore = work_dir / 'debug.keystore'
            keystore_pass = work_dir / 'debug.pass.txt'
            run('keytool', '-genkeypair', '-noprompt', '-keystore', keystore,
                '-storepass', 'android', '-keypass', 'android', '-alias', 'androiddebugkey',
                '-keyalg', 'RSA', '-validity', '14000', '-dname', 'CN=Android Debug,O=Android,C=US')
            keystore_pass.write_text('android')
        run(zipalign, '-P', '16', '-f', '4', repacked, aligned)
        run(apksigner, 'sign', '--in', aligned, '--out', signed,
            '--ks', keystore, '--ks-pass', 'file:' + str(keystore_pass))
        run(apksigner, 'verify', signed)
        shutil.copyfile(signed, target)
    print('Wrote ' + str(target))
    if args.reinstall:
        reinstall_apk(android_sdk / 'platform-tools/adb', aapt2, target)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        print('Error: ' + str(error), file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr, file=sys.stderr)
        sys.exit(1)
