# DAP work requiring broader integration

This roadmap records the harder work from the DAP coverage review. These are
future features, separate from the protocol fixes and inspection improvements in
the current branch. Implement and test a feature before advertising its DAP
capability. Reference: [Debug Adapter Protocol specification](https://microsoft.github.io/debug-adapter-protocol/specification).

## Next priorities

### Editor DAP integration

Replace the editor's MobDebug client with a DAP client while preserving the
existing breakpoint, stepping, call-stack, variables, and debug-console UI.
Integrate runtime activation through `debugger.start()`, listener discovery,
multiple engine instances, reconnects, and connection/error reporting.

Acceptance: both starting a project for debugging and attaching to an already
running project work from the editor; detaching leaves the project running;
engine exits and failed connections restore a usable editor state.

### Source discovery and retrieval

Implement `source`, `loadedSources`, and `loadedSource` events using an engine
source catalog. Track code loaded before attachment, reloads, and dynamic Lua
chunks. Packaged bytecode may require retaining original source text in debug
builds. Keep source references stable during a session and distinguish unavailable
source from a file that the client can read locally.

Acceptance: a client can inspect available source without having the original
project directory; source listings and breakpoints remain correct across reloads.
Add `modules` and module events when the engine can provide meaningful module
identities and lifecycle information.

### Engine console output

Forward normal engine output through DAP without changing existing logging.
Capture messages from other threads into a bounded queue, preserve ordering,
and handle listener registration, recursion, disconnection, and shutdown safely.

Acceptance: output remains available while Lua is paused; a disconnected or slow
client cannot block engine logging or cause unbounded buffering.

## Later features

### Process lifecycle

Define process ownership before adding `launch`, `restart`, `terminate`, and
disconnect termination options. The in-process server cannot launch an engine
that does not exist yet; that requires an external adapter or launcher. Wire
engine shutdown and restart to the appropriate lifecycle operations. Add
`process` and `exited` events when reliable process/exit information is available.
Use `runInTerminal` and `startDebugging` only when the client/launcher workflow
needs these adapter-to-client requests.

### Function breakpoints and exception handling

Function breakpoints need identities that work across aliases, anonymous
functions, independent contexts, and reloads. Stopping on every thrown or caught
Lua error requires integration beyond the current `dmScript::PCall` callback.
Add advanced exception filters/options only when their semantics are supported
consistently by both Lua 5.1 and LuaJIT.

### Finer stepping and locations

Statement/column stepping, `stepInTargets`, and precise code locations need richer
compiler/runtime metadata than the existing line hooks. Define behavior for
multiple statements on one line and calls through aliases or tail calls before
advertising stepping granularity. `locations` also needs stable references to
declaration locations.

### Cancellation and progress

Long evaluations currently execute synchronously on the Lua owner thread.
Supporting `cancel` requires cooperative interruption while continuing to process
requests. Blocking native calls cannot be interrupted by a Lua instruction hook.
Preserve hooks, Lua stacks, and debugger state after cancellation; add progress
events only for operations whose progress can be reported meaningfully.

### General hover expressions

Keep ordinary hovers limited to direct variable/table inspection. Supporting
arbitrary Lua expressions without side effects requires a dedicated evaluator;
calling Lua functions or metamethods can mutate state or affect external systems.
Unsupported hover expressions should fail without executing them.

## Deferred or outside the current Lua debugger scope

- **Data watchpoints:** detecting writes reliably needs VM instrumentation;
  metatable interception alone misses locals and writes to existing table keys.
- **Independent coroutine execution and termination:** this requires defining
  scheduling and lifetime behavior without changing the game's semantics.
- **Reverse execution, `goto`, and `restartFrame`:** restoring or manipulating Lua
  execution must also keep native engine state consistent. Recording/replay or
  deeper runtime support would be required.
- **Native memory, disassembly, and C/C++ debugging:** use a native debugger
  backend instead of extending this Lua server into one.

For each implemented feature, add focused integration tests with comments
explaining the scenario and expected behavior. Run the applicable suites with
LuaJIT, bundled Lua 5.1, and the engine extension, and retain the release artifact
check that excludes debugger implementation and registration.
