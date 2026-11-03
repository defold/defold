# Corrected local profiler measurements

**Decision: REJECTED for adoption.** The gain is measured, but the candidate does
not support required listener/context lifetime behavior. See [DECISION.md](DECISION.md).

Measured on 2026-11-03 on an Apple M5 Max (18 logical CPUs), macOS 27.0.1,
arm64, AppleClang 21.0.0.21000334, with `-O2 -g` and assertions enabled.
The input engine revision is `a91f13e60923d6e1a070df4214258ba90992e0e6`.
The exact experiment and input-source SHA-256 values are recorded in
[summary.json](results/macos-arm64-2026-11-03/summary.json).

The four-worker short workload with a sample consumer took 43.27% less wall time
in the prototype: separate baseline/prototype medians were 88.594/50.376 us per
completed batch. All nine paired changes were between -43.55% and -39.51%.
The eight-times-heavier layout workload changed by -0.30%, with pair changes from
-9.08% to +4.82%; this is inconclusive. One-worker short batches improved 10.69%.
These results apply to the rejected [frozen-registry candidate](DECISION.md).
They do not establish gains for a complete game or a production lifetime design.

Earlier scratch results for this complete render/job fixture are withdrawn.
Its material had no registered tag, causing repeated render errors on stderr
inside the timed interval. The old runner checked only stdout. The corrected
measurement fixture registered the tag and the runner rejected stderr and
diagnostics. Every number below comes from that clean run. Earlier atomic/texture-query
experiments that did not use this fixture are outside this correction.

## Complete workload timings

Each value is microseconds per completed CPU task batch. This includes text
submission, flushing, vertex generation, render-list drawing through the null
adapter, 32 layout jobs where enabled, worker-idle waiting and completion
callbacks. It excludes setup, warm-up, checksum generation and teardown.
It is neither lock-only time nor a GPU/full-game frame measurement.

Nine ABBA/BAAB pairs each contain four phases of 250 batches. The percentage
column is the median of the nine within-pair percentage changes. The displayed
wall/CPU times are separate medians of pair means, so their ratios need not equal
the median percentage. Workers were not pinned; other experiment builds and
checks finished before timing. The existing engine job scopes are used without
additional per-layout scopes.

Mode 0 is linked but uninitialized profiling, mode 1 records without a consumer,
and mode 2 visits completed sample trees synchronously. Work is layout repetitions
per job; zero workers means text rendering only.

| Workers | Work | Mode | Base wall us | Local wall us | Paired change | Base CPU us | Local CPU us |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 1 | 0 | 34.225 | 34.481 | +0.34% | 34.220 | 34.456 |
| 0 | 1 | 1 | 37.529 | 36.021 | -3.93% | 37.514 | 35.998 |
| 0 | 1 | 2 | 37.260 | 35.610 | -4.34% | 37.238 | 35.586 |
| 1 | 1 | 0 | 86.049 | 85.962 | -0.03% | 126.480 | 126.904 |
| 1 | 8 | 0 | 553.209 | 551.208 | -0.21% | 594.650 | 592.726 |
| 1 | 1 | 1 | 97.907 | 86.547 | -11.62% | 148.294 | 128.224 |
| 1 | 8 | 1 | 580.716 | 563.081 | -1.68% | 627.478 | 605.726 |
| 1 | 1 | 2 | 99.141 | 88.459 | -10.69% | 150.056 | 131.686 |
| 1 | 8 | 2 | 550.228 | 547.070 | -0.56% | 595.616 | 589.272 |
| 4 | 1 | 0 | 48.992 | 48.969 | +0.46% | 170.620 | 169.900 |
| 4 | 8 | 0 | 183.466 | 178.226 | +0.09% | 728.862 | 718.894 |
| 4 | 1 | 1 | 108.711 | 70.007 | -37.21% | 290.618 | 211.498 |
| 4 | 8 | 1 | 193.497 | 177.600 | -3.85% | 763.932 | 716.940 |
| 4 | 1 | 2 | 88.594 | 50.376 | -43.27% | 268.296 | 184.242 |
| 4 | 8 | 2 | 216.967 | 212.407 | -0.30% | 816.392 | 786.630 |

