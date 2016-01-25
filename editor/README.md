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
`lein test` will run all the tests including the integration tests

## Running the Editor
`lein run` will launch the editor as well as providing a nprel port
for you to jack into

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
