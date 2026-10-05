# Validation summary

Two rejected development attempts were excluded from the accepted measurements:

1. An initial scheduler consumed before waking simulation. The synthetic slow-consumer fixture detected the loss of overlap. The final version wakes before consuming on both browser and completion callbacks.
2. An initial replay diverged between engine modes. The private adapter was corrected to remove nondeterministic timing/randomness interactions. All twelve accepted runs then matched all 25 checkpoints. Project-specific causes and changes are not published.

Validation recorded at collection time:

- Two native render-admission tests, eight assertions.
- Fifteen JavaScript launch/replay/diagnostic contract tests, including one private adapter test.
- Four private Python adapter/report tests.
- Four synthetic browser fixture modes: direct, ready scheduling, ready with a stalled consumer, and ready with context loss. Checks included matching frozen pixels, motion/instancing, input and sound completion, pause/resume, preparation overlap, shutdown and the consumption bound.
- Twelve sustained foreground gameplay runs, three repeats of each engine mode, with deterministic checkpoint and scene-cleanup checks.
- Vanilla source verification compared 5,408 engine/build files against the baseline revision with no differences or missing files.

These are historical validation results. The private adapter, its tests, raw logs and full provenance are retained locally outside the publishable evidence. The public source no longer contains the adapter-specific tests. The PoC was built from the recorded parent commit plus uncommitted engine changes; the short build configuration is not a complete reproducibility manifest.

All modes used the same optimized pthread-capable platform, common probes and project archive. Each repeat started a fresh Chrome process. CPU tracing and stack painting were disabled. Foreground checks ran on AC power with audio processing enabled and output muted. After collection, the normal synthetic build was restored, temporary HTTP servers stopped, and the previous screen-saver timeout restored. The original project was not edited.