Sham controls switch two identical baseline policies. Their median paired changes
were +2.70% for text-only, -0.38% for four-worker short jobs and -0.25% for heavier
jobs. The text-only sham pairs ranged from -10.64% to +36.58%, so the small text-only
gain needs cleaner confirmation. Linked-but-uninitialized cases have no established
improvement. Full raw phases are in [samples.csv](results/macos-arm64-2026-11-03/samples.csv).

## Mutex diagnostics

These runs use separate instrumented executables, 500 batches per run and three
repetitions. Timers are sampled at approximately 1/64 acquisitions. They do not
supply the performance percentages above. Per-batch acquisition counts are stable
across repetitions and both job sizes:

| Consumer | Mutex | Base acquisitions | Local acquisitions |
|---|---|---:|---:|
| None | profile_front | 188 | 4 |
| None | profile_basic | 189 | 5 |
| None | graphics_asset | 34 | 34 |
| None | jobs | 226 | 226 |
| Sample visitor | profile_front | 188 | 37 |
| Sample visitor | profile_basic | 235 | 84 |
| Sample visitor | graphics_asset | 34 | 34 |
| Sample visitor | jobs | 226 | 226 |

With a consumer, 46 backend acquisitions per batch are recursive callback getter
calls in both implementations. Reducing acquisition frequency is the experiment;
no mutex is replaced with a spin lock. The graphics/job counts are unchanged.
Wait and hold observations, sample counts and tails are in
[locks.json](results/macos-arm64-2026-11-03/locks.json). Holds exclude condition sleep;
condition-wait reacquisition is absent from explicit lock wait measurements. Mach
clock resolution is about 41.7 ns here, limiting interpretation of tiny durations.
Percentiles/maxima use retained samples; sampled means include all observations.

## Validation and remaining work

[validation.json](results/macos-arm64-2026-11-03/validation.json) records 45 subprocess
checks: 43 passed and two matched specific expected assertions. The standalone
baseline/prototype and empty-name variant passed natively, under ASan and under
TSan. Every standalone run performs five waves of eight recorder threads while
callback and property mutator threads run concurrently, checks 8,194 callbacks and
24,609 nodes per wave, reenters property APIs from callbacks, and exercises pool
overflow/recovery. Full workload and policy-switch cases passed in native and
sanitizer variants. Standalone sanitizer builds use instrumented dlib; workload
sanitizers reuse uninstrumented font/render dependency archives.

Every timed phase preserved 3,456 vertices and vertex-byte checksum
`15413629891590012915`. Job results and completion counts matched. Callback counts,
node counts and weighted name checksums matched across policies; this does not
prove exact sample-tree topology or timestamp equality. The callback workload has
33 callbacks and 46 nodes per batch with jobs enabled.

The baseline's second initialization after finalization still asserts because its
property-initialization counter outlives its mutex. The prototype deliberately
asserts if finalized while frozen. These are expected assertion checks, not
successful lifecycle operations. Dynamic listener changes, concurrent teardown,
other backends/platforms and full game scenes remain untested.

The adoption review rejects the implementation because its frozen-registry
contract does not cover required listener/context lifetime behavior. The source,
patches, feature flag, extra build targets and measurement runners have been
removed from the current tree; the report and raw data remain. The exact measured
source snapshot is commit `6943a4e5e37889fb51b4f41b70fd881385ee8584`, archived with
the local report evidence in `profiler-recording-evidence-cd3b5e2ec2.bundle`.
The data records its source SHA-256 values.
The benchmark-time C/C++ files were formatted with clang-format 23.1.4 and
Uncrustify 0.83.0. The report-only correction adds or edits no C/C++ source, so
formatting is not applicable to that correction. The accepted runtime implementation
is unchanged.
