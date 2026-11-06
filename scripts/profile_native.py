#!/usr/bin/env python3
"""Record native CPU/GPU, resource, and optional energy profiling."""

import argparse
import ctypes
import datetime
import json
import os
from pathlib import Path
import re
import select
import shlex
import signal
import subprocess
import sys
import threading
import time

from profile_native_energy import EnergyCapture


def recording_command(args, package, capture):
    command = ["/usr/bin/xcrun", "xctrace", "record", "--template", "Game Performance",
               "--instrument", "os_signpost", "--package", str(package),
               "--instrument", "Defold Resource Loading", "--output", str(capture),
               "--time-limit", str(args.duration) + "s"]
    if getattr(args, "energy", False):
        command += ["--instrument", "Defold Energy"]
    if getattr(args, "power", False):
        command += ["--instrument", "Power Profiler", "--device", args.device, "--attach", str(args.attach)]
    return command


def save_capture(capture, recorder_path, result, metadata):
    if result != 0 or not capture.is_dir() or "Recording completed." not in recorder_path.read_text():
        raise RuntimeError("Recording failed; see " + str(recorder_path))
    metadata["recorder_exit_code"] = result
    metadata["trace_bytes"] = sum(path.stat().st_size for path in capture.rglob("*") if path.is_file())
    capture.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")
    print("Native trace bytes: " + str(metadata["trace_bytes"]), flush=True)
    print("Open in Instruments: " + shlex.join(["open", "-a", "Instruments", str(capture)]), flush=True)


def check_power_device(device, environment):
    instruments = subprocess.run(["/usr/bin/xcrun", "xctrace", "list", "instruments"],
                                 env=environment, capture_output=True, text=True, check=True)
    if "Power Profiler" not in instruments.stdout.splitlines():
        raise RuntimeError("This Xcode installation does not provide Power Profiler")
    info = subprocess.run(["/usr/bin/xcrun", "devicectl", "device", "info", "details", "--device", device,
                           "--timeout", "15", "--json-output", "-"],
                          env=environment, capture_output=True, text=True, check=True)
    result = json.loads(info.stdout)["result"]
    properties = result.get("properties", {})
    hardware = properties.get("hardware", result.get("hardwareProperties", {}))
    software = properties.get("software", result.get("deviceProperties", {}))
    version = software.get("osVersionNumber", "0")
    if isinstance(version, dict):
        version = version["stringValue"]
    if (hardware.get("reality") != "physical" or hardware.get("deviceType") not in ("iPhone", "iPad")
            or int(version.split(".")[0]) < 26):
        raise RuntimeError("Power Profiler requires a physical iPhone/iPad running iOS/iPadOS 26 or later")
    connection = properties.get("connection", result.get("connectionProperties", {}))
    if connection.get("state", connection.get("tunnelState")) != "connected":
        raise RuntimeError("The power capture device is not connected; pair and unlock it in Xcode")
    return {"source": "Power Profiler", "device": device, "os_version": version}


def record_power(args, environment, package, capture, power):
    recorder_path = capture.with_suffix(".xctrace.log")
    print("Profiler capture: " + str(capture), flush=True)
    print("Recorder log: " + str(recorder_path), flush=True)
    print("Use wireless debugging with the device unplugged for battery consumption readings", flush=True)
    with recorder_path.open("w") as output:
        recorder = subprocess.Popen(recording_command(args, package, capture),
                                    stdout=output, stderr=subprocess.STDOUT, env=environment)
        try:
            result = recorder.wait(timeout=args.duration + 45)
        finally:
            if recorder.poll() is None:
                recorder.terminate()
                try:
                    recorder.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    recorder.kill()
                    recorder.wait()
    save_capture(capture, recorder_path, result,
                 {"trace": str(capture), "duration_seconds": args.duration, "pid": args.attach,
                  "power": power, "application_ownership": "attached"})


def check(result):
    if result:
        raise OSError(result, os.strerror(result))


