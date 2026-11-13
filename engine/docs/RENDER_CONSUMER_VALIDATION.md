# Renderer-side frame preparation: tests and benchmarks

Measured 2026-11-11. This report checks whether the renderer can prepare draws from
saved frame data, and what copying and passing that data costs. It covers this
stage of the implementation, not later graphics-service or resource changes.
Production threaded rendering is not enabled by this work.

## What is being compared?

Game code is the **producer**: it creates the inputs for a frame. Rendering is the
**consumer**: it reads those inputs, checks which objects are visible, sorts them
and groups compatible objects into draw batches. A **snapshot** is a saved copy of
the inputs, so game code can change its own data without changing that frame.

The benchmark compares three ways to run the current renderer:

- **Direct:** submit render items without copying a frame snapshot.
- **Snapshot inline:** copy the inputs, then consume them on the same thread.
- **Snapshot threaded:** copy the inputs, then consume them on a native test thread.

It also runs the direct path on an earlier revision, called the **baseline**.
The workload is synthetic; it is not a complete game or a GPU performance test.

## Outcome

- The existing direct path shows no clear slowdown against the baseline in
  this workload: median differences range from -1.7% to +0.14%.
- Capturing and consuming inline adds 0.39–7.65 microseconds/frame (0.4–4.8%)
  relative to the current direct path. The largest percentage is the 10,000-item,
  static, single-view case, where little preparation work needs repeating.
- The threaded test is slower than inline here. Its small game update leaves
  little work to run alongside rendering. This measures copying and passing work
  between threads, not a demonstrated game speedup.
- All modes and the baseline produce matching checksums of draw order, visible-item
  counts and batch counts. The measured storage stops growing after warmup.
- The review found no blocking issue in how the snapshot path owns and accesses
  CPU data. At this checkpoint, resource protection, built-in component migration
  and production thread integration remained separate work.

See [The renderer frame](RENDERER_FRAME.md) for the current frame flow.

## What the tests check

`AcceptedOverlappingFramesKeepSnapshotViewsIsolated` checks that preparing the next
frame cannot change one already being rendered. Threads coordinate using explicit
signals, not sleeps:

1. Pause frame N inside its first draw batch, then copy the inputs for N+1.
2. Confirm submitting N+1 waits for N instead of replacing an accepted frame.
3. Draw each frame from camera A, then B, then A again. Check visibility using both
   renderer-provided bounds and custom component callbacks.
4. Overwrite live inputs and delete camera handles after recording.
5. Reuse both frame-storage slots across four accepted frames, changing positions
   and component draw order. Delete the game collection while the last two
   snapshots still keep its renderer state alive.
6. Check input values, visibility, ordering, timing and cleanup. Verify frame
   references are released and renderer state is eventually destroyed.

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

These are focused tests, not a full engine test run. The local prebuilt Bob-light
generates model test data using an older schema, which limits broader gamesys
testing. Browser and real-GPU tests were not run for this checkpoint.

## Benchmark method

The opt-in benchmark lives in `engine/render/src/test/render_consumer_benchmark.cpp`.
It runs from `test_render`; ordinary unit-test invocation does not benchmark.

- Apple M1 Pro, 10 cores, 16 GiB RAM; macOS 26.5.2; Xcode 26.5.
- AC power; arm64 `RelWithDebInfo` / `-O2`; null graphics backend.
- Baseline: `b742c0f2f9`. Current renderer: `85d99ba154`, plus the new benchmark.
- Identical benchmark source on both revisions. A compatibility define adjusts
  access to renamed working-memory fields and excludes the snapshot path, which
  the baseline does not support. It does not change the direct workload.
- Each case: 100 warmup frames, then 5,000 measured frames. Three process runs per
  revision, alternating baseline/current order. No concurrent builds or tests.
- A separate 5,000-frame run times individual stages and checks memory capacity.
  Those extra measurements are not enabled for the end-to-end results table.
- Setup and warmup are excluded. Elapsed time includes finishing the final frame
  and releasing its CPU storage. Invalid test-setup runs and shorter trial runs
  are excluded.

Every path generates the same inputs and uses persistent render items, sphere-based
visibility checks, depth sorting and a checksum that includes batch order. Static
cases set item properties only on the first frame; moving cases refresh them every
frame. One or four camera viewing volumes (frusta) test visibility caching and view
changes.

