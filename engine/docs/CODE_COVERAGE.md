# C/C++ code coverage for engine unit tests

The engine libraries (`engine/*`) can be built with [Clang source-based code coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) on macOS and Linux. The unit tests then write raw profiles, and [coverage_report.py](../../scripts/coverage_report.py) merges them into HTML, lcov, and JSON reports.

## Usage

Build and run the tests with coverage, then write the report:

```bash
./scripts/build.py build_engine --platform=arm64-macos -- --with-coverage
python scripts/coverage_report.py --platform=arm64-macos
```

The report is written to `engine/build/<platform>/coverage/report/`. Pass `--out <dir>` to change this.

| File | Contents |
|---|---|
| `html/index.html` | Annotated sources with line, region, and branch counts |
| `lcov.info` | lcov tracefile for Codecov, IDE plugins, or `genhtml` |
| `summary.json` | `llvm-cov export -summary-only` per-file and total counts |
| `report.txt` | `llvm-cov report` text table |
| `summary.md` | Per-library totals, used for the CI job summary |

To focus on a few libraries, configure CMake directly with the same option, as described in the [CMake guide](../../scripts/cmake/README.md):

```bash
cmake -S . -B engine/build/arm64-macos -G Ninja -DTARGET_PLATFORM=arm64-macos \
  -DDEFOLD_SELECTED_ENGINE_LIBS=dlib -DWITH_COVERAGE=ON
ninja -C engine/build/arm64-macos build_tests run_tests
python scripts/coverage_report.py --build-dir engine/build/arm64-macos
```

`run_tests` deletes the old profiles before it starts. If you run individual `run_<test>` targets instead, delete `coverage/profraw` in the build directory yourself after rebuilding, so that profiles from older binaries are not merged.

Building any test target copies public headers into `DEFOLD_SDK_ROOT`. From a git worktree without its own `tmp/dynamo_home`, point `DEFOLD_SDK_ROOT` at a copy rather than the main checkout's SDK, so the build does not overwrite headers there.

## How it works

- **Option.** `WITH_COVERAGE` is declared next to the sanitizer options in [features.cmake](../../scripts/cmake/features.cmake). `--with-coverage` maps to it in `build.py` and `ci.py`.
  - **Limits.** Configure fails for anything other than macOS or Linux, when combined with a sanitizer, or when the compiler is not Clang or AppleClang. The compiler check is in [defold_post_project.cmake](../../scripts/cmake/defold_post_project.cmake).
- **Flags.**
  - **Where they go.** The flags are attached to the `defold_sdk` interface target, which every engine target links. Compile options: `-fprofile-instr-generate=<DEFOLD_COVERAGE_PROFILE_DIR>/%m-%p.profraw -fcoverage-mapping`. Link options: the same `-fprofile-instr-generate=…`.
  - **Embedded path.** Embedding the path means tests that change into their source directory, and tools run during the build, do not write `default.profraw` into the source tree. `%p` gives every process its own file, and `%m` separates binaries ([Clang docs, "Running the instrumented program"](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)). `LLVM_PROFILE_FILE` still overrides the path.
  - **Profile directory.** `DEFOLD_COVERAGE_PROFILE_DIR` is a cache path and defaults to `<build>/coverage/profraw`.
