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

Accepted changes are already committed on `spinlock-and-atomic-opts` in
[PR #13634](https://github.com/defold/defold/pull/13634). This means accepted for
retention in that change; it does not imply the PR has been merged. The rejected
profiler candidate contributes no runtime implementation to the current tree.
Only this report and the measured data remain from that follow-up.

The profiler measurement boundary is a completed CPU task batch, including text
render submission through the null graphics adapter and font-layout job completion.
The lock counters and sampled lock durations are separate diagnostics. Read
[RESULTS.md](RESULTS.md) for the exact boundaries and raw data. Earlier noisy
profiler-workload results are withdrawn, as described there.

Job batching, callback-scoped name reads, buffered sample handoff, remaining atomic
flag reads and the pending-destruction-list lock have not been tested. They are
backlog items rather than accepted implementations.
