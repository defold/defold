# LZ4 1.10.0 → 1.11.0

Measured on 2026-11-05, Apple M5 Max (18 cores, 128 GiB), macOS 27.0.1 ARM64,
Apple clang 21.0.0. The benchmark uses the actual `dmLZ4` wrapper, HC level 9,
and identical `-O2 -g -fno-omit-frame-pointer` flags for both releases. Five
interleaved rounds per configuration, at least 0.2 seconds per operation per
round, 200 samples in the initial comparison. Values below are medians in decimal MB/s.

Unpatched upstream 1.11.0 does **not** get faster on this machine. Compression
is 0.6–4.1% slower, with unchanged compressed sizes. Mixed-asset decompression
is 9.1% slower. The new frame worker API scales well for large inputs, but it
cannot be enabled for existing raw-block resources without changing their format.
Defold's build now applies [defold.patch](../defold.patch) to restore HC inlining;
the initial before/after tables below refer to the unpatched releases.

## Engine path: raw blocks

| Input | Compression 1.10.0 | 1.11.0 | Change | Decompression 1.10.0 | 1.11.0 | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Scripts, 4 KiB | 541.3 | 518.9 | -4.1% | 15,907.8 | 15,100.8 | -5.1% |
| Scripts, 64 KiB | 185.6 | 179.7 | -3.2% | 12,081.0 | 12,084.8 | +0.0% |
| Mixed assets, 4,303,290 bytes | 135.5 | 133.6 | -1.4% | 16,875.9 | 15,332.0 | -9.1% |
| Mixed assets repeated to 64 MiB | 134.2 | 131.6 | -1.9% | 15,902.6 | 14,812.6 | -6.9% |
| Random, 4 MiB | 86.0 | 85.5 | -0.6% | 56,011.6 | 41,488.3 | -25.9% |

The compressed sizes are identical between releases for every corpus: 968,
13,125, 1,563,378, 23,978,111, and 4,210,712 bytes respectively. Bob normally
rejects compression when the output exceeds 95% of the input size, so the random
case exercises the compressor/decoder but would be stored uncompressed in a
normal archive. Some smaller differences overlap the observed sample ranges;
the mixed-assets and random-data decoder regressions do not. Full min/max values
are in the [summary data](results-m5-max.json).

The mixed corpus concatenates the repository's `Knight.glb`, `DejaVuSansMono.ttf`,
`testimage.rgba`, and `booster_on_sfx.wav` fixtures. Script slices come from the
gamesys test scripts. The 64 MiB case repeats those assets to expose worker
scaling. Corpus sizes and SHA-256 hashes are recorded in the summary. These
measurements isolate the compression library; they do not measure complete
project builds, I/O, JNA copying, or game loading.

## Frame experiment: worker threads

LZ4 1.11.0, HC level 9, 64 KiB linked blocks, checksums disabled, reused
compression contexts. Frame decoding is single threaded. Every threaded result
is decompressed and compared byte for byte to the original, and its compressed
bytes are compared against a one-worker reference before timing.

| Workers | 64 MiB compression MB/s | Speedup vs 0 | First compression ms | Allocated compression context MiB |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 131.7 | 1.00× | 510.7 | 0.44 |
| 1 | 130.4 | 0.99× | 516.4 | 3.69 |
| 2 | 260.7 | 1.98× | 258.7 | 6.00 |
| 4 | 505.2 | 3.84× | 134.2 | 10.63 |
| 8 | 889.6 | 6.76× | 76.0 | 19.88 |

All these frames occupy 23,988,387 bytes, 10,276 bytes more than the corresponding
raw block. Worker counts 1, 2, 4, and 8 produced identical frames. Linked frames
with 0 workers are not generally guaranteed to match threaded frames, although
their sizes match on this corpus.

On the 4.3 MB mixed input, four workers reach 373.7 MB/s (2.79× the zero-worker
frame result); eight workers add no benefit. On 4 KiB/64 KiB inputs, extra workers
provide no useful speedup. Upstream processes inputs fitting one job on the
calling thread, so requesting workers does not make small resources parallel.

Context sizes come from `LZ4F_cctx_size()`, excluding input/output buffers, thread
stacks, and allocator overhead. First-call times include lazy worker startup and
buffer allocation. They are separate from the warm throughput measurements.

## Memory and code size

The engine's one-shot `LZ4_compress_HC()` allocation grows from 262,200 to
327,784 bytes (+65,584 bytes, approximately 64 KiB per concurrent compression).
`LZ4_sizeofStateHC()` remains 262,200 bytes in both releases, but 1.11.0 allocates
`LZ4_STREAMHC_EXTSIZE` for the one-shot API, including workspace for levels 10+.
This extra allocation also applies to Defold's level 9.

