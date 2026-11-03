# Threading improvement decisions

Every tested candidate receives an **ACCEPTED** or **REJECTED** decision for
retention in the implementation. The evidence states the measurement boundary,
observed gain and validation limits. An inconclusive timing is an assessment of
the evidence, not a third adoption status. Untested ideas have no decision yet.

| Tested candidate | Decision | Evidence and reason |
|---|---|---|
| Internal atomic helpers, spinlock release and font/graphics/HTTP completion publication | **ACCEPTED** | Explicit publication ordering and focused validation; no complete-task speedup is claimed for the completion-flag changes. |
| Profiler initialized-state acquire reads | **ACCEPTED** | Lower guard overhead in isolated profiler workloads. |
| Basic profiler thread-name lookup under its existing lock | **ACCEPTED** | Removes a redundant recursive acquisition; isolated single-thread flat scopes with callback traversal improved 8.3%. |
| Texture metadata batching | **ACCEPTED** | Fewer asset lookups/acquisitions; isolated short effect-text vertex generation improved 6.0% and the resource upload path improved 28.6% with the asset mutex enabled. These are CPU-path measurements, not game-frame gains. |
| Generic spinlock load polling, yield and backoff | **REJECTED** | Throughput gains came with millisecond-scale sampled waits and unacceptable fairness in the tested contention cases. |
| Message registry prepare-first and check/prepare/recheck variants | **REJECTED** | No gain in the mixed creator/poster workload; prepare-first also worsened duplicate/full-registry failures. |
| Profiler thread-local recording with a frozen listener registry | **REJECTED** | A short complete-task batch improved 43.27%, but the implementation requires a harness-managed frozen registry and lacks production lifetime handling. Heavy-workload timing was inconclusive. See [decision](DECISION.md) and [measurements](RESULTS.md). |
| Release-mode logger listener-count acquire load | **ACCEPTED** | Complete suppressed `LogInternal` calls improved 11.52% with one caller and 98.33% in aggregate cost per call with four callers on the local Mac. This is the no-listener/no-file early-return path, not formatted logging or frame time. |
| Bulk or fused job submission | **REJECTED** | Bulk submission improved the short four-worker task fixture 4.00%, but results across real font-generation cases were mixed. Fused submission gave repeatable 1–2% gains in four-worker vector-font cases, insufficient to retain the broader job-system refactor and new helper. |
| Callback-scoped profiler name reads | **REJECTED** | Removed 46 recursive acquisitions per task batch without a meaningful complete-task gain. |
| Buffered profiler sample handoff | **REJECTED** | The tested bounded queue changed callback delivery and callback-context lifetime; it failed compatibility before timing. |
| Job-system, DNS and audio flag acquire reads | **REJECTED** | Complete tested operations showed no meaningful gain. |
| HTTP cancellation-predicate acquire read | **REJECTED** | The isolated predicate improved 30.69%; complete-request benefit was not established. |
| Lightweight pending-destruction-list locks | **REJECTED** | Neither a normal mutex nor Darwin's unfair lock improved the complete lifecycle consistently; both regressed the 128-world case. |

Accepted changes are retained on `spinlock-and-atomic-opts`, associated with
[PR #13634](https://github.com/defold/defold/pull/13634). Local follow-up commits
may precede the next push. Acceptance does not imply that the PR has been merged.
Rejected candidates contribute no runtime implementation or opt-in build targets
to the current tree. Their reports and measured data remain.

The profiler measurement boundary is a completed CPU task batch, including text
render submission through the null graphics adapter and font-layout job completion.
The lock counters and sampled lock durations are separate diagnostics. Read
[RESULTS.md](RESULTS.md) for the exact boundaries and raw data. Earlier noisy
profiler-workload results are withdrawn, as described there.

The remaining candidates above have now been tested locally. See
[follow-up decisions and measurements](FOLLOWUPS.md) for their exact boundaries,
controls, tradeoffs and validation limits.
