# Web model-buffer ownership and memory attribution

The first optimization milestone is implemented and measured. **The previous
geometry allocation-growth signal is explained by reclaimable Lua garbage and
probe overhead in this workload.** The new model-buffer path removes an upload
copy, with a small memory saving and essentially unchanged throughput.

| Geometry workload | Direct | Threaded, copied | Threaded, owned model buffers |
| --- | ---: | ---: | ---: |
| Updates/s, mean | 67.33 | 84.17 | 84.69 |
| Update p99, mean ms | 16.43 | 15.42 | 15.27 |
| Peak sampled allocation, mean MiB | 34.52 | 46.38 | 46.11 |
| WASM capacity, MiB | 46.125 | 55.375 | 55.375 |
| Two frame slots, capacity MiB | 0 | 6.432 | 6.645 |

The ownership change saves **about 0.27 MiB** against threaded copied capture and
avoids copying **504,000 bytes per frame**. Its +0.6% throughput difference is too
small to establish a meaningful speedup with three repeats. Frame-owned arrays
add capacity while the producer loses its scratch buffers and the copied upload
arena shrinks; this explains why frame capacity rises while total allocation
falls. Allocator storage and WASM capacity are different metrics: this saving did
not reduce WASM capacity.

![Throughput and memory](benchmarks/web-owned-model-memory-2026-10-04/comparison.png)

## What changed

- Model batching borrows reusable arrays from the frame being built, writes
  generated vertex/CPU-skinned and instance data directly into them, and returns
  them before recording uploads. The component retains no corresponding scratch
  storage. Upload replay references the frame-owned arrays without copying them
  into the general payload arena.
- Separate dispatches/passes get separate arrays. Two immutable slots, resource
  mutation barriers, ordered uploads and bounded admission remain in place.
  Pose textures, meshes and other component types retain their current paths.
- A private web diagnostic samples allocator and Lua memory without building a
  Lua snapshot table. The geometry harness can perform full GC at both measurement
  boundaries on the Lua owner, outside its timed interval. Snapshot serialization
  can independently be disabled to isolate probe overhead.
- The collector records these settings and GC boundaries. Production acceptance
  reporting rejects forced-GC diagnostic runs. A dedicated report preserves
  performance and allocation attribution separately.

The new path is opt-in and defaults to zero. Add this to the existing broad
component-threading configuration:

```ini
[render]
poc_web_owned_model_buffers = 1
```

The experiment keeps completion dispatch and the **5 MiB worker stack** unchanged.
It does not change engine defaults, sprite work placement or frame pacing.

## What the GC experiment established

With snapshot serialization enabled, two-minute runs ended with only **96 bytes
of post-GC growth direct and 144 bytes in either threaded mode**. End-of-run GC
reclaimed approximately 0.62 MiB direct and 1.11 MiB threaded. With serialization
disabled, all three one-minute diagnostics ended **40 bytes** above their post-GC
starting baselines. Lua heap changes after collection were also below 150 bytes.

This supports ordinary garbage awaiting collection, amplified by the probe's
snapshot tables and JSON strings, as the explanation for the earlier slope.
Protected Lua calls also create temporary error-handler closures; this milestone
does not change that behavior or introduce forced GC into ordinary gameplay.
These fixed-population runs establish no retained growth of practical size over
the measured interval; they do not prove leak freedom across long sessions,
resource churn, other projects or extensions.

![Garbage collection boundaries](benchmarks/web-owned-model-memory-2026-10-04/gc-boundaries.png)

## Validation and scope

- 725 native tests passed: 97 render, 563 gamesys and 65 graphics. New regression
  cases cover retired-slot reuse, independent frames, pass ordering, opt-out
  behavior and capacity rejection.
- Six large-scene browser checks verify the intended mesh/model material paths
  at 200 and 1,000 groups. Four lifecycle modes produce identical frozen pixels:
  direct, copied overlap, owned overlap and owned overlap with a slow consumer.
  They cover animation, instancing, resource replacement and preparation overlap.
- 22 harness tests pass (15 Python and seven JavaScript).
- Nine foreground Release timing runs: three per mode, 10 seconds warmup and
  60 seconds measurement, with rotated order. Six separate foreground GC
  diagnostics use 120 seconds with serialization and 60 seconds without it.
  Chrome 154 on this Apple M1 Pro, hardware WebGL, AC power, fixed canvas, actual
  focus/visibility checks. No traces or stack watermarking during collection.

The scene has 2,000 meshes and 5,000 tiny-triangle models, including CPU/GPU and
instanced animation. It is useful for attribution, but its generated geometry is
small. A representative model-heavy game remains necessary. Direct uses the
modified Release engine with threading disabled, not clean vanilla. p99 measures
update intervals, not physical input-to-display latency. Memory sampling covers
allocator storage, not browser/JavaScript or GPU allocation.

**Production readiness is unchanged:** owned capture still allocates about 34%
more than direct here. The larger memory overhead and the earlier real-game
pacing regression remain. Sprite work placement and paced dispatch are the next
separate implementation experiments.

[Detailed report, run ranges and graphs](benchmarks/web-owned-model-memory-2026-10-04/REPORT.md),
[raw measurements](benchmarks/web-owned-model-memory-2026-10-04/raw-results.tar.gz),
[build hashes](benchmarks/web-owned-model-memory-2026-10-04/builds.json), and
[validation evidence](benchmarks/web-owned-model-memory-2026-10-04/validation.json)
are preserved. Frozen local bundles are `tmp/web-owned-model-bundle` and
`tmp/web-owned-model-fixture-bundle`. The screen-saver timeout was restored to
1,200 seconds; the original CMake content setting was restored after validation.
