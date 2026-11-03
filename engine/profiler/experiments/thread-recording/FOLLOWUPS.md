# Remaining threading candidates: decisions and measurements

Measured locally on 2026-11-03. **Accept the release-mode logger gate change;
reject the other candidates for adoption.** The only production change from
this round replaces the listener-count read in `LogInternal` with the existing
internal acquire-load helper. A regression test covers release-mode listener
delivery. No experimental runtime modes, APIs or build targets are retained.

These decisions concern retention on `spinlock-and-atomic-opts`. A rejected
candidate can have a measured benefit in a narrow case; that does not establish
that its implementation and maintenance costs are justified across its callers.

## Decisions

| Candidate | Decision | Evidence and adoption reason |
|---|---|---|
| Logger listener-count read | **ACCEPTED** | Repeatable improvement in complete suppressed log calls: 11.52% with one caller; 98.33% in aggregate wall cost per call with four callers. One internal read changes; registration, dispatch and lifecycle locking remain intact. |
| Bulk job creation/submission | **REJECTED** | Short four-worker text/layout task batch improved 4.00%; one-worker timing regressed 0.56% and heavier work improved only 0.31%. The real font caller required scratch arrays and gave mixed, scheduling-sensitive results. |
| Fused single-child job submission | **REJECTED** | Repeatable 1–2% gains in four-worker vector-font cases, little SDF benefit. Retention would factor three core job operations and add an internal cross-library helper for one caller. The small gain does not justify that broader change in this round. The smaller alternative using the existing recursive APIs gave no reliable gain. |
| Profiler callback-scoped name reads | **REJECTED** | Eliminates 46 recursive acquisitions per task batch, but complete-task changes are around zero. A TLS marker and callback-entry bookkeeping do not pay for themselves here. |
| Buffered profiler sample handoff | **REJECTED** | The tested bounded asynchronous queue changed delivery timing, callback-context lifetime and overflow behavior. It failed the compatibility gate; no speedup is claimed. |
| Job-system initialized-state read | **REJECTED** | Complete-task changes range from negligible to noise. The four-worker short task improved 0.12% with profiling and 0.32% without it. |
| DNS completion/cancellation reads | **REJECTED** | Complete local hostname lookup improved 0.03%; thread creation and existing polling dominate. Lifetime reference-count operations were preserved. |
| Audio running/pause/status reads | **REJECTED** | No gain in complete threaded `Update()` status queries with the null device: +0.14% paired wall change. Pause/resume and shutdown checks passed. This does not measure mixing or decoding. |
| HTTP cancellation-predicate read | **REJECTED** | The actual isolated predicate improved 30.69%, but no complete HTTP request or transfer benefit was measured. Helper timing alone is insufficient adoption evidence. |
| Pending-destruction-list mutex replacements | **REJECTED** | Normal and Darwin unfair mutexes both regressed the complete 128-world lifecycle by roughly 19–20%; one-world cases were noisy. Try-lock deferral behavior was preserved. |

## Measurement boundaries and protocol

Host: Apple M5 Max, 18 logical CPUs, macOS 27.0.1, AppleClang
21.0.0.21000334, `-O2 -g`, assertions enabled. Workers were unpinned.
This round contains no MINIPC or other-platform results.

The source baseline is `554a5b68a85a9dd9db5a6edd8eed0c68a5ec23c0`, whose
production code matches `a91f13e60923d6e1a070df4214258ba90992e0e6`.
All candidate sources and CMake hooks were kept outside the repository. They
select original/candidate operations in one binary at producer quiescence.
Unchanged-operation controls use the same policy labels. The temporary hooks
were removed after measurement.

Each process alternates ABBA and BAAB phases. A pair averages its two baseline
phases and two candidate phases, then computes `100 * (candidate / base - 1)`.
Reported changes are medians of those paired percentages; negative means less
time. Before/after values are separate medians of pair averages, so their ratio
need not reproduce the paired percentage. Confirmation tables pool pairs from
the stated repeated processes. Ranges, where shown, are observed pair ranges,
not confidence intervals. No concurrent builds or other test runs ran during
the timing processes. OS scheduling and background activity still affect them.

