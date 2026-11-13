# Consumer-owned render preparation: validation

Measured 2026-11-11. This closes the CPU ownership and performance checks for the
consumer-preparation deliverable; it does not enable production threaded rendering.

## Outcome

- The existing direct path shows no clear regression against the branch base in
  this workload: median differences range from -1.7% to +0.14%.
- Capturing and consuming inline adds 0.39–7.65 microseconds/frame (0.4–4.8%)
  relative to the current direct path. The largest percentage is the 10,000-item,
  static, single-view case, where little preparation work needs repeating.
- The native threaded test is slower than inline here. The small synthetic game
  update gives it little useful work to overlap; this is not evidence of a game
  speedup. It measures the ownership split, copying and handoff costs.
- All modes and the baseline produce matching ordered checksums, visible-item
  counts and batch counts. Tracked storage stops growing after warmup.
- The ownership review found no blocking issue within the snapshot CPU path.
  Resource lifetime protection, real component migration and production executors
  remain separate work.

See [The renderer frame](RENDERER_FRAME.md) for the current frame flow.

## Regression coverage

The new `AcceptedOverlappingFramesKeepSnapshotViewsIsolated` test uses explicit
condition-variable events, not sleeps:

1. Pause frame N inside its first draw batch, then fully capture N+1.
2. Confirm publication of N+1 waits for N instead of replacing accepted work.
3. Execute three A/B/A camera passes per frame with both typed and custom culling.
4. Overwrite live inputs and delete camera handles after recording.
5. Reuse both storage slots across four accepted frames, changing positions and
   component draw order; delete the game collection while the last two snapshots
   still hold renderer references.
6. Check captured values, visibility, ordering, timing, frame cleanup, retirement
   and eventual renderer destruction.

Validation on arm64 macOS:

| Suite | Thread support disabled | Thread support enabled |
| --- | ---: | ---: |
| Render | 125 | 125 |
| Recorded commands | 22 | 22 |
| Render script | 72 | 72 |
| Frame queue | 25 | 25 |
| Component renderer state | 11 | 11 |
| Gamesys component rendering | 38 | 38 |
| **Total passing** | **293** | **293** |

Thread-specific cases return early in the disabled build; the enabled build
executes them. The new overlap regression also passed 50 consecutive enabled runs.
Another 65 compatibility tests passed with thread support disabled: 54 selected
sprite/constants/GUI/particle tests, eight gamesys profiler tests and three
profiler-render tests.

This is focused validation, not a full engine qualification. Full gamesys
validation is limited by the local prebuilt Bob-light generating model fixtures
with an older schema. Browser and real-GPU validation were not performed.

## Benchmark method

The opt-in benchmark lives in `engine/render/src/test/render_consumer_benchmark.cpp`.
It runs from `test_render`; ordinary unit-test invocation does not benchmark.

- Apple M1 Pro, 10 cores, 16 GiB RAM; macOS 26.5.2; Xcode 26.5.
- AC power; arm64 `RelWithDebInfo` / `-O2`; null graphics backend.
- Baseline: `b742c0f2f9`. Current renderer: `85d99ba154`, plus the new benchmark.
- Identical benchmark source on both revisions. The baseline compatibility define
  changes scratch-member access and excludes the unsupported snapshot consumer;
  the direct workload is unchanged.
- Each case: 100 warmup frames, then 5,000 measured frames. Three process runs per
  revision, alternating baseline/current order. No concurrent builds or tests.
- A separate 5,000-frame run enables phase clocks and capacity checks; those
  instrumented timings are not used in the throughput table.
- Setup and warmup are excluded; wall time includes completion and retirement of
  the final reader. Initial invalid fixture runs and shorter pilot runs are excluded.

Every path generates the same inputs, uses persistent render items, sphere
culling, depth sorting and an ordered batch checksum. Static cases apply unchanged
item setters only on the first frame; moving cases refresh them each frame.
One or four distinct frusta exercise visibility caching and repeated view changes.

The three current paths are direct legacy submission, owned snapshots consumed
inline, and owned snapshots consumed by a native test thread with two CPU slots.
The snapshot paths copy the full synthetic input array every frame.

There is no geometry generation, texture upload, actual driver draw or GPU work.
The 32-byte synthetic input is not a proposed sprite layout or sprite memory estimate.
The fixture bypasses actual component capture callbacks and Lua command generation;
it measures renderer preparation and the generic frame queue, not a complete game.

