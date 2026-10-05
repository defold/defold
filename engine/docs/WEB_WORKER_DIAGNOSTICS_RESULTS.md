# Web worker diagnostics and smaller-stack evaluation

The new diagnostics identify publication backpressure as the dominant wait in the
light gameplay replay. Reducing the worker stack from 5 MiB to 2 MiB saves about
3 MiB of live allocator reservation, but does not solve the p99 regression.
Threading, sprite work placement, diagnostic collection and stack sizing remain
opt-in; the engine's default worker stack remains 5 MiB.

The [comparison report](benchmarks/web-worker-diagnostics-2026-10-04/REPORT.md)
contains graphs, individual runs, memory breakdowns, trace attribution and raw
results. Direct mode uses the same modified Release engine with threading off;
it is not clean vanilla.

## What changed

- Added bounded per-update diagnostics with sequential IDs and main/worker
  timestamps. Graphics-owner calls separate queueing, owner execution and worker
  resumption. No diagnostic buffer is allocated when disabled.
- Corrected deferred sprite scratch accounting: the render owner samples it and
  the producer reads it from a retired frame slot. Aggregates cover every sprite
  world, including collection proxies.
- Added component-frame upload and constant capacities as subsets of total frame
  capacity, with regression checks against double counting.
- Extended the reusable replay runner to collect explicitly marked diagnostics.
  Contracts reject those runs as acceptance timings and detect dropped or
  inconsistent records. Cross-owner clock rounding has a one-microsecond
  tolerance; raw timestamps are preserved.

## What the gameplay evidence says

Three uninstrumented repetitions of the same 15,000-tick offline Underwatermelon
replay completed with matching checkpoints and cleanup. Each measured 14,400
updates, about 120 wall-clock seconds. Every mode used the same explicit 1/60 s
simulation step.

| Mode | Updates/s | p99 interval | Peak live allocation | WASM capacity |
|---|---:|---:|---:|---:|
| Direct | 120.00 | 11.72 ms | 15.69 MiB | 32 MiB |
| Threaded, 5 MiB stack | 119.97 | 13.70 ms | 21.22 MiB | 32 MiB |
| Threaded, 2 MiB stack | 120.00 | 14.02 ms | 18.17 MiB | 32 MiB |

The smaller stack frees space inside the allocator. This is not evidence of an
equal drop in operating-system RSS: much of a reserved stack may never be touched,
and the backing WASM memory stayed at 32 MiB in this workload.

Separate instrumented runs show about 7.7 ms of publication wait per update on
average and about 13 ms in the slowest 1% of intervals. Simulation plus preparation
was roughly 0.4 ms, worker wake-up averaged 0.03–0.05 ms, and the measured window
had no synchronous graphics-owner calls. These are wall-time spans, not CPU
utilization. Short consumption spans plus long ready-to-consume waits are
consistent with browser render cadence driving the backpressure. Other
JavaScript/extension rendezvous remain outside the detailed graphics-call probe.

The next scheduling experiment should target render admission and consumption
while preserving bounded buffering. Increasing queue depth can trade producer
waits for latency and memory, so it is not justified by throughput alone.

The synthetic regression checks retained the throughput benefit: 30,000 bunnies
averaged 79.57 updates/s direct versus 118.87 with the 2 MiB threaded stack
(+49.4%). Mixed geometry averaged 67.03 versus 83.69 (+24.9%). Their threaded
live allocations fell by 3.00 MiB, while WASM capacity remained unchanged.
Bunny p99 varied between repeats; the individual points are preserved in the
report, so the mean should not be read as a guaranteed tail-latency improvement.

## Validation and remaining evidence

The 2 MiB stack passed the broad component fixture, including sprite/GUI/label/
tilemap/particle rendering, physics, sound, cameras, proxies/factories, CPU and GPU
skinning, mesh updates, instancing, pause/resume and controlled context-loss
shutdown. Frozen pixel checkpoints match direct mode. The fixture and gameplay
watermarks observed about 18 KiB of stack writes; this does not establish the
worst case for arbitrary Lua or native extension call paths.

Native validation passed 98 render tests, 563 component tests, and three focused
scheduler/rendezvous tests. Thirteen JavaScript and 23 Python runner, replay and report regression tests
passed for the new diagnostics and memory accounting.

The supplied larger games were checked from isolated copies. The external space
game reached gameplay in direct and threaded modes. Dwarfcopter builds but its
threaded custom renderer hits the unsupported `NewRenderTarget` path; its direct
render-script migration also needs completion. The
[compatibility notes](benchmarks/web-worker-diagnostics-2026-10-04/ADVANCED_PROJECTS.md)
retain generic findings; private project adapters, logs and screenshots are
excluded. At the time of these smoke checks, neither game supplied accepted
performance evidence. The later [space game comparison](SPACE_GAME_WEB_REPLAY_RESULTS.md)
adds a matched replay and clean-vanilla control. A slower physical device, other
browsers, physical latency and energy remain unproven.