def system_library():
    libc = ctypes.CDLL("/usr/lib/libSystem.B.dylib")
    pointer = ctypes.POINTER
    libc.posix_spawnattr_init.argtypes = [pointer(ctypes.c_void_p)]
    libc.posix_spawnattr_setflags.argtypes = [pointer(ctypes.c_void_p), ctypes.c_short]
    libc.posix_spawnattr_destroy.argtypes = [pointer(ctypes.c_void_p)]
    libc.posix_spawn_file_actions_init.argtypes = [pointer(ctypes.c_void_p)]
    libc.posix_spawn_file_actions_adddup2.argtypes = [pointer(ctypes.c_void_p), ctypes.c_int, ctypes.c_int]
    libc.posix_spawn_file_actions_destroy.argtypes = [pointer(ctypes.c_void_p)]
    libc.posix_spawn.argtypes = [pointer(ctypes.c_int), ctypes.c_char_p, pointer(ctypes.c_void_p),
                               pointer(ctypes.c_void_p), pointer(ctypes.c_char_p), pointer(ctypes.c_char_p)]
    libc.notify_register_file_descriptor.argtypes = [
        ctypes.c_char_p, pointer(ctypes.c_int), ctypes.c_int, pointer(ctypes.c_int)]
    libc.notify_cancel.argtypes = [ctypes.c_int]
    return libc


def spawn_suspended(libc, command, environment, output):
    attributes = ctypes.c_void_p()
    actions = ctypes.c_void_p()
    pid = ctypes.c_int()
    check(libc.posix_spawnattr_init(ctypes.byref(attributes)))
    try:
        # Darwin's POSIX_SPAWN_START_SUSPENDED: no application code runs before attach.
        check(libc.posix_spawnattr_setflags(ctypes.byref(attributes), 0x0080))
        check(libc.posix_spawn_file_actions_init(ctypes.byref(actions)))
        try:
            check(libc.posix_spawn_file_actions_adddup2(ctypes.byref(actions), output.fileno(), 1))
            check(libc.posix_spawn_file_actions_adddup2(ctypes.byref(actions), output.fileno(), 2))
            argv = (ctypes.c_char_p * (len(command) + 1))(*map(os.fsencode, command), None)
            env = [os.fsencode(key + "=" + value) for key, value in environment.items()]
            envp = (ctypes.c_char_p * (len(env) + 1))(*env, None)
            check(libc.posix_spawn(ctypes.byref(pid), argv[0], ctypes.byref(actions),
                                  ctypes.byref(attributes), argv, envp))
            return pid.value
        finally:
            libc.posix_spawn_file_actions_destroy(ctypes.byref(actions))
    finally:
        libc.posix_spawnattr_destroy(ctypes.byref(attributes))


