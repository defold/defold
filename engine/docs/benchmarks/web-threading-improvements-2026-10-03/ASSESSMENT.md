## Assessment

**The window-state cache is a worthwhile improvement to this web PoC. The immediate post-render retry does not demonstrate an additional benefit worth choosing it for.** Keep threading and the experimental controls opt-in: the results now support a stronger throughput case for heavy sprites on this tested desktop, but they still expose memory and latency costs.

With the cache and completion dispatch, mean throughput versus the same engine's direct path changes by **+36.1% at 30k bunnies**, **+24.5% at 50k stationary sprites**, **+21.4% for balanced work**, and **−7.8% for fill-heavy rendering**. At 10k, every mode reaches the 120 Hz cap. The gains in the heavy sprite scenes occur in all three repeats, with clear separation from direct; these are run ranges, not statistical confidence intervals.

The cache removes one synchronous graphics-owner call per update in these preloaded scenes. With the original rAF scheduler, that call occupied 8.27 ms/update at 30k and 14.23 ms/update at 50k. It delayed simulation until main had finished rendering. Cached runs record zero such calls during measurement and can overlap the two stages. This is the main demonstrated improvement.

The retry policy recorded only **nine successful extra dispatches across its fifteen full runs**, including warm-up. Most runs recorded zero. Its small throughput differences should not be attributed to a successful new steady-state scheduling mechanism. The worker generally becomes idle after the immediate retry check has already executed.

For the measured workloads:

- **Heavy sprites:** cache + rAF is a useful starting point. At 30k it gives 112.07 updates/s versus 82.11 direct, and submission p99 improves from 13.63 to 10.70 ms. Cached completion has similar mean throughput but a worse 14.76 ms p99 in this case. At 50k, all cached schedules beat direct, with some variation between repeats.
- **Balanced simulation/rendering:** cache + completion gives 79.14 updates/s versus 65.21 direct. Cached rAF and retry give about 60. The submission p99 remains slightly worse than direct: 19.26 versus 18.27 ms.
- **Light or fill-heavy work:** direct remains preferable in this experiment. Light work gains no throughput from threading; fill-heavy work remains slower despite the cache.

Latency has improved relative to the old threaded path in the heavy sprite scenes, but it is still a tradeoff against direct. At 30k, dispatch-to-submit age falls from 24.84 ms with original rAF to 17.55 ms with cached rAF. At 10k, cached rAF/retry is about 11.55–11.57 ms versus 16.53 ms for completion dispatch. The older simulation-start metric excluded the window wait and therefore should not be used alone to compare these schedulers. These are CPU pipeline measurements, **not physical input-to-display latency**.

The changes do not reduce snapshot memory: instrumented 30k runs use 93.88 MiB of WASM capacity in every threaded mode versus 78.19 MiB direct. Without diagnostics, cached completion retains the gain (111.80 versus 82.79 updates/s), but its sampled WASM capacity is 79.81–95.81 MiB versus 66.50 MiB direct. Capacity grows in chunks and is not live allocation size. Browser CPU time per update also remains higher in the heavy sprite cases; the throughput improvement comes from overlap, not a demonstrated reduction in total work or energy.

The milestone therefore delivers the window-cache improvement and a measured negative result for the minimal retry experiment. It does **not** deliver one scheduler that combines completion's balanced-workload throughput with rAF's lower light-load age. The next scheduling experiment should admit completion-driven updates only when the current browser interval has an unused dispatch opportunity, then verify that it preserves the long-simulation gain without light-load run-ahead. Snapshot consumer/copy costs remain the next rendering optimization. A lower-powered device and a representative gameplay replay are still required before recommending this as a general web default.
