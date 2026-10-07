# Full-name field hash comparison

Measured 2026-09-30T15:58:52.632776+00:00. One million instances; optimized release builds, no sanitizers or allocation tracking during timing.

Six balanced AB/BA pairs. Each core run has one warmup and seven measured samples: **42 samples per version and case**. Each threaded run has 32 frames at 1, 2, 4 and 8 workers; the first two frames per worker count are excluded. No build or sanitizer workload ran alongside these timing runs.

Positive speed change means faster: `100 × (before / after − 1)`. The paired range is the minimum/maximum change across six paired run medians, not a confidence interval.

| Benchmark | Before (ms) | After (ms) | Speed change | Paired range |
| --- | ---: | ---: | ---: | ---: |
| Create population | 3.0085 | 3.0520 | -1.43% | -4.0% to +2.5% |
| Spawn wave | 1.6385 | 1.6750 | -2.18% | -9.3% to +2.5% |
| Despawn wave | 1.0550 | 1.0510 | +0.38% | -12.0% to +6.2% |
| Movement | 0.1720 | 0.1740 | -1.15% | -3.9% to +1.1% |
| Explosion | 1.3115 | 1.3185 | -0.53% | -2.7% to +1.1% |
| Nearby light contribution | 0.6400 | 0.6395 | +0.08% | -4.0% to +2.0% |
| Shuffled position lookup | 8.2430 | 8.1985 | +0.54% | -5.6% to +4.7% |
| Threaded update (4 workers) | 1.7635 | 1.7750 | -0.65% | -4.0% to +0.4% |

Every paired range crosses zero. The small aggregate shifts do not establish a consistent throughput improvement or regression. Paths were already resolved when matching tables; the row loop still reads through cached offsets. The change simplifies field identity and also supports the same full-name hash for ID getters, setters, reset and template overrides.

Threaded speed changes at 1 / 2 / 4 / 8 workers: -0.86% / -1.01% / -0.65% / -0.50%.

`DataQueryField` shrinks from 32 to 16 bytes. Metadata grows from 24 to 32 bytes per field, shared by all rows in a table; the extra local hash preserves structured input/views. Row bytes are unchanged. Per-operation allocation requests remain: Create population 22, Spawn wave 6, all other standalone cases 0. Query path allocations are removed. The threaded fixture initializes the process-wide hashing mutex outside per-store accounting.

Validation: 47 data tests and 9 threaded tests pass under TSAN (357,188 assertions). The full million-instance, 32-frame workload and allocation teardown pass TSAN at 1, 2, 4 and 8 workers. Core counts, hit counts and checksums agree across every before/after sample. The HTML examples compile as C11 and C++20.

The [current report](../../report.html) uses the final scheduled run (run 6, seven core samples), so its medians may differ slightly from the aggregate above. Defold timing, memory and library-size data were refreshed. Other backends retain their measured timings; workload outputs were revalidated.

[Raw measurements and commands](comparison.json) include binary digests. [Source change](change.patch) is relative to the working tree captured immediately before this experiment, including previously uncommitted work. Binary copies remain in the ignored `engine/data/build/full-field-hashes/{before,after}` directories.
