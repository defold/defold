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

Attaching to a running project sends `/_defold/debugger/start.lua` through the
existing `run_script` engine service. The script calls the native
`debugger.start()` API with port `8172 + project.instance_index` and prints the
actual listener port, including when a listener already exists. The editor
discovers this output for launched targets. Targets without a local output stream
use the per-instance port. Attachment stops at the next Lua line. Rebooted engines
also use the per-instance port.

The selected stack frame's DAP ID is used for console evaluation. Locals and
upvalues load when selecting a frame. The global scope appears as an expandable
`_G` entry; globals and table children load when expanded. The console accepts
expressions and Lua statements. Inspection runs
off the JavaFX thread, and responses from a previous stop or frame selection are
discarded. Engine console output still uses the existing log stream; DAP output
events supply debugger messages and logpoints.

Detach closes the DAP session and leaves the engine running. Stop first detaches
so a paused engine can process its exit request, then uses the editor's existing
engine/process shutdown path.

## Engine changes needed for remote devices

The native engine extension currently starts its listener on `127.0.0.1`.
The editor already connects to the selected target's address. To enable direct
connections from the editor to another device:

1. Add a `debugger.address` configuration setting to
   `engine/debugger/src/debugger_extension.cpp`, retaining `127.0.0.1` by default.
2. Pass that address to `dmDebugger::New(port, address)`, which already accepts
   a bind address. Allow runtime activation through `debugger.start()` to use
   the configured address, or add an optional address argument for late attach.
3. Log the actual bind address and port instead of a hardcoded loopback address.
4. Set the address to `0.0.0.0` or the device's interface address when remote
   debugging is explicitly enabled. The debugger can evaluate Lua, so this
   listener should only be enabled on a trusted development network.

For remote dynamic-port discovery, additionally expose the active listener port
through the engine service or discovery metadata and refresh it after runtime
activation and reboot. Until then, remote targets must use the per-instance
port, with port forwarding when the engine still binds to loopback.

## Validation

From `editor`, run:

```sh
lein test editor.debugging.dap-test editor.debug-view-test editor.engine-test editor.targets-test
```

The protocol tests use a local TCP adapter to check initialization ordering,
UTF-8 framing, out-of-order responses, breakpoint replacement, inspection,
control, cancellation, and disconnects. The JavaFX tests exercise stale session,
stack, and variable responses.

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