| Fixture | Timed interval | Outside the interval |
|---|---|---|
| Text/layout task batch | 16 text submissions, text vertex generation and render-list work through the null adapter; enqueue and finish 32 real `TextLayout` jobs when workers exist; wait for workers to finish their profiling scopes and dispatch completion callbacks | Fixture construction, warmup, output hashing and teardown. No GPU work or full game frame. |
| Font generation | `FontGenCreateJobData`, start, caller-assisted update through completion, wait for workers to retire and destroy the batch | Font loading, job-context setup, output hashing and glyph-result cleanup. Real NotoSans outlines or SDF bitmaps, not just a queue operation. |
| Suppressed logger | Complete `LogInternal(INFO, ...)` calls in release mode, with no listener and no log file | Worker startup. Formatting and I/O never execute because this API path returns early. |
| DNS | Complete `GetHostByNameT("localhost", IPv4)` call, including worker creation, existing polling and join | Address validation and fixture setup. No remote DNS endpoint. |
| Audio | Complete threaded `dmSound::Update()` status-query call with the null sound device and actual audio thread | Initialization, deterministic pause/resume checks and finalization. No mixing/decoding throughput measurement. |
| HTTP predicate | Real `HasRequestTimedOut` helper on a real client structure, through a non-inlined call, with no cancellation or expired timeout | Fixture setup and separate cancellation/expiry checks. No request/transfer is included. |
| Pending destruction | Create, publish, defer, service and free 1 or 128 component worlds, including the game/render-thread handoff and completion signal | Context/thread setup. Worlds close before their first render; no GPU work or initialized-world destroy callbacks. |

Both monotonic wall time and process CPU time are retained in the raw data.
Tagged acquisition counts and sampled lock wait/hold durations come from
separate instrumented runs. Those durations are diagnostics, not task timings.

## Accepted logger change

The old `dmAtomicGet32` is a read-modify-write on this platform. The accepted
change uses an acquire load for the read-only release-mode gate. Listener-count
updates remain atomic read-modify-write operations; the listener array remains
protected by its existing lock. There is no allocation, data-layout or API change.
MSVC keeps the stronger operation used by the existing helper implementation;
the local gains below are not a Windows performance claim.

Confirmation used 17 pairs per process, one million calls per caller per phase,
and three processes per cell. Four-caller wall time is the makespan divided by
**all four callers' calls**: an aggregate throughput cost, not an individual
call's latency. Clock/gate overhead is amortized over the phase.

| Callers | Baseline wall ns/call | Acquire wall ns/call | Paired change | Baseline CPU ns/call | Acquire CPU ns/call | Unchanged-control change |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 1.751 | 1.543 | -11.52% | 1.752 | 1.543 | +0.14% |
| 4 | 25.040 | 0.417 | -98.33% | 98.481 | 1.656 | -0.09% |

Each of the three one-caller runs improved 11.20–11.83%; each four-caller run
improved 98.33–98.34%. The large four-caller difference reflects removal of
shared read-modify-write contention on this very short early-return path.
These numbers describe suppressed calls only. They do not describe formatted
log delivery, disk/network logging or an engine-frame speedup.

## Profiler and job task results

The task fixture uses 13 pairs and 1,000 completed batches per phase. Profiling
mode 2 includes sample-tree traversal and name reads. Mode 0 leaves profiling
uninitialized. Short/heavy jobs use work multipliers 1/8. Zero workers omit the
32 jobs, making that row an inactive control for job candidates.

| Candidate | Workers / profiling / work | Baseline µs/batch | Candidate µs/batch | Paired change |
|---|---|---:|---:|---:|
| Callback name reads | 0 / 2 / short | 36.941 | 36.977 | -0.23% |
| Callback name reads | 1 / 2 / short | 98.649 | 98.881 | +0.37% |
| Callback name reads | 4 / 2 / short | 84.999 | 84.981 | -0.12% |
| Callback name reads | 4 / 2 / heavy | 178.263 | 178.376 | -0.05% |
| Bulk jobs | 1 / 2 / short | 99.282 | 99.766 | +0.56% |
| Bulk jobs | 4 / 2 / short | 88.542 | 84.940 | -4.00% |
| Bulk jobs | 4 / 2 / heavy | 178.316 | 177.625 | -0.31% |
| Job initialized read | 1 / 2 / short | 99.144 | 99.164 | -0.05% |
| Job initialized read | 4 / 2 / short | 85.981 | 85.863 | -0.12% |
| Job initialized read | 4 / 2 / heavy | 178.923 | 178.889 | +0.05% |
| Job initialized read | 4 / 0 / short | 47.916 | 47.774 | -0.32% |

The unchanged short four-worker control had a -0.06% median but a
-3.71% to +7.80% pair range. The callback candidate also produced an apparent
-3.94% change with profiling disabled, where its optimization does not run;
that noisy inactive result is not a gain. All remaining control rows are in the
raw data.

