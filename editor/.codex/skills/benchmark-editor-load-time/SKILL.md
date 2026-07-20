---
name: benchmark-editor-load-time
description: Run repeatable Defold Editor project load-time benchmarks against the configured load-test project and summarize phase and total timings. Use when asked to measure, compare, or investigate the load-time performance impact of a Defold Editor change, including requests for detailed load metrics.
---

# Benchmark Editor Load Time

Run each measured sample in a fresh Leiningen REPL from the Editor project root.

## Standard benchmark

1. Preserve the user's working tree and revision. For an impact comparison, measure both the baseline and candidate under the same conditions without changing either code state unless explicitly authorized.
2. Let the spawned process inherit `DM_DEV_LOAD_PROJECT_PATH` from the user's `shell_environment_policy`. Do not set, inspect, or validate it. When it is unset, let the `load-project` module use its default project path.
3. Start an interactive REPL with:

   ```sh
   lein with-profile +performance repl
   ```

4. Wait for the REPL prompt, then evaluate:

   ```clojure
   (require 'load-project)
   ```

5. Allow up to 10 minutes for the load to finish. Stream or poll the output while it runs; each completed phase emits a performance record. The sample is complete when the `total` performance record appears and the REPL prompt returns.
6. Capture the complete phase output, including the total elapsed time, elapsed time excluding GC, allocation, and heap figures.
7. Exit the REPL and repeat until there are three successful samples. Start a new JVM for every sample; do not evaluate the `require` twice in one REPL, because the loaded namespace retains state and the second evaluation is not a valid sample.

Do not count a failed or interrupted run. Keep the code state, command, project path, hardware, power conditions, and relevant background workload consistent across samples.

## Detailed metrics

Only enable detailed metrics when the user requests a deeper breakdown. Start the REPL with both profiles:

```sh
lein with-profile +performance,+metrics repl
```

Require `load-project` as above. After it completes, inspect `load-project/load-metrics` when the collected breakdown is needed.

Treat metrics-enabled runs as diagnostic. Detailed tracking skews the timing numbers, so never mix them with or compare them directly to standard benchmark samples. Use standard runs to measure performance impact.

## Report results

- Report every successful run rather than only an aggregate.
- Summarize the median of the three runs for total elapsed time and elapsed time excluding GC; include the range to show variation.
- Identify phase-level regressions or improvements from the emitted phase timings.
- For baseline-versus-candidate comparisons, report absolute and percentage changes between the medians and state whether metrics were disabled.
- If a matched baseline is unavailable, clearly label the result as a measurement of the current code state rather than an impact comparison.
- Note failed runs, environmental disruptions, or other caveats separately.
