# Larger sprite-game compatibility checks

These initial startup/gameplay smoke checks were not performance measurements.
Original projects were left unchanged. Private project copies, adapters, source
revisions, dependencies, logs, screenshots and build manifests are excluded from
published evidence and retained only in Git-ignored local storage.

| Project | Initial result |
| --- | --- |
| Space game (external project) | Short gameplay smoke checks ran in direct and component-threaded modes using a private compatibility adapter. |
| Dwarfcopter | Content and native extensions built. Runtime compatibility remained blocked; threaded startup rejected render-target creation. |

The initial space game checks had no matched deterministic replay or long-run
cleanup contract and supplied no evidence of a performance benefit. The later
[anonymous replay comparison](../../SPACE_GAME_WEB_REPLAY_RESULTS.md) supersedes
that limitation with matched checkpoints, cleanup and repeated foreground runs.
Project-specific implementation and adaptation details are intentionally private.

## Engine ownership gaps

Producer-side `NewRenderTarget` is not admitted by the graphics packet PoC.
Create, resize and delete must reach the graphics owner with safe resource
lifetimes before custom render-target workloads are supported. Component coverage
alone does not establish coverage of every graphics-resource operation.

Native extensions also need an explicit ownership contract. Browser callbacks
must queue work to the Lua owner, and DOM operations must dispatch to browser
main. A successful gameplay smoke test does not certify focus, resize,
fullscreen, pointer-lock or shutdown paths.

## Remaining work

- Add graphics-owner render-target lifecycle support and lifetime tests.
- Validate extension callback and DOM ownership while simulation is active.
- Extend the real-game evidence to a slower device and additional browsers.

Threading and scheduling changes remain opt-in. The public notes intentionally
contain no game source, assets, screenshots, replay state or project build recipes.
