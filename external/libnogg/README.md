# Ogg Vorbis decoding

Defold uses libnogg 1.18 to decode Vorbis packets and libogg to read Ogg pages. The sound adapter supports mono and stereo, pauses when streamed data runs out, and trims each logical stream at its final granule position. Chained streams must share a sample rate and channel count.

The stock engine and Extender use `decoder_ogg_nosimd` by default on every platform. This archive uses the portable C decoder. Its HTML5 build explicitly disables Wasm SIMD128. Both decoder archives are included in the SDK.

Set `-DDEFOLD_ENABLE_FEATURES=simd` in a CMake build to select `decoder_ogg`. It uses NEON on ARM64 and NEON-enabled ARMv7 Android, SSE2 on x86-64, and SIMD128 on HTML5. To opt in through an app manifest, add `decoder_ogg` to `libs` and `decoder_ogg_nosimd` to `excludeLibs` in the desired platform context. Both archives provide the same `AudioDecoderVorbis` registration.

## Source and patch

The source package contains the unmodified `include/`, `src/`, `COPYING`, and `README` from the 1.18 release. Its SHA-256 is recorded in CMake. The complete upstream release has SHA-256 `9600b7d2c03e447bb0ccbb3541bbe07714f5cd4fe00225ae754463b1b592a370`. [Upstream release](https://achurch.org/libnogg/tarballs/libnogg-1.18.tar.gz) · [Source package](../../packages/libnogg-1.18-source.tar.gz)

CMake applies `defold-simd.patch` to the extracted sources. It groups bins with equal floor gains for vector multiplication. The NEON and Wasm SIMD kernels reconstruct four coupled stereo coefficients at a time and use vector lane permutations and four parallel butterflies in the inverse MDCT. To benchmark upstream libnogg with the same adapter, set `FETCHCONTENT_SOURCE_DIR_DEFOLD_LIBNOGG` to a pristine extracted release. [SIMD patch](defold-simd.patch)

## Tests

Build the `test_sound` and `test_sound_perf` targets. Run `test_sound --test-filter=SoundDecoder` for streaming, chaining, and large Vorbis headers. Run `test_sound_perf --test-filter=MeasureVorbis` to time decoding and skipping seven mono and stereo clips. For Wasm, use the same filters with `node test_sound.js` and `node test_sound_perf.js`.
