#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0. See https://www.defold.com/license

"""Black-box tests: a Python DAP client against the actual C++/Lua TCP server."""
import argparse
import json
import pathlib
import queue
import socket
import subprocess
import sys
import tempfile
import textwrap
import threading
import time
import unittest


DEBUGGEE = None


class Client:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.socket.settimeout(5)
        self.sequence = 0
        self.server_sequence = 0
        self.buffer = b""
        self.pending = []

    def close(self):
        self.socket.close()

    def frame(self, command, arguments=None):
        self.sequence += 1
        message = {"seq": self.sequence, "type": "request", "command": command}
        if arguments is not None:
            message["arguments"] = arguments
        data = json.dumps(message, ensure_ascii=False).encode("utf-8")
        frame = f"Content-Length: {len(data)}\r\n\r\n".encode("ascii") + data
        return self.sequence, frame

    def batch(self, *requests):
        frames = [self.frame(*request) for request in requests]
        self.socket.sendall(b"".join(frame for _, frame in frames))
        return [sequence for sequence, _ in frames]

    def send(self, command, arguments=None, fragment=False):
        sequence, frame = self.frame(command, arguments)
        if fragment:
            for start in range(0, len(frame), 3):
                self.socket.sendall(frame[start:start + 3])
                time.sleep(0.0002)
        else:
            self.socket.sendall(frame)
        return sequence

    def receive(self):
        while b"\r\n\r\n" not in self.buffer:
            self._read()
        header, body = self.buffer.split(b"\r\n\r\n", 1)
        fields = dict(line.split(b":", 1) for line in header.split(b"\r\n"))
        length = int(fields[b"Content-Length"])
        if not 0 < length <= 1024 * 1024:
            raise AssertionError(f"Invalid Content-Length: {length}")
        offset = len(header) + 4
        while len(self.buffer) < offset + length:
            self._read()
        result = json.loads(self.buffer[offset:offset + length])
        self.buffer = self.buffer[offset + length:]
        if result["seq"] <= self.server_sequence:
            raise AssertionError("Server seq must increase across responses and events")
        self.server_sequence = result["seq"]
        return result

    def _read(self):
        data = self.socket.recv(65536)
        if not data:
            raise EOFError("DAP connection closed")
        self.buffer += data

    def wait(self, predicate):
        for index, message in enumerate(self.pending):
            if predicate(message):
                return self.pending.pop(index)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            message = self.receive()
            if predicate(message):
                return message
            self.pending.append(message)
        raise TimeoutError(self.pending)

    def response(self, sequence, success=True):
        message = self.wait(lambda m: m["type"] == "response" and m["request_seq"] == sequence)
        if message["success"] != success:
            raise AssertionError(message)
        return message.get("body", {})

    def request(self, command, arguments=None, success=True):
        return self.response(self.send(command, arguments), success)

    def event(self, name):
        return self.wait(lambda m: m["type"] == "event" and m["event"] == name).get("body", {})

    def initialize(self, **arguments):
        return self.request("initialize", {"adapterID": "defold", "pathFormat": "path", **arguments})

    def attach(self, **arguments):
        self.attach_sequence = self.send("attach", arguments)
        self.event("initialized")

    def configured(self):
        self.request("configurationDone")
        self.response(self.attach_sequence)


class DAPTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="defold-dap-")
        self.process = None
        self.clients = []

    def tearDown(self):
        for client in self.clients:
            client.close()
        if self.process:
            if self.process.poll() is None:
                self.process.kill()
            self.process.wait(timeout=5)
            self.process.stdout.close()
            self.process.stderr.close()
        self.temp.cleanup()

    def start(self, source, second=None):
        self.source = textwrap.dedent(source).lstrip("\n")
        self.path = pathlib.Path(self.temp.name) / "main.lua"
        self.path.write_text(self.source, encoding="utf-8")
        paths = [str(self.path)]
        if second:
            self.second_path = pathlib.Path(self.temp.name) / "second.lua"
            self.second_path.write_text(textwrap.dedent(second).lstrip("\n"), encoding="utf-8")
            paths.append(str(self.second_path))
        self.process = subprocess.Popen([DEBUGGEE, *paths], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        lines = queue.Queue()

        def read_stdout():
            for line in self.process.stdout:
                lines.put(line)
            lines.put(None)

        self.lines = lines
        threading.Thread(target=read_stdout, daemon=True).start()
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            line = lines.get(timeout=10)
            if line is None:
                self.fail(f"Debuggee exited during startup: {self.process.stderr.read()}")
            if "Lua DAP debugger listening on 127.0.0.1:" in line:
                line = "PORT " + line.rsplit(":", 1)[1].strip()
            if line.startswith("PORT "):
                self.port = int(line.split()[1])
                return self.connect()
        self.fail("Debuggee did not start")

    def connect(self):
        client = Client(self.port)
        self.clients.append(client)
        self.client = client
        return client

    def line(self, marker):
        matches = [i for i, line in enumerate(self.source.splitlines(), 1) if f"-- {marker}" in line]
        self.assertEqual(len(matches), 1, (marker, self.source))
        return matches[0]

    def breakpoints(self, *breakpoints, path=None):
        return self.client.request("setBreakpoints", {"source": {"path": str(path or self.path)}, "breakpoints": list(breakpoints)})["breakpoints"]

    def stopped(self, reason="breakpoint"):
        event = self.client.event("stopped")
        self.assertEqual(event["reason"], reason)
        self.assertTrue(event["allThreadsStopped"])
        self.thread = event["threadId"]
        frames = self.client.request("stackTrace", {"threadId": self.thread})["stackFrames"]
        self.assertTrue(frames)
        self.frame = frames[0]["id"]
        return frames

    def scopes(self):
        scopes = self.client.request("scopes", {"frameId": self.frame})["scopes"]
        return {s["name"]: s["variablesReference"] for s in scopes}

    def variables(self, reference, **args):
        return self.client.request("variables", {"variablesReference": reference, **args})["variables"]

    def evaluate(self, expression, **args):
        return self.client.request("evaluate", {"expression": expression, "frameId": self.frame, **args})

    def resume(self, command="continue"):
        body = self.client.request(command, {"threadId": self.thread})
        self.assertTrue(self.client.event("continued")["allThreadsContinued"])
        return body

    def finished(self, expected=0):
        self.client.event("terminated")
        self.assertEqual(self.process.wait(timeout=8), 0, self.process.stderr.read())
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            line = self.lines.get(timeout=5)
            if line.startswith("RESULT "):
                self.assertEqual(int(line.split()[1]), expected)
                return
        self.fail("No completion status")

    # Exercises fragmented and coalesced requests, initialization/attach ordering,
    # capability reporting, zero-based positions, and rejection of unsupported
    # requests and arguments without losing the session.
    def test_initialization_framing_and_protocol_errors(self):
        c = self.start("local n = 1\nn = n + 1\n")
        c.request("threads", success=False)
        c.request("initialize", {"pathFormat": "uri"}, success=False)
        seq = c.send("initialize", {"adapterID": "déföld 🦊", "linesStartAt1": False, "columnsStartAt1": False}, fragment=True)
        capabilities = c.response(seq)
        self.assertTrue(capabilities["supportsConfigurationDoneRequest"])
        self.assertNotIn("supportsStepBack", capabilities)
        c.request("initialize", success=False)
        c.attach(stopOnEntry=True)
        c.request("attach", success=False)
        c.request("setExceptionBreakpoints", {"filters": ["all"]}, success=False)
        a, b = c.batch(("threads",), ("notARequest",))
        self.assertEqual(len(c.response(a)["threads"]), 1)
        c.response(b, False)
        c.configured()
        frames = self.stopped("entry")
        self.assertEqual(frames[0]["line"], 0)
        self.assertEqual(frames[0]["column"], 0)
        c.request("configurationDone", success=False)
        c.request("stepBack", {"threadId": self.thread}, success=False)
        c.request("next", {"threadId": self.thread, "granularity": "instruction"}, success=False)
        c.request("disconnect", {"terminateDebuggee": True}, success=False)
        self.resume()
        self.finished()

    # Checks scope contents, nil/Unicode values, typed and cyclic table entries,
    # variable filtering/paging, and evaluation errors. The resumed Lua program
    # verifies edits to locals, upvalues, globals, and table entries.
    def test_variables_evaluate_and_mutation(self):
        c = self.start('''
            g = 11
            local up = 7
            local function inner(arg)
                local x = 3
                local absent = nil
                local text = 'héllo 🦊 "quoted"'
                local t = {value=4, nested={flag=true}, [1]='one', [false]=9}
                t.self = t
                x = x + 1 -- inspect
                assert(x == 22 and up == 9 and g == 12 and t.value == 6 and absent == nil)
                assert(text == 'héllo 🦊 "quoted"')
                return x + arg + up
            end
            local result = inner(5)
            assert(result == 36)
        ''')
        c.initialize()
        c.attach()
        bps = self.breakpoints({"line": self.line("inspect")})
        self.assertFalse(bps[0]["verified"])
        c.configured()
        frames = self.stopped()
        self.assertEqual(frames[0]["line"], self.line("inspect"))
        self.assertEqual(frames[0]["source"]["path"], str(self.path))
        self.assertEqual(c.event("breakpoint")["breakpoint"]["id"], bps[0]["id"])
        scopes = self.scopes()
        self.assertEqual(set(scopes), {"Locals", "Upvalues", "Globals"})
        values = {v["name"]: v for v in self.variables(scopes["Locals"])}
        self.assertEqual(values["x"]["value"], "3")
        self.assertEqual(values["absent"]["value"], "nil")
        self.assertEqual(values["absent"]["variablesReference"], 0)
        self.assertIn("héllo 🦊", values["text"]["value"])
        self.assertEqual(self.evaluate("x + up + g + arg")["result"], "26")
        self.assertEqual(self.evaluate("absent == nil")["result"], "true")
        c.request("evaluate", {"frameId": self.frame, "expression": "error('bad evaluation')"}, success=False)
        c.request("evaluate", {"frameId": self.frame, "expression": "this isn't Lua"}, success=False)
        table_ref = values["t"]["variablesReference"]
        table = {v["name"]: v for v in self.variables(table_ref)}
        self.assertEqual(table['["self"]']["variablesReference"], table_ref)
        nested = self.variables(table['["nested"]']["variablesReference"])
        self.assertEqual(nested[0]["value"], "true")
        self.assertEqual(len(self.variables(table_ref, filter="indexed")), 1)
        self.assertEqual(len(self.variables(table_ref, filter="named")), 4)
        self.assertEqual(len(self.variables(scopes["Locals"], start=1, count=2)), 2)
        for reference, name, value in [(scopes["Locals"], "x", "20"), (scopes["Upvalues"], "up", "9"),
                                       (scopes["Globals"], "g", "12"), (table_ref, '["value"]', "6")]:
            result = c.request("setVariable", {"variablesReference": reference, "name": name, "value": value})
            self.assertEqual(result["value"], value)
        self.evaluate("x = x + 1", context="repl")
        self.assertEqual(self.evaluate("x")["result"], "21")
        self.assertEqual(self.evaluate("{answer = x + up}")["type"], "table")
        c.request("setVariable", {"variablesReference": table_ref, "name": "missing", "value": "1"}, success=False)
        c.request("setVariable", {"variablesReference": scopes["Locals"], "name": "x", "value": "!"}, success=False)
        self.resume()
        self.finished()

    # Checks the exact source lines reached by step-in, step-over, and step-out
    # through nested calls, plus stack paging and the total frame count.
    def test_step_in_over_out_and_stack_paging(self):
        c = self.start('''
            local function leaf(x)
                local y = x + 1 -- leaf
                return y
            end
            local function inner(x)
                local y = x * 2 -- inner
                y = leaf(y) -- call_leaf
                return y -- return_inner
            end
            local result = inner(3) -- call_inner
            assert(result == 7) -- after_inner
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("call_inner")})
        c.configured()
        self.stopped()
        self.resume("stepIn")
        frames = self.stopped("step")
        self.assertEqual(frames[0]["line"], self.line("inner"))
        self.assertEqual(len(frames), 2)
        page = c.request("stackTrace", {"threadId": self.thread, "startFrame": 1, "levels": 1})
        self.assertEqual(len(page["stackFrames"]), 1)
        self.assertEqual(page["totalFrames"], 2)
        self.resume("next")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("call_leaf"))
        self.resume("next")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("return_inner"))
        self.resume("stepOut")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("after_inner"))
        self.resume()
        self.finished()

    # Checks that stepping over Lua, recursive, and native tail calls completes
    # the invocation and stops on the caller's next source line.
    def test_step_over_tail_calls(self):
        c = self.start('''
            local function leaf()
                local value = 3
                return value
            end
            local function tail()
                return leaf() -- tail
            end
            local function recursive(n)
                if n == 0 then return 7 end
                return recursive(n - 1) -- recursive
            end
            local function native()
                return math.abs(-9) -- native
            end
            local value = tail()
            assert(value == 3) -- after_tail
            local recursive_value = recursive(3)
            assert(recursive_value == 7) -- after_recursive
            local native_value = native()
            assert(native_value == 9) -- after_native
        ''')
        c.initialize()
        c.attach()
        remaining = [{"line": self.line(marker)} for marker in ("tail", "recursive", "native")]
        self.breakpoints(*remaining)
        c.configured()
        for marker in ("tail", "recursive", "native"):
            self.assertEqual(self.stopped()[0]["line"], self.line(marker))
            remaining.pop(0)
            self.breakpoints(*remaining)
            self.resume("next")
            self.assertEqual(self.stopped("step")[0]["line"], self.line("after_" + marker))
            self.resume()
        self.finished()

    # Checks that step-in enters a tail-called function and step-out returns to
    # the source line following the original caller's invocation.
    def test_step_in_and_out_of_tail_call(self):
        c = self.start('''
            local function leaf()
                local value = 3 -- leaf
                return value
            end
            local function tail()
                return leaf() -- tail
            end
            local value = tail()
            assert(value == 3) -- after
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("tail")})
        c.configured()
        self.stopped()
        self.resume("stepIn")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("leaf"))
        self.resume("stepOut")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("after"))
        self.resume()
        self.finished()

    # Checks repeated breakpoint hits on real iterations of single-line loops
    # containing arithmetic, native calls, or Lua calls. Local values distinguish
    # successive iterations from duplicate hooks at the same source line.
    def test_breakpoints_repeat_on_one_line_loops(self):
        c = self.start('''
            local function add(a, b) return a + b end
            local n = 0
            for i = 1, 3 do n = n + i end -- plain
            assert(n == 6)
            n = 0
            for i = 1, 3 do n = math.abs(n) + i end -- native
            assert(n == 6)
            n = 0
            for i = 1, 3 do n = add(n, i) end -- lua
            assert(n == 6)
        ''')
        c.initialize()
        c.attach()
        remaining = [self.line(marker) for marker in ("plain", "native", "lua")]
        # Lua 5.1 also reports the initial loop setup. Start after the first
        # iteration so both runtimes stop at the same execution state.
        self.breakpoints(*({"line": line, "condition": "n > 0"} for line in remaining))
        c.configured()
        while remaining:
            for value in (1, 3):
                self.assertEqual(self.stopped()[0]["line"], remaining[0])
                self.assertEqual(self.evaluate("n")["result"], str(value))
                if value == 3:
                    remaining.pop(0)
                    self.breakpoints(*({"line": line, "condition": "n > 0"} for line in remaining))
                self.resume()
        self.finished()

    # Checks that step-over revisits the same source line for the next loop
    # iteration, with the accumulated value advancing from 1 to 3.
    def test_step_over_revisits_one_line_loop(self):
        c = self.start("local n = 0\nfor i = 1, 3 do n = n + i end -- loop\nassert(n == 6)\n")
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("loop"), "condition": "n == 1"})
        c.configured()
        self.stopped()
        self.assertEqual(self.evaluate("n")["result"], "1")
        self.breakpoints()
        self.resume("next")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("loop"))
        self.assertEqual(self.evaluate("n")["result"], "3")
        self.resume()
        self.finished()

    # Checks that table references evaluate assignments in their originating
    # frame when caller and callee have different locals named x. Covers nested
    # and cyclic tables and references returned by evaluate and setVariable.
    def test_table_assignments_use_the_originating_frame(self):
        c = self.start('''
            local function inner(t)
                local x = 99
                local stop = 1 -- inspect
                assert(t.value == 42 and t.nested.value == 42)
                assert(t.replacement.value == 42 and t.evaluated.value == 42)
                return x + stop
            end
            local x = 42
            local t = {value=0, nested={value=0}, replacement={value=0}, evaluated={value=0}}
            t.self = t
            assert(inner(t) == 100)
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("inspect")})
        c.configured()
        frames = self.stopped()
        callee = {v["name"]: v for v in self.variables(self.scopes()["Locals"])}
        self.frame = frames[1]["id"]
        caller = {v["name"]: v for v in self.variables(self.scopes()["Locals"])}
        table_ref = caller["t"]["variablesReference"]
        table = {v["name"]: v for v in self.variables(table_ref)}
        self.assertEqual(table['["self"]']["variablesReference"], table_ref)
        self.assertEqual(self.evaluate("t")["variablesReference"], table_ref)
        changed = c.request("setVariable", {"variablesReference": callee["t"]["variablesReference"],
                                            "name": '["value"]', "value": "x"})
        self.assertEqual(changed["value"], "99")
        replacement = c.request("setVariable", {"variablesReference": table_ref, "name": '["replacement"]',
                                                "value": "{value=0}"})
        references = [table_ref, table['["nested"]']["variablesReference"],
                      replacement["variablesReference"], self.evaluate("t.evaluated")["variablesReference"]]
        for reference in references:
            changed = c.request("setVariable", {"variablesReference": reference, "name": '["value"]', "value": "x"})
            self.assertEqual(changed["value"], "42")
        self.resume()
        self.finished()

    # Checks stopping on the second hit and on a true Lua condition, plus one
    # log message per iteration with expression interpolation and escaped braces.
    # The final sum confirms that debugger operations preserve loop execution.
    def test_conditional_hit_breakpoints_and_logpoints(self):
        c = self.start('''
            local sum = 0
            for i = 1, 5 do
                sum = sum + i -- conditional
                sum = sum + 0 -- hit
                sum = sum + 0 -- log
            end
            assert(sum == 15)
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("conditional"), "condition": "i == 3"},
                         {"line": self.line("hit"), "hitCondition": "2"},
                         {"line": self.line("log"), "logMessage": "i={i} sum={sum} {{done}}"})
        c.configured()
        frames = self.stopped()
        self.assertEqual(frames[0]["line"], self.line("hit"))
        self.assertEqual(self.evaluate("i")["result"], "2")
        self.resume()
        frames = self.stopped()
        self.assertEqual(frames[0]["line"], self.line("conditional"))
        self.assertEqual(self.evaluate("i")["result"], "3")
        self.resume()
        outputs = [c.event("output")["output"] for _ in range(5)]
        self.assertEqual(outputs[0], "i=1 sum=1 {done}\n")
        self.assertEqual(outputs[-1], "i=5 sum=15 {done}\n")
        self.finished()

    # Checks replacement/removal of a source's breakpoints and verification once
    # its lines are known. Invalid breakpoint data, IDs, stack ranges, and a pause
    # request while already stopped must return unsuccessful responses.
    def test_breakpoint_replacement_and_invalid_arguments(self):
        c = self.start('''
            local n = 0 -- first
            n = n + 1 -- removed
            n = n + 2 -- last
            assert(n == 3)
        ''')
        c.initialize()
        c.attach(stopOnEntry=True)
        c.request("setBreakpoints", {"source": {"path": str(self.path)}, "breakpoints": "bad"}, success=False)
        c.request("setBreakpoints", {"source": {"path": str(self.path)}, "breakpoints": [{"line": -1}]}, success=False)
        bad = self.breakpoints({"line": 1, "hitCondition": "x > 4"})
        self.assertFalse(bad[0]["verified"])
        self.breakpoints({"line": self.line("removed")})
        c.configured()
        self.stopped("entry")
        changed = self.breakpoints({"line": self.line("last")})
        self.assertTrue(changed[0]["verified"])
        c.request("scopes", {"frameId": 999999}, success=False)
        c.request("variables", {"variablesReference": 999999}, success=False)
        c.request("stackTrace", {"threadId": -1}, success=False)
        c.request("stackTrace", {"threadId": self.thread, "startFrame": -1}, success=False)
        c.request("pause", {"threadId": self.thread}, success=False)
        self.resume()
        self.assertEqual(self.stopped()[0]["line"], self.line("last"))
        self.assertEqual(self.breakpoints(), [])
        self.resume()
        self.finished()

    # Checks that native calls, nested Lua calls, and multiple calls on one line
    # emit exactly one log message per source-line visit across loop iterations.
    def test_logpoints_ignore_call_return_events(self):
        c = self.start('''
            local function identity(n)
                n = math.abs(n) -- nested
                return n
            end
            local n = -1
            for i = 1, 3 do
                n = math.abs(n) -- native
                n = identity(n) -- lua
                n = math.abs(n) + math.abs(0) -- multiple
            end
            assert(n == 1)
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("native"), "logMessage": "native {i}"},
                         {"line": self.line("lua"), "logMessage": "lua {i}"},
                         {"line": self.line("nested"), "logMessage": "nested {n}"},
                         {"line": self.line("multiple"), "logMessage": "multiple {i}"})
        c.configured()
        self.finished()
        outputs = [event["body"]["output"] for event in c.pending if event.get("event") == "output"]
        expected = [output for i in range(1, 4)
                    for output in (f"native {i}\n", f"lua {i}\n", "nested 1\n", f"multiple {i}\n")]
        self.assertEqual(outputs, expected)

    # Checks that call-return hooks neither advance hit counts nor reevaluate
    # false conditions: second-hit breakpoints stop in iteration 2, and the
    # condition executes exactly once in each of the three iterations.
    def test_breakpoint_hits_ignore_call_return_events(self):
        c = self.start('''
            local function identity(n)
                return n
            end
            checks = 0
            local n = -1
            for i = 1, 3 do
                n = math.abs(n) -- native
                n = identity(n) -- lua
                n = math.abs(n) + math.abs(0) -- multiple
                n = math.abs(n) -- conditional
            end
            assert(n == 1 and checks == 3)
        ''')
        c.initialize()
        c.attach()
        markers = ("native", "lua", "multiple")
        self.breakpoints(*({"line": self.line(marker), "hitCondition": "2"} for marker in markers),
                         {"line": self.line("conditional"),
                          "condition": "(function() checks = checks + 1; return false end)()"})
        c.configured()
        for marker in markers:
            self.assertEqual(self.stopped()[0]["line"], self.line(marker))
            self.assertEqual(self.evaluate("i")["result"], "2")
            self.resume()
        self.finished()

    # Checks that continuing with a suspended coroutine selected resumes the
    # stopped main thread to the next breakpoint without stopping again on the
    # native call's return hook.
    def test_continue_other_thread_ignores_call_return_event(self):
        c = self.start('''
            local co = coroutine.create(function()
                local n = 9
                coroutine.yield(n)
                return n
            end)
            assert(coroutine.resume(co))
            local n = 1
            n = math.abs(n) -- stop
            assert(n == 1) -- after
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("stop")}, {"line": self.line("after")})
        c.configured()
        self.stopped()
        other = next(t["id"] for t in c.request("threads")["threads"] if t["id"] != self.thread)
        self.assertTrue(c.request("continue", {"threadId": other})["allThreadsContinued"])
        c.event("continued")
        self.assertEqual(self.stopped()[0]["line"], self.line("after"))
        self.resume()
        self.finished()

    # Checks pause/continue and disconnect/reconnect, including rejection of
    # frame and variable IDs from an earlier stop and successful evaluation
    # after attaching a new client to the same running program.
    def test_pause_stale_references_disconnect_and_reconnect(self):
        c = self.start('''
            finish = false
            local n = 0
            while not finish do
                n = n + 1
                pump()
            end
            assert(n > 0)
        ''')
        c.initialize()
        c.attach()
        c.configured()
        thread = c.request("threads")["threads"][0]["id"]
        c.request("scopes", {"frameId": 1}, success=False)
        c.request("pause", {"threadId": thread})
        self.stopped("pause")
        old_frame = self.frame
        old_ref = self.scopes()["Locals"]
        self.resume()
        c.request("pause", {"threadId": thread})
        self.stopped("pause")
        self.assertNotEqual(self.frame, old_frame)
        c.request("scopes", {"frameId": old_frame}, success=False)
        c.request("variables", {"variablesReference": old_ref}, success=False)
        c.request("disconnect")
        c.event("terminated")
        c.close()
        c = self.connect()
        c.initialize()
        c.attach(stopOnEntry=True)
        c.configured()
        self.stopped("entry")
        self.evaluate("finish = true", context="repl")
        self.resume()
        self.finished()

    # Checks that closing the TCP connection while Lua is stopped releases the
    # pause and lets the debuggee process exit without another client request.
    def test_abrupt_disconnect_resumes_lua(self):
        c = self.start('''
            local n = 1 -- stop
            assert(n == 1)
        ''')
        c.initialize()
        c.attach(stopOnEntry=True)
        c.configured()
        self.stopped("entry")
        c.close()
        self.assertEqual(self.process.wait(timeout=5), 0, self.process.stderr.read())

    # Checks that caught pcall errors are ignored and an uncaught error stops
    # before unwinding, with its message and live local available for inspection.
    # Continuing then propagates the original Lua error to the host.
    def test_uncaught_error_stack_and_exception_info(self):
        c = self.start('''
            local caught = pcall(function() error('caught') end)
            assert(not caught)
            local function fail()
                local detail = 42
                error('uncaught failure')
                return detail
            end
            fail()
        ''')
        c.initialize()
        c.attach()
        c.request("setExceptionBreakpoints", {"filters": ["uncaught"]})
        c.configured()
        self.stopped("exception")
        info = c.request("exceptionInfo", {"threadId": self.thread})
        self.assertEqual(info["breakMode"], "unhandled")
        self.assertIn("uncaught failure", info["description"])
        self.assertEqual(self.evaluate("detail")["result"], "42")
        c.request("setExceptionBreakpoints", {"filters": []})
        self.resume()
        self.finished(expected=2)

    # Checks distinct thread identities for independent Lua states, breakpoint
    # hits and frame evaluation in created/wrapped coroutines and the second
    # state, and preservation of coroutine yield/return results.
    def test_coroutines_and_multiple_lua_states(self):
        c = self.start('''
            local function worker(value)
                local result = value + 1 -- worker
                coroutine.yield(result)
                return result + 1
            end
            local co = coroutine.create(worker)
            local ok, value = coroutine.resume(co, 10)
            assert(ok and value == 11)
            ok, value = coroutine.resume(co)
            assert(ok and value == 12)
            local wrapped = coroutine.wrap(worker)
            assert(wrapped(20) == 21)
            assert(wrapped() == 22)
            assert(not pcall(wrapped))
        ''', second="local second = 5\nassert(second == 5)\n")
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("worker")})
        self.breakpoints({"line": 2}, path=self.second_path)
        c.configured()
        self.stopped()
        threads = [t for t in c.request("threads")["threads"] if "/ coroutine" not in t["name"]]
        self.assertEqual(len(threads), 2)
        self.assertNotEqual(threads[0]["name"], threads[1]["name"])
        self.assertNotEqual(self.thread, threads[0]["id"])
        self.assertEqual(self.evaluate("value")["result"], "10")
        self.resume()
        self.stopped()
        self.assertEqual(self.evaluate("value")["result"], "20")
        self.resume()
        self.stopped()
        self.assertEqual(self.thread, threads[1]["id"])
        self.assertEqual(self.evaluate("second")["result"], "5")
        self.resume()
        self.finished()

    # Checks breakpoint matching with localRoot and zero-based line numbering,
    # including the source path and line returned in the stopped stack frame.
    def test_local_root_mapping_and_zero_based_breakpoints(self):
        c = self.start("local value = 1\nvalue = value + 1 -- stop\nassert(value == 2)\n")
        c.initialize(linesStartAt1=False)
        # Runtime uses an absolute filename here; client supplies a local root.
        c.attach(localRoot=self.temp.name)
        self.breakpoints({"line": self.line("stop") - 1})
        c.configured()
        frame = self.stopped()[0]
        self.assertEqual(frame["line"], self.line("stop") - 1)
        self.assertEqual(frame["source"]["path"], str(self.path))
        self.resume()
        self.finished()

    # Checks drive-letter normalization for relative and absolute Lua sources,
    # mixed path separators, and differently cased client/root drives. Returned
    # source paths must preserve the casing of the remaining path components.
    def test_windows_drive_case_in_source_mapping(self):
        c = self.start('''
            local source = "local n = 1\\nn = n + 1\\nassert(n == 2)\\n"
            local paths = {"@main/Relative.script", "@C:/Projects/Game/main/Upper.script",
                           "@c:/Projects/Game/main/lower.script"}
            for _, path in ipairs(paths) do
                local fn = assert(loadstring(source, path))
                fn()
            end
        ''')
        c.initialize()
        c.attach(localRoot="C:\\Projects\\Game\\")
        paths = ("c:\\Projects\\Game\\main\\Relative.script",
                 "c:/Projects/Game/main/Upper.script", "C:/Projects/Game/main/lower.script")
        for path in paths:
            self.breakpoints({"line": 2}, path=path)
        c.configured()
        for path in paths:
            frame = self.stopped()[0]
            self.assertEqual(frame["line"], 2)
            self.assertEqual(frame["source"]["path"], "c" + path[1:].replace("\\", "/"))
            self.assertEqual(self.evaluate("n")["result"], "1")
            self.resume()
        self.finished()

    # Checks that nil locals shadow globals in a custom function environment,
    # REPL assignments update that environment, and an evaluated closure retains
    # a snapshot of the frame's locals after the program resumes.
    def test_nil_shadowing_frame_environments_and_escaping_closures(self):
        c = self.start('''
            local function inner()
                local shadow = nil
                local x = 4
                x = x + 1 -- inspect
                assert(g == 9 and shadow == nil and saved() == 4)
            end
            setfenv(inner, {g=7, shadow=99, assert=assert})
            inner()
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("inspect")})
        c.configured()
        self.stopped()
        self.assertEqual(self.evaluate("shadow")["result"], "nil")
        self.assertEqual(self.evaluate("g")["result"], "7")
        self.evaluate("g = 9; saved = function() return x end", context="repl")
        self.assertEqual(self.evaluate("saved()")["result"], "4")
        self.resume()
        self.finished()

    # Checks step-in through coroutine.resume, step-out through yield back to the
    # resumer, and an exit event plus removal from threads when the coroutine ends.
    def test_coroutine_stepping_and_exit_events(self):
        c = self.start('''
            local co = coroutine.create(function()
                local n = 1 -- worker
                coroutine.yield(n)
                return n + 1
            end)
            local ok, value = coroutine.resume(co) -- call
            assert(ok and value == 1) -- yielded
            ok, value = coroutine.resume(co)
            assert(ok and value == 2) -- completed
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("call")}, {"line": self.line("completed")})
        c.configured()
        self.stopped()
        main = self.thread
        self.resume("stepIn")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("worker"))
        coroutine = self.thread
        self.assertNotEqual(main, coroutine)
        self.resume("stepOut")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("yielded"))
        self.assertEqual(self.thread, main)
        self.resume()
        self.stopped()
        exit_event = c.wait(lambda m: m.get("event") == "thread" and m["body"]["reason"] == "exited")
        self.assertEqual(exit_event["body"]["threadId"], coroutine)
        self.assertEqual([t["id"] for t in c.request("threads")["threads"]], [main])
        self.resume()
        self.finished()

    # Checks next, stepIn, and stepOut on a coroutine's final return: execution
    # stops on the resumer's next line and reports the completed thread's exit.
    # Exercises both coroutine.resume and coroutine.wrap.
    def test_steps_return_from_completed_coroutines(self):
        c = self.start('''
            local function worker()
                local value = 3
                return value -- worker
            end
            for i = 1, 3 do
                local co = coroutine.create(worker)
                local ok, value = coroutine.resume(co)
                assert(ok and value == 3) -- after_resume
            end
            for i = 1, 3 do
                local wrapped = coroutine.wrap(worker)
                local value = wrapped()
                assert(value == 3) -- after_wrap
            end
        ''')
        c.initialize()
        c.attach()
        main = c.request("threads")["threads"][0]["id"]
        self.breakpoints({"line": self.line("worker")})
        c.configured()
        for marker in ("after_resume", "after_wrap"):
            for command in ("next", "stepIn", "stepOut"):
                with self.subTest(marker=marker, command=command):
                    self.assertEqual(self.stopped()[0]["line"], self.line("worker"))
                    coroutine = self.thread
                    self.resume(command)
                    self.assertEqual(self.stopped("step")[0]["line"], self.line(marker))
                    self.assertEqual(self.thread, main)
                    c.wait(lambda m: m.get("event") == "thread" and
                           m["body"]["reason"] == "exited" and m["body"]["threadId"] == coroutine)
                    self.resume()
        self.finished()

    # Checks wrapped-coroutine arguments and yield/return values, including nils,
    # across debugger detach, plus dead-coroutine errors and preservation of the
    # identity of a non-string error object.
    def test_coroutine_wrap_results_and_errors_after_detach(self):
        c = self.start('''
            local function pack(...) return {n=select('#', ...), ...} end
            local wrapped = coroutine.wrap(function(a, b)
                assert(a == 5 and b == nil)
                local x, y = coroutine.yield(nil, 'yield', nil)
                assert(x == 7 and y == nil)
                return nil, 9, nil
            end)
            local values = pack(wrapped(5, nil))
            assert(values.n == 3 and values[1] == nil and values[2] == 'yield' and values[3] == nil) -- detach
            values = pack(wrapped(7, nil))
            assert(values.n == 3 and values[1] == nil and values[2] == 9 and values[3] == nil)
            local ok, message = pcall(wrapped)
            assert(not ok and type(message) == 'string')
            local original = {message='wrapped failure'}
            ok, message = pcall(coroutine.wrap(function() error(original) end))
            assert(not ok and message == original)
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("detach")})
        c.configured()
        self.stopped()
        c.request("disconnect")
        self.finished()

    # Checks inspection and mutation of a yielded coroutine's bindings and tables
    # while preserving its suspended status, frames, thread list, and resume value.
    # Also checks evaluation errors and escaped closures surviving garbage collection.
    def test_evaluate_and_mutate_a_yielded_coroutine(self):
        c = self.start('''
            local up = 7
            g = 5
            local co = coroutine.create(function(arg)
                local x = 42
                local t = {value=1}
                local resumed = coroutine.yield(arg + up)
                assert(resumed == 'resume-value')
                assert(x == 43 and t.value == 44 and up == 8 and g == 9)
                assert(saved() == 43)
                return x + t.value + up + g
            end)
            local ok, value = coroutine.resume(co, 10)
            assert(ok and value == 17)
            local stop = 1 -- inspect
            ok, value = coroutine.resume(co, 'resume-value')
            assert(ok and value == 104)
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("inspect")})
        c.configured()
        self.stopped()
        main_frame = self.frame
        threads = c.request("threads")["threads"]
        coroutine = next(t for t in threads if "/ coroutine" in t["name"])
        frames = c.request("stackTrace", {"threadId": coroutine["id"]})["stackFrames"]
        self.frame = frames[0]["id"]
        locals_ref = self.scopes()["Locals"]
        self.assertEqual(self.evaluate("x")["result"], "42")
        self.assertEqual(self.evaluate("x + arg + up + g")["result"], "64")
        for expression in ("this isn't Lua", "error('evaluation failed')"):
            c.request("evaluate", {"frameId": self.frame, "expression": expression}, success=False)
        self.evaluate("x = x + 1; up = up + 1; g = 9; saved = function() return x end", context="repl")
        values = {v["name"]: v for v in self.variables(locals_ref)}
        self.assertEqual(values["x"]["value"], "43")
        changed = c.request("setVariable", {"variablesReference": values["t"]["variablesReference"],
                                            "name": '["value"]', "value": "x + 1"})
        self.assertEqual(changed["value"], "44")
        self.evaluate("collectgarbage('collect')")
        self.assertEqual(self.evaluate("saved()")["result"], "43")
        self.assertEqual(c.request("stackTrace", {"threadId": coroutine["id"]})["stackFrames"], frames)
        self.assertEqual(c.request("threads")["threads"], threads)
        self.frame = main_frame
        self.assertEqual(self.evaluate("coroutine.status(co)")["result"], '"suspended"')
        self.resume()
        self.finished()

    # Checks evaluation of a selected caller's local and step-over through
    # recursive calls, stopping at the return line with the completed result.
    def test_recursive_step_over_and_caller_evaluation(self):
        c = self.start('''
            local function factorial(n)
                if n == 0 then return 1 end
                local value = factorial(n - 1) -- recurse
                return n * value -- return
            end
            local caller_value = 123
            local result = factorial(3)
            assert(result == 6 and caller_value == 123)
        ''')
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("recurse"), "condition": "n == 3"})
        c.configured()
        frames = self.stopped()
        caller = c.request("evaluate", {"frameId": frames[1]["id"], "expression": "caller_value"})
        self.assertEqual(caller["result"], "123")
        self.resume("next")
        self.assertEqual(self.stopped("step")[0]["line"], self.line("return"))
        self.assertEqual(self.evaluate("value")["result"], "2")
        self.resume()
        self.finished()

    # Checks large string lengths and escaping of non-UTF-8/NUL bytes, then
    # evaluates a debugger pump and garbage collection without losing access
    # to the stopped frame's original local value.
    def test_large_binary_values_and_reentrant_update(self):
        c = self.start("local x = 1\nx = x + 1 -- inspect\nassert(x == 2)\n")
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("inspect")})
        c.configured()
        self.stopped()
        self.assertEqual(len(self.evaluate("string.rep('x', 50000)")["result"]), 50002)
        binary = self.evaluate("string.char(255, 0, 10)")["result"]
        self.assertIn("\\u00ff", binary)
        self.assertIn("\\u0000", binary)
        self.assertEqual(self.evaluate("pump()")["result"], "nil")
        self.assertEqual(self.evaluate("collectgarbage('collect')")["result"], "0")
        self.assertEqual(self.evaluate("x")["result"], "1")
        self.resume()
        self.finished()

    # Checks that failed logpoint and condition expressions emit diagnostics
    # while Lua continues and completes its expected variable updates.
    def test_logpoint_and_condition_errors_preserve_execution(self):
        c = self.start("local x = 1\nx = x + 1 -- log\nx = x + 1 -- condition\nassert(x == 3)\n")
        c.initialize()
        c.attach()
        self.breakpoints({"line": self.line("log"), "logMessage": "value={missing()}"},
                         {"line": self.line("condition"), "condition": "missing()"})
        c.configured()
        self.assertIn("missing", c.event("output")["output"])
        self.assertIn("missing", c.event("output")["output"])
        self.finished()

    # Checks that invalid or duplicate length headers, malformed JSON, and invalid
    # UTF-8 close the connection. Each failure permits reconnecting, and a final
    # valid session can still pause, modify, and finish the original Lua program.
    def test_malformed_json_and_headers(self):
        bad_messages = [b"Content-Length: -1\r\n\r\n", b"Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}",
                        b"Content-Length: 4\r\n\r\n{bad", b"Content-Length: 2\r\n\r\n\xff\xff"]
        c = self.start("finish=false\nwhile not finish do pump() end\n")
        c.initialize()
        c.attach()
        c.configured()
        for message in bad_messages:
            c.socket.sendall(message)
            with self.assertRaises((EOFError, ConnectionResetError)):
                c.receive()
            c.close()
            c = self.connect()
        c.initialize()
        c.attach(stopOnEntry=True)
        c.configured()
        self.stopped("entry")
        self.evaluate("finish=true", context="repl")
        self.resume()
        self.finished()

    # Checks rejection of a non-string logMessage, a newline from an empty
    # logpoint, and interpolation of a table expression containing a quoted brace.
    def test_empty_logpoints_and_nested_expressions(self):
        c = self.start("local x = 1\nx = x + 1 -- empty\nx = x + 1 -- nested\nassert(x == 3)\n")
        c.initialize()
        c.attach()
        c.request("setBreakpoints", {"source": {"path": str(self.path)},
                                     "breakpoints": [{"line": 1, "logMessage": 42}]}, success=False)
        self.breakpoints({"line": self.line("empty"), "logMessage": ""},
                         {"line": self.line("nested"), "logMessage": "nested={({x='}'}).x}"})
        c.configured()
        self.assertEqual(c.event("output")["output"], "\n")
        self.assertEqual(c.event("output")["output"], "nested=}\n")
        self.finished()

    # Checks that an oversized Content-Length closes an uninitialized session
    # and releases the host waiting for a client so its Lua script can run.
    def test_malformed_transport_closes_session(self):
        c = self.start("local value = 1\nassert(value == 1)\n")
        c.socket.sendall(b"Content-Length: 99999999999999999999\r\n\r\n")
        with self.assertRaises((EOFError, ConnectionResetError)):
            c.receive()
        self.assertEqual(self.process.wait(timeout=5), 0, self.process.stderr.read())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--debuggee", required=True)
    args, remaining = parser.parse_known_args()
    DEBUGGEE = str(pathlib.Path(args.debuggee).resolve())
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
