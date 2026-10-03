# Editor Lua debugging

The editor connects directly to the engine's native Debug Adapter Protocol
server. It requires a native debug or headless engine containing the
`LuaDebugger` extension. Older engines with only MobDebug, release engines, and
Web builds cannot be used with this client.

## Starting and attaching

Starting a project for debugging launches the engine with `debugger.enabled=1`,
`debugger.wait=1`, and `debugger.port=0`. The editor reads the chosen port from the
engine's output. Each locally launched engine can therefore use its own port.
Only the instance selected for debugging waits for configuration.

The client sends `initialize` and `attach`, waits for `initialized`, installs the
enabled breakpoints, and sends `configurationDone`. Startup scripts then run with
their breakpoints installed. Breakpoint changes replace the complete set for each
changed source, without pausing the project.

Attaching to a running project sends `/_defold/debugger/start.lua` for locally
launched targets or `/_defold/debugger/start_remote.lua` for remote targets through
the existing `run_script` engine service. Both scripts call the native
`debugger.start()` API with port `8172 + project.instance_index` and print the
actual listener port, including when a listener already exists. Local attachment
uses the engine's configured address, which defaults to localhost. Remote
attachment passes `0.0.0.0` to listen on the device's IPv4 interfaces. The editor
discovers this output for launched targets. Targets without a local output stream
use the per-instance port. Attachment stops at the next Lua line. Rebooted engines
also use the per-instance port. Debug reboots can send eight arguments and require
an engine with the matching eight-argument reboot message.

The selected stack frame's DAP ID is used for console evaluation. Locals and
upvalues load when selecting a frame. The global scope appears as an expandable
`_G` entry; globals and table children load when expanded. The console accepts
expressions and Lua statements. Table-valued console results print their nested
contents. Repeated references and tables beyond depth 16 retain their identity
strings to bound expansion.

Expanded variable paths, selection, and scroll position are retained when
stepping, hitting another breakpoint, switching frames, or refreshing after
evaluation. Matching names reopen even in a different
file or function. Reopened tables use fresh DAP references and values; only the
previously opened paths are loaded, so cyclic tables do not expand indefinitely.
Valid Lua identifier keys are displayed directly (for example, `score`); other
keys retain their bracketed form (for example, `["end"]` and `[1]`). Each table
request still loads all its direct children without pagination.

Inspection runs off the JavaFX thread, and responses from a previous stop or
frame selection are discarded. Engine console output still uses the existing
log stream; DAP output events supply debugger messages and logpoints. Transport
reading, transport writing, protocol handling, and ordered callbacks run in
separate loops. Callbacks may make blocking DAP requests. Apart from connection
setup, client operations are blocking; UI callers dispatch them to background
threads.

Detach closes the DAP session and leaves the engine running. Stop first detaches
so a paused engine can process its exit request, then uses the editor's existing
engine/process shutdown path.

## Remote devices

Remote attachment requires an engine supporting `debugger.start(port, address)`.
Starting a project for debugging on a remote target also passes
`debugger.address=0.0.0.0` when rebooting the engine. The editor connects to the
selected device's actual address and the per-instance debugger port; no port
forwarding is required. Locally launched targets retain the localhost default.

An already running listener keeps its original address and port. Restart a remote
engine if its listener was previously started on localhost or an unknown port.
Remote dynamic-port discovery still requires exposing the active debugger port
through engine service or discovery metadata. Use the per-instance port until
that is supported. Enable network debugging only on a trusted development network,
since the debugger can evaluate Lua.

## Validation

From `editor`, run:

```sh
lein test editor.debugging.dap-test editor.debugging.variables-test editor.debug-view-test editor.engine-test editor.app-view-test editor.targets-test
```

The protocol tests use a local TCP adapter to check initialization ordering,
UTF-8 framing, out-of-order responses, breakpoint replacement, inspection,
control, cancellation, and disconnects. The JavaFX tests exercise stale session,
stack, and variable responses, restoring expanded paths with fresh values, cyclic
tables, and preserving the viewport when table entries change.

To additionally run the editor client against a built native Lua test host, set
the JVM property `defold.dap.debuggee` to the absolute path of `dap_debuggee`,
`dap_debuggee_lua`, or `dap_debuggee_engine` when running
`editor.debugging.dap-test`. The test checks a real breakpoint, nested table
inspection, evaluation, stepping, and detachment followed by normal Lua exit.

For an editor smoke test, use `lein run`, set Preferences > Dev > Custom Engine
to a debug engine built from this branch, and open a Lua project. Exercise both
starting with a breakpoint in `init()` and attaching to an already running
project. Check step into/over/out, selecting a caller frame, expanding `self`,
console evaluation, changing breakpoints while running, detach/reattach, stop,
and multiple engine instances.
