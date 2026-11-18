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

"""Check JSON number conversion against independent binary64 expectations.

Generate cases in temporary storage, then ask the C++ test executable to parse
both modes. Exact halfway cases use rational arithmetic and explicit expected
bits; other cases use Python's conversion or original random binary64 bits.
"""

import argparse
from fractions import Fraction
from pathlib import Path
import random
import struct
import subprocess
import tempfile


def generate_cases(path):
    random_source = random.Random(13690)
    count = 0
    with path.open("wb") as output:
        def emit(token, expected=None):
            nonlocal count
            if expected is None:
                expected = struct.unpack("<Q", struct.pack("<d", float(token)))[0]
            data = token.encode("ascii")
            output.write(struct.pack("<IQ", len(data), expected) + data)
            count += 1

        def value(bits):
            # The virtual successor to the largest finite value is 2^1024.
            if bits == 0x7ff0000000000000:
                return Fraction(1 << 1024)
            return Fraction.from_float(struct.unpack("<d", struct.pack("<Q", bits))[0])

        def midpoint(bits, tail=0):
            middle = (value(bits) + value(bits + 1)) / 2
            power = middle.denominator.bit_length() - 1
            center = middle.numerator * 5**power * 10**tail
            power += tail
            for delta in (-1, 0, 1):
                token = f"{center + delta}e-{power}"
                expected = bits if delta < 0 or (delta == 0 and bits % 2 == 0) else bits + 1
                emit(token, expected)
                if bits in (0, 0x7fefffffffffffff):
                    emit("-" + token, expected | (1 << 63))

        for exponent in range(-342, 309):
            for _ in range(128):
                coefficient = random_source.randrange(1, 10**random_source.randrange(1, 20))
                sign = "-" if random_source.randrange(2) else ""
                emit(f"{sign}{coefficient}e{exponent}")

        for _ in range(100000):
            bits = random_source.getrandbits(64)
            if bits & 0x7ff0000000000000 == 0x7ff0000000000000:
                bits &= ~(1 << 62)
            number = struct.unpack("<d", struct.pack("<Q", bits))[0]
            emit(format(number, ".17g"), bits)

        for _ in range(10000):
            digits = random_source.randrange(20, 1201)
            coefficient = str(random_source.randrange(1, 10))
            coefficient += "".join(str(random_source.randrange(10)) for _ in range(digits - 1))
            exponent = random_source.randrange(-400, 401) - digits + 1
            sign = "-" if random_source.randrange(2) else ""
            emit(f"{sign}{coefficient}e{exponent}")

        for exponent in range(2047):
            base = exponent << 52
            for bits in (base, max(0, base - 1), base + 1):
                midpoint(bits)
        midpoint(0x7fefffffffffffff)
        for _ in range(2000):
            midpoint(random_source.randrange(0, 0x7ff0000000000000))
        for bits in (0, 1, (1 << 52) - 1, 1 << 52, 0x3ff0000000000000, 0x3ff0000000000001, 0x7fefffffffffffff):
            midpoint(bits, 900)

        for token in ("0", "-0", "0.0", "-0.0", "1e999999999999999999999", "-1e-999999999999999999999",
                      "0e999999999999999999999", "-0.000e-999999999999999999999"):
            emit(token)
        for digits in (801, 4096, 1048576):
            emit("1" + "0" * digits + f"e-{digits}", 0x3ff0000000000000)
            emit("0." + "0" * digits + f"1e{digits + 1}", 0x3ff0000000000000)
            emit("-0e" + "9" * digits, 1 << 63)
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="defold-json-numbers-") as temporary:
        cases = Path(temporary) / "numbers.bin"
        count = generate_cases(cases)
        try:
            result = subprocess.run([str(args.executable.resolve()), "--numbers", str(cases)],
                                    capture_output=True, text=True, timeout=60)
        except subprocess.TimeoutExpired:
            parser.exit(1, "JSON number conversion timed out\n")
        diagnostics = result.stdout + result.stderr
        sanitizer_failure = any(message in diagnostics for message in
                                ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:", "MemorySanitizer"))
        if result.returncode or sanitizer_failure or result.stdout.strip() != str(count):
            parser.exit(1, f"JSON number conversion failed (exit {result.returncode}):\n{diagnostics}\n")
        print(f"JSON numbers: {count} cases, {count * 2} parse checks passed")


if __name__ == "__main__":
    main()