Separate tagged runs confirmed these acquisitions per completed short batch:

| Implementation | Profiler frontend | Basic backend (recursive subset) | Job system | Graphics assets |
|---|---:|---:|---:|---:|
| Baseline | 188 | 235 (46) | 226 | 34 |
| Callback name reads | 188 | 189 (0) | 226 | 34 |
| Bulk jobs | 188 | 235 (46) | 163 | 34 |
| Job initialized read | 188 | 235 (46) | 226 | 34 |

The callback candidate uses a TLS marker only while its existing recursive
backend lock is already held. Normal registry mutation, scope and frontend
locking remain. It removes real recursive calls, but their removal does not
translate into a meaningful complete-task benefit in these measurements.

### Real font caller and smaller job variants

The bulk font variant collects job descriptions and handles in temporary arrays,
sets their parent and publishes them under one lock, then wakes workers. Those
allocation/preparation costs are included. Results varied substantially: the
one-worker/eight-outline case appeared 58.95% faster, but its pair range was
-94.59% to +41.66%. That is not a reliable font-speedup claim. Caller-assisted
generation can cancel/take over queued jobs, so scheduling changes the path.

The fused variant avoids scratch arrays and combines create, parent assignment
and enqueue for each glyph under one acquisition, retaining one wake signal per
child. Confirmation used 13 pairs, two processes per cell, 128 batches per
vector-font phase and 16 per SDF phase:

| Workers / glyphs / output / cache | Fused paired change | Unchanged-control change |
|---|---:|---:|
| 0 / 8 / outlines / none | +0.59% | +0.13% |
| 1 / 8 / outlines / none | -6.78% | -4.37% |
| 1 / 64 / outlines / none | -2.41% | -1.72% |
| 4 / 8 / outlines / none | -1.66% | -0.24% |
| 4 / 64 / outlines / none | -1.36% | +0.11% |
| 4 / 64 / outlines / all | -0.70% | -2.22% |
| 4 / 64 / outlines / mixed | -1.94% | +0.94% |
| 4 / 64 / SDF / none | -0.47% | +0.16% |

All-cache and zero-worker rows do not execute the fused operation. The
one-worker controls are especially unstable; differences there cannot be
attributed wholly to the candidate.

A final, longer check used 512 batches per phase, two processes per cell. It
also tested a smaller implementation that takes an outer mutex and calls the
three existing recursive APIs. That reduces ownership transfers but adds a
recursive call; it does not reduce the total number of acquisitions.

| Workers / glyphs / cache, outlines | Fused | Smaller nested-lock version | Unchanged control |
|---|---:|---:|---:|
| 1 / 8 / none | -7.29% | -2.33% | -8.56% |
| 4 / 64 / none | -1.64% | +0.84% | +0.09% |
| 4 / 64 / mixed | -2.13% | +0.50% | -0.51% |

The four-worker fused improvement survives this check. It is a real, narrow
benefit considered in the rejection decision, rather than dismissed as noise.
The smaller implementation does not provide it. No bulk or fused job API is
retained.

## Other atomic readers

| Operation | Baseline | Candidate | Paired wall change |
|---|---:|---:|---:|
| Complete local hostname lookup | 2,507.766 µs | 2,503.544 µs | -0.03% |
| Threaded audio status `Update()` | 1.591 ns/call | 1.623 ns/call | +0.14% |
| Isolated HTTP cancellation predicate | 1.615 ns/call | 1.099 ns/call | -30.69% |

DNS used 13 pairs of eight lookups per phase. Audio and HTTP used 17 pairs of
500,000 calls per phase. DNS finished-reference increments still determine
which owner frees the lookup context. Audio running/pause/status writers and
reset exchanges remain unchanged. The HTTP result is explicitly a helper-only
measurement, insufficient to establish request-level benefit.

## Pending-destruction-list lock

This fixture compiles the actual component-world pending-list code and render
execution implementation with the existing threaded-rendering capability. It
compares the current recursive mutex with a normal pthread mutex and Darwin's
`os_unfair_lock`, both preserving try-lock failure/deferral behavior. The
private adapter adds dispatch overhead, so these are tested implementation
comparisons, not intrinsic lock costs or a universal ranking.