Both snapshot paths copy the full synthetic input array every frame. The threaded
test uses two CPU storage slots so one frame can be built while another is consumed.

There is no geometry generation, texture upload, actual driver draw or GPU work.
Each synthetic input uses 32 bytes; this is not a proposed sprite layout or a sprite
memory estimate. The benchmark does not use real component capture callbacks or
Lua command generation. It measures renderer preparation and the frame queue.

## End-to-end results

Median elapsed time in microseconds/frame; lower is better.

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

These are the middle results from three runs, not confidence intervals. Operating
system scheduling can still affect them. For example, the baseline
1,000/static/four-view case ranged from 63.977 to 96.394 microseconds,
and the baseline 1,000/moving/one-view case from 25.605 to 28.507. The corresponding
current direct ranges were 63.269–63.445 and 25.620–25.984. Small differences do not
prove an improvement, and these results do not define an acceptable overhead for
all inline rendering.

## Where the time goes

Separate run with per-stage timers, 10,000 moving items and four views,
microseconds/frame:

| Phase | Snapshot inline | Snapshot threaded |
| --- | ---: | ---: |
| Generate game inputs | 9.166 | 9.784 |
| Capture/copy inputs and record commands | 5.419 | 5.664 |
| Acquire slot, including retirement/wait | 0.040 | 0.134 |
| Submit, including inline consumption or producer wait | 878.119 | 885.229 |
| Consumer setup and item updates | 90.718 | 93.135 |
| Consumer culling, sorting and batch dispatch | 787.265 | 785.673 |
| Consumer cleanup | 0.048 | 0.049 |

The submit row includes time also counted in the consumer rows: **do not add these
values together**. Draw preparation includes checksum work and drawing through the
null backend, which performs no real GPU work. Culling and sorting are not timed
separately.

**Retirement** means releasing a finished frame's CPU references and making its
storage reusable. Final retirement averages below 0.001 microseconds/frame here,
but measures only resetting the last slot. Normal per-frame retirement is included
in the acquire-slot row. This benchmark has no resource-release callbacks, so it
does not measure their eventual cost.

## Storage and allocation behavior

Allocated capacity in bytes, the same for static/moving and one/four-view cases.
**Scratch** means temporary working arrays that the renderer reuses:

| Tracked storage | 1,000 items | 10,000 items |
| --- | ---: | ---: |
| Fixed frame queue, snapshot inline | 35,418 | 323,418 |
| Fixed frame queue, snapshot threaded | 70,596 | 646,596 |
| Selected draw scratch arrays | 14,848 | 158,488 |
| Selected retained item/list/dispatch arrays | 193,632 | 2,204,592 |
| Source input array | 32,000 | 320,000 |
| Persistent item handles | 8,000 | 80,000 |

Inline uses one frame slot; threaded uses two. The measured renderer working arrays
and retained item/list/dispatch arrays have the same capacities in the direct and
snapshot paths.

For 1,000 or 10,000 items, snapshot copies total 32,152 or 320,152 bytes/frame with
one view, and 32,536 or 320,536 with four views. The code reserves four or seven
regions inside reusable frame memory, respectively. These are not separate heap
allocations.

No measured snapshot case needed an additional dynamic frame-memory block. In the
per-stage run, the tracked renderer arrays did not grow after warmup. These counters
do not cover every allocation, memory-allocator overhead or GPU memory. They do not
show that the whole engine performs zero heap allocations.

## Reproducing

Build the CMake `test_render` target using the engine build instructions, with
`WITH_THREADED_RENDERING=ON` to include the native test-thread case. Then run:

```sh
<test_render> --render-consumer-benchmark --benchmark-iterations=5000
<test_render> --render-consumer-benchmark --benchmark-iterations=5000 --benchmark-phases
```

Repeat end-to-end runs without per-stage timers before comparing revisions. For the
historical baseline, copy the same benchmark source/header and test entry point to
`b742c0f2f9`, and compile the benchmark source with
`RENDER_CONSUMER_BENCHMARK_BASELINE`. That mode runs only the direct control.
It exists for this comparison, not as a second production renderer implementation.
