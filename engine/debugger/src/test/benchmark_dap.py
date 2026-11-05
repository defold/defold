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

"""Measure attached Lua hook cost as the number of idle coroutines increases."""
import argparse
import pathlib
import statistics

import test_dap


def measure(coroutines, iterations):
    case = test_dap.DAPTestCase()
    case.setUp()
    try:
        client = case.start('''
            local held = {}
            for i = 1, %d do
                held[i] = coroutine.create(function() coroutine.yield() end)
            end
            local active = coroutine.create(function()
                local started = os.clock()
                local sum = 0
                for i = 1, %d do
                    sum = sum + i
                end
                print(string.format("DAP_BENCH %%.9f", os.clock() - started))
                assert(sum == %d)
            end)
            assert(coroutine.resume(active))
            assert(#held == %d)
        ''' % (coroutines, iterations, iterations * (iterations + 1) // 2, coroutines))
        client.initialize()
        client.attach()
        client.configured()
        client.event("terminated")
        if case.process.wait(timeout=10):
            raise RuntimeError(case.process.stderr.read())
        while True:
            line = case.lines.get(timeout=5)
            if line is None:
                raise RuntimeError("Missing benchmark result")
            if "DAP_BENCH " in line:
                return float(line.split("DAP_BENCH ", 1)[1])
    finally:
        case.tearDown()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--debuggee", required=True, type=pathlib.Path)
    parser.add_argument("--coroutines", type=int, nargs="+", default=[0, 500])
    parser.add_argument("--iterations", type=int, default=10000)
    parser.add_argument("--samples", type=int, default=5)
    args = parser.parse_args()
    if args.iterations < 1 or args.samples < 1 or any(n < 0 for n in args.coroutines):
        parser.error("iterations and samples must be positive; coroutine counts must be nonnegative")
    test_dap.DEBUGGEE = str(args.debuggee.resolve())
    for count in args.coroutines:
        samples = [measure(count, args.iterations) for _ in range(args.samples)]
        print("%d idle coroutines: median %.3f ms (%d iterations, %d samples)" %
              (count, statistics.median(samples) * 1000, args.iterations, args.samples))
