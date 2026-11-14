# Build Editor

## IMPORTANT PREREQUISITE!

Make sure you have followed the [setup guide](README_SETUP.md) before attempting to build the editor. If you do not install all of the required software from the setup guide your attempts to build the editor will likely fail.


## Build using existing engine and tools
You can build and run the editor and use an existing released version of the Defold engine and command line tools:

```sh
# Open a terminal/console and change directory to `defold`
cd defold

# Setup the shell environment (consider putting it in an alias in your bash profile)
./scripts/build.py shell

# Installs packages and builds source dependencies (also needed after distclean)
./scripts/build.py install_ext

# Change directory to the editor subdirectory
cd editor

# Build with pre-built engine binaries suitable for your editor version
lein init <sha1>

# start the editor
lein run
```

*NOTE* To select a sha1 here, use one of the matching [Defold releases](https://github.com/defold/defold/releases). E.g. if you're on the dev branch, use the "alpha" release. if you're on the beta branch, use the "beta" release. And if you're using the master branch use the "stable" release.

## Build using local engine and tools
These build steps build the engine and command line tools using the [engine build instructions](../README_BUILD.md).

The `--platform=` is implied in these examples, as it defaults to the host platform (x86_64-win32, x86_64-linux, x86_64-macos or arm64-macos).

```sh
# Open a terminal/console and change directory to `defold`
cd defold

# Setup the shell environment (consider putting it in an alias in your bash profile)
./scripts/build.py shell

# Installs packages and builds source dependencies (also needed after distclean)
./scripts/build.py install_ext

# Build the engine
./scripts/build.py build_engine --skip-tests -- --skip-build-tests

# Build builtins
./scripts/build.py build_builtins

# Build reference documentation
./scripts/build.py build_docs

# Build Bob
./scripts/build.py build_bob --skip-tests --keep-bob-uncompressed

# Change directory to the editor subdirectory
cd editor

# Build and run the editor
lein init
lein run
```

`lein init` installs Bob into the editor's local Maven repository and prepares generated resources. When using a local Bob build, rerun `lein init` without a SHA after rebuilding `bob.jar` or changing inputs to generated resources, before starting the editor again. With a released engine, continue to use `lein init <sha1>` for the matching archived artifacts.

## Building the Intel macOS bundle on Apple silicon

After setting up the build environment, run from `editor/`:

```sh
python3 scripts/bundle.py --platform=x86_64-macos --version=<version> --engine-artifacts=<engine-sha1> --skip-tests build
```

The build uses the native ARM JDK for compilation and packaging. It downloads
matching Intel JMODs and uses the ARM `jlink` to create the Intel runtime. Native
libraries for both the build host and target are prepared for compilation; only
the target libraries are retained in the bundle. For `--engine-artifacts=dynamo-home`,
both ARM and Intel engine artifacts must be available locally.

Rosetta is not required to build the bundle. Build-time tests run on the native
host JVM. To run the editor tests on the Intel JVM, use the `test` command with
`--platform=x86_64-macos`; that requires Rosetta on Apple silicon.

## Building all editor bundles on Linux

An x86-64 Linux host can prepare all four editor targets with one resource
initialization, one Java/Clojure compilation and one common project JAR. After
setting up the build environment, run from `editor/`:

```sh
python3 scripts/bundle.py --platform=x86_64-linux --platform=x86_64-win32 --platform=x86_64-macos --platform=arm64-macos --version=<version> --engine-artifacts=<engine-sha1> --skip-tests build
```

Compilation uses the Linux JDK and native libraries. The common project JAR
contains compiled classes, shared resources and all staged platforms. Each target's
standalone JAR adds its JavaFX dependencies and filters native libraries, retaining
the shared files needed for bundling games. Each bundle selects its target's
launcher and Java runtime. With `--engine-artifacts=dynamo-home`, all four desktop
engine builds must be available locally. Tests run on the build host; use the
separate `test` command on the appropriate host for platform testing.

Linux ZIP/tar.gz and Windows ZIP files are written to `target/editor/`. Windows
signing can run on Linux with `--codesign` and the existing Google Cloud signing
options. macOS apps are written unsigned to
`target/editor/prepared/Defold-<platform>.tar`. Transfer those tar files intact to
the same directory in a checkout of the **same commit** on a Mac, then run:

```sh
python3 scripts/bundle.py --platform=x86_64-macos --platform=arm64-macos finalize
```

This creates both macOS ZIPs and DMGs without compiling or downloading a JDK.
The two targets finalize in parallel on the same Mac, with separate staging
directories. Preparation and packaging of one app can overlap signing and
notarization of the other. Each app is signed before its ZIP and DMG are created,
and the command waits for both targets and fails if either target fails.
Add `--codesign` and the existing notarization options for signed, notarized DMGs
and signed updater ZIPs. The tar handoff preserves executable permissions and
symlinks, and finalization rejects apps from another editor commit.
Before signing, finalization replaces symlinks with their target contents so
existing editor updaters can extract the ZIP as regular files. The ZIP and DMG
therefore contain the same signed app contents.
Each app has a 20-minute deadline for submission, notarization and stapling.
Commands get up to five attempts within that deadline; waiting retries use
the existing submission ID. Logs identify the platform and submission ID.

CI uses `ci/ci.sh build-editor --platform=x86_64-linux --platforms=x86_64-linux,x86_64-win32,x86_64-macos,arm64-macos`
for the Linux job, followed by
`ci/ci.sh finalize-editor --platforms=x86_64-macos,arm64-macos` on one ARM Mac.
The existing Linux and Windows editor test jobs run separately. Native macOS
build commands continue to produce their final bundles directly.

## Running the Editor for Performance Checks

From `editor/`, run `lein with-profile +performance run` when measuring editor runtime performance. The `performance` profile disables exception decoration, schema checks, and compiled spec assertions, which otherwise affect measurements. Use `lein run` for normal development.


## Running Tests
Run all the tests including the integration tests:

```sh
lein test
```

If you are using a repl, you can also run the tests by calling `(suite/suite)`.


## Running Benchmarks
Run the benchmarks:

```sh
lein benchmark
```

The results will be stored in the `test/benchmark/bench-result.txt` file. Make sure to have everything on your system closed down.


## Setup NREPL for debugging
If you want to work on the editor chances are you want to `connect` or `jack-in` to a REPL as described below.

For this to work you will need a `~/.lein/profiles.clj` file and put the nREPL, Cider (etc) dependencies there;

```
{:user {:plugins [[cider/cider-nrepl "0.25.8"]
                  [refactor-nrepl "2.5.0" :exclusions [org.clojure/clojure]]]
        :dependencies [[org.clojure/tools.nrepl "0.2.12"]]}}
```

Please note that Lein will introduce a nREPL dependency automagically, but its a good idea to override to your preferred version here anyway.

When developing through the REPL, reload the whole changed file from time to time and before finishing. Reloading only individual forms can miss reflection warnings that appear when the namespace is compiled as a whole. Enable `(set! *warn-on-reflection* true)` and, in files with arithmetic, `(set! *unchecked-math* :warn-on-boxed)` near the top of the file to catch reflection and boxed math. Address avoidable warnings, for example by adding appropriate type hints. See [the Cursive guide](README_CURSIVE.md#load-file-in-repl) for the whole-file reload command.

## Bundling games and running in browser

As a temporary solution, we use Bob (from Editor1) as the content pipeline for bundling and running in the browser. In order to setup Bob locally, you need to:

- Install and build dependencies for the platform, e.g. `python scripts/build.py install_ext --platform=wasm-web`
- Build the engine for the specific platform, e.g. `python scripts/build.py build_engine --platform=wasm-web --skip-tests -- --skip-build-tests`
- Build Bob with local artifacts, `python scripts/build.py build_bob`
- `lein init`, which will install `bob.jar` as a local maven package


## Using a local bob.jar

When developing it can be convenient to use a local bob.jar instead of the one bundled with the editor. This can by adding `,-Ddefold.dev=true` to the end of the `vmargs =` field of the editor config.

* On macOS the config is inside the application file: `Defold.app/Contents/Resources/config`
* On Windows and Linux the config file is located next to the Defold executable.

## Experimenting with editor UI scale

Set **Preferences > General > UI Scale** and restart the editor. The percentage
multiplies the system display scale: 150% on a display using 200% scaling gives
an effective 300% scale for the editor content.

To override the preference for an experiment, append `,-Ddefold.uiScale=1.5` to
the installed editor's `vmargs` entry, or run from `editor/` with:

```sh
JAVA_TOOL_OPTIONS=-Ddefold.uiScale=1.5 lein run
```

The override accepts factors from 1.0 to 3.0 and takes precedence over Preferences.
Remove it to use the saved preference again. Native window decorations, the macOS
system menu bar, and native file dialogs keep the operating system's scale.
