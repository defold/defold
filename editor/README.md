## JavaFX Clojure Editor

### Requirements
* Java 1.8
* Leiningen

### Environment Setup
* `cd` to the `defold` directory
* run `./scripts/build.py shell`

This is the shell environment that you must have to run the project.
Consider putting it in an alias in your bash profile.

## Setup
* Build the engine with `scripts/build.py build_engine --skip-tests`
  from the `defold` directory
* From the `defold/editor` directory, run `lein protobuf`
* From the `defold/editor` directory, run `lein builtins`

## Running Tests
`lein test` will run all the tests including the integration tests.

If you are using a repl, you can also run the tests by calling `(suite/suite)`.

## Setup NREPL for debugging

If you want to work on the editor chances are you want to `connect` or `jack-in` to a REPL as described below.

For this to work you will need a `~/.lein/profiles.clj` file and put the nREPL, Cider (etc) dependencies there;

```
{:user {:plugins [[cider/cider-nrepl "0.10.2"]
                  [refactor-nrepl "1.1.0" :exclusions [org.clojure/clojure]]]
        :dependencies [[org.clojure/tools.nrepl "0.2.12"]]}}
```

Please note that Lein will introduce a nREPL dependency automagically, but its a good idea to override to your preferred version here anyway.

## Running the Editor
`lein run` will launch the editor as well as providing a nprel port
for you to jack into

**PLEASE NOTE:** 2 NREPL servers are started, you must connect to the first one!

## Jacking into a REPL

You can also use `M-x cider-jack-in` or launch the editor inside Cursive for debugging with breakpoints etc.

First set the envrinment varaible `DYNAMO_HOME`. Example of a value `/Users/martin/work/defold/tmp/dynamo_home`.

After you jacked in do the following to load and start the app;

```
user=> (dev)
dev=> (go)
```

## Running Benchmarks
`lein benchmark` will run the benchmarks and put the results to the
`test/benchmark/bench-result.txt` file. Make sure to have everything
on your system closed down

## Generating the docs
Running `lein doc` will generate the codox to the target/docs directory

## Styling
A single stylesheet is set on the root node (by convention) in the scene. The stylesheet `editor.css` is loaded as a regular java resource, from the uberjar or from the file-system in dev-mode. If an `editor.css` is found in the current working directory that file will take precedence over the aforementioned java resource.

The stylesheet can be reloaded with the function key `F5`.

### JavaFX Styling
The best way to understand how JavaFX styling works is by studying the default stylesheet `modena.css` included in `jfxrt.jar`
