# Native Lua DAP debugger

`debugger` is a separate C++ library for debugging Lua 5.1 and LuaJIT through the
[Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/overview).
Native debug and headless engines link the `LuaDebugger` extension. Release
engines exclude the library and its registration symbol at link time, through
the CMake targets and the Extender release manifest.

## Connecting

Start a built project with a native debug engine and these arguments:

```sh
dmengine --config=debugger.enabled=1 --config=debugger.port=8172 --config=debugger.wait=1
```

When launching from the Defold editor, put each `--config` argument on its own
line in **Preferences > General > Engine Arguments**, then use **Project > Build**.
The editor's Debug action starts MobDebug on the same default port.

The debugger listens on `debugger.address` (`127.0.0.1` by default). It is disabled
by default. The default port is 8172; port 0 selects an available port and prints
it in the engine log.

`debugger.wait=1` waits for a client to finish configuration before running startup
scripts. Without that setting the engine starts immediately and can be attached
to later. A disconnect while waiting releases the engine.

To enable DAP in a native debug engine that was started without
`debugger.enabled=1`, execute this Lua code in the running project:

```lua
local port = debugger.start()
```

The code can be sent through the engine's existing `run_script` service, the same
mechanism the editor uses to start MobDebug when attaching. No restart or startup
flag is required. `debugger.start([port [, address]])` returns the listening port
immediately; it does not wait for a client or pause the project. An omitted port uses
`debugger.port` (8172 by default); pass 0 to select an available port. An omitted
address uses `debugger.address`; pass `nil` as the port to override only the
address. Repeated calls return the existing listener's port without changing its
address or port. Invalid arguments or a bind failure raise a Lua error and allow
retrying with another address or port.
If the configured listener fails during engine startup, the engine logs the
reason and continues; `debugger.start(0)` can retry on an available port.

For a direct connection from an editor on another device, bind to `0.0.0.0`
(all IPv4 interfaces) or a specific IPv4 interface address:

```sh
dmengine --config=debugger.enabled=1 --config=debugger.address=0.0.0.0 --config=debugger.port=8172
```

The same setting applies to late activation. To enable remote attachment without
changing startup configuration, send `debugger.start(8172, "0.0.0.0")` through
the existing `run_script` service. The client connects to the device's actual IP
address and listening port. `0.0.0.0` is only the bind address. The engine log
reports the selected bind address and actual port.

The debugger can evaluate Lua and has no authentication. Enable a network
listener only on a trusted development network.

Runtime activation registers all live script contexts and discovers existing
coroutines through reachable Lua references, including frame locals and function
upvalues. Already suspended coroutines can be inspected without resuming them.
Each attachment repeats discovery, including after a disconnect, to find
coroutines created through cached original coroutine functions while detached.
Until activation, the extension leaves Lua hooks, coroutine functions, and JIT
settings alone and opens no DAP socket. The `debugger` Lua module is available
only in native debug and headless engines.

A DAP client connects directly over TCP using UTF-8 JSON and `Content-Length`
framing. Send `initialize`, then `attach`, configure breakpoints after the
`initialized` event, and send `configurationDone`. The `attach` response follows
the configuration response.

The attach arguments are:

```json
{
  "localRoot": "/absolute/path/to/the/game/project",
  "stopOnEntry": true,
  "evaluationTimeout": 1000
}
```

`localRoot` maps client files to Defold's project-relative Lua source names. It is
optional when client and runtime use the same paths. Native path strings are
supported, including Windows separators. URI paths are rejected. Client line and
column bases are negotiated by `initialize`.

`evaluationTimeout` limits Lua evaluation to 1000 milliseconds by default. It
accepts an integer from 1 to 60000 and applies to evaluations, assignments,
breakpoint conditions, and logpoint expressions. A timeout reports an error and
keeps the session usable; side effects performed before the timeout remain.
Closing the client releases the paused engine after the evaluation times out.
The limit is checked at Lua instructions, so it cannot interrupt a blocking
native function or code that explicitly disables debug hooks.

This module provides the DAP server. The editor's existing MobDebug client is
unchanged; connect with a client that supports DAP TCP servers.

### VS Code

