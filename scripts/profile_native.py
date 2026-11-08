#!/usr/bin/env python3
"""Record native CPU/GPU, resource, and optional energy profiling."""

import argparse
import datetime
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys


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
        recorder = subprocess.Popen([
            "/usr/bin/xcrun", "xctrace", "record", "--template", "Game Performance",
            "--instrument", "os_signpost", "--package", str(package),
            "--instrument", "Defold Resource Loading", "--instrument", "Power Profiler",
            "--device", args.device, "--attach", str(args.attach),
            "--time-limit", str(args.duration) + "s", "--output", str(capture),
        ], stdout=output, stderr=subprocess.STDOUT, env=environment)
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


def record(args, environment, capture):
    environment.update({
        "DM_PROFILE_NATIVE_ENABLE": "1",
        "DM_PROFILE_NATIVE_TRACE": "1",
        "DM_PROFILE_NATIVE_TRACE_DELAY_MS": "0",
        "DM_PROFILE_NATIVE_TRACE_DURATION_MS": str(args.duration * 1000),
        "DM_PROFILE_NATIVE_TRACE_ENERGY": "1" if args.energy else "0",
        "DM_PROFILE_NATIVE_TRACE_PATH": str(capture),
    })
    print("Profiler capture: " + str(capture), flush=True)
    os.execve(args.command[0], args.command, environment)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle-id", required=True, help="Directory name under ~/Library/Logs/")
    parser.add_argument("--duration", type=int, default=5, help="Recording duration in seconds (default: 5); the application stays running")
    parser.add_argument("--energy", action="store_true", help="Include macOS powermetrics estimates; macOS will ask for administrator authorization")
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
        if args.duration * 1000 > 0xffffffff:
            parser.error("--duration exceeds the engine's millisecond range")
        executable = Path(args.command[0]).expanduser().resolve(strict=True)
        args.command[0] = str(executable)
        if not os.access(executable, os.X_OK):
            parser.error("Executable is not executable: " + str(executable))
        capture_name = executable.stem

    # xctrace stores the target's environment in the trace. Pass only launch
    # essentials, never shell credentials or the rest of the caller's environment.
    environment = {key: os.environ[key] for key in
                   ("HOME", "USER", "LOGNAME", "TMPDIR", "LANG", "DEVELOPER_DIR", "DM_PROFILE_NATIVE_XCTRACE") if key in os.environ}
    environment["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    power = check_power_device(args.device, environment) if args.power else None
    capture_dir = Path.home() / "Library/Logs" / args.bundle_id / "Profiling"
    capture_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + str(os.getpid())
    capture = capture_dir / (capture_name + "-" + stamp + ".trace")
    if args.power:
        source = Path(__file__).resolve().parents[1] / "engine/engine/instruments/DefoldResources.instrpkg"
        package = capture.with_suffix(".instrdst")
        subprocess.run(["/usr/bin/xcrun", "instrumentbuilder", str(source), "-o", str(package)],
                       env=environment, check=True)
        print("Profiler instrument package: " + str(package), flush=True)
        record_power(args, environment, package, capture, power)
    else:
        record(args, environment, capture)


if __name__ == "__main__":
    main()