## End-to-end results

Median wall time in microseconds/frame; lower is better.

| Items | Motion | Views | Baseline direct | Current direct | Snapshot inline | Snapshot threaded |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 1,000 | static | 1 | 11.439 | 11.289 | 11.683 | 16.558 |
| 1,000 | static | 4 | 64.089 | 63.337 | 64.409 | 74.385 |
| 1,000 | moving | 1 | 25.784 | 25.622 | 26.050 | 31.852 |
| 1,000 | moving | 4 | 75.375 | 74.287 | 74.791 | 83.193 |
| 10,000 | static | 1 | 163.726 | 160.909 | 168.557 | 176.493 |
| 10,000 | static | 4 | 786.533 | 787.605 | 792.486 | 807.442 |
| 10,000 | moving | 1 | 297.213 | 293.579 | 300.603 | 310.914 |
| 10,000 | moving | 4 | 883.467 | 884.294 | 887.687 | 903.022 |

These are three-run medians, not confidence intervals. Scheduling noise remains:
the baseline 1,000/static/four-view case ranged from 63.977 to 96.394 microseconds,
and the baseline 1,000/moving/one-view case from 25.605 to 28.507. The corresponding
current direct ranges were 63.269–63.445 and 25.620–25.984. Do not interpret small
differences as proven improvements or a general inline regression budget.

## Where the time goes

Separate instrumented run, 10,000 moving items and four views, microseconds/frame:

| Phase | Snapshot inline | Snapshot threaded |
| --- | ---: | ---: |
| Generate game inputs | 9.166 | 9.784 |
| Capture/copy inputs and record commands | 5.419 | 5.664 |
| Acquire slot, including retirement/wait | 0.040 | 0.134 |
| Submit, including inline consumption or producer wait | 878.119 | 885.229 |
| Consumer setup and item updates | 90.718 | 93.135 |
| Consumer culling, sorting and batch dispatch | 787.265 | 785.673 |
| Consumer cleanup | 0.048 | 0.049 |

The submit row overlaps the consumer rows: **do not add these values together**.
Draw preparation also includes checksum work and empty null-backend drawing;
culling and sorting are not timed separately. Final retirement averages below
0.001 microseconds/frame here, but covers only the final slot reset. Normal
per-frame retirement is included in acquisition. This fixture has no retained
resource-release callbacks, so it does not measure their eventual cost.

## Storage and allocation behavior

Measured capacities in bytes, common to static/moving and one/four-view cases:

| Tracked storage | 1,000 items | 10,000 items |
| --- | ---: | ---: |
| Fixed frame queue, snapshot inline | 35,418 | 323,418 |
| Fixed frame queue, snapshot threaded | 70,596 | 646,596 |
| Selected draw scratch arrays | 14,848 | 158,488 |
| Selected retained item/list/dispatch arrays | 193,632 | 2,204,592 |
| Source input array | 32,000 | 320,000 |
| Persistent item handles | 8,000 | 80,000 |

Inline uses one frame slot; threaded uses two. Selected scratch/retained
capacities are the same for direct and snapshot paths in this fixture.

Snapshot copies/frame are 32,152 or 320,152 bytes for one view, and 32,536 or
320,536 bytes for four views. There are four or seven arena reservations/frame,
respectively. These are reservations inside reusable storage, not heap allocations.

All measured snapshot cases report zero new dynamic frame blocks. The phase run
reports zero growth frames for the selected scratch/retained arrays after warmup.
These counters do not track every allocation, allocator overhead or GPU memory;
they are not a claim of zero total heap allocation.

## Reproducing

Build the CMake `test_render` target using the engine build instructions, with
`WITH_THREADED_RENDERING=ON` to include the native test-thread case. Then run:

```sh
<test_render> --render-consumer-benchmark --benchmark-iterations=5000
<test_render> --render-consumer-benchmark --benchmark-iterations=5000 --benchmark-phases
```

Repeat throughput runs without phase clocks before comparing revisions. For the
historical baseline, copy the same benchmark source/header and test entry point to
`b742c0f2f9`, and compile the benchmark source with
`RENDER_CONSUMER_BENCHMARK_BASELINE`. That mode runs only the direct control.
It exists for this comparison, not as a second production renderer implementation.
