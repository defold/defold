"""Run the Linux window integration tests in isolated desktop sessions."""
import argparse
from contextlib import contextmanager, ExitStack
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time


@contextmanager
def server(command, env, directory, **kwargs):
    with subprocess.Popen(command, cwd=directory, env=env, **kwargs) as process:
        try:
            yield process
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()


def wait_ready(process, command, env, expected=None):
    deadline = time.monotonic() + 15
    while process.poll() is None and time.monotonic() < deadline:
        probe = subprocess.run(command, env=env, capture_output=True, timeout=5)
        if probe.returncode == 0 and (expected is None or expected in probe.stdout):
            return
        time.sleep(0.025)
    raise RuntimeError(f"Display service did not become ready: {command}")


def input_test(executable, app_env, x11, directory, nested):
    def mouse(*args):
        subprocess.run(['xdotool', *args], env=x11, check=True, timeout=10)

    with server([executable, '--input'], app_env, directory, stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0) as app:
        def expect(event):
            deadline = time.monotonic() + 15
            while app.poll() is None and time.monotonic() < deadline:
                if not select.select([app.stdout], [], [], max(0, deadline - time.monotonic()))[0]:
                    break
                line = app.stdout.readline().decode().strip()
                if line == event:
                    return
                if line:
                    print(line, flush=True)
            raise RuntimeError(f'Input test did not report {event} on {app_env["DM_TEST_WINDOW_BACKEND"]}')

        expect('READY')
        selector = ['--class', 'Weston Compositor'] if nested else ['--name', 'Defold Linux input test']
        window = subprocess.check_output(['xdotool', 'search', '--sync', '--onlyvisible', *selector],
                                         env=x11, timeout=10).splitlines()[-1].decode()
        mouse('windowactivate', '--sync', window, 'mousemove', '--window', window, '200', '200')
        # Activating the outer Weston window does not tell us when the game surface can receive input.
        expect('POINTER_READY')
        mouse('mousedown', '1')
        expect('KEYBOARD_READY')
        mouse('keydown', 'a')
        expect('PRESSED')
        mouse('keyup', 'a', 'mouseup', '1', 'click', '4')
        expect('LOCKED')
        mouse('mousemove_relative', '--', '24', '12')
        expect('MOVED')
        mouse('keydown', 'dead_acute')
        expect('ACCENT_DOWN')
        mouse('keyup', 'dead_acute', 'key', 'e')
        expect('COMPOSED')
        mouse('keydown', 'Alt_L', 'keydown', 'k')
        expect('SHORTCUT')
        mouse('keyup', 'k', 'keyup', 'Alt_L', 'keydown', '108', 'key', '5', 'keyup', '108')
        expect('ALTGR')
        mouse('keydown', 'b')
        try:
            expect('COPIED')
            paste = ['wl-paste', '--no-newline'] if nested else ['xclip', '-selection', 'clipboard', '-out']
            if subprocess.check_output(paste, env=app_env, timeout=10) != b'Defold clipboard':
                raise RuntimeError('Clipboard text did not round-trip')
            app.stdin.write(b'done\n')
            app.stdin.flush()
            if app.wait(timeout=10):
                raise RuntimeError('Linux input test failed')
        finally:
            mouse('keyup', 'a', 'keyup', 'b', 'mouseup', '1')


