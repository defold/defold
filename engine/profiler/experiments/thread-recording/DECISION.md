# Profiler thread-local recording: REJECTED

**Decision: REJECTED for retention in the implementation.** No optional build mode
or runtime switch is retained for this candidate. The measured source snapshot is
commit `6943a4e5e37889fb51b4f41b70fd881385ee8584`. Its source and harness are archived
with the local report evidence in `profiler-recording-evidence-cd3b5e2ec2.bundle`.

The candidate cached each worker's existing sample-pool owner and internalized
names in TLS, published shared scope-name hashes atomically, and bypassed the
frontend/backend mutexes for individual scopes. Completed roots still delivered
callbacks synchronously under frontend-before-backend mutex ordering. Name-cache
misses and callback/context changes retained backend locking.

The corrected four-worker short render/layout batch with a sample consumer used
43.27% less wall time: separate baseline/candidate medians were 88.594/50.376 us per
completed batch. All nine paired changes were between -43.55% and -39.51%.
The heavier workload changed by -0.30%, with pair changes between -9.08% and +4.82%;
that result does not establish a gain. See [RESULTS.md](RESULTS.md).

The implementation required one fixed Basic listener and quiescent producers
before switching policies. It asserted on listener registration/removal,
initialization or finalization while frozen. These restrictions depend on a
guarantee supplied by the benchmark harness. A production listener/context
lifetime protocol and backend concurrency contract are absent. The measured gain
therefore does not justify retaining this implementation, even behind a flag.

Concurrency validation passed for the tested fixed-registry contract, including
cold shared names, an empty name, callback/property mutation, callback reentry,
overflow recovery and five worker creation/join waves. It does not validate
registration changes, concurrent teardown, other backends or other platforms.
The baseline restart assertion is a separate existing defect, not evidence that
the candidate handles restart. The candidate also adds a per-thread growing name
table and two TLS keys; cold allocation cost and binary size were not measured.

The thread-local cache, atomic hash publication and scope-lock removal were tested
as one candidate. None receives an independent acceptance claim from these timings.
A future implementation supporting the required lifecycle behavior would be a
new candidate with its own tests, measurements and accept/reject decision.