The combined Mach-O `__TEXT` size of the two raw-library object files decreases
from 121,855 to 104,619 bytes (-17,236 bytes, -14.1%) with the benchmark flags.
This includes all raw-library functions and is not a final engine executable
size measurement. Frame sources and thread dependencies are not added to the
engine library.

## Integration decision

The engine and Bob continue to use raw blocks. Bob's Java `LZ4` class calls
`dlib_shared` through JNA; there is no separate Java LZ4 dependency to update.
`ArchiveBuilder.write()` already compresses separate resources in a thread pool.

The new API is experimental `LZ4F_preferences_t.nbWorkers`, requiring frame
sources and `LZ4F_MULTITHREAD=1`. Defold currently stores each compressed archive,
live update, or font-glyph resource as a raw block. A production frame integration
would need format identification and compatible readers, plus a worker policy
that accounts for Bob's existing resource-level parallelism and worker memory.
This update keeps the format intact and exercises the new API in the benchmark.
See the [upstream API contract](https://github.com/lz4/lz4/blob/v1.11.0/lib/lz4frame.h)
and [release notes](https://github.com/lz4/lz4/releases/tag/v1.11.0).

The upgrade includes upstream correctness fixes; these measurements do not
establish a performance benefit for Defold's existing path. The decoder regressions
should be considered before landing and checked on other supported targets.

## Follow-up: Java threading, SIMD, and the slowdown

Bob already runs each resource's read/compress/encrypt/hash task in a Java executor
whose size comes from `project.getMaxCpuThreads()` (`--max-cpu-threads`). Each
compression calls the native HC compressor through JNA. The frame worker speedup
above measures parallelism within one large input; it is not a predicted bundle
speedup over Bob's existing resource-level parallelism.

SIMD is enabled in the tested ARM64 build. The preprocessor selects
`LZ4_FAST_DEC_LOOP=1`, `LZ4_WILDCOPY64=1`, and `__ARM_NEON=1`. Disassembly of the
actual Defold-toolchain `lz4.c.o` confirms 128-bit vector loads/stores in
`LZ4_decompress_safe`, including `ldp q0, q1` / `stp q0, q1`. The HC implementation
also contains vectorized copies; its match search primarily uses scalar/word
operations. Neither release is accidentally running with the fast loop disabled.

The advertised compression gains chiefly cover fast compression with reused
states/dictionaries and HC levels 11–12. Defold's one-shot HC level 9 call does
not exercise those same paths. The allocation and copy-loop controls below do
not isolate the HC level 9 slowdown. A later inlining control identifies a
reproducible contributor, as described in the next section. The measurements
should not be generalized to every compression level, CPU, or compiler.

Five additional interleaved rounds per configuration tested two controls, keeping
the primary benchmark results above intact:

| HC level 9 compression MB/s | 1.10.0 default | 1.11.0 default | 1.10.0 base allocation | 1.11.0 base allocation |
| --- | ---: | ---: | ---: | ---: |
| Scripts, 4 KiB | 544.4 | 525.8 | 554.5 | 527.1 |
| Mixed assets | 140.7 | 139.0 | 140.8 | 139.3 |

The base-allocation control allocates `LZ4_sizeofStateHC()` bytes, calls
`LZ4_compress_HC_extStateHC()` at level 9, then frees the state each time.
It removes the additional workspace allocation without introducing state reuse.
It changes 1.11.0 compression throughput by less than 1% on these compressible
inputs, so the extra allocation does not explain the observed slowdown there.

A separate `LZ4_WILDCOPY64=0` control retains SIMD and the fast decoder loop while
using the narrower copy path. Random-data decoding improves from 42,659 to
59,117 MB/s (1.10.0: 59,816 MB/s), but mixed-asset decoding falls from 15,830 to
15,172 MB/s. Thus the new copy strategy affects the results, but disabling it
does not resolve the mixed-resource decoder regression. No production flags
were changed. All 75 follow-up samples, summary ranges, and control definitions
are in [diagnostics-m5-max.json](diagnostics-m5-max.json).

## HC9 regression: an upstream inlining change

An upstream change between 1.10.0 and 1.11.0 replaced `LZ4_FORCE_INLINE int` with
`static int` on `LZ4HC_compress_generic_internal()`:
[commit 710ed4f0](https://github.com/lz4/lz4/commit/710ed4f0d7e79d39544d67cc0965ba332e1989ca).
This is a concrete contributor to the measured HC9 slowdown on Apple clang 21
with `-O2`.

A controlled comparison loaded both raw-library builds into the same process,
alternating their measurement order. Both used the same input, compiler flags,
HC level 9, and one-shot API. The 1.11.0 counterfactual restores only that one
inlining annotation; it retains the new algorithms and larger allocation.

| Input | Restored inline vs stock 1.11.0 | Restored 1.11.0 vs 1.10.0 |
| --- | ---: | ---: |
| Scripts, 4 KiB | +8.94% | -0.86% |
| Mixed assets, 4.3 MB | +2.04% | -0.48% |

These are medians of paired throughput ratios: nine alternating pairs for the
restoration control and eleven for the restored-vs-old comparison. Restoring
inlining recovers most of the measured compression gap on these two inputs.
The residual differences are small; this does not establish that all remaining
performance differences, or the separate decoder regression, have been explained.

Machine-code inspection confirms the relevant compiler decision. Stock 1.11.0
emits a separate `LZ4HC_compress_generic_internal` function, receiving a runtime
dictionary-mode argument. The restored build, like 1.10.0, instead emits the
specialized `LZ4HC_compress_generic_noDictCtx` body. Inlining lets the compiler
eliminate dictionary-mode work for Defold's no-dictionary compression path;
the performance change is more than the fixed cost of one function call.

The original upstream commit was also checked against its parent: it reduces
4 KiB throughput by about 2.5% in that older source snapshot. Its effect in the
full 1.11.0 source is larger, illustrating interaction with later code changes
and compiler optimization. SIMD remains enabled in all variants, and the extra
1.11.0 allocation remains present in the restored build.

The restoration increases the HC object's Mach-O `__TEXT` from 47,676 to
60,400 bytes (+12,724 bytes). The vendored sources remain byte-for-byte upstream;
the production CMake build now applies the restoration to a build copy using
`external/lz4/defold.patch`. Other target platforms remain unmeasured.

[hc9-inlining-m5-max.json](hc9-inlining-m5-max.json) records all 76 paired
samples, including release endpoints, the original commit and its parent, and
the restoration controls. [defold.patch](../defold.patch) contains the one-line
change against the vendored `src` layout. CMake applies it automatically before
compiling the HC source; the original update script continues importing pristine
upstream files. The standalone runner accepts prepared stock/patched source
trees for comparison using `--formats raw --levels 9`.

## Follow-up: independent state reuse and HC levels 11–12

The benchmark now supports `raw-reuse`, retaining an HC state and resetting it
before every resource, and configurable compression levels. This changes only
the benchmark. Production Bob and the engine still use one-shot HC level 9.

Five interleaved rounds across both library versions, two state modes and three
levels produced 180 additional samples. These are a separate run; compare values
within each table rather than combining absolute speeds across benchmark runs.

For LZ4 1.11.0 at level 9:

| Input | One-shot MB/s | Reused state MB/s | Throughput change | Compressed bytes |
| --- | ---: | ---: | ---: | ---: |
| Scripts, 4 KiB | 488.2 | 752.4 | +54.1% | 968 in both modes |
| Scripts, 64 KiB | 180.6 | 183.3 | +1.5% | 13,125 in both modes |
| Mixed assets, 4.3 MB | 133.4 | 133.7 | +0.2% | 1,563,378 in both modes |

The state is reset with `LZ4_resetStreamHC_fast()` before
`LZ4_compress_HC_continue()`, so resources retain no dependency on previously
compressed data. Before timing, the benchmark primes the state with another
file sharing the input's prefix, resets it, then verifies that compressed bytes
match a fresh one-shot call and decode with the engine's ordinary decoder.
All same-level reuse comparisons pass, including levels 11 and 12. Context
creation is outside timing; the potential gain is steady-state compression in
an archive worker, not a measured whole-Bob speedup.

For one-shot LZ4 1.11.0 on the mixed-assets input:

| HC level | Compression MB/s | Time vs level 9 | Compressed bytes | Size saving vs level 9 |
| ---: | ---: | ---: | ---: | ---: |
| 9 | 133.4 | 1.00× | 1,563,378 | — |
| 11 | 41.8 | 3.19× | 1,561,733 | 0.105% |
| 12 | 31.2 | 4.28× | 1,561,407 | 0.126% |

Levels 11/12 save 0.52% on the 4 KiB script input and 0.83% on the 64 KiB script
input, while compression throughput falls substantially. These levels retain
the same raw-block format and require no decoder change. On these samples,
decompression is similar or slightly slower; a higher compression level does
not automatically improve decode speed.

The release's higher-level improvements compare against the **same level in the
old library**. For example, level 11 on the 64 KiB script input improves from
50.8 MB/s in 1.10.0 to 66.1 MB/s in 1.11.0, about 30%, but remains much slower
than 1.11.0 level 9 at 180.6 MB/s. Improvements vary with the input: level 11 on
the 4 KiB sample regresses, while level 12 improves. Full results, including
min/max values and decompression, are in [state-levels-m5-max.json](state-levels-m5-max.json);
all samples are in [state-levels-m5-max.csv](state-levels-m5-max.csv).

Useful next experiments, in order:

1. Measure a real Bob archive with one reusable native state per Java worker,
   resetting per resource and releasing states when the workers finish. Include
   wall time, allocation/GC cost, peak memory and per-resource compressed hashes.
   The small-block native result above needs validation with JNA and real files.
2. Measure levels 3/6/9/11/12 on representative compiled resources, looking for
   a useful build-time/package-size tradeoff. Higher levels normally target
   smaller output at a compression-speed cost, without a strict guarantee that
   every file becomes smaller. The mixed sample gives little reason to raise
   the default to 11 or 12.
3. Test an immutable dictionary of up to 64 KiB per resource type, using held-out
   files and counting dictionary bytes in total package size. Working states
   can reference a common prepared dictionary, but each needs its own mutable
   state and the runtime needs the same dictionary when decoding. This requires
   dictionary identification, delivery and reader support.

Keeping one continuous history across all files is a different experiment:
LZ4 retains only the previous 64 KiB, rather than learning the entire archive.
It can exploit similarity between adjacent files, but makes decoding depend on
previous content and compression order. That conflicts with Defold's current
independent resource loading and live update model. A mutable compressor state
also cannot be used concurrently by all Java workers.

To reproduce this follow-up, generate the three inputs with `default_corpus()`
from `run.py`, then run the normal command with
`--formats raw raw-reuse --levels 9 11 12` and three `--input` arguments. Input
sizes and hashes match the initial corpus. The benchmark's independent-reset
and round-trip checks are part of each invocation.

## Validation and reproduction

- The production `defold.patch` build path passes **5 engine LZ4 tests** and
  **17 Bob archive tests** with the patched static library and rebuilt
  `dlib_shared`. Verified that editing the patch regenerates/recompiles the
  build copy, a subsequent unchanged build does no work, and the source tree
  remains pristine. The installed library contains the specialized
  `LZ4HC_compress_generic_noDictCtx` body.
- Built and installed the updated raw library with Defold's CMake toolchain in
  an isolated SDK, then built `dlib_shared` and ran `run_test_lz4`: **5 tests pass**.
  New tests cover empty, boundary-sized and multi-megabyte compressible/random
  blocks, undersized output, and malformed input. The pre-existing legacy fixture
  still decodes.
- The same engine tests pass with `WITH_ASAN=ON`. Separately compiled all raw and
  frame sources with ASan/UBSan and passed **24 configurations**, covering empty,
  mixed, 64 MiB, and random inputs with raw compression or 0/1/2/4/8 frame workers.
  The extended level/state harness passes **32 additional configurations** with
  ASan/UBSan, including empty inputs, independent state resets, levels 9/11/12,
  and the existing frame path.
- Compiled the current `LZ4.java`, `ArchiveBuilder.java`, and `ArchiveTest.java`
  against the existing Bob dependency JAR, pointing `DM_BOB_ROOTFOLDER` at the
  rebuilt native library: **17 archive tests pass**, including concurrent native
  compression. This was a focused JUnit run, not a full Bob rebuild.
- **36 cross-version native round trips pass**: each engine version compresses
  nine inputs, then both versions decode each result, including empty and 64 MiB
  inputs. Confirmed that the rebuilt shared library reports version 1.11.0.
- Old and new vendored sources match their upstream release files byte for byte.
  The upstream license is unchanged and still matches `licenses/NOTICE-lz4`.
- First-party C++ checked with clang-format 23.1.4 and Uncrustify 0.83.0. Preserved
  Uncrustify's struct alignment and intentional line breaks. Formatted only the
  added test region. Upstream sources remain unmodified; `git diff --check`
  reports one upstream trailing space in `lz4hc.h:185`.
- Reviewed the complete change for scope, ownership, API compatibility and
  formatting. No actionable correctness findings remain. No associated open LZ4
  PR was found; review-comment verification is not applicable. Only ARM64 macOS
  was tested; other platforms and full project loading remain unverified.

Run the command in [README.md](README.md) to reproduce the benchmark. The release
archive hashes are in [results-m5-max.json](results-m5-max.json), alongside the
40 median/min/max rows. [samples-m5-max.csv](samples-m5-max.csv) contains all 200
individual samples. The runner never performs network downloads.