def stop_process(pid):
    try:
        # This runner owns the child. Enforce the runtime bound even during startup
        # or if the application handles SIGTERM; signposts are already out of process.
        os.kill(pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def record(args, environment, package, capture, energy=None):
    libc = system_library()
    notify_fd = ctypes.c_int(-1)
    notify_token = ctypes.c_int()
    notification = "com.defold.profiling.ready." + str(os.getpid())
    check(libc.notify_register_file_descriptor(
        notification.encode(), ctypes.byref(notify_fd), 0, ctypes.byref(notify_token)))
    pid = None
    recorder = None
    timer = None
    metadata = {"trace": str(capture), "duration_seconds": args.duration}
    stdout_path = capture.with_suffix(".stdout.log")
    recorder_path = capture.with_suffix(".xctrace.log")
    print("Profiler capture: " + str(capture), flush=True)
    print("Engine stdout: " + str(stdout_path), flush=True)
    try:
        with stdout_path.open("w") as output:
            pid = spawn_suspended(libc, args.command, dict(environment, DM_PROFILE_NATIVE_ENABLE="1"), output)
        metadata.update(pid=pid, command=args.command, stdout=str(stdout_path))
        with recorder_path.open("w") as output:
            recorder = subprocess.Popen(recording_command(args, package, capture) + [
                "--attach", str(pid),
                "--notify-tracing-started", notification,
            ], stdout=output, stderr=subprocess.STDOUT, env=environment)
        deadline = time.monotonic() + 30
        while True:
            ready, _, _ = select.select([notify_fd.value], [], [], 0.1)
            state = subprocess.run(["/bin/ps", "-p", str(pid), "-o", "stat="],
                                   capture_output=True, text=True, check=True)
            if "T" not in state.stdout:
                raise RuntimeError("Application resumed before recording readiness")
            if ready:
                os.read(notify_fd.value, 4)
                break
            if recorder.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError("Recorder did not become ready; see " + str(recorder_path))

        if energy:
            energy.start(pid)
        started = time.monotonic()

        def stop_at_limit():
            stop_process(pid)
            metadata["stop_after_seconds"] = time.monotonic() - started
            print("Application runtime limit reached: " + str(args.duration) + " seconds", flush=True)

        timer = threading.Timer(args.duration, stop_at_limit)
        timer.start()
        os.kill(pid, signal.SIGCONT)
        print("Recording ready; application resumed", flush=True)
        result = recorder.wait(timeout=args.duration + 45)
        timer.join()
        _, status = os.waitpid(pid, 0)
        pid = None
        metadata["application_exit_code"] = os.waitstatus_to_exitcode(status)
        if energy:
            metadata["energy"] = energy.finish()
        save_capture(capture, recorder_path, result, metadata)
        if metadata["application_exit_code"] not in (0, -signal.SIGKILL):
            raise RuntimeError("Application failed with exit code " + str(metadata["application_exit_code"]) +
                               "; see " + str(stdout_path))
        if metadata["application_exit_code"] == 0:
            print("Application exited before the runtime limit", flush=True)
    finally:
        if timer:
            timer.cancel()
            timer.join()
        if pid is not None:
            stop_process(pid)
            os.waitpid(pid, 0)
        try:
            if energy:
                energy.close()
        finally:
            if recorder and recorder.poll() is None:
                recorder.terminate()
                try:
                    recorder.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    recorder.kill()
                    recorder.wait()
            libc.notify_cancel(notify_token.value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle-id", required=True, help="Directory name under ~/Library/Logs/")
    parser.add_argument("--duration", type=int, default=5, help="Recording and application runtime limit in seconds (default: 5)")
    parser.add_argument("--energy", action="store_true", help="Include macOS powermetrics estimates; authorize sudo in this terminal first")
    parser.add_argument("--power", action="store_true", help="Include Power Profiler for an attached physical iPhone/iPad process")
    parser.add_argument("--device", help="Physical iPhone/iPad identifier for --power")
    parser.add_argument("--attach", type=int, help="Existing device process ID for --power; the application stays running")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="-- executable [engine options] game.projectc")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("Native capture requires macOS and Xcode")
    if not re.fullmatch(r"[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)*", args.bundle_id):
        parser.error("--bundle-id must be a bundle identifier")
    if args.duration <= 0:
        parser.error("--duration must be greater than zero")
    if args.command and args.command[0] == "--":
        args.command.pop(0)
    if args.power or args.device or args.attach is not None:
        if not args.power or not args.device or args.attach is None or args.attach <= 0:
            parser.error("Device power capture requires --power --device <identifier> --attach <pid>")
        if args.energy or args.command:
            parser.error("Device power capture attaches to an existing app; omit --energy and the local launch command")
        capture_name = args.bundle_id + "-power"
    else:
        if not args.command:
            parser.error("Supply -- executable [engine options] game.projectc")
        executable = Path(args.command[0]).expanduser().resolve(strict=True)
        args.command[0] = str(executable)
        if not os.access(executable, os.X_OK):
            parser.error("Executable is not executable: " + str(executable))
        capture_name = executable.stem

    # xctrace stores the target's environment in the trace. Pass only launch
    # essentials, never shell credentials or the rest of the caller's environment.
    environment = {key: os.environ[key] for key in
                   ("HOME", "USER", "LOGNAME", "TMPDIR", "LANG", "DEVELOPER_DIR") if key in os.environ}
    environment["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    power = check_power_device(args.device, environment) if args.power else None
    if args.energy:
        authorized = subprocess.run(["/usr/bin/sudo", "-n", "/usr/bin/powermetrics", "--help"],
                                    env=environment, capture_output=True)
        if authorized.returncode != 0:
            parser.error("macOS energy capture requires administrator access for powermetrics; run sudo -v in this terminal first")
    capture_dir = Path.home() / "Library/Logs" / args.bundle_id / "Profiling"
    capture_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + str(os.getpid())
    capture = capture_dir / (capture_name + "-" + stamp + ".trace")
    source = Path(__file__).resolve().parents[1] / "engine/engine/instruments/DefoldResources.instrpkg"
    package = capture.with_suffix(".instrdst")
    subprocess.run(["/usr/bin/xcrun", "instrumentbuilder", str(source), "-o", str(package)],
                   env=environment, check=True)
    print("Profiler instrument package: " + str(package), flush=True)
    if args.power:
        record_power(args, environment, package, capture, power)
    else:
        energy = None
        if args.energy:
            bridge = capture.with_suffix(".energy-writer")
            subprocess.run(["/usr/bin/xcrun", "clang", "-O2", "-Wall", "-Wextra", "-Werror",
                            str(source.with_name("energy_signposts.c")), "-o", str(bridge)], env=environment, check=True)
            energy = EnergyCapture(args.duration, environment, bridge, capture)
        record(args, environment, package, capture, energy)


if __name__ == "__main__":
    main()
