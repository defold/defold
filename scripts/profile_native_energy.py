"""Collect macOS power estimates and publish them to the native Instruments trace."""

import datetime
import json
import math
import os
from pathlib import Path
import plistlib
import select
import subprocess
import tempfile
import threading
import time
import xml.etree.ElementTree as ET


def samples(stream):
    """powermetrics emits NUL-separated XML property lists, not a single plist."""
    pending = b""
    while True:
        block = stream.read1(65536)
        if not block:
            if pending.strip():
                yield plistlib.loads(pending)
            return
        pending += block
        while b"\0" in pending:
            sample, pending = pending.split(b"\0", 1)
            if sample.strip():
                yield plistlib.loads(sample)


def normalize(sample, pid):
    elapsed_ns = int(sample["elapsed_ns"])
    if elapsed_ns <= 0:
        raise ValueError("powermetrics reported an invalid sample duration")
    result = {"elapsed_ns": elapsed_ns, "system_power_w": {}}
    timestamp = sample.get("timestamp")
    if timestamp is not None:
        result["sampled_at"] = timestamp.replace(tzinfo=datetime.timezone.utc).isoformat()
    for component in ("cpu", "gpu", "ane"):
        value = sample.get("processor", {}).get(component + "_power")
        if value is not None:
            result["system_power_w"][component] = float(value) / 1000
    if not result["system_power_w"]:
        raise ValueError("powermetrics did not report CPU/GPU/ANE power; this hardware or output format is unsupported")
    task = next((task for task in sample.get("tasks", []) if task.get("pid") == pid), None)
    if task is not None and "energy_impact_per_s" in task:
        result["application_energy_impact_per_second"] = float(task["energy_impact_per_s"])
    values = list(result["system_power_w"].values())
    if "application_energy_impact_per_second" in result:
        values.append(result["application_energy_impact_per_second"])
    if any(not math.isfinite(value) or value < 0 for value in values):
        raise ValueError("powermetrics reported an invalid power or energy impact value")
    return result


