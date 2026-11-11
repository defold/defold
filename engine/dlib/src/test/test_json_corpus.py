#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

"""Run the pinned JSONTestSuite corpus against either JSON test executable."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def run(executable, timeout):
    root = Path(__file__).parent / "data" / "json_test_suite"
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    cases = [item for item in manifest["files"] if item["path"].startswith("test_parsing/")]
    names = {Path(item["path"]).name for item in cases}
    if names != {path.name for path in (root / "test_parsing").iterdir()}:
        raise ValueError("Corpus files differ from the pinned manifest")
    expectations = manifest["expectations"]
    if not set(expectations).issubset(names):
        raise ValueError("Corpus expectations refer to missing fixtures")
    # Keep malformed UTF-8, NULs and line endings byte-for-byte identical to upstream.
    for item in manifest["files"]:
        data = (root / item["path"]).read_bytes()
        digest = hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()
        if len(data) != item["size"] or digest != item["sha"]:
            raise ValueError("Corpus content differs from upstream: " + item["path"])

    failures = []
    for item in cases:
        name = Path(item["path"]).name
        policy = expectations.get(name)
        if policy:
            expected = (policy["decode"], policy["preserve"])
            reason = policy["reason"]
        elif name.startswith("y_"):
            expected = (True, True)
            reason = "upstream requires acceptance"
        elif name.startswith("n_"):
            expected = (False, False)
            reason = "upstream requires rejection"
        else:
            raise ValueError("Missing explicit implementation-defined expectation: " + name)
        try:
            result = subprocess.run(
                [str(executable), "--corpus", str(root / item["path"])],
                capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"{name}: timed out after {timeout:g}s") from error
        if result.returncode or result.stderr:
            diagnostic = result.stderr.decode("utf-8", errors="replace")
            raise RuntimeError(f"{name}: exit {result.returncode}\n{diagnostic}")
        expected_output = f"{int(expected[0])} {int(expected[1])}".encode("ascii")
        if result.stdout.strip() != expected_output:
            failures.append(f"{name}: expected {expected_output!r}, got {result.stdout!r} ({reason})")
    if failures:
        raise RuntimeError("\n".join(failures))
    print(f"JSONTestSuite: {len(cases)} fixtures, {len(cases) * 2} parse checks passed "
          f"({len(expectations)} explicit policy expectations)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    try:
        run(args.executable.resolve(), args.timeout)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, str(error) + "\n")
