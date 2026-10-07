# Pixel web frame-pacing milestone

**Component threading remains promising as an opt-in feature for sprite-heavy web games. The new readiness budget works, but these measurements do not justify enabling it over unbudgeted ready scheduling.** The most important correction was removing periodic benchmark output from the timing window: worker stdout can wait synchronously for browser main.

Completed on the physical Pixel 4a: **60 synthetic acceptance runs** (three rotated repetitions), **three game compatibility replays**, and **18 separate diagnostic runs**. Within each workload, all modes use identical Release PoC engine/content bundles with different opt-in settings. Threaded variants use overlapping component preparation, deferred sprite geometry on browser main and 2 MiB worker stacks. “Direct” is that engine with threading disabled, not a newly built vanilla control. This milestone introduces no timer-policy change.

## Repeated synthetic results

| Workload | Direct updates/s | Threaded updates/s | Ready updates/s | Ready + 32 ms budget |
|---|---:|---:|---:|---:|
| 20k sprites | 35.03 | 44.86 | 52.30 | 50.69 |
| 30k sprites | 22.37 | 32.79 | 35.27 | 34.92 |
| Fill heavy | 25.91 | 26.40 | 26.16 | 26.21 |
| 200 meshes + 500 models | 59.99 | 59.98 | 59.99 | 59.96 |
| 2,000 meshes + 5,000 models | 1.89 | 1.77 | 2.18 | 2.02 |

The threading gain is workload-dependent. Ordinary threading improves both sprite cases; readiness increases throughput further. Fill-heavy rendering stays effectively flat. The lighter geometry scene remains near 60 updates/s, with no throughput reason to enable threading. The oversized geometry scene remains too slow for practical gameplay.

| Sprite workload | Direct p99 | Threaded p99 | Ready p99 | Budgeted p99 |
|---|---:|---:|---:|---:|
| 20k sprites | 32.43 ms | 30.83 ms | 24.33 ms | 26.48 ms |
| 30k sprites | 49.25 ms | 38.37 ms | 33.33 ms | 32.98 ms |

**Both sprite acceptance targets pass:** budgeted readiness retains at least 90% of ready throughput and has p99 below 105% of direct. This is not evidence that the budget is better: unbudgeted ready is faster at 20k and in the geometry stress case, with better p99 there too. At 30k, the two ready policies are close.

The lighter geometry p99 rises from **18.10 ms direct to 21.05 ms budgeted** while throughput remains unchanged. That is a remaining pacing cost, not a universal improvement.

## What changed and what the traces show

1. **Quiet benchmark controls.** `STATUS_REPORTS=0 MEMORY_SAMPLES=0` suppress periodic Bunnymark status and synthetic snapshot output. A final snapshot sample is emitted after measurement; bounded timing histograms and sparse browser allocator polling remain. This corrects measurement overhead; it does not make application logging asynchronous.
2. **Opt-in readiness budget.** With schedule 4, `render.poc_web_ready_budget_ms=32` permits completion-callback consumption only when the age of the latest browser-tick credit plus the previous consumption duration fits 32 ms. Otherwise the frame waits for the next browser callback. Hidden/stop servicing bypasses the estimate. Zero preserves the existing policy; defaults are unchanged.
3. **Reusable comparison reporting.** The collector verifies the selected budget and quiet control. The report preserves every accepted repetition, separates traces from acceptance runs, checks queue/capacity bounds, and publishes only anonymous measurements.

The matched reporting-on/off diagnostic uses identical engine, content and runner bytes. At 30k sprites, periodic reporting produced **47.55 ms p99**, versus **35.45 ms quiet**, while throughput was **35.15 versus 35.75 updates/s**. The slowest 1% of worker intervals had approximately **37.41 ms versus 17.77 ms** in the simulation stage. The generated runtime confirms that worker `fd_write` synchronously proxies to browser main. Treat the previous logging-on p99 results as measurements of that reporting workload, not engine scheduling overhead alone.

The earlier ready-policy geometry collapse did not repeat under quiet measurement. Coarse traces locate heavy-scene delays mainly in render consumption and producer publication waits, but do not separate GPU/driver stalls, JavaScript GC and CPU execution. The underlying browser/driver contribution is not fully isolated, so this is not a proven fix for every cause of the old slowdown.

