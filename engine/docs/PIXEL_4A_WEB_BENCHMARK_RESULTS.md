# Pixel 4a web benchmark results

**Follow-up measurement correction:** these synthetic runs include periodic
Bunnymark status and snapshot output during measurement. In this web build,
worker stdout synchronously calls browser main, so it can wait behind rendering.
The [frame-pacing milestone](WEB_PIXEL_PACING_RESULTS.md) repeats the comparison
with that output disabled and evaluates an opt-in readiness budget separately.
Keep the measurements below as historical evidence, but do not interpret their
p99 regressions as engine scheduling overhead alone.

Completed **75 accepted runs**: twelve real-game replays and 63 synthetic runs. Device: Pixel 4a, Android 13, Chrome 154.0.8037.126, Adreno 618. These are physical-device web measurements.

**Component threading is worth pursuing as an opt-in web feature for sprite-heavy workloads. These results do not support a universal default change.** The 20k and 30k sprite gains repeated in all three passes, with every threaded run faster than every direct run for its workload. The balanced scene also improved. Simulation-heavy work and the space game showed little benefit, while the ready-callback scheduler substantially regressed the geometry stress case.

Throughput is only part of the result. Higher update throughput sometimes came with worse long intervals. The ready policy helped the 30k sprite case relative to ordinary threading, but it worsened fill-heavy p99 and roughly halved geometry-stress throughput. Keep it experimental until those regressions are understood.

## Workload comparison

| Workload | Direct updates/s | Threaded updates/s | Ready updates/s | Threaded change | Ready change | Extra live MiB (threaded / ready) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 10k sprites | 56.11 | 59.88 | 59.93 | +6.7% | +6.8% | 4.48 / 4.49 |
| 20k sprites | 30.71 | 43.48 | 47.10 | +41.6% | +53.4% | 6.71 / 6.77 |
| 30k sprites | 21.74 | 28.87 | 33.55 | +32.8% | +54.3% | 8.77 / 8.78 |
| Simulation heavy | 18.35 | 18.41 | 19.10 | +0.3% | +4.1% | 2.43 / 2.44 |
| Balanced | 14.33 | 16.37 | 16.27 | +14.3% | +13.6% | 4.33 / 4.21 |
| Fill heavy | 23.35 | 24.87 | 24.52 | +6.5% | +5.0% | 4.68 / 4.68 |
| 2,000 meshes + 5,000 models | 1.44 | 1.50 | 0.74 | +4.4% | -48.8% | 8.71 / 8.65 |
| Space game | 44.98 | 44.57 | 45.19 | -0.9% | +0.5% | 5.82 / 5.80 |

![Throughput benefit and allocation cost](benchmarks/pixel-4a-web-2026-10-06/comparison-overview.png)

The table compares threading with **PoC direct**, the same modified engine with threading disabled. The game report additionally includes a clean-source, pthread-capable vanilla control. Synthetic cases do not include that separate vanilla build. All threading and scheduling options remain opt-in.

Values are means of three runs; allocation values are means of sampled per-run peaks. Extra live allocation was about **6.7 MiB at 20k sprites** and **8.8 MiB at 30k**, roughly **14–15%** above direct. Geometry added about **8.7 MiB / 25%**. WASM capacity stayed equal between modes in the sprite cases, but grew by about 9–11 MiB in some other workloads. Equal capacity does not mean equal live memory use.

## Pacing and regressions

| Workload | Direct p99 | Threaded p99 | Ready p99 | Assessment |
| --- | ---: | ---: | ---: | --- |
| 20k sprites | 37.23 ms | 41.35 ms | 41.23 ms | About 11% worse despite higher throughput |
| 30k sprites | 51.67 ms | 62.33 ms | 54.12 ms | Ordinary threading +21%; ready +5% |
| Balanced | 83.40 ms | 75.32 ms | 76.62 ms | Both improve throughput and pacing |
| Fill heavy | 52.05 ms | 53.03 ms | 72.02 ms | Ready p99 worsens about 38% |
| Geometry stress | 958.72 ms | >1,000 ms in all runs | >1,000 ms in all runs | Too slow for practical gameplay; ready throughput falls 49% |

Geometry collected only **20–46 update intervals per run**. Its out-of-range percentiles are bounds, not fabricated averages. The repeatable ready-policy slowdown warrants profiling, but these measurements alone do not identify its cause. A lighter geometry workload is needed before generalizing to ordinary 3D games.

The game comparison remained effectively flat: 44.98 updates/s direct, 44.57 threaded and 45.19 with ready scheduling. Vanilla averaged 46.50, with its cooler first run at 51.12 and later runs at 45.85 and 42.53. That variation makes a small refactoring or threading effect inconclusive in this replay. All twelve runs matched gameplay checkpoints and passed cleanup; neither those checks nor throughput certify visual correctness or input-to-display latency.

[Synthetic results, per-run points, p99 and memory graphs](benchmarks/pixel-4a-web-2026-10-06/synthetic/REPORT.md) · [Real-game results and graphs](benchmarks/pixel-4a-web-2026-10-06/replay/REPORT.md)

The next engineering priorities are to profile the ready-policy geometry slowdown and the sprite/fill long intervals, then validate a fix against this frozen comparison. Follow with a lighter geometry case and a representative sprite-heavy game on the phone. Energy, input latency, lifecycle/resource mutation and extension coverage remain separate production requirements.

## Interpretation limits

- Three rotated repeats per mode; synthetic runs have 10 seconds warmup and 30 seconds measurement. The game runs use 600 warmup ticks and 14,400 measured updates. These are not hours-long per-mode soak tests.
- Every run starts with Android thermal status 0/1 and at least 90% aggregate CPU idle. Later throttling is retained. The phone stays on external power; starting temperatures vary. This is a sustained warm-device comparison, not isolated CPU capacity or a battery-energy test.
- All twelve game replays matched checkpoints and passed cleanup. The earlier incomplete attempt and its different Chrome version are excluded from the new averages. The external game remains anonymous.
- The heavy geometry scene contains 2,000 meshes and 5,000 models. Some updates exceed the histogram’s 1,000 ms range. Those p99 values are explicitly unavailable with a lower bound; throughput and memory remain separately reported. Geometry was restarted under that explicit reporting policy, with no engine or bundle change. The other six synthetic cases continued under the original frozen runner; their accepted earlier files were copied byte-for-byte into a separate complete collection.
- **Updates/s** measures update throughput, not confirmed displayed frames. **p99** describes the slowest tail of update intervals, not physical input latency. **Live allocation** and **WASM capacity** overlap and must not be added; neither measures total process RSS or GPU memory.
- No energy, input-to-display latency, other mobile browsers or battery-only measurements were collected. One phone and these workloads cannot establish production readiness for all projects.

Temporary phone brightness, rotation and stay-awake settings were restored and verified. Benchmark port mappings, the local server and the host keep-awake process were removed. Raw game evidence remains in ignored local storage; published files contain anonymous measurements and no external game implementation details.
