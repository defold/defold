# LLM Handoff Report: `test_testmain` Binary Log Output Issue

## Context

Repository:

```text
/Users/mathiaswesterdahl/work/defold
```

Target module:

```text
/Users/mathiaswesterdahl/work/defold/engine/testmain
```

Test binary:

```text
/Users/mathiaswesterdahl/work/defold/engine/testmain/build/src/test/test_testmain
```

The user is investigating a CI issue involving `test_testmain` on macOS with ASAN enabled. The important distinction is that the test process itself does not currently fail locally. The reproduced local symptom is that the test writes embedded NUL bytes (`0x00`) to stdout, which can make CI/log tooling treat the output as binary or hide useful log text.

## Current Finding

The `test_testmain` executable exits successfully with status `0`.

Running it with ASAN options does not produce an AddressSanitizer report:

```sh
ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:symbolize=1 ./build/src/test/test_testmain
```

This is expected because the issue is not memory corruption. The issue is that `jc_test` explicitly writes C string terminators into its output buffer and then flushes those bytes to stdout.

## Local Repro

Build or reuse the x86_64 macOS ASAN test binary, then capture stdout and inspect the bytes:

```sh
cd /Users/mathiaswesterdahl/work/defold/engine/testmain
./build/src/test/test_testmain > /tmp/test_testmain.out
xxd -g 1 -c 32 /tmp/test_testmain.out
```

Observed output still contains embedded `00` bytes. Example:

```text
00000000: 64 6d 54 65 73 74 4d 61 69 6e 0a 50 6c 61 74 66 6f 72 6d 49 6e 69 74 0a 00 44 65 62 75 67 67 65  dmTestMain.PlatformInit..Debugge
00000020: 72 41 74 74 61 63 68 65 64 3a 20 30 50 6c 61 74 66 6f 72 6d 49 6e 69 74 20 50 41 53 53 20 28 38  rAttached: 0PlatformInit PASS (8
00000040: 20 c2 b5 73 29 0a 00 64 6d 54 65 73 74 4d 61 69 6e 20 74 6f 6f 6b 20 31 33 30 20 c2 b5 73 0a 52   ..s)..dmTestMain took 130 ..s.R
```

The key bytes are:

```text
... 0a 00 44 ...
... 0a 00 64 ...
```

Those `00` bytes are the reproduced issue.

## CI-Like Build Notes

The nightly macOS ASAN workflow uses `x86_64-macos`, not the default host target. On an Apple Silicon machine, a command without `--platform` is likely to build `arm64-macos`.

Closest local build command:

```sh
cd /Users/mathiaswesterdahl/work/defold
./scripts/build.py shell
cd engine/testmain
waf --prefix=$DYNAMO_HOME distclean configure build --platform=x86_64-macos --opt-level=0 --with-asan
```

Confirmed binary properties from the previous investigation:

```text
Mach-O 64-bit executable x86_64
```

The binary links ASAN:

```text
@rpath/libclang_rt.asan_osx_dynamic.dylib
```

## Root Cause

The root cause is in `jc_test.h`, specifically `jc_buffered_string::Append(const char*)`.

Problematic code:

```cpp
void Append(const char* str)
{
    Append(str, strlen(str)+1);
}
```

This copies the terminating NUL byte into the buffered output. Later, `jc_test_print_logger::FlushBuffer()` writes `str->size` bytes to stdout, so the embedded terminator is emitted as an actual `0x00` byte.

Installed header path currently checked:

```text
/Users/mathiaswesterdahl/work/defold/tmp/dynamo_home/ext/include/jc_test/jc_test.h
```

The line still exists there:

```cpp
Append(str, strlen(str)+1);
```

## Package State

Git status showed that the package version has been bumped:

```text
D  packages/jctest-0.13-common.tar.gz
A  packages/jctest-0.14-common.tar.gz
 M scripts/build.py
 M share/ext/jctest/build_jctest.sh
```

Observed version updates:

```diff
-    "jctest-0.13",
+    "jctest-0.14",
```

```diff
-readonly VERSION=0.13
+readonly VERSION=0.14
```

However, extracting the header from the new package still shows the same problematic line:

```sh
tar -xOzf packages/jctest-0.14-common.tar.gz include/jc_test/jc_test.h | rg -n "Append\\(str, strlen\\(str\\)"
```

Observed result:

```text
973:        Append(str, strlen(str)+1);
```

Therefore the current `jctest-0.14-common.tar.gz` package does not yet fix the issue.

## Required Fix

Change:

```cpp
void Append(const char* str)
{
    Append(str, strlen(str)+1);
}
```

to:

```cpp
void Append(const char* str)
{
    Append(str, strlen(str));
}
```

The package must then be rebuilt or repacked so that this fixed header is inside:

```text
/Users/mathiaswesterdahl/work/defold/packages/jctest-0.14-common.tar.gz
```

After that, refresh the installed externals so this installed copy is updated too:

```text
/Users/mathiaswesterdahl/work/defold/tmp/dynamo_home/ext/include/jc_test/jc_test.h
```

## Verification Steps

After fixing and repacking `jctest-0.14-common.tar.gz`, refresh externals:

```sh
cd /Users/mathiaswesterdahl/work/defold
./scripts/build.py install_ext
```

Rebuild the test:

```sh
cd /Users/mathiaswesterdahl/work/defold/engine/testmain
waf --prefix=$DYNAMO_HOME distclean configure build --platform=x86_64-macos --opt-level=0 --with-asan
```

Capture and inspect output:

```sh
./build/src/test/test_testmain > /tmp/test_testmain.out
xxd -g 1 -c 32 /tmp/test_testmain.out
```

Expected result:

```text
No 00 bytes appear in the test output stream.
```

Automated pass/fail check:

```sh
./build/src/test/test_testmain > /tmp/test_testmain.out
LC_ALL=C tr -d '\000' < /tmp/test_testmain.out | cmp -s /tmp/test_testmain.out - \
  && echo "OK: no NUL bytes" \
  || echo "FAIL: NUL bytes reproduced"
```

Expected fixed result:

```text
OK: no NUL bytes
```

## Important Notes

ASAN is not expected to fail for this bug. It is an output formatting/logging bug, not a memory safety bug.

The test named `test_testmain` is relevant because it is one of the first tests to run in CI. Its output can introduce NUL bytes early in the CI log, making subsequent log handling confusing.

The stale CMake build tree under:

```text
/Users/mathiaswesterdahl/work/defold/engine/build/arm64-macos
```

references `engine/testmain/CMakeLists.txt`, but that file is not tracked on the current `dev` checkout. Do not use that stale CMake build tree as the primary signal for this investigation. The current `testmain` module is built via Waf.