| Replacement | Worlds per lifecycle batch | Baseline µs/batch | Candidate µs/batch | Paired change | Observed pair range |
|---|---:|---:|---:|---:|---:|
| Normal mutex | 1 | 3.471 | 3.922 | +8.78% | -24.86% to +32.79% |
| Normal mutex | 128 | 5.776 | 6.725 | +18.73% | +7.49% to +23.92% |
| Unfair lock | 1 | 3.399 | 3.117 | -5.97% | -23.45% to +16.19% |
| Unfair lock | 128 | 5.578 | 6.688 | +20.34% | +7.29% to +33.47% |

Each cell uses 11 pairs of 500 batches per phase. Unchanged controls were
+2.96% for one world and -0.82% for 128. A deterministic contention check holds
the pending-list lock while the render thread completes a service attempt;
service must defer, then drain after release. All variants pass this behavior
check. The synchronous rendering mode does not need this lock. These results
provide no reason to replace it or introduce a new portable primitive.

## Buffered handoff compatibility gate

The probe deep-copies sample subtrees into an eight-slot queue and traverses
them through the actual sample getters on a consumer thread. Condition signals
control delivery without sleeps. It observes:

| Observation | Current synchronous callback | Tested queue |
|---|---:|---:|
| Callback delivered when the outer scope ends | 1 | 0 |
| Callback into the old logical context after unregister/release | 0 | 1 |
| Delivery of 32 roots with consumer paused and capacity eight | 32 | 8 delivered, 24 dropped |

The old context remains physically allocated with a logical lifetime marker;
the test does not deliberately access freed memory. This rejects the tested
drop-in queue. A compatible design would need callback replacement/draining,
context ownership and backpressure rules. That different design has not been
implemented or benchmarked here. No performance number is assigned to a
candidate that fails this gate.

## Validation and retained evidence

The accepted logger code passed all **11 tests** in each native, AddressSanitizer
and ThreadSanitizer build. The new release-mode test uses logger finalization's
thread join to drain delivery before checking output. The full suite initially
hit a sandbox socket-bind restriction; rerunning with the required local socket
access passed all three builds. No sleep-based assertion was added.

Both fused and nested font candidates passed all **39 font-generation tests**
in native, ASan and TSan builds. Additional candidate checks passed in all three
modes for glyph output/cache behavior, pending-list contention, logger guards,
DNS cancellation/worker cleanup, audio pause/resume/shutdown and HTTP
cancellation/timeout values. Profiler checks cover callback reentry, concurrent
callback/property changes, cold shared names, sample overflow and recovery.
Task output checks compare vertices, layout results, callback/node counts and
name/output hashes between policies.

Sanitizer scope matters: the standalone profiler probes and logger suite use
instrumented dlib dependencies. Full task/font/pending fixtures instrument
their copied implementation and harness sources but link some normal font,
render or other dependency archives. These checks do not certify the entire
engine or untested platforms/backends.

The final code critique found no actionable issue in the retained logger change:
the guard does not replace dispatch locking, no ownership/lifetime transition
changes, and no public declaration or dependency is added. Existing accepted
branch changes remain intact. Clang-format 23.1.4 ran on the touched logger
lines and test block; Uncrustify 0.83.0 matched those regions while preserving
surrounding alignment. `git diff --check` passed. GitHub inline feedback,
submitted reviews and conversation comments were checked separately; the
existing PR had no actionable review requests at the final refresh.

Raw evidence is in [results/macos-arm64-candidates-2026-11-03](results/macos-arm64-candidates-2026-11-03):

- `samples.csv`: all 5,564 phase samples from 103 cases; blank columns mean a
  field does not apply to that fixture. Checksums are decimal strings.
- `pairs.csv`: all 1,391 baseline/candidate pair averages and percentages.
- `summary.csv`: per-process medians, including every control and initial run.
- `locks.json`: separate tagged acquisition and sampled duration diagnostics.
- `validation.json`: individual checks, suite summaries and handoff observations.
- `metadata.json`: case parameters, phase iteration counts, compiler/source
  fingerprints and the hash of the separately saved `sources.tar.gz` archive.

Case names map to parameters in `metadata.json`. `additional/font` is the bulk
font candidate; `confirmation/font-fused` is the fused candidate;
`nested-confirmation` contains the final longer comparison. Pending variants
0/1/2 mean unchanged/normal/unfair. Cache modes 0/1/2 mean none/all/mixed.
Process CPU values are normalized by the same task/call denominator as wall
time. Pending p99/max columns describe sampled complete lifecycle batches.
All source archives and detailed stdout logs are saved with this task's local
artifacts; benchmark code is not part of the production tree.