- **Opt-out.** Targets marked with `defold_target_skip_sanitizer()`, including every `*_noasan` library and therefore the `*_shared` JNI libraries loaded by Bob, are not instrumented. They are also left out of the report.
- **Object list.**
  - **Contents.** [functions_coverage.cmake](../../scripts/cmake/functions_coverage.cmake) writes `<build>/defold_coverage_objects_<config>.txt`. It lists every test executable registered with `defold_register_test_target`, plus the object files of every library created with `defold_add_library`.
  - **Why object files.** `llvm-cov` only reports functions present in the binaries it is given ([llvm-cov guide](https://llvm.org/docs/CommandGuide/llvm-cov.html)). Without the library objects, sources that no test links would be missing, and the percentage would be inflated: for six dlib tests, 41.5% of 66 files without the objects versus 18.0% of 112 files with them.
  - **Why not the archive.** Passing a static archive directly to Apple `llvm-cov` 21 failed with "no coverage data found".
- **Test runner.** With coverage enabled, `run_tests` first deletes the profile directory. It then runs [run_tests.py](../../scripts/cmake/run_tests.py) with `--keep-going`, so one failing test does not stop the remaining tests from producing profiles. The command still fails if any test failed.
- **Report.** [coverage_report.py](../../scripts/coverage_report.py) reads `CMakeCache.txt` and merges the profiles with `llvm-profdata merge -sparse --failure-mode=all`, so a truncated profile from a crashed test does not fail the merge. It then runs `llvm-cov show/export/report` with all binaries from a response file.
  - **Tool lookup.** `llvm-profdata` and `llvm-cov` are taken from the directory of the resolved `CMAKE_CXX_COMPILER` first, because the raw profile format is tied to the compiler revision ([Clang docs, "Format compatibility guarantees"](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)). On macOS that is the packaged `XcodeDefault<ver>.xctoolchain` in `tmp/dynamo_home/ext/SDKs`, which ships both tools; `xcrun` may resolve to a different Xcode. On Linux the next places tried are `/usr/lib/llvm-<major>/bin` and `llvm-cov-<major>`. `--llvm-bin` overrides the lookup.
  - **Excluded from the report:**
    - test sources (`/test/`);
    - vendored code, taken from `excluded_paths` in [license_data.py](../../license_data.py), which includes `external/`;
    - generated DDF and other sources in `engine/<lib>/build/` and other build directories;
    - staged header copies in `DEFOLD_SDK_ROOT`;
    - system and toolchain headers.

## CI

The `bld-eng-coverage-linux-x64` job in [engine-nightly.yml](../../.github/workflows/engine-nightly.yml) runs nightly. It can also be started by hand with `workflow_dispatch` and target `coverage`.

1. Builds `x86_64-linux` with `ci/ci.sh --with-coverage … engine`, using Clang 20 installed by `ci.sh install`.
2. Runs `coverage_report.py --llvm-bin=/usr/lib/llvm-20/bin`.
3. Appends `summary.md` to the job summary.
4. Uploads the HTML, lcov, and JSON files as the `coverage-x86_64-linux` artifact, kept for 14 days.

Linux is used because macOS GitHub runners skip the dlib HTTP tests. Coverage is not part of the per-push matrix, to keep that job inside its time limit. Engine CI does not run on `pull_request`, and `contrib/**` builds have no secrets, so PR comments and Codecov uploads are left out for now. Codecov accepts the lcov file ([supported formats](https://docs.codecov.com/docs/supported-report-formats)) if it is added later for trusted runs.

## Measurements

These numbers are from arm64 macOS, `RelWithDebInfo`, with dlib only (74 tests).

| Step | Result |
|---|---|
| Build tests | 8.6 s wall-clock time |
| Run tests | 35 s |
| Profiles | 703 `.profraw` files, 36 MB |
| Report | under 1 s for 261 binaries and objects |
| Coverage | 77.4% lines, 85.6% functions, 63.9% branches over 122 files in `engine/dlib/src/{dlib,dmsdk,simd}` |

## Known limitations

- **Hash-mismatch warning.** `llvm-cov` prints `warning: N functions have mismatched data` (about 1500 for dlib). `-dump` shows these are inline header functions, such as `dmMath::DegToRad`, recorded with hash `0x0` in objects where they are unused, while other binaries contain the used version. The counts of the used version are still reported.
- **Platform-specific code.** Sources such as `*_win32.cpp`, `*_android.cpp`, and other graphics backends are only covered on the matching host, so a Linux report under-represents macOS and Windows code. Merging reports from several hosts would need `-path-equivalence` or lcov path rewriting, which has not been tried.
- **Not covered.** Windows, mobile, and web targets, and Lua, Java, and C# code.
- **Crashing processes.** Death tests, `engine/crash` tests, and `_exit` paths may not write a profile. This loses only that process's data. Continuous mode (`%c`) would help but is Darwin-only ([Clang docs](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)).
- **GPU tests.** Tests that depend on a GPU may be skipped or use software rendering in CI, so graphics backend numbers are low and noisy.

## Not verified yet

- The nightly job has not run yet. Running it once by hand will confirm the rest of this list:
  - Ubuntu's `llvm-20` package provides `/usr/lib/llvm-20/bin/llvm-profdata` and `llvm-cov`, and the `libclang_rt.profile` runtime. The job checks for both tools before building.
  - Full-engine build time, test time, and profile volume with coverage. gamesys and engine tests are much larger than dlib.
  - `llvm-cov` time and memory with the full engine object list.
- Whether coverage can be combined with any sanitizer. This is rejected at configure time for now.
- gcovr 8.5+ for Cobertura XML from LLVM profiles ([gcovr docs](https://gcovr.com/en/stable/guide/compiling.html)). Its discovery of binaries and profiles is not documented there.

## Why Clang source-based coverage rather than gcov

- **Optimised builds.** The coverage mapping is created before optimisation, so `RelWithDebInfo` keeps accurate counts ([Clang docs, "Impact of llvm optimizations"](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)).
- **Many binaries, shared libraries.** Many test executables link the same static libraries. With gcov, every process would write the same `.gcda` counters next to the shared objects, while `run_tests` runs up to three tests at a time. Profiles named with `%p` avoid that, and `llvm-cov -object` merges per source file.
- **Toolchains already available.** Both toolchains in use already ship matching tools: the packaged Xcode toolchain on macOS and Clang 20 on Linux CI. Linux links with lld, so the `--gc-sections` limitation of BFD older than 2.26 does not apply ([Clang docs, "Drawbacks and limitations"](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)).
- **Report formats.** `llvm-cov` produces HTML, lcov, and JSON directly ([llvm-cov guide](https://llvm.org/docs/CommandGuide/llvm-cov.html)).
