"""Reuse one JVM for a sequence of isolated Bob/tool invocations."""

import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import uuid


class BobWorker:
    def __init__(self, java, classes, classpath, cwd=None, env=None):
        self.prefix = "BOB_WORKER_" + uuid.uuid4().hex + ":"
        self.command = [java, *shlex.split(os.environ.get("DM_JAVA_RUNTIME_FLAGS", "")),
                        "-cp", os.pathsep.join([str(classes), classpath]), "BobWorker", self.prefix]
        self.process = subprocess.Popen(self.command, cwd=cwd, env=env, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")

    def __enter__(self):
        return self

    def invoke(self, args, main_class="com.dynamo.bob.Bob"):
        print("Java batch: " + main_class + " " + shlex.join(args), flush=True)
        try:
            self.process.stdin.write(json.dumps([[main_class], args]) + "\n")
            self.process.stdin.flush()
        except BrokenPipeError:
            raise subprocess.CalledProcessError(self.process.wait() or 1, self.command)
        for line in self.process.stdout:
            if line.startswith(self.prefix):
                result = int(line[len(self.prefix):])
                if result:
                    raise subprocess.CalledProcessError(result, [main_class, *args])
                return
            sys.stdout.write(line)
        # A native crash or a tool calling System.exit must never mark this job done.
        raise subprocess.CalledProcessError(self.process.wait() or 1, [main_class, *args])

    def __exit__(self, exc_type, exc, traceback):
        try:
            self.process.stdin.close()
        except BrokenPipeError:
            pass
        if exc_type and self.process.poll() is None:
            self.process.terminate()
        try:
            result = self.process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            raise
        finally:
            self.process.stdout.close()
        if not exc_type and result:
            raise subprocess.CalledProcessError(result, self.command)


def dirty_jobs(manifest, common_dependencies=()):
    """CMake schedules a lane; retain per-project incremental/failure stamps."""
    manifest = Path(manifest)
    jobs = json.loads(manifest.read_text(encoding="utf-8"))
    common = [manifest, Path(__file__), *map(Path, common_dependencies)]
    for job in jobs:
        stamp = Path(job["stamp"])
        if not stamp.exists() or any(Path(path).stat().st_mtime_ns > stamp.stat().st_mtime_ns
                                     for path in [*common, *job["dependencies"]]):
            yield job