Install [Lua Debug](https://marketplace.visualstudio.com/items?itemName=actboy168.lua-debug)
to register a Lua debug configuration in VS Code. Open the folder containing
`game.project` and add `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Attach to Defold Lua",
      "type": "lua",
      "request": "attach",
      "address": "127.0.0.1:8172",
      "debugServer": 8172,
      "localRoot": "${workspaceFolder}",
      "stopOnEntry": false
    }
  ]
}
```

Run the project with the listener enabled as above, then select **Attach to
Defold Lua** in Run and Debug. `debugServer` connects VS Code directly to
Defold's DAP listener; `address` satisfies Lua Debug's attach configuration.
For a remote engine, forward its DAP port to localhost before attaching from
VS Code, or use another DAP client that connects to the remote host directly.

## Supported requests

| Requests | Behavior |
| --- | --- |
| `initialize`, `attach`, `configurationDone`, `disconnect` | Attach to a running engine, optionally stop on entry, detach and resume, reconnect. |
| `setBreakpoints` | Replace all breakpoints for one source; ordinary, conditional, hit-count, and log breakpoints. |
| `breakpointLocations` | Return sorted, known executable lines within a source range. |
| `setExceptionBreakpoints`, `exceptionInfo` | `uncaught` errors crossing `dmScript::PCall`, inspected before unwinding. An empty filter list disables exception stops. |
| `threads` | Independent Lua contexts and live coroutines, with stable IDs and start/exit events. |
| `pause`, `continue`, `next`, `stepIn`, `stepOut` | Pause all Lua execution; continue and step by source line. Stepping follows coroutine entry and return to its resumer. |
| `stackTrace`, `scopes`, `variables` | Lua frames, locals, upvalues, function globals, nested tables, and script-instance `self` fields. Stack and variable paging, named/indexed filters, and cyclic values are supported. |
| `evaluate`, `setVariable`, `setExpression` | Evaluate in the selected frame or global scope and edit variables or assign to Lua expressions. |
| `completions` | Suggest visible identifiers and direct table or `self` members without executing application Lua. |

Breakpoints are initially pending until their executable lines are observed in a
loaded Lua function. A `breakpoint` event reports verification. Pending
breakpoints can be set before a script or module loads. `hitCondition` is a
positive integer: stop on that exact matching visit. Conditions are Lua expressions;
when combined with a hit count, only visits where the condition is true count.
Logpoints interpolate `{expression}` and use `{{` and `}}` for literal braces;
they emit DAP `output` events without stopping. Errors in a condition or logpoint
expression are reported as output and leave program execution running.

`breakpointLocations` uses debug information from observed calls and frames at a
stop. Unknown sources and functions return no locations until their executable
lines are known. It does not parse source files or guess locations. Locations are
line-based, and a column range must include the first column of a reported line.
Executable lines are collected once per observed function during an attachment.
The function cache uses weak keys, allowing closures to be collected and newly
loaded functions to be discovered without rescanning every call. At each stop
and source request, locations are refreshed from the still-live observed
functions in all Lua states. Collected functions no longer contribute lines;
breakpoints on removed lines return to pending and emit a `breakpoint` event.
Live older closures continue to contribute their lines after a reload. A new
attachment starts with no source knowledge, including when `localRoot` changes.

Locals shadow upvalues and globals, including when the local is `nil`. Evaluation
with a `frameId` uses the selected function's environment. Without `frameId`,
evaluation and expression assignment use the stopped Lua thread's global
environment. `context: "repl"` also accepts Lua
statements and assignments. Functions created by evaluation retain a snapshot of
the frame's bindings after the request completes. Evaluation runs on a temporary
Lua thread so it can enforce its instruction-hook timeout without disturbing the
stopped thread's VM state. It reads and writes the selected frame's bindings
and has the same side effects as executing that Lua code normally.
On LuaJIT, frame evaluation also receives the function's varargs (`...`),
including nil arguments. Lua 5.1 does not expose varargs through its debug API,
so evaluation receives no varargs on that runtime. Global evaluation always
receives no varargs.

`setExpression` accepts a Lua assignment target and a value expression. It returns
the assigned value and evaluates the target and value once in normal Lua
assignment order. It also works on a
yielded coroutine without resuming that coroutine.

Hover evaluation is restricted to identifiers and direct table paths such as
`player.health`, `items[1].name`, and `settings["display mode"]`. String, numeric,
and boolean literal keys are supported. Hovers read locals, upvalues, and raw
table entries without running Lua. Calls, arithmetic, and lookups requiring
`__index` are rejected. An ordinary missing key returns `nil`; a local containing
`nil` still shadows any global with the same name.

`completions` suggests identifiers in the selected frame, or globals if `frameId`
is omitted. Dot notation completes table fields, and colon notation completes
function-valued members. Direct paths with literal keys can identify nested
tables. Completion does not execute calls or metamethods. Results are sorted,
limited to 256 matching names, and use UTF-16 columns and the negotiated position
bases, including for multiline input. Only keys that are valid Lua identifiers
are offered as completion names.

Table entry names preserve key types: `["name"]`, `[1]`, and `[false]` are
different keys. Pass the displayed name back to `setVariable`. Inspection avoids
calling Lua-defined metamethods. Like MobDebug's Defold serializer, the engine
adapter expands game-object, GUI, and render script instances using their backing data
tables. Their `self` fields support expansion, hover, completion, and editing,
including in suspended coroutines. Vectors, quaternions, matrices, hashes, and
URLs show their engine values without invoking `__tostring`. Userdata table keys
retain their identities so equal component values remain distinct keys. Userdata
with a string `__name` in their metatable show that name and identity. Defold's
`RegisterUserType`, `RegisterUserTypeLocal`, and `SetUserType` populate this field
automatically; extensions can also supply it directly. Inspection reads it with
`lua_rawget` and never calls `__tostring` to build a summary. Files and known
LuaSocket types use their registry names. Live GUI nodes show their native subtype,
such as `gui.box`, `gui.text`, `gui.pie`, `gui.bone`, or `gui.Spine`. Custom subtype
hashes without reverse-hash names use `gui.custom_<id>`; deleted or foreign-scene
nodes fall back to `NodeProxy`. Unnamed userdata and functions show their type and
identity. More detailed `tostring(value)` descriptions remain available explicitly
in the Debug Console.
Frame and variable references become invalid on
resume; old IDs are rejected even at a later stop.

Displayed strings and string keys are editable Lua literals. Readable UTF-8 is
preserved; binary bytes use three-digit decimal escapes supported by both Lua
5.1 and LuaJIT. Accepting an unchanged string edit preserves its original bytes.

Variable types and table child counts are returned when the client declares
`supportsVariableType` and `supportsVariablePaging`, respectively. Stack paging
is advertised through `supportsDelayedStackTraceLoading`. Source descriptors
include file names. Variables expose `evaluateName` only when a direct path
identifies the binding; shadowed bindings and paths whose parent was reassigned
omit it. Binary string keys use Lua-compatible decimal escapes in these paths.

Clients declaring `supportsInvalidatedEvent` receive variable invalidations after
REPL evaluation and assignment attempts, including evaluations that change
state before returning an error. Hovers, watches, other evaluation contexts, and
completions do not invalidate views, preventing repeated watch refreshes.

All Lua contexts belong to one engine thread and stop together. A pause takes
effect at the next Lua hook; it cannot interrupt a blocking native function.
LuaJIT compilation is disabled while attached, and the prior JIT enablement and
debug hooks are restored on detach. Coroutines are held weakly while running and
pinned during stack inspection, so the debugger does not retain completed
coroutines indefinitely.
Stepping also follows returns and yields through cached `coroutine.resume`
functions and `coroutine.wrap` closures created before activation. Stack traces
omit Lua 5.1's synthetic tail-call frames, which have no inspectable function.

The server handles fragmented/coalesced frames and partial writes. Messages are
limited to 1 MiB, headers to 4 KiB, and queues to 4 MiB. Invalid framing or JSON
closes the session and resumes Lua. Invalid requests get unsuccessful DAP
responses. Disconnect never terminates the engine.

An inspection or evaluation response exceeding 1 MiB fails that request while
keeping the session paused. Retry table inspection with `start` and `count`, or
evaluate a smaller string slice, such as `value:sub(1, 1000)`. The limit applies
to the encoded JSON, including escaped binary bytes, and does not truncate or
modify application values.

Launch, reverse execution, instruction/function/data breakpoints, source-content
fetching, and native stack inspection are not implemented or advertised. Console
output from the engine keeps its existing destination; DAP output is used for
logpoints and debugger expression errors. The TCP server is excluded on Web.

## Tests

In a full native host build configured with `BUILD_TESTS=ON`, run:

```sh
cmake --build <build-directory> --target test_debugger_dap test_debugger_dap_lua test_debugger_release
cmake --build <build-directory> --target test_debugger_dap_engine
cmake --build <build-directory> --target test_debugger_dap_instances
```

The extension integration target is available when `script` and `extension` are
configured. The release check and script-instance targets are part of the full
native engine build. The release check builds and inspects `dmengine_release`,
using `dmengine_headless` to verify that it detects DAP code and registration.
The script-instance target runs `dap_debuggee_instances`, a headless engine test
host with physics bindings, using the `engine_test_content` target's compiled
project. All targets are also registered with the repository's `run_tests` sequence.

`src/test/test_dap.py` uses Python's standard library and a real TCP connection.
The same suite exercises the standalone C++ host with LuaJIT, the bundled Lua
5.1 runtime, and the actual engine extension with `dmScript::PCall`. It covers
every supported request, emitted events, mutation effects asserted by the
debuggee, recursion and coroutines, stale references, reconnects, malformed
messages, Unicode/binary/large values, and preservation of Lua stacks. The host
also checks restored hooks after detach.
The engine host additionally tests runtime activation after scripts and
coroutines have run, activation across existing contexts, reconnecting, and
retrying failed starts. Listener tests verify the loopback default, startup and
runtime address configuration, explicit interface overrides, and recovery from
invalid addresses. Network-interface tests skip on hosts with only loopback.
The suite also checks combined conditions/hit counts, global evaluation,
expression assignment, inspection without side effects, metadata negotiation,
completion positions, known breakpoint locations, and invalidation events.
Coroutine regressions cover discovery on attachment and reconnection, stepping
through original coroutine APIs, nested resumes, and collection during a step.
Tail-call inspection checks scopes, evaluation, and local assignment on both
runtimes.
The headless app loads the project fixtures in `engine/engine/src/test/debugger`
through the normal resource and component lifecycle. The DAP client checks `self`
expansion, cyclic fields, completions, and edits in game-object, GUI, and render
callbacks and suspended coroutines. It also checks that application getters and
metamethods are not called during inspection.

To run individual wire tests:

```sh
python3 engine/debugger/src/test/test_dap.py --debuggee <path-to-dap_debuggee> DAPTests.test_variables_evaluate_and_mutation
python3 engine/debugger/src/test/test_dap.py --engine <path-to-dmengine_headless> --engine-content engine/engine/build/src/test/build/default EngineDAPTests
```

To compare attached hook overhead with different numbers of idle coroutines:

```sh
python3 engine/debugger/src/test/benchmark_dap.py --debuggee <path-to-dap_debuggee> --coroutines 0 500 --samples 5
```

The benchmark measures CPU time inside the active coroutine, excluding startup
and creation of the idle coroutines. Compare the same host, runtime, and build
configuration. Timing thresholds are deliberately separate from the functional
tests, which verify lookup correctness and collection without speed assumptions.

The engine script suite additionally checks that the generic `ScriptExtension`
error callback sees the original error value and live locals before unwinding.

## Embedding

`src/debugger.h` exposes creation, state registration/removal, pumping, startup
waiting, and the error callback. All calls must run on the Lua owner thread.
Initialize sockets before creating a debugger, pump it every engine frame, and
remove each state before `lua_close`. For a host using raw `lua_pcall`, invoke
`OnError` from its protected-call error handler while the original error is still
at stack index 1. The engine extension performs these steps automatically and
uses per-script finalization so every non-shared Lua context is removed.

Hosts can optionally register a `SetUserdataTableResolver` callback for userdata
backed by a Lua table. It must push the backing table on success, preserve the
stack on failure, and avoid executing application code or raising errors. The
callback can receive a yielded coroutine. The engine extension installs the
adapter for Defold's three script-instance types automatically.