def run(args, directory):
    clean = dict(os.environ, XDG_RUNTIME_DIR=str(directory), LIBGL_ALWAYS_SOFTWARE='1',
                 GALLIUM_DRIVER='llvmpipe', LC_ALL='C.UTF-8')
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'DM_TEST_X11_MONITOR'):
        clean.pop(name, None)
    if args.headless:
        subprocess.run([args.headless], cwd=directory, env=clean, check=True, timeout=60)
    with ExitStack() as stack:
        read_fd, write_fd = os.pipe()
        reader = stack.enter_context(os.fdopen(read_fd))
        with os.fdopen(write_fd, 'w') as writer:
            # A key held until the app acknowledges it should produce a single press.
            stack.enter_context(server(['Xvfb', '-displayfd', str(writer.fileno()), '-screen', '0',
                                        '1280x1024x24', '-r', '-noreset', '-nolisten', 'tcp'],
                                       clean, directory, pass_fds=(writer.fileno(),)))
        if not select.select([reader], [], [], 15)[0]:
            raise RuntimeError('Xvfb did not publish a display')
        display = int(reader.readline())
        x11 = dict(clean, DISPLAY=f':{display}', DM_TEST_X11_MONITOR='screen')
        if args.native:
            subprocess.run([args.native, '--test-filter', 'LinuxWindow.X11FocusWithoutWindowManager'],
                           cwd=directory, env=dict(x11, DM_TEST_X11_NO_WM='1'), check=True, timeout=30)
        wm = stack.enter_context(server(['openbox'], x11, directory))
        wait_ready(wm, ['xprop', '-root', '_NET_SUPPORTING_WM_CHECK'], x11, b'window id #')
        wayland = dict(clean, WAYLAND_DISPLAY='defold-test', DM_TEST_WAYLAND_SCALE='2')
        weston = stack.enter_context(server(['weston', '--no-config', '--backend=headless-backend.so',
                                            '--socket=defold-test', '--scale=2', '--idle-time=0'],
                                           wayland, directory))
        # Weston publishes its listening socket after initialization. The clients
        # then synchronize with its registry and initial xdg_surface configure.
        wait_ready(weston, ['test', '-S', directory / wayland['WAYLAND_DISPLAY']], wayland)

        if args.native:
            # Make each optional Wayland library unloadable on an X11-only installation.
            missing = directory / 'without-wayland'
            missing.mkdir()
            for library in ('libwayland-client.so.0', 'libwayland-cursor.so.0', 'libwayland-egl.so.1',
                            'libxkbcommon.so.0', 'libdecor-0.so.0'):
                (missing / library).touch()
            x11_only = dict(x11, LD_LIBRARY_PATH=str(missing) + ':' + clean.get('LD_LIBRARY_PATH', ''),
                            DM_TEST_WINDOW_BACKEND='x11')
            subprocess.run([args.placement, '--test-filter', 'WindowPlacementTest.NativeWindowIdentity'],
                           cwd=directory, env=x11_only, check=True, timeout=30)
            subprocess.run([args.native, '--test-filter', 'LinuxWindow.X11WindowClass'],
                           cwd=directory, env=dict(x11_only, RESOURCE_NAME='game-instance'), check=True, timeout=30)
            # Mesa can use the bridge internally. Hide its symbol from the window backend only.
            subprocess.run([args.native, '--test-filter', 'LinuxWindow.VulkanDisplaySelection'],
                           cwd=directory, env=dict(x11, DM_TEST_WINDOW_BACKEND='x11', DM_TEST_X11_NO_XCB='1'),
                           check=True, timeout=30)
            missing_randr = directory / 'without-xrandr'
            missing_randr.mkdir()
            (missing_randr / 'libXrandr.so.2').touch()
            no_randr = dict(x11, LD_LIBRARY_PATH=str(missing_randr) + ':' + clean.get('LD_LIBRARY_PATH', ''),
                            DM_TEST_X11_MONITOR='Display')
            subprocess.run([args.native, '--test-filter', 'LinuxWindow.X11RandRMonitors'],
                           cwd=directory, env=no_randr, check=True, timeout=30)
            subprocess.run([args.native, '--no-display'], cwd=directory, env=clean, check=True, timeout=15)
            no_libdecor = directory / 'without-libdecor'
            no_libdecor.mkdir()
            (no_libdecor / 'libdecor-0.so.0').touch()
            subprocess.run([args.native, '--test-filter', 'LinuxWindow.WaylandFallbackFrame'], cwd=directory,
                           env=dict(wayland, DM_TEST_WAYLAND_FALLBACK='1',
                                    LD_LIBRARY_PATH=str(no_libdecor) + ':' + clean.get('LD_LIBRARY_PATH', '')),
                           check=True, timeout=30)
            rejected = subprocess.run([args.native, '--reject-glx-context'], cwd=directory,
                                      env=dict(x11, MESA_GL_VERSION_OVERRIDE='3.3'), check=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
            if b'OpenGL 4.6 (core profile): GLXBad' not in rejected.stdout and b'OpenGL 4.6 (core profile): Bad' not in rejected.stdout:
                raise RuntimeError(f'Missing GLX driver diagnostic: {rejected.stdout.decode()}')
        for backend, env in [('x11', dict(x11, WAYLAND_DISPLAY='defold-test', DM_TEST_X11_REMAP_KEYS='1')), ('wayland', wayland)]:
            env['DM_TEST_WINDOW_BACKEND'] = backend
            print(f'Running Linux window tests on {backend}', flush=True)
            subprocess.run([args.placement], env=env, cwd=directory, check=True, timeout=120)
            if args.native:
                subprocess.run([args.native], env=env, cwd=directory, check=True, timeout=120)
            if args.vulkan:
                subprocess.run([args.vulkan], env=env, cwd=directory, check=True, timeout=120)
        if args.native:
            fallback = dict(wayland, DISPLAY=':65534', DM_TEST_WINDOW_BACKEND='wayland')
            subprocess.run([args.native, '--test-filter', 'LinuxWindow.VulkanDisplaySelection'],
                           cwd=directory, env=fallback, check=True, timeout=30)
            subprocess.run(['setxkbmap', '-layout', 'us', '-variant', 'intl'], env=x11, check=True)
            input_test(args.native, dict(x11, DM_TEST_WINDOW_BACKEND='x11'), x11, directory, False)
            # XTest supplies real seat events to a nested compositor. The client only sees Wayland.
            nested = dict(clean, WAYLAND_DISPLAY='defold-input', DM_TEST_WINDOW_BACKEND='wayland')
            config = directory / 'weston-input.ini'
            # Wayland clients use the compositor's repeat settings for the same held keys.
            config.write_text('[keyboard]\nkeymap_layout=us\nkeymap_variant=intl\nrepeat-rate=0\n')
            compositor = stack.enter_context(server(['weston', f'--config={config}', '--backend=x11-backend.so',
                                                     '--socket=defold-input', '--shell=kiosk-shell.so', '--idle-time=0'],
                                                    x11, directory))
            wait_ready(compositor, ['test', '-S', directory / nested['WAYLAND_DISPLAY']], nested)
            input_test(args.native, nested, x11, directory, True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--placement', type=Path, required=True)
    parser.add_argument('--native', type=Path)
    parser.add_argument('--headless', type=Path)
    parser.add_argument('--vulkan', type=Path)
    args = parser.parse_args()
    args.placement = args.placement.resolve()
    if args.native:
        args.native = args.native.resolve()
    if args.headless:
        args.headless = args.headless.resolve()
    if args.vulkan:
        args.vulkan = args.vulkan.resolve()
    with tempfile.TemporaryDirectory(prefix='defold-window-') as directory:
        run(args, Path(directory))


if __name__ == '__main__':
    main()
