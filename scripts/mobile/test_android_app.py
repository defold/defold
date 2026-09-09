#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

"""Build and run the Android APK integration test under ASAN on an emulator."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import struct
import subprocess
import sys
import threading
import time
import uuid
import wave
import zipfile
import zlib


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = 'com.defold.androidasan'
ACTIVITY = PACKAGE + '/com.dynamo.android.DefoldActivity'
MARKER = 'ANDROID_APP_TEST '
PLATFORMS = {
    'x86_64-android': ('x86_64', 'x86_64', 'X86-64'),
}


def run(command, *, env=None, log=None, timeout=60, check=True):
    command = [str(arg) for arg in command]
    print('+ ' + shlex.join(command), flush=True)
    if log:
        with log.open('ab') as output:
            try:
                return subprocess.run(command, env=env, stdout=output,
                                      stderr=subprocess.STDOUT, timeout=timeout, check=check)
            except subprocess.SubprocessError:
                print('\n'.join(log.read_text(errors='replace').splitlines()[-40:]), file=sys.stderr)
                raise
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=timeout)
    if check and result.returncode:
        print((result.stdout + result.stderr).decode(errors='replace'), file=sys.stderr)
        result.check_returncode()
    return result


def build_apk(args):
    abi, asan_arch, machine = PLATFORMS[args.platform]
    host = {'Darwin': 'darwin-x86_64', 'Linux': 'linux-x86_64'}[platform.system()]
    toolchain = args.ndk / 'toolchains/llvm/prebuilt' / host
    readelf = toolchain / 'bin/llvm-readelf'
    engine = args.engine or args.dynamo_home / 'bin' / args.platform / 'libdmengine.so'
    elf = run([readelf, '--file-header', '--dynamic', engine]).stdout.decode()
    runtime_name = 'libclang_rt.asan-%s-android.so' % asan_arch
    if machine not in elf or runtime_name not in elf:
        raise RuntimeError('The engine must be built for %s with --with-asan: %s' % (args.platform, engine))

    run_id = uuid.uuid4().hex
    work = args.output / 'work' / run_id
    project = work / 'project'
    shutil.copytree(ROOT / 'engine/engine/src/test/android_app', project)
    with (project / 'game.project').open('a') as output:
        output.write('\n[android_test]\nrun_id = %s\n' % run_id)
    # Generate a small PCM fixture instead of storing another binary asset.
    with wave.open(str(project / 'test.wav'), 'wb') as sound:
        sound.setparams((1, 2, 44100, 0, 'NONE', 'not compressed'))
        sound.writeframes(b''.join(struct.pack('<h', int(1000 * math.sin(i * 2 * math.pi * 440 / 44100)))
                                   for i in range(4410)))

    native = work / 'native' / args.platform
    # Retain the exact unstripped engine, independently of Bob's APK packaging.
    shutil.copy2(engine, args.output / 'libdmengine.so')
    log = args.output / 'build.log'
    env = os.environ.copy()
    env['DYNAMO_HOME'] = str(args.dynamo_home)
    env['DM_BOB_ROOTFOLDER'] = str(work / 'bob')
    # Use the Java app code from this checkout, just like the native GLFW code.
    # bob-light.jar may contain classes.dex from a previously packaged GLFW.
    android_jar = args.sdk / 'platforms' / ('android-%d' % args.api) / 'android.jar'
    classes = work / 'classes'
    classes.mkdir()
    java_sources = []
    for folder in ('external/glfw/java', 'engine/sound/src/java',
                   'engine/gamesys/src/java', 'engine/dlib/src/java'):
        java_sources.extend(sorted((ROOT / folder).rglob('*.java')))
    run(['javac', '--release', '8', '-g', '-classpath', android_jar, '-d', classes, *java_sources], log=log)
    dex = work / 'dex'
    dex.mkdir()
    run([args.build_tools / 'd8', '--lib', android_jar, '--min-api', '27',
         '--output', dex, *sorted(classes.rglob('*.class'))], log=log, timeout=120)
    # bob-light leaves these Android resources out of the jar. Stage them in
    # this invocation's Bob cache so bundling uses the matching SDK and Java.
    bob_lib = work / 'bob/lib'
    bob_lib.mkdir(parents=True)
    if platform.system() == 'Linux':
        # AndroidTools sets LD_LIBRARY_PATH to this directory. Full Bob
        # extracts libc++ here, but bob-light leaves that resource out.
        linux_lib = work / 'bob/x86_64-linux/lib'
        linux_lib.mkdir(parents=True)
        shutil.copy2(ROOT / 'com.dynamo.cr/com.dynamo.cr.bob/lib/x86_64-linux/libc++.so', linux_lib / 'libc++.so')
    shutil.copy2(android_jar, bob_lib / 'android.jar')
    shutil.copy2(dex / 'classes.dex', bob_lib / 'classes.dex')
    (bob_lib / 'vkquality').mkdir()
    shutil.copy2(args.dynamo_home / 'ext/share/vkquality/assets/vkqualitydata.vkq', bob_lib / 'vkquality/vkqualitydata.vkq')
    bob_native = work / 'bob/libexec' / args.platform
    bob_native.mkdir(parents=True)
    shutil.copy2(args.dynamo_home / 'ext/lib' / args.platform / 'libvkquality.so', bob_native / 'libvkquality.so')
    shutil.copy2(args.dynamo_home / 'ext/share/java/bundletool-all.jar', work / 'bob/bundletool-all.jar')
    # Bundling also uses Jackson and other jars omitted from bob-light.
    classpath = os.pathsep.join(str(path) for path in
                               (args.bob, ROOT / 'engine/engine/content', args.dynamo_home / 'share/java/shaderc.jar',
                                ROOT / 'com.dynamo.cr/com.dynamo.cr.bob/lib/aws/*'))
    bob = ['java', '-cp', classpath, 'com.dynamo.bob.Bob', '--root', project, '--platform', args.platform,
           '--architectures', args.platform, '--variant', 'debug', '--archive',
           '--binary-output', native.parent, '--bundle-output', work / 'bundle',
           '--bundle-format', 'apk']
    run([*bob, 'build'], env=env, log=log, timeout=600)
    # Bob cleans native engine binaries during build. Supply the ASAN engine
    # afterwards so bundle uses it instead of downloading a release engine.
    native.mkdir(parents=True, exist_ok=True)
    shutil.copy2(engine, native / 'libdmengine.so')
    run([*bob, 'bundle'], env=env, log=log, timeout=600)
    apks = list((work / 'bundle').rglob('*.apk'))
    if len(apks) != 1:
        raise RuntimeError('Expected one test APK, found: %s' % apks)
    unsigned = work / 'test.apk'
    shutil.copy2(apks[0], unsigned)

    env['ANDROID_NDK_ROOT'] = str(args.ndk)
    env['ANDROID_SDK_ROOT'] = str(args.sdk)
    env['ANDROID_BUILD_TOOLS_VERSION'] = args.build_tools.name
    run(['bash', ROOT / 'scripts/mobile/android-repack-asan.sh', unsigned], env=env, log=log, timeout=120)
    apk = args.output / 'test-asan.apk'
    shutil.copy2(work / 'test.repack.apk', apk)
    with zipfile.ZipFile(apk) as package:
        if 'lib/%s/%s' % (abi, runtime_name) not in package.namelist() or 'lib/%s/wrap.sh' % abi not in package.namelist():
            raise RuntimeError('The test APK is missing its ASAN runtime or wrap.sh')
        packaged_engine = package.read('lib/%s/libAndroidASANTest.so' % abi)
        if hashlib.sha256(packaged_engine).digest() != hashlib.sha256(engine.read_bytes()).digest():
            raise RuntimeError('The APK does not contain the supplied ASAN engine')
    metadata = {'run_id': run_id, 'platform': args.platform, 'apk': str(apk)}
    (args.output / 'test.json').write_text(json.dumps(metadata, indent=2) + '\n')
    return metadata


class Emulator:
    def __init__(self, args):
        self.command = [str(args.sdk / 'platform-tools/adb'), '-s', args.device]
        self.output = args.output

    def adb(self, *command, check=True, timeout=60):
        return run([*self.command, *command], check=check, timeout=timeout)

    def shell(self, *command, check=True):
        return self.adb('shell', shlex.join(str(arg) for arg in command), check=check).stdout.decode().strip()

    def key(self, key):
        # Defold polls keys per frame. Hold test controls across a frame even
        # when the emulator is slow or the app is restoring its surface.
        options = ['--duration', '200'] if key in ('F1', 'F2', 'F3') else []
        self.shell('input', 'keyevent', *options, 'KEYCODE_' + key)

    def launch(self):
        self.shell('am', 'start', '-W', '-n', ACTIVITY)

    def screenshot(self, name):
        # Raw screencap avoids a Pillow dependency. The header is 12 or 16
        # bytes depending on Android's color-space metadata; pixels are RGBA.
        data = self.adb('exec-out', 'screencap').stdout
        width, height, pixel_format = struct.unpack_from('<III', data)
        header_size = len(data) - width * height * 4
        if pixel_format != 1 or header_size not in (12, 16) or not (0 < width <= 8192 and 0 < height <= 8192):
            raise RuntimeError('Unexpected Android screenshot format')
        pixels = data[header_size:]
        stride = width * 4
        raw = b''.join(b'\0' + pixels[y * stride:(y + 1) * stride] for y in range(height))

        def chunk(kind, value):
            return struct.pack('>I', len(value)) + kind + value + struct.pack('>I', zlib.crc32(kind + value))

        png = b'\x89PNG\r\n\x1a\n'
        png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
        png += chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
        (self.output / (name + '.png')).write_bytes(png)
        offset = ((height // 2) * width + width // 2) * 4
        return tuple(pixels[offset:offset + 3]), width, height


class AppLog:
    def __init__(self, emulator, run_id):
        self.events = []
        self.error = None
        self.exit_code = None
        self.condition = threading.Condition()
        self.run_id = run_id
        self.process = subprocess.Popen([*emulator.command, 'logcat', '-v', 'threadtime', '-T', '1'],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
        self.path = emulator.output / 'logcat.log'
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.reader.start()

    def read(self):
        with self.path.open('w') as output:
            for line in self.process.stdout:
                output.write(line)
                output.flush()
                with self.condition:
                    if re.search(r'(ERROR|SUMMARY): AddressSanitizer|AddressSanitizer:DEADLYSIGNAL|AddressSanitizer CHECK failed|wrap.sh terminated by signal|ERROR:SCRIPT:', line):
                        self.error = line.strip()
                    exit_status = re.search(r'\bDEFOLD_ASAN_EXIT (\d+)', line)
                    if exit_status:
                        self.exit_code = int(exit_status[1])
                        if self.exit_code:
                            self.error = 'The ASAN app exited with code %d' % self.exit_code
                    if MARKER in line:
                        try:
                            event = json.loads(line.split(MARKER, 1)[1])
                        except ValueError:
                            self.error = 'Malformed app test event: ' + line.strip()
                            event = {}
                        if event.get('run_id') == self.run_id:
                            print('app: ' + json.dumps(event), flush=True)
                            if event['event'] == 'failure':
                                self.error = event.get('message', 'Lua test failure')
                            if event['event'] == 'ready' and any(e['event'] == 'ready' for e in self.events):
                                self.error = 'The app restarted during the test'
                            self.events.append(event)
                    self.condition.notify_all()

    def check(self):
        if self.error:
            raise RuntimeError(self.error)
        if self.process.poll() is not None:
            raise RuntimeError('logcat stopped before the test completed')

    def wait(self, name, *, after=0, predicate=lambda event: True, timeout=40):
        print('Waiting for app event: ' + name, flush=True)
        deadline = time.monotonic() + timeout
        with self.condition:
            while True:
                self.check()
                for event in self.events[after:]:
                    if event['event'] == name and predicate(event):
                        return event
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise RuntimeError('Timed out waiting for app event: ' + name)
                self.condition.wait(min(remaining, 1))

    def close(self):
        self.process.terminate()
        self.process.wait(timeout=10)
        self.reader.join(timeout=10)

    def wait_for_exit(self):
        deadline = time.monotonic() + 20
        with self.condition:
            while self.exit_code is None:
                self.check()
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise RuntimeError('The ASAN wrapper did not report the app exit status')
                self.condition.wait(min(remaining, 1))
            self.check()


class Gamepad:
    # Linux input codes, as consumed by Android's uinput test tool. Register a
    # real virtual device so InputDevice discovery and disconnection both run.
    AXES = (0, 1, 2, 5, 10, 9, 16, 17)  # X, Y, Z, RZ, BRAKE, GAS, HAT0X, HAT0Y

    def __init__(self, emulator, number):
        self.log = (emulator.output / ('gamepad-%d.log' % number)).open('wb')
        self.process = subprocess.Popen([*emulator.command, 'shell', 'uinput', '-'], stdin=subprocess.PIPE,
                                        stdout=self.log, stderr=subprocess.STDOUT, text=True)
        abs_info = []
        for axis in self.AXES:
            low, high = (-32767, 32767) if axis in (0, 1, 2, 5) else ((0, 255) if axis in (9, 10) else (-1, 1))
            abs_info.append({'code': axis, 'info': {'value': 0, 'minimum': low, 'maximum': high,
                                                   'fuzz': 0, 'flat': 0, 'resolution': 0}})
        self.send({'id': 1, 'command': 'register', 'name': 'Defold ASAN Gamepad',
                   'vid': 0x1209, 'pid': 1, 'bus': 'usb',
                   'configuration': [{'type': 100, 'data': [1, 3]},
                                     {'type': 101, 'data': [304, 305, 307, 308, 310, 311, 315]},
                                     {'type': 103, 'data': list(self.AXES)}], 'abs_info': abs_info})

    def send(self, command):
        if self.process.poll() is not None:
            raise RuntimeError('uinput stopped; see the gamepad log')
        self.process.stdin.write(json.dumps(command) + '\n')
        self.process.stdin.flush()

    def input(self, button, axes):
        events = [1, 304, button]
        for axis, value in zip(self.AXES, axes):
            events.extend([3, axis, value])
        events.extend([0, 0, 0])  # EV_SYN / SYN_REPORT
        self.send({'id': 1, 'command': 'inject', 'events': events})

    def close(self):
        if self.process.stdin and not self.process.stdin.closed:
            # EOF unregisters the device; wait for removal before reconnecting.
            self.process.stdin.close()
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=10)
        self.log.close()


def run_app(args, metadata):
    emulator = Emulator(args)
    if emulator.shell('getprop', 'ro.kernel.qemu') != '1':
        raise RuntimeError('This test requires a dedicated Android emulator')
    abi = PLATFORMS[metadata['platform']][0]
    if abi not in emulator.shell('getprop', 'ro.product.cpu.abilist').split(','):
        raise RuntimeError('The emulator does not support ' + abi)
    if int(emulator.shell('getprop', 'ro.build.version.sdk')) < 35:
        raise RuntimeError('The APK integration test requires Android API 35 or newer')
    emulator.shell('command', '-v', 'uinput')
    emulator.adb('uninstall', PACKAGE, check=False)
    emulator.adb('install', metadata['apk'], timeout=120)
    old_rotation = emulator.shell('wm', 'user-rotation').split()
    old_fixed = emulator.shell('wm', 'fixed-to-user-rotation')
    old_ime = emulator.shell('settings', 'get', 'secure', 'show_ime_with_hard_keyboard')
    app = AppLog(emulator, metadata['run_id'])
    gamepad = None
    passed = False
    try:
        emulator.key('WAKEUP')
        emulator.shell('wm', 'dismiss-keyguard')
        emulator.shell('wm', 'fixed-to-user-rotation', 'enabled')
        emulator.shell('wm', 'user-rotation', 'lock', '0')
        emulator.shell('settings', 'put', 'secure', 'show_ime_with_hard_keyboard', '1')
        emulator.launch()
        app.wait('ready', timeout=120)
        pid = emulator.shell('pidof', PACKAGE)
        if not pid:
            raise RuntimeError('The test app exited during startup')
        maps = emulator.shell('run-as', PACKAGE, 'cat', '/proc/%s/maps' % pid)
        (args.output / 'maps.txt').write_text(maps)
        if 'libclang_rt.asan-' not in maps:
            raise RuntimeError('The app did not load the ASAN runtime')

        def probe(name, landscape=None):
            cursor = len(app.events)
            emulator.key('F1')
            frame = app.wait('frame', after=cursor)
            expected = (frame['r'] * 255, frame['g'] * 255, frame['b'] * 255)
            deadline = time.monotonic() + 10
            while True:
                app.check()
                rgb, width, height = emulator.screenshot(name)
                if all(abs(actual - target) <= 12 for actual, target in zip(rgb, expected)):
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError('Rendering did not recover: %s, got %s, expected %s' % (name, rgb, expected))
                time.sleep(0.2)
            if landscape is not None and (frame['width'] > frame['height']) != landscape:
                raise RuntimeError('The app did not adopt the requested orientation')
            cursor = len(app.events)
            emulator.shell('input', 'swipe', width // 2, height // 2, width // 2, height // 2, 100)
            app.wait('touch', after=cursor)
            if emulator.shell('pidof', PACKAGE) != pid:
                raise RuntimeError('The app process changed during a lifecycle transition')

        probe('startup', landscape=False)
        for cycle in range(args.cycles):
            cursor = len(app.events)
            gamepad = Gamepad(emulator, cycle)
            connection = app.wait('connected', after=cursor)
            app.wait('gamepad', after=cursor, predicate=lambda e: e['button'] == 0 and all(abs(v) < 0.05 for v in e['axes']))
            if not connection.get('guid') or len(connection['guid']) != 32:
                raise RuntimeError('Missing gamepad identification')
            cursor = len(app.events)
            gamepad.input(1, [32767, -32767, -32767, 32767, 255, 255, 1, -1])
            app.wait('button', after=cursor, predicate=lambda e: e.get('pressed') and e['value'] > 0.9)
            expected_axes = [1, -1, -1, 1, 1, 1, 1, -1]
            app.wait('gamepad', after=cursor, predicate=lambda e: e['button'] == 1 and
                     e['hat'] == 3 and  # GLFW_HAT_UP | GLFW_HAT_RIGHT
                     all(abs(actual - target) < 0.05 for actual, target in zip(e['axes'], expected_axes)))
            cursor = len(app.events)
            gamepad.input(0, [0] * 8)
            app.wait('button', after=cursor, predicate=lambda e: e.get('released'))
            app.wait('gamepad', after=cursor, predicate=lambda e: e['button'] == 0 and e['hat'] == 0 and
                     all(abs(v) < 0.05 for v in e['axes']))

            if cycle % 2 == 0:
                cursor = len(app.events)
                gamepad.input(1, [0] * 8)
                app.wait('button', after=cursor, predicate=lambda e: e.get('pressed'))

            # Disconnect while backgrounded on alternating cycles. This also
            # checks that a held button is cleared on the next connection.
            cursor = len(app.events)
            emulator.key('HOME')
            app.wait('focus_lost', after=cursor)
            if cycle % 2 == 0:
                gamepad.close()
                gamepad = None
            time.sleep(0.5)
            emulator.launch()
            app.wait('focus_gained', after=cursor)
            if gamepad is not None:
                gamepad.close()
                gamepad = None
            app.wait('disconnected', after=cursor)
            probe('resume-%d' % cycle)

            cursor = len(app.events)
            rotation = 1 if cycle % 2 == 0 else 0
            emulator.shell('wm', 'user-rotation', 'lock', str(rotation))
            app.wait('resize', after=cursor, predicate=lambda e: (e['width'] > e['height']) == (rotation == 1))
            probe('rotation-%d' % cycle, landscape=(rotation == 1))

        cursor = len(app.events)
        emulator.key('F2')
        app.wait('keyboard', after=cursor)
        def wait_keyboard(visible):
            deadline = time.monotonic() + 15
            while True:
                app.check()
                state = emulator.shell('dumpsys', 'input_method')
                # InputMethodService can retain mIsInputViewShown after its
                # window is hidden. Use InputMethodManager's active state.
                shown = re.search(r'^\s*mInputShown=(true|false)\s*$', state, re.MULTILINE)
                if not shown:
                    raise RuntimeError('Android did not report the active IME visibility')
                if (shown[1] == 'true') == visible:
                    return
                if time.monotonic() >= deadline:
                    raise RuntimeError('The Android soft keyboard did not become ' + ('visible' if visible else 'hidden'))
                time.sleep(0.2)

        wait_keyboard(True)
        emulator.shell('input', 'text', 'asan')
        app.wait('text', after=cursor, predicate=lambda e: e['text'] == 'asan')
        cursor = len(app.events)
        emulator.key('BACK')
        # The IME consumes the first Back. Once it is hidden, exercise the
        # activity's Back callback and the native input event separately.
        wait_keyboard(False)
        cursor = len(app.events)
        emulator.key('BACK')
        app.wait('back', after=cursor)
        probe('after-keyboard')
        cursor = len(app.events)
        emulator.key('F3')
        app.wait('complete', after=cursor)
        # This arrives after native engine teardown, including any ASAN report.
        app.wait_for_exit()
        deadline = time.monotonic() + 20
        while emulator.shell('pidof', PACKAGE, check=False):
            app.check()
            if time.monotonic() >= deadline:
                raise RuntimeError('The test app did not exit after completion')
            time.sleep(0.2)
        app.check()
        passed = True
    finally:
        if gamepad:
            gamepad.close()
        for service in ('input', 'input_method', 'window', 'activity'):
            try:
                (args.output / ('dumpsys-%s.txt' % service)).write_text(emulator.shell('dumpsys', service, check=False))
            except (OSError, subprocess.SubprocessError):
                pass
        emulator.shell('am', 'force-stop', PACKAGE, check=False)
        emulator.shell('wm', 'fixed-to-user-rotation', old_fixed, check=False)
        if old_rotation:
            emulator.shell('wm', 'user-rotation', *old_rotation, check=False)
        if old_ime == 'null':
            emulator.shell('settings', 'delete', 'secure', 'show_ime_with_hard_keyboard', check=False)
        else:
            emulator.shell('settings', 'put', 'secure', 'show_ime_with_hard_keyboard', old_ime, check=False)
        app.close()
        (args.output / 'result.json').write_text(json.dumps({'passed': passed, 'events': app.events}, indent=2) + '\n')
    print('Android ASAN APK integration test passed.', flush=True)


def main():
    sys.path.insert(0, str(ROOT / 'build_tools'))
    import sdk

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=PLATFORMS, default='x86_64-android')
    parser.add_argument('--engine', type=Path, help='unstripped ASAN libdmengine.so (default: installed engine)')
    parser.add_argument('--dynamo-home', type=Path, default=os.environ.get('DYNAMO_HOME', ROOT / 'tmp/dynamo_home'))
    parser.add_argument('--sdk', type=Path, default=os.environ.get('ANDROID_SDK_ROOT') or os.environ.get('ANDROID_HOME'))
    parser.add_argument('--ndk', type=Path, default=os.environ.get('ANDROID_NDK_ROOT') or os.environ.get('ANDROID_NDK_HOME'))
    parser.add_argument('--api', type=int, default=36, help='Android API used to compile the Java activity')
    parser.add_argument('--build-tools', type=Path, help='Android SDK build-tools directory')
    parser.add_argument('--bob', type=Path, help='bob-light.jar from this checkout')
    parser.add_argument('--device', default=os.environ.get('ANDROID_SERIAL'), help='dedicated emulator serial')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/android-asan-app')
    parser.add_argument('--cycles', type=int, default=3)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--build-only', action='store_true', help='build the test APK without launching an emulator')
    mode.add_argument('--skip-build', action='store_true', help='rerun the APK recorded in output/test.json')
    args = parser.parse_args()
    if args.cycles < 3:
        parser.error('At least three connection/lifecycle cycles are required')
    if not args.build_only and not args.device:
        parser.error('Select a dedicated emulator with --device or ANDROID_SERIAL')
    args.dynamo_home = args.dynamo_home.resolve()
    args.sdk = (args.sdk or args.dynamo_home / 'ext/SDKs/android-sdk').resolve()
    args.ndk = (args.ndk or args.dynamo_home / 'ext/SDKs' / ('android-ndk-r' + sdk.ANDROID_NDK_VERSION)).resolve()
    args.build_tools = args.build_tools or args.sdk / 'build-tools' / sdk.ANDROID_BUILD_TOOLS_VERSION
    args.bob = args.bob or args.dynamo_home / 'share/java/bob-light.jar'
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    try:
        metadata = json.loads((args.output / 'test.json').read_text()) if args.skip_build else build_apk(args)
        if not args.build_only:
            run_app(args, metadata)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('Android ASAN APK test failed: %s\nArtifacts: %s' % (error, args.output), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