class EnergyCapture:
    def __init__(self, duration, environment, bridge, capture):
        self.duration = duration
        self.environment = environment
        self.bridge_path = bridge
        self.capture = capture
        self.sample_path = capture.with_suffix(".energy.jsonl")
        self.log_path = capture.with_suffix(".energy.log")
        self.collector = None
        self.bridge = None
        self.worker = None
        self.error = None
        self.sample_count = 0
        self.application_sample_count = 0
        self.log = None
        self.closed = False

    def start(self, pid):
        self.pid = pid
        self.started = time.monotonic()
        self.log = self.log_path.open("w")
        self.bridge = subprocess.Popen([str(self.bridge_path)], stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=self.log, env=self.environment)
        ready, _, _ = select.select([self.bridge.stdout], [], [], 5)
        if not ready or os.read(self.bridge.stdout.fileno(), 1) != b"R":
            raise RuntimeError("Energy signpost helper did not become ready; see " + str(self.log_path))
        # The isolated supervisor owns the privileged sampler. Closing stdin
        # cancels it without another sudo command or killing the user's game.
        supervisor = Path(__file__).with_name("profile_native_energy_sampler.py").resolve()
        self.collector = subprocess.Popen([
            "/usr/bin/sudo", "-n", "/usr/bin/python3", "-I", "-S", str(supervisor), str(self.duration * 2),
        ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log, env=self.environment)
        self.worker = threading.Thread(target=self.read_samples, daemon=True)
        self.worker.start()
        print("Energy samples: " + str(self.sample_path), flush=True)

    def read_samples(self):
        try:
            with self.sample_path.open("w") as output:
                for sample in samples(self.collector.stdout):
                    data = normalize(sample, self.pid)
                    data["received_after_seconds"] = time.monotonic() - self.started
                    output.write(json.dumps(data, allow_nan=False) + "\n")
                    elapsed_ns = data["elapsed_ns"]
                    for index, component in enumerate(("cpu", "gpu", "ane")):
                        if component in data["system_power_w"]:
                            value = data["system_power_w"][component]
                            self.bridge.stdin.write(f"S {index} {value:.9g} {elapsed_ns}\n".encode())
                    impact = data.get("application_energy_impact_per_second")
                    if impact is not None:
                        self.bridge.stdin.write(f"A {self.pid} {impact:.9g} {elapsed_ns}\n".encode())
                        self.application_sample_count += 1
                    self.bridge.stdin.flush()
                    self.sample_count += 1
        except Exception as error:
            self.error = error
            self.cancel_sampler()
            # Release any final output while the supervisor stops its child.
            while self.collector.stdout.read1(65536):
                pass

    def finish(self):
        try:
            # Recording has already ended. A slow sampler cannot extend cleanup
            # by the requested capture duration, and close cancels on timeout.
            self.collector.wait(timeout=5)
            self.worker.join(timeout=5)
            if self.worker.is_alive():
                raise RuntimeError("Energy sample reader did not finish")
        finally:
            self.close()
        if self.error:
            raise RuntimeError("Energy collection failed: " + str(self.error)) from self.error
        if self.collector.returncode != 0 or self.bridge.returncode != 0:
            raise RuntimeError("Energy collection failed; see " + str(self.log_path))
        if not self.sample_count or not self.application_sample_count:
            raise RuntimeError("Energy capture has no system power or target-process impact samples; see " + str(self.log_path))
        retained = self.trace_counts()
        return {"source": "powermetrics", "samples": str(self.sample_path),
                "system_sample_count": self.sample_count, "application_sample_count": self.application_sample_count,
                "trace_row_counts": retained}

    def trace_counts(self):
        counts = {}
        with tempfile.TemporaryDirectory(prefix="defold-energy-") as directory, self.log_path.open("a") as log:
            for schema in ("defold-system-power", "defold-application-energy"):
                output = Path(directory) / (schema + ".xml")
                subprocess.run([
                    "/usr/bin/xcrun", "xctrace", "export", "--input", str(self.capture),
                    "--xpath", '/trace-toc/run[@number="1"]/data/table[@schema="' + schema + '"]',
                    "--output", str(output),
                ], env=self.environment, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
                root = ET.parse(output).getroot()
                rows = root.findall(".//row")
                if schema == "defold-application-energy":
                    values = {element.get("id"): element.text for element in root.iter() if element.get("id")}
                    rows = [row for row in rows if (row[1].text or values.get(row[1].get("ref"))) == str(self.pid)]
                counts[schema] = len(rows)
                if not rows:
                    raise RuntimeError("Saved trace contains no " + schema + " rows for this capture; see " + str(self.log_path))
        return counts

    def cancel_sampler(self):
        if self.collector and self.collector.stdin and not self.collector.stdin.closed:
            self.collector.stdin.close()

    def close(self):
        if self.closed:
            return
        try:
            self.cancel_sampler()
            if self.bridge and self.worker and self.worker.is_alive():
                # Unblock a reader stuck publishing to a stalled helper.
                self.bridge.kill()
            if self.collector:
                self.collector.wait(timeout=5)
            if self.worker:
                self.worker.join(timeout=5)
                if self.worker.is_alive():
                    raise RuntimeError("Energy sample reader did not finish")
        finally:
            if self.bridge:
                if self.bridge.stdin and not self.bridge.stdin.closed and (not self.worker or not self.worker.is_alive()):
                    try:
                        self.bridge.stdin.close()
                    except BrokenPipeError:
                        pass
                try:
                    self.bridge.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.bridge.kill()
                    self.bridge.wait()
                if self.bridge.stdin and not self.bridge.stdin.closed:
                    try:
                        self.bridge.stdin.close()
                    except BrokenPipeError:
                        pass
                self.bridge.stdout.close()
            # Closing a buffered pipe while another thread is blocked reading it
            # would hang cleanup if the sampler failed to honor its sample count.
            if self.collector and self.collector.stdout and (not self.worker or not self.worker.is_alive()):
                self.collector.stdout.close()
            if self.log:
                self.log.close()
        self.closed = True
