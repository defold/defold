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

# Extracts packages - you only need to do this once
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

# Extracts packages - you only need to do this once
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


## Jacking into a REPL
You can also use `M-x cider-jack-in` or launch the editor inside Cursive for debugging with breakpoints etc.

First set the environment variable `DYNAMO_HOME`. Example of a value `/Users/martin/work/defold/tmp/dynamo_home`.

After you jacked in do the following to load and start the app;

```
user=> (dev)
dev=> (go)
```


## Generating the docs
Running `lein doc` will generate the codox to the target/docs directory.


## Bundling games and running in browser

As a temporary solution, we use Bob (from Editor1) as the content pipeline for bundling and running in the browser. In order to setup Bob locally, you need to:

- Build the engine for the specific platform, e.g. `python scripts/build.py build_engine --platform=js-web --skip-tests -- --skip-build-tests`
- Build Bob with local artifacts, `python scripts/build.py build_bob`
- `lein init`, which will install `bob.jar` as a local maven package


## Using a local bob.jar

When developing it can be convenient to use a local bob.jar instead of the one bundled with the editor. This can by adding `,-Ddefold.dev=true` to the end of the `vmargs =` field of the editor config.

* On macOS the config is inside the application file: `Defold.app/Contents/Resources/config`
* On Windows and Linux the config file is located next to the Defold executable.
