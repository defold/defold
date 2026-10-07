# Headless model benchmark

`benchmark_models` measures model/rig CPU update, render-list submission, and draw
preparation using the null graphics backend. It links and initializes `profile`
so profiler-property locking remains part of the workload. The `performance_tests`
build feature is disabled by default. When enabled with `BUILD_TESTS=ON`,
`build_tests` includes the benchmark, while `run_tests` keeps running only the
normal suites. Run the benchmark separately so concurrent tests do not distort
the measurements. It has no timing-based pass/fail thresholds.

Build with the usual CMake SDK setup, `BUILD_TESTS=ON`, `-O2` (Release or
RelWithDebInfo), and no sanitizers. From the repository root:

```sh
cmake -S . -B engine/build/arm64-macos -DBUILD_TESTS=ON -DDEFOLD_ENABLE_FEATURES=performance_tests
cmake --build engine/build/arm64-macos --target benchmark_models -j 8
cd engine/gamesys/build/arm64-macos/gamesys-test-runtime
../src/gamesys/test/benchmark_models --instances=10000 --warmup=30 --samples=7 --frames=60
```

Append `performance_tests` to any other enabled features. The existing build-script
feature flag works too: `./scripts/build.py build_engine -- --enable-feature=performance_tests`.
Clear the feature (or add it to `DEFOLD_DISABLE_FEATURES`) to remove the benchmark
target from the build configuration. Normal deterministic model/rig tests remain
available without the feature; they assert output and resource reuse without
comparing elapsed time.

The five cases cover native world matrices, native world/normal matrices,
translated components, a generic `mat3` conversion, and a static population with
one animated model. Every object moves on each frame, at a fixed 1/60-second
timestep. Scene construction, position setters, frame cleanup, assertions and
counter collection are outside the timed region. `update_us` includes game-object
update and post-update; `submit_us` includes render-list construction; `draw_us`
includes sorting, dispatch, model buffer preparation and null draw calls.
`cpu_frame_us` covers those three phases together. Do not add it to the phases.

Rows prefixed with `MODEL_BENCHMARK,` contain means for each fixed-length sample.
Cold rows measure the first frame before warmup, including custom-data cache
creation. Draw counts and instance-buffer bytes must match between variants;
custom-cache counts can differ when the optimization shares vertex data.

Build the same benchmark and fixtures for the baseline and proposed versions,
with identical compiler flags and dependencies. Keep other changes, including
render-list sorting, identical when isolating model/rig optimizations. Use the
same staged runtime assets and run one binary at a time. From the repository root:

```sh
python3 engine/gamesys/src/gamesys/test/compare_model_benchmarks.py \
    /absolute/path/to/baseline/benchmark_models \
    /absolute/path/to/candidate/benchmark_models \
    --runtime "$PWD/engine/gamesys/build/arm64-macos/gamesys-test-runtime" \
    --output "$PWD/tmp/model-benchmark-results" --instances=10000 --runs=3
```

The comparison alternates process order, saves raw logs and `results.json`, and
checks workload counters. It reports pooled medians of sample means and keeps
individual run medians. The null backend allocates and copies host buffers for
testing; its overhead differs from graphics drivers. Results establish CPU
preparation costs and cache sharing, not GPU time, driver upload cost, total
application frame time or whole-process memory use. A graphical benchmark is
still needed for those measurements.