- Geometry stress, ready: only **3 of 270** whole-run consumptions occurred on completion callbacks. These counts include warmup and shutdown. The budget consequently had little outside-rAF work to change in this scene.
- Geometry stress, budgeted ready: only **3 of 250** whole-run consumptions occurred on completion callbacks. These counts include warmup and shutdown. The budget consequently had little outside-rAF work to change in this scene.

`budgetDeferrals` counts callbacks rejected by the estimate, including callbacks that would also lack render credit. It is not a count of prevented renders. Actual outside-rAF work is counted by `readyRenders`. A separate slow-draw correctness fixture reduced that count from **50 to zero** with a 1 ms budget, preserving identical pixels and simulation/preparation overlap.

## Memory and correctness

At 20k sprites, budgeted threading uses approximately **6.60 MiB** more live allocation than direct; at 30k, **8.84 MiB** more. The readiness budget itself adds no measured retained frame or queue capacity. All threaded synthetic checks retained **two frame slots and at most one submitted frame outstanding**. Its new bookkeeping is three doubles and one counter per loop, plus alignment, with no additional frame payload.

Live allocator bytes and WASM capacity overlap and must not be added. WASM capacity is unchanged between direct and threaded sprite runs here because both fit the same growth allocation; that does not mean their live memory is equal. Browser/JavaScript and GPU memory are not fully measured, and Android process RSS was unavailable.

Validation passed: eight native admission tests (58 assertions), focused JavaScript/Python collector and report tests, matching fixture pixels, animated/instanced geometry checks, simulation/preparation overlap, and context-loss shutdown. The three space-game replays matched deterministic checkpoints and cleanup with audio active. These are one compatibility run per selected mode, not a new repeated game-performance verdict. A replay launcher configuration error stopped an earlier attempt before measurement; that failed startup is preserved privately.

## Assessment and remaining production evidence

- Continue with opt-in component threading for sprite-heavy projects. The quiet repeated data strengthens its throughput and pacing case on this device.
- Keep the new budget disabled by default. Its control is functional and bounded, but the measured tradeoffs do not establish an advantage over unbudgeted readiness.
- Do not change universal defaults based on this collection. Low-load geometry has extra p99 cost, fill-heavy work gains little, and retained snapshot memory still costs several MiB.
- Before production: measure physical input-to-display latency and energy, validate other browsers/devices and sustained lifecycle/resource/extension scenarios, and investigate the remaining browser/driver behavior if readiness becomes a supported policy. Applications with frequent logging or other synchronous owner calls still need their own evaluation. Context-loss shutdown is not proof of context restoration.

## Report, graphs and reproduction

[Full tables, throughput/p99 graphs, memory graphs and temperature history](benchmarks/pixel-web-pacing-2026-10-06/REPORT.md) · [Target checks](benchmarks/pixel-web-pacing-2026-10-06/gates.json) · [Capacity checks](benchmarks/pixel-web-pacing-2026-10-06/capacity-gates.json)

**p99** is the 99th percentile of wall-clock update intervals: roughly 99% of intervals are at or below it. It is rounded upward to the 0.05 ms histogram bin and is not CPU utilization, displayed frame rate or physical input latency. Updates/s counts simulation intervals.

Synthetic runs request 10 seconds of warmup and measure 30 seconds, with tracing and stack watermarking disabled. Runs rotate mode order and keep normal Android thermal management active. Startup requires external power, thermal status 0/1 and at least 90% aggregate CPU idle; cooldowns and later throttling are preserved. The report shows per-run dots and temperatures rather than hiding variation. Diagnostic sets are: 1 original focused scenes; 2 original geometry stress; 3 initial quiet control; 4 matched reporting-on; 5 matched reporting-off.

Use the [runner instructions](../../scripts/web/benchmark/README.md#quiet-frame-pacing-evaluation) and [mode documentation](WEB_COMPONENT_POC.md). `pacing_report.py` generates the anonymous tables, graphs and gates from complete frozen collections. Raw game evidence, adapters, project contents, logs and bundle identities stay private and are not included in the published artifacts.
