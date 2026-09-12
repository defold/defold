# Native Lua DAP debugger

`debugger` is a separate C++ library for debugging Lua 5.1 and LuaJIT through the
[Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/overview).
Debug and headless engines link the `LuaDebugger` extension. Release engines and
Extender release variants exclude the library and its registration symbol. The
implementation also compiles out when `DM_RELEASE` is defined.

## Connecting

Start a native debug engine with:

```sh
dmengine --config=debugger.enabled=1 --config=debugger.port=8172 --config=debugger.wait=1
```

The debugger listens on `127.0.0.1`. It is disabled by default. The default port is
8172; port 0 selects an available port and prints it in the engine log.
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
flag is required. `debugger.start([port])` returns the listening port immediately;
it does not wait for a client or pause the project. An omitted port uses
`debugger.port` (8172 by default); pass 0 to select an available port. Repeated
calls return the existing listener's port. Invalid ports or a bind failure raise
a Lua error and allow retrying with another port.
If the configured listener fails during engine startup, the engine logs the
reason and continues; `debugger.start(0)` can retry on an available port.

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
  "stopOnEntry": true
}
```

`localRoot` maps client files to Defold's project-relative Lua source names. It is
optional when client and runtime use the same paths. Native path strings are
supported, including Windows separators. URI paths are rejected. Client line and
column bases are negotiated by `initialize`.

This module provides the DAP server. The editor's existing MobDebug client is
unchanged; connect with a client that supports DAP TCP servers.

## Supported requests

| Requests | Behavior |
| --- | --- |
| `initialize`, `attach`, `configurationDone`, `disconnect` | Attach to a running engine, optionally stop on entry, detach and resume, reconnect. |
| `setBreakpoints` | Replace all breakpoints for one source; ordinary, conditional, hit-count, and log breakpoints. |
| `breakpointLocations` | Return sorted, known executable lines within a source range. |
| `setExceptionBreakpoints`, `exceptionInfo` | `uncaught` errors crossing `dmScript::PCall`, inspected before unwinding. An empty filter list disables exception stops. |
| `threads` | Independent Lua contexts and live coroutines, with stable IDs and start/exit events. |
| `pause`, `continue`, `next`, `stepIn`, `stepOut` | Pause all Lua execution; continue and step by source line. Stepping follows coroutine entry and return to its resumer. |
| `stackTrace`, `scopes`, `variables` | Lua frames, locals, upvalues, function globals, and nested tables. Stack and variable paging, named/indexed filters, and cyclic tables are supported. |
| `evaluate`, `setVariable`, `setExpression` | Evaluate in the selected frame or global scope and edit variables or assign to Lua expressions. |
| `completions` | Suggest visible identifiers and direct table members without executing Lua. |

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

Locals shadow upvalues and globals, including when the local is `nil`. Evaluation
with a `frameId` uses the selected function's environment. Without `frameId`,
evaluation and expression assignment use the stopped Lua thread's global
environment. `context: "repl"` also accepts Lua
statements and assignments. Functions created by evaluation retain a snapshot of
the frame's bindings after the request completes. Evaluation runs on the stopped
Lua thread, using a temporary thread for yielded coroutines to preserve their
suspended state. In either case it reads and writes the selected frame's bindings
and has the same side effects as executing that Lua code normally.

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
calling user metamethods. Other Lua types, including userdata and functions, are
displayed with their type and identity. Frame and variable references become
invalid on resume; old IDs are rejected even at a later stop.

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

Launch, reverse execution, instruction/function/data breakpoints, source-content
fetching, and native stack inspection are not implemented or advertised. Console
output from the engine keeps its existing destination; DAP output is used for
logpoints and debugger expression errors. The TCP server is excluded on Web.

## Tests

After configuring the repository with CMake and `BUILD_TESTS=ON`, run:

```sh
cmake --build <build-directory> --target test_debugger_dap test_debugger_dap_lua test_debugger_release
cmake --build <build-directory> --target test_debugger_dap_engine
```

The engine integration target is available when `script` and `extension` are
configured. All targets are also registered with the repository's `run_tests`
sequence. Waf runs the native LuaJIT DAP suite when building the debugger module
with tests enabled.

`src/test/test_dap.py` uses Python's standard library and a real TCP connection.
The same suite exercises the standalone C++ host with LuaJIT, the bundled Lua
5.1 runtime, and the actual engine extension with `dmScript::PCall`. It covers
every supported request, emitted events, mutation effects asserted by the
debuggee, recursion and coroutines, stale references, reconnects, malformed
messages, Unicode/binary/large values, and preservation of Lua stacks. The host
also checks restored hooks after detach. A separate compiled-artifact check
ensures `DM_RELEASE` contains neither debugger code nor extension registration.
The engine host additionally tests runtime activation after scripts and
coroutines have run, activation across existing contexts, reconnecting, and
retrying failed starts.
The suite also checks combined conditions/hit counts, global evaluation,
expression assignment, inspection without side effects, metadata negotiation,
completion positions, known breakpoint locations, and invalidation events.
Coroutine regressions cover discovery on attachment and reconnection, stepping
through original coroutine APIs, nested resumes, and collection during a step.
Tail-call inspection checks scopes, evaluation, and local assignment on both
runtimes.

To run individual wire tests:

```sh
python3 engine/debugger/src/test/test_dap.py --debuggee <path-to-dap_debuggee> DAPTests.test_variables_evaluate_and_mutation
```

The engine script suite additionally checks that the generic `ScriptExtension`
error callback sees the original error value and live locals before unwinding.

See [ROADMAP.md](ROADMAP.md) for features requiring broader engine, editor, or
Lua runtime integration.

## Embedding

`src/debugger.h` exposes creation, state registration/removal, pumping, startup
waiting, and the error callback. All calls must run on the Lua owner thread.
Initialize sockets before creating a debugger, pump it every engine frame, and
remove each state before `lua_close`. For a host using raw `lua_pcall`, invoke
`OnError` from its protected-call error handler while the original error is still
at stack index 1. The engine extension performs these steps automatically and
uses per-script finalization so every non-shared Lua context is removed.
