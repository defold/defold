# Native Lua DAP debugger

`debugger` is a C++ library for debugging Lua 5.1 and LuaJIT through the
[Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/overview).
Native debug and headless engines link the `LuaDebugger` extension and expose
the `debugger` Lua module. Release engines exclude the library and its
registration symbol through the CMake targets and the Extender release
manifest. The TCP server is excluded on Web.

The extension provides a DAP server. The Defold editor uses MobDebug; connect
with a client that supports DAP over TCP, such as [VS Code](#vs-code).

## Connecting

### At startup

From a project directory containing `build/default/game.projectc`, start a
native debug engine with these arguments:

```sh
dmengine --config=debugger.enabled=1 \
  --config=debugger.port=8172 --config=debugger.wait=1
```

When launching from the Defold editor, put each `--config` argument on its own
line in **Preferences > General > Engine Arguments**, then use **Project > Build**.
The editor's Debug action starts MobDebug, which uses the same default port.

| Setting | Default | Behavior |
| --- | --- | --- |
| `debugger.enabled` | `0` | Set to `1` to start the listener during engine startup. |
| `debugger.address` | `127.0.0.1` | IPv4 address to bind to. The default accepts local connections only. |
| `debugger.port` | `8172` | Listening port. Use `0` to select an available port. |
| `debugger.wait` | `0` | Set to `1` to wait for client configuration before running startup scripts. |

The engine log reports the bind address and selected port. With `debugger.wait=0`,
the engine starts immediately and accepts attachment later. A client disconnect
while waiting releases the engine. If the listener fails during startup, the
engine logs the reason and continues; `debugger.start(0)` can retry on an
available port.

### While running

To enable DAP without startup flags or a restart, execute this Lua code in the
running project:

```lua
local port = debugger.start()
```

The code can be sent through the engine's existing `run_script` service, the same
mechanism the editor uses to start MobDebug when attaching.

`debugger.start([port [, address]])` returns the listening port immediately; it
does not wait for a client or pause the project. An omitted port uses
`debugger.port`; pass `0` to select an available port. The port must be an integer
from 0 to 65535. An omitted address uses `debugger.address`; pass `nil` as the port
to override only the address. Repeated calls return the existing listener's port
without changing its address or port. Invalid arguments or a bind failure raise
a Lua error, allowing a retry with another address or port.

Until activation, the extension leaves Lua hooks, coroutine functions, and JIT
settings alone and opens no DAP socket. Activation registers all live script
contexts; see [Threads and coroutines](#threads-and-coroutines) for discovery and
attachment behavior.

### Remote connections

For a direct connection from an editor on another device, bind to `0.0.0.0`
(all IPv4 interfaces) or a specific IPv4 interface address:

```sh
dmengine --config=debugger.enabled=1 \
  --config=debugger.address=0.0.0.0 --config=debugger.port=8172
```

For runtime activation, use `debugger.start(8172, "0.0.0.0")`. The client connects
to the device's actual IP address and listening port; `0.0.0.0` is only the bind
address.

The debugger can evaluate Lua and has no authentication. Enable a network
listener only on a trusted development network.

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
Defold Lua** in Run and Debug. VS Code's
[`debugServer` setting](https://code.visualstudio.com/api/extension-guides/debugger-extension#development-setup-for-mock-debug)
connects directly to Defold's DAP listener; `address` satisfies Lua Debug's
attach configuration. If you change the listening port, update both values.
For a remote engine, forward its DAP port to localhost before attaching from
VS Code, or use a DAP client that connects to the remote host directly.

## DAP support

### Session setup

A client connects directly over TCP using UTF-8 JSON and `Content-Length`
framing. To configure a session:

1. Send `initialize` and wait for its response.
2. Send `attach` and wait for the `initialized` event.
3. Configure breakpoints and exception filters, then send `configurationDone`.
4. Receive the `configurationDone` response, followed by the `attach` response.

All attach arguments are optional:

| Argument | Default | Behavior |
| --- | --- | --- |
| `localRoot` | Unset | Absolute client path to the game project, used to map project-relative Lua source names. |
| `stopOnEntry` | `false` | Pause Lua execution once client configuration is complete. |
| `evaluationTimeout` | `1000` | Evaluation time limit in milliseconds; an integer from 1 to 60000. |

`localRoot` is optional when client and runtime use the same paths. Native path
strings are supported, including Windows separators. `initialize` accepts
`pathFormat: "path"`; URI paths are not supported. Client line and column bases
are also negotiated by `initialize`.

### Supported requests

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

Stack inspection, evaluation, assignment, and completion requests require a
paused session. `threads` is also available while Lua is running.

### Breakpoints

A breakpoint remains pending until its line is known to be executable in a
loaded Lua function. A `breakpoint` event reports verification. Breakpoints can
be set before a script or module loads. Pass `hitCondition` as a string containing
a positive integer, such as `"3"`, to stop on that exact matching visit. Conditions
are Lua expressions; when combined with a hit count, only visits where the
condition is true count.

Logpoints interpolate `{expression}` and use `{{` and `}}` for literal braces;
they emit DAP `output` events without stopping. Errors in a condition or logpoint
expression are reported as output and leave program execution running.

`breakpointLocations` uses debug information from observed calls and frames at a
stop. Unknown sources and functions return no locations until their executable
lines are known. It does not parse source files or guess locations. Locations are
line-based, and a column range must include the first column of a reported line.

Observed functions are cached with weak keys, allowing closures to be collected
and newly loaded functions to be discovered without rescanning every call. At
each stop and when setting or querying breakpoints, locations are refreshed from
the still-live observed functions in all Lua states. Collected functions no
longer contribute lines; breakpoints on removed lines return to pending and emit
a `breakpoint` event.
Live older closures continue to contribute their lines after a reload. A new
attachment starts with no source knowledge, including when `localRoot` changes.

### Evaluation and assignment

Locals shadow upvalues and globals, including when the local is `nil`. Evaluation
with a `frameId` uses the selected function's environment. Without `frameId`,
evaluation and expression assignment use the stopped Lua thread's global
environment. `context: "repl"` also accepts Lua statements and assignments.
Functions created by evaluation retain a snapshot of the frame's bindings after
the request completes.

Evaluation runs on a temporary Lua thread so it can enforce its instruction-hook
timeout without disturbing the stopped thread's VM state. It reads and writes
the selected frame's bindings and has the same side effects as executing that
Lua code normally. On LuaJIT, frame evaluation also receives the function's
varargs (`...`), including nil arguments. Lua 5.1 does not expose varargs through
its debug API, so evaluation receives no varargs on that runtime. Global
evaluation always receives no varargs.

`setExpression` accepts a Lua assignment target and a value expression. It
returns the assigned value and evaluates the target and value once in normal
Lua assignment order. It also works on a yielded coroutine without resuming it.

The `evaluationTimeout` attach argument applies to evaluations, assignments,
breakpoint conditions, and logpoint expressions. A timeout reports an error and
keeps the session usable; side effects performed before the timeout remain.
A client disconnect releases the engine after evaluation finishes or times out.
The limit is checked at Lua instructions, so it cannot interrupt a blocking
native function or code that explicitly disables debug hooks.

Clients declaring `supportsInvalidatedEvent` receive variable invalidations after
REPL evaluation and assignment attempts, including evaluations that change state
before returning an error. Hovers, watches, other evaluation contexts, and
completions do not invalidate views, preventing repeated watch refreshes.

#### Multiple REPL results

REPL evaluation preserves multiple Lua returns, including nils. A response with
more than one return has the Defold-specific `defoldResultCount` field and a
`variablesReference` to a synthetic result container. Its `result` is a
comma-separated summary, and its `type` is `"tuple"` when the client supports
variable types. For example, `nil, "error details", nil` returns a count of three.
Fetching the container with `variables` yields `[1]`, `[2]`, and `[3]` in return
order, with `nil` in the first and last slots. Filtering and zero-based paging
work on these indexed slots; paging metadata is included when the client
supports it.

The container and its slots are read-only, but table values expand and their
members can be edited normally. References follow the same lifetime rules as
other variable references. Single returns retain the ordinary value shape, and
a statement or call with no returns still shows `nil`; neither includes
`defoldResultCount`. Watches, breakpoint conditions, logpoints, and assignments
continue to use only the first return.

Console clients should detect `defoldResultCount`, fetch the container's
children, and render each child independently in return order using their
ordinary value/table formatter. Do not render the synthetic container as a Lua
table. Preserve nil slots and avoid evaluating the expression again to retrieve
additional returns. If execution resumes before expansion finishes, use the
response's `result` summary as the fallback.

### Inspection and completion

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

Table entry names preserve key types. String keys that are valid Lua identifiers
appear as plain names, such as `name`; other keys use brackets, such as `["1"]`,
`[1]`, and `[false]`. Pass the displayed name back to `setVariable`.

Displayed strings and string keys are editable Lua literals. Readable UTF-8 is
preserved; binary bytes use three-digit decimal escapes supported by both Lua
5.1 and LuaJIT. Accepting an unchanged string edit preserves its original bytes.

Frame and variable references become invalid when execution resumes; old IDs are
rejected even at a later stop. Evaluation that resumes a coroutine can invalidate
its frames and associated references before the paused session resumes.

Variable types and table child counts are returned when the client declares
`supportsVariableType` and `supportsVariablePaging`, respectively. Stack paging
is advertised through `supportsDelayedStackTraceLoading`. Source descriptors
include file names. Variables expose `evaluateName` only when a direct path
identifies the binding; shadowed bindings and paths whose parent was reassigned
omit it. Binary string keys use Lua-compatible decimal escapes in these paths.

#### Defold userdata

Inspection avoids calling Lua-defined metamethods. The engine adapter expands
game-object, GUI, and render script instances using their backing data tables.
Their `self` fields support expansion, hover, completion, and editing, including
in suspended coroutines. Vectors, quaternions, matrices, hashes, and URLs show
their engine values without invoking `__tostring`. Userdata table keys retain
their identities so equal component values remain distinct keys.

Userdata with a non-empty string `__name` in their metatable show that name and
identity. Defold's `RegisterUserType`, `RegisterUserTypeLocal`, and `SetUserType`
populate this field automatically; extensions can also supply it directly.
Inspection reads it with `lua_rawget`. Files and known LuaSocket types use their
registry names. Unnamed userdata and functions show their type and identity.

Live GUI nodes show their native subtype, such as `gui.box`, `gui.text`, `gui.pie`,
`gui.bone`, or `gui.Spine`. Custom subtype hashes without reverse-hash names use
`gui.custom_<id>`; deleted or foreign-scene nodes fall back to `NodeProxy`.
More detailed `tostring(value)` descriptions remain available explicitly in the
Debug Console.

### Threads and coroutines

All Lua contexts belong to one engine thread and stop together. A pause takes
effect at the next Lua hook; it cannot interrupt a blocking native function.
LuaJIT compilation is disabled while attached, and the prior JIT enablement and
debug hooks are restored on detach. Coroutines are held weakly while running and
pinned during stack inspection, so the debugger does not retain completed
coroutines indefinitely.

Activation discovers existing coroutines through reachable Lua references,
including frame locals and function upvalues. Already suspended coroutines can
be inspected without resuming them. Each attachment repeats discovery, including
after a disconnect, to find coroutines created through cached original coroutine
functions while detached.

Stepping also follows returns and yields through cached `coroutine.resume`
functions and `coroutine.wrap` closures created before activation. Stack traces
omit Lua 5.1's synthetic tail-call frames, which have no inspectable function.

## Limits and unsupported features

The server handles fragmented/coalesced frames and partial writes. Messages are
limited to 1 MiB, headers to 4 KiB, and queues to 4 MiB. Invalid framing, JSON, or
request envelopes close the session and resume Lua. Well-formed requests with
unsupported commands or invalid arguments are rejected without closing the
session. Disconnect never terminates the engine.

An inspection or evaluation response exceeding 1 MiB fails that request while
keeping the session paused. Retry table inspection with `start` and `count`, or
evaluate a smaller string slice, such as `value:sub(1, 1000)`. The limit applies
to the encoded JSON, including escaped binary bytes, and does not truncate or
modify application values.

Launch, reverse execution, instruction/function/data breakpoints, source-content
fetching, and native stack inspection are not implemented or advertised. Console
output from the engine keeps its existing destination; DAP output is used for
logpoints and breakpoint-condition errors.

## Tests

Run these commands from the repository root, using a full native host build
configured with `BUILD_TESTS=ON`. See the [engine build guide](../../README_BUILD.md)
and [CMake guide](../../scripts/cmake/README.md) for setup.

```sh
cmake --build "<build-directory>" --target \
  run_test_dap_json test_debugger_dap test_debugger_dap_lua \
  test_debugger_dap_engine test_debugger_dap_instances test_debugger_release
```

Replace `<build-directory>` with the configured CMake build directory. The
targets build their prerequisites and run the following checks:

| Target | Coverage |
| --- | --- |
| `run_test_dap_json` | JSON parser unit tests, including capacity, duplicate keys, and node limits. |
| `test_debugger_dap` | Wire tests against the standalone `dap_debuggee` host, using LuaJIT on desktop platforms. |
| `test_debugger_dap_lua` | The same wire tests against bundled Lua 5.1; requires the `lua` target. |
| `test_debugger_dap_engine` | Wire tests against `dap_debuggee_engine`, including extension lifecycle and `dmScript::PCall`; requires `script` and `extension`. |
| `test_debugger_dap_instances` | Script-instance tests against `dap_debuggee_instances`, a headless engine host with custom GUI and physics bindings, using compiled `engine_test_content`. |
| `test_debugger_release` | Checks `dmengine_release` for DAP code and registration, using `dmengine_headless` to verify that the check detects them. |

The script-instance and release checks require a full native engine build.
All checks are also included in the repository's `run_tests` target.

[test_dap.py](src/test/test_dap.py) uses Python's standard library and real TCP
connections. It covers supported requests and events, breakpoint conditions and
hit counts, evaluation and assignment, multiple REPL results, inspection without
side effects, client metadata, completion positions, and source locations.
Regression cases cover coroutine discovery and stepping, tail calls, stale
references, reconnects, malformed messages, Unicode and binary values, response
limits, evaluation timeouts, and restoration of Lua stacks and debug hooks.

The extension host also tests runtime activation across existing contexts and
coroutines, listener configuration, and retries after failed starts. Network
interface tests skip on hosts with only loopback. The script-instance host loads
the [project fixtures](../engine/src/test/debugger) through the normal resource
and component lifecycle to test `self` inspection and editing, native userdata,
reboots, and shared or separate script contexts. The engine script suite
separately checks that the generic `ScriptExtension` error callback sees the
original error value and live locals before unwinding.

### Individual tests

After building the corresponding host and test content, select a test method or
class:

```sh
python3 engine/debugger/src/test/test_dap.py \
  --debuggee "<path-to-dap_debuggee>" \
  DAPTests.test_variables_evaluate_and_mutation

python3 engine/debugger/src/test/test_dap.py \
  --engine "<path-to-dap_debuggee_instances>" \
  --engine-content engine/engine/build/src/test/build/default \
  EngineDAPTests
```

Replace the executable placeholders with the paths from your build.

### Hook overhead benchmark

To compare attached hook overhead with different numbers of idle coroutines:

```sh
python3 engine/debugger/src/test/benchmark_dap.py \
  --debuggee "<path-to-dap_debuggee>" --coroutines 0 500 --samples 5
```

The benchmark measures CPU time inside the active coroutine, excluding startup
and creation of the idle coroutines. Compare the same host, runtime, and build
configuration. Timing thresholds are deliberately separate from the functional
tests, which verify lookup correctness and collection without speed assumptions.

## Embedding

[debugger.h](src/debugger.h) exposes creation, state registration and removal,
pumping, startup waiting, and the error callback. All calls, including Lua
execution, must run on the Lua owner thread. Initialize sockets before creating
a debugger, register each Lua state with `AddLuaState`, call `Update` every
engine frame, and remove each state with `RemoveLuaState` before `lua_close`.
For a host using raw `lua_pcall`, invoke `OnError` from its protected-call error
handler while the original error is still at stack index 1. The engine extension
performs these steps automatically and uses per-script finalization to remove
every non-shared Lua context.

Hosts can optionally register a `SetUserdataTableResolver` callback for userdata
backed by a Lua table. It must push the backing table on success, preserve the
stack on failure, and avoid executing application code or raising errors. The
callback can receive a yielded coroutine.

`SetUserdataFormatter` optionally supplies display strings for known userdata.
Its callback must write a NUL-terminated representation and return `true`, or
return `false` to use the default display. It must preserve the Lua stack and
avoid executing application code or raising errors. The engine extension
installs both callbacks for Defold's script instances and native userdata.
