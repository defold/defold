# Defold repository guidance

## Engine

Use CMake for engine builds and tests. Follow the [CMake guide](scripts/cmake/README.md) and use `scripts/build.py` for current commands.

Add the full Defold License header to every new first-party engine source file. When modifying an existing first-party engine source file that has no license header or only an abbreviated Defold header, add or replace it with the full header. Preserve third-party license notices instead of replacing them. Put the header before the code (after a shebang, if present), use the file's comment syntax, and update the Defold Foundation end year to the current year. For C and C++ files, use this full header:

```txt
// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
```

Run from the repository root:

- `./scripts/build.py shell`: configure the build environment, including `DYNAMO_HOME` and toolchain paths.
- For a new target, run `./scripts/build.py install_ext` followed by `./scripts/build.py build_ext`. Pass `--platform=<platform>` when targeting another platform. Repeat `build_ext` when external sources or the toolchain change.
- `./scripts/build.py build_engine`: build the engine and run tests for the configured target.
- `./scripts/build.py build_bob --keep-bob-uncompressed --skip-tests`: build `bob.jar` without running tests.
- For focused work, use `cmake --build <build-dir> --target <target>` to rebuild a target, or `--target run_<test-target>` to build and run a test, such as `run_test_dlib`. The default `all` target excludes test binaries.

The default build directory is `engine/build/<platform>`; use the actual configured directory for separate builds. To isolate generated library outputs as well, set `-DDEFOLD_BUILD_HOME=<absolute-build-root>` when configuring with a separate `-B` directory.

## Editor

Follow [editor/AGENTS.md](editor/AGENTS.md) and the [editor build guide](editor/README_BUILD.md).
Follow [Defold Editor Development: Tips and Tricks](https://forum.defold.com/t/defold-editor-development-tips-and-tricks/80710) for the live REPL workflow. Reload whole files to catch reflection warnings, enable boxed-math warnings with `(set! *unchecked-math* :warn-on-boxed)`, and address avoidable reflection and boxing.
Use the `cljfx.plorer` namespace via the live REPL for all Defold Editor UI interactions.

Run commands from `editor/`: `lein init` prepares dependencies and generated resources; rerun it after changes to Bob or generated resources. Use `lein run` to start the editor. Use `lein with-profile +performance run` when checking editor runtime performance; it disables exception decoration, schema checks, and compiled spec assertions. Use `lein with-profile +headless test` for unattended tests and `lein preflight` for formatting, lint, and test checks.

## Changes and tests

- Follow existing style and `.clang-format` for engine code. Keep C++ C-like: avoid exceptions, heavy STL use, lambdas, and `auto` unless established in the surrounding code. Prefer separate platform files for substantial OS differences.
- All new or modified Clojure code must be validated with the [clojure-code-style](editor/.codex/skills/clojure-code-style/) skill before finishing the task. Check every rule and run its required `clj-kondo` lint checks.
- Add focused tests for behavior changes. Every new test must have a comment immediately above its declaration, outside the test function, explaining exactly what it verifies and what regression it guards against, if applicable.
- Keep PRs focused on one problem and target `defold:dev` unless instructed otherwise. Link related issues, describe the behavior change, and report validation and tested platforms. Include screenshots for editor UI changes. CI must pass before merge.
