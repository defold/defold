# Preserved diagnostic pilots

No run in the final 27-run performance matrix was rejected.

Earlier diagnostic attempts are preserved in `diagnostic-pilots.tar.gz`:

- `web-gameplay-diagnostics`: the replay harness correctly rejected a trace-enabled comparison. The adapter was then extended to require explicit diagnostic selection and mark those outputs; report contracts reject them as acceptance timings.
- `web-gameplay-diagnostics-v2`: the direct diagnostic completed. The threaded engine/replay also completed, but strict timestamp validation rejected six cross-owner values inverted by 0.000244140625 ms (one absolute-clock double ULP). A regression-tested tolerance of one microsecond was added. Both threaded diagnostic configurations were rerun in `web-gameplay-diagnostics-v3`, with no dropped records, successful validation and identical gameplay checksums.

Only v3 supplies the threaded attribution plots. Pilot measurements are not included in performance means. Raw timestamps were never rewritten.
