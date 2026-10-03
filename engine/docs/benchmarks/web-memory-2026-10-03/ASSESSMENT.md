**The snapshot changes reduce real allocator usage while preserving the threaded throughput gain.** The optional 2 MiB worker stack saves another 3 MiB. Keep the default stack at 5 MiB until individual projects/extensions have been validated, and keep linear WASM growth opt-in.

| Workload | Direct live MiB (before) | Threaded before MiB | Compact + 2 MiB stack MiB | Live saving | Threading-overhead reduction |
| --- | ---: | ---: | ---: | ---: | ---: |
| bunny30k | 59.01 | 73.16 | 67.57 | 5.59 MiB (7.6%) | 39.5% |
| render50k | 61.94 | 85.14 | 75.62 | 9.52 MiB (11.2%) | 41.0% |

**At the unchanged 5 MiB default stack**, compaction/reservation alone saves 2.59 MiB at 30k and 6.52 MiB at 50k. Snapshot capacity falls from 9.04 to 6.45 MiB and from 18.04 to 11.52 MiB respectively. The 50k scene creates sprites incrementally, so its subsequent growth still has temporary copy costs and modest spare capacity.

With compact snapshots and the 2 MiB stack, throughput changes from **111.95 to 112.68 updates/s** at 30k and **65.58 to 66.34** at 50k. These small improvements are not the main result; the memory saving is consistent across repeats. Update p99 averages change from **14.00 to 14.53 ms** and **21.30 to 21.73 ms**, with overlapping run ranges. This is not evidence of a major throughput regression, but it does not prove zero latency cost.

The 4 MiB growth experiment lowers sampled WASM capacity from **95.81 to 76.00 MiB** at 30k and from **115.00 to 108.27 MiB on average** at 50k, compared with the old threaded configuration. Compared with compact + 2 MiB stack alone, its extra capacity saving is 3.81 MiB and 6.73 MiB respectively. It does **not** materially reduce live allocator usage; it reduces unused capacity. Its steady-state p99 is similar in this collection, but startup/extended-play growth stalls remain outside the measured interval.

The worker watermark touched 9,936 bytes in Bunnymark, 4,688 bytes in the 50k synthetic scene, 2,768 bytes in the sprite correctness fixture and 5,184 bytes in the broad-2D fixture. These are deepest observed writes during those particular worker lifetimes, not guaranteed worst-case stack requirements. Native geometry/lifetime tests and browser correctness tests pass; arbitrary native extensions and deep script/native call chains remain untested.
