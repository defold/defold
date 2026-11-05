# LZ4 update benchmark

See [RESULTS.md](RESULTS.md) for the recorded 1.10.0 to 1.11.0 comparison.

This POSIX benchmark compiles the actual `dmLZ4` wrapper against each release,
using HC level 9 and the same `-O2 -g -fno-omit-frame-pointer` flags. The separate
frame experiment builds upstream `lz4frame.c` with `LZ4F_MULTITHREAD=1` and links
`Threads::Threads`; none of these frame sources or dependencies enter the engine.
Use complete upstream releases, with matching raw-block and frame sources.
The benchmark compiles the supplied source tree as-is. Defold's production build
separately applies [defold.patch](../defold.patch); the initial release comparison
in the report uses unpatched upstream sources.

From the repository root, with LZ4 1.10.0 and 1.11.0 unpacked locally:

```sh
python3 external/lz4/benchmark/run.py \
  --before /path/to/lz4-1.10.0/lib \
  --after /path/to/lz4-1.11.0/lib \
  --output /tmp/lz4-benchmark
```

The runner uses only Python's standard library, CMake, Ninja, and a C/C++ compiler.
It measures five rounds, alternating the releases and reversing order each round.
Each compression/decompression measurement lasts at least 0.2 seconds. Run on an
otherwise idle machine. Results are medians with min/max values in `summary.json`;
`samples.jsonl` preserves every measured round. MB/s uses uncompressed decimal MB.
Pass `--input /path/to/resource` repeatedly to measure actual compiled resources.
Use `--formats raw raw-reuse --levels 9 11 12` to compare compression levels
and independently reset, reusable HC states. Defaults remain level 9 with
one-shot raw blocks and the frame experiment.

The default corpus contains 4 KiB and 64 KiB slices of repository test scripts,
a concatenation of model/font/RGBA/audio fixtures, the same mixed data repeated to
64 MiB, and seeded random bytes. `summary.json` records sizes and SHA-256 hashes.
These are library measurements, not end-to-end Bob or game loading benchmarks.

The `raw` mode at level 9 uses the engine wrapper and its one-shot allocation
behavior. Other raw levels call `LZ4_compress_HC()` directly with the requested
level. The `raw-reuse` mode retains an HC state and calls
`LZ4_resetStreamHC_fast()` before each block, followed by
`LZ4_compress_HC_continue()`. It shares no history between blocks. Before timing,
the benchmark seeds a different file's content into the state, resets, and checks
that output matches one-shot compression byte for byte and decodes without a
dictionary. This models one state owned by each Bob worker, without changing
production compression or archive readers.

Frames reuse a context,
64 KiB linked blocks, HC level 9, and no frame/block checksums. Each invocation
records the first compression separately, including lazy worker startup and buffer
allocation. The timed loops then measure warm compression/decompression. Frame
decompression remains single threaded. The benchmark checks exact round trips and
compares threaded frame bytes against a one-worker reference before timing.
Linked frames with zero workers can differ from frames with one or more workers.
HC and frame context creation is outside the timer; one-shot raw allocations
remain inside the timed compression calls.

`context_bytes` reports the base one-shot raw HC state size (`LZ4_sizeofStateHC()`),
the retained HC state size for `raw-reuse`, or the allocated frame compression
context size including worker buffers (1.11.0 only). It excludes input,
output, thread stacks, and allocator overhead, and is not a process RSS measurement.
The base HC state size is unchanged, but 1.11.0's one-shot compressor allocates an
extended state of `LZ4_STREAMHC_EXTSIZE` bytes: the base size plus 65,584 bytes.
That additional allocation also applies at Defold's level 9, even though the extra
workspace is used by levels 10 and above. This cost is separate from frame workers.
LZ4 1.10.0 also allocates temporary workspace during levels 10+ compressions;
the retained state count does not include that transient workspace.
