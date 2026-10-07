# Pixel 4a web test status

**Historical initial-attempt status. The retry is now complete: see [75-run results and graphs](PIXEL_4A_WEB_BENCHMARK_RESULTS.md).** The observations below remain separate from the completed comparison.

**The four short correctness checks passed; the sustained performance comparison is incomplete.**

Vanilla, PoC direct, component threaded, and component threaded with the ready callback completed matched 1,800-tick replays and cleanup on the physical Pixel 4a (Android 13, Chrome 151, Adreno 618). Both threaded modes reported the expected activation.

The long comparison is held because the powered phone remains moderately thermally throttled while idle. Initial background-activity attempts were excluded. Only one long vanilla run is accepted under the final warm-device protocol; the other modes and the synthetic suite still need comparable measurements. No threading speedup or memory-overhead verdict can be drawn yet.

[Detailed status, baseline graphs and anonymous data](benchmarks/pixel-4a-web-2026-10-06/STATUS.md) explain the thermal observations, validation, metrics and remaining work. The graphs contain vanilla setup observations only, not a comparison between threading modes.

The frozen bundles and private raw evidence are preserved for resumption once suitable device conditions are available. External game implementation details remain excluded from publication.
