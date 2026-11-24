# Bob

Bob is Defold's command-line content build system. See the [IntelliJ IDEA guide](README_IDEA.md) for IDE setup and debugging, and the [Bob module README](com.dynamo.cr.bob/README.md) for packaging and content-pipeline details.

## Build locally

Complete the [repository setup](../README_SETUP.md) and build the engine at least once using the [engine build guide](../README_BUILD.md), or provide the matching archived engine artifacts. Bob packages engine artifacts and tools from the build environment.

From the repository root, start the Defold shell, install external dependencies, and build Bob:

```sh
./scripts/build.py shell
./scripts/build.py install_ext
./scripts/build.py build_bob
```

`build_bob` builds `com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar`, installs it in `$DYNAMO_HOME/share/java/bob.jar`, and runs Bob tests by default. Run `install_ext` before the first Bob build; the [packaging guide](com.dynamo.cr.bob/README.md#packaging) explains which tools it supplies.

For a quick build without tests, run:

```sh
./scripts/build.py build_bob --keep-bob-uncompressed --skip-tests
```

If you use the local JAR in the editor, follow the [editor build guide](../editor/README_BUILD.md#build-using-local-engine-and-tools) to refresh the editor after rebuilding Bob.

## Run tests locally

`build_bob` runs the Bob test suite unless `--skip-tests` is set. To test an already-built `com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar` without rebuilding it, run from the repository root:

```sh
./scripts/build.py test_bob
```

To run one JUnit test class against the existing JAR, run from the test module:

```sh
cd com.dynamo.cr/com.dynamo.cr.bob.test
../com.dynamo.cr.bob/gradlew -PtestClass=com.dynamo.bob.pipeline.ShaderProgramBuilderTest runSingleTest -x distBob
```

See the [IntelliJ IDEA guide](README_IDEA.md#testing) for running individual tests in the IDE.

## Build flags

These common options belong to `scripts/build.py` and can be passed with `build_bob`:

| Option | Effect |
| --- | --- |
| `--skip-tests` | Build Bob without running its tests. |
| `--keep-bob-uncompressed` | Store entries in `bob.jar` without compression. |
| `--platform=<platform>` | Select the target platform; the host platform is the default. |

Use `./scripts/build.py --help` for all build-script options. For Bob's own command-line options, run `java -jar com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar --help` from the repository root.

## Paths

In general, Bob paths use forward slashes and have no trailing separator: `/foo/bar` rather than `/foo/bar/`.

## Library cache

Library caching uses the Git SHA-1 of the actual commit:

- The SHA-1 is stored in the ZIP file comment.
- The SHA-1 is used as the ETag and `If-None-Match` value for `304 Not Modified` cache validation.
- It may differ from the requested version when that version is symbolic (for example, `HEAD` or `1.0`) or names an annotated tag object.
