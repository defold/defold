# libnogg Vorbis decoder trial

Add `nogg` to `DEFOLD_ENABLE_FEATURES` when configuring a Defold engine with CMake. On ARM64 macOS this enables libnogg's NEON path. On HTML5 add both `nogg` and `simd` to compile that path to Wasm SIMD128. The normal engine link selects a single Vorbis decoder.

CMake fetches the pinned libnogg 1.18 release and applies a local patch. The patch groups bins with identical floor gains for vector multiplication and reconstructs four coupled stereo coefficients at a time. [Upstream libnogg](https://achurch.org/libnogg/) and [SIMD patch](simd.patch)

The Defold stream adapter uses the vendored libogg page reader so it can pause on a partial page and resume when the resource supplies more bytes. It passes page granule positions to libnogg's packet decoder to trim the final output. Playback ends at the first logical stream in a chained Ogg file.

Build `test_sound` and `test_sound_perf` in the configured build directory. On macOS, run either executable directly. On Wasm, run `node test_sound.js --test-filter=SoundDecoder` for the decoder checks and `node test_sound_perf.js --test-filter=MeasureNogg` for the seven-file decode and skip benchmark. The same benchmark accepts `--test-filter=MeasureStdb` for Defold's stb_vorbis adapter. Configure without `nogg` on macOS to benchmark Tremolo with `--test-filter=MeasureTremolo`.

To measure upstream libnogg with the same Defold adapter, set `FETCHCONTENT_SOURCE_DIR_DEFOLD_LIBNOGG` to a pristine extracted 1.18 release. This CMake override uses the supplied source directly and skips the patch step.
