# Shared engine code for Java

`engine/engine` owns `dmengine_shared` and `dmengine.jar`. Engine modules contribute
static libraries containing the native code they expose and their JNI adapters.
The shared library does not start an engine instance.

The first contribution is `gamesys_shared`, defined in
`engine/gamesys/src/gamesys/jni`. It compiles `geometry.cpp` together with the
generated conversions for `geometry.h` and the native method adapters.
The runtime gamesys library compiles the same geometry source without JNI.
Libraries loaded into the JVM are built without engine sanitizer instrumentation.

## Build and test

From the repository root, with the engine build environment configured:

```sh
cmake --build engine/build/arm64-macos --target dmengine_shared dmengine_jar
cmake --build engine/build/arm64-macos --target run_test_engine_shared
```

Use the appropriate configured build directory for other desktop platforms.
Installation places the native library in `lib/<platform>` and the jar in
`share/java`. Generated Java and C++ conversion files stay in the build directory.

`com.defold.engine.GeometryJni` exposes positions, texture coordinates and indices.
Its parameter type, `Geometry.Slice9Params`, is generated from the C++ header.
Positions are normalized XY pairs; callers apply their component size and
transform. UVs and indices use the same row-major grid. Flip fields are bytes
with values 0 or 1, matching the native bitfields.

The library loads through `java.library.path`. An embedding application can
instead set `dmengine.library` to the absolute path of its unpacked native library
before first use. Loading failures propagate as Java errors.

The jar's entry point takes a class name in `com.defold.engine` and forwards the
remaining arguments to its `public static void main(String[])`. It has no
feature-specific command registry. For example, generate
slice-9 positions, UVs and indices as JSON using explicit dimensions, margins,
pivot, flips, texture dimensions and UV corners:

```sh
java -Djava.library.path=/path/to/lib/arm64-macos -jar /path/to/share/java/dmengine.jar \
  GeometryJni slice9 32.5 24.25 2 3 4 5 0.75 0.25 0 0 64 64 0 0 0 1 1 1 1 0
```

Use `GeometryJni --help` for argument order. The module can also be invoked directly
with `java -Djava.library.path=... -cp dmengine.jar com.defold.engine.GeometryJni
slice9 ...`. Test fixture values belong in the tests, not in the entry points.

Shared tests cover the packaged launcher, generic dispatch, argument forwarding
and error propagation. Module tests own their API and command-line fixtures.
Together they exercise both loading paths, JNI conversions and invalid inputs.
`-Xcheck:jni` is enabled for the Java integration tests.

## Adding a module

Keep its bindings and Java facade beside that module's native code. Add the
module's static target to `ENGINE_SHARED_MODULES` and list its generated and
handwritten Java files in the target's `DEFOLD_JAVA_SOURCES` property.
Configure the contributor before `engine/engine`, so its target properties are
available when the shared JAR is assembled. Keep its Java API tests beside the
bindings and list their sources and main classes in `DEFOLD_JAVA_TEST_SOURCES`
and `DEFOLD_JAVA_TEST_CLASSES`. The shared build compiles and runs those tests
without including them in the shipped JAR. Module-specific Python CLI tests go
in `DEFOLD_JAVA_CLI_TEST_SCRIPTS`; each receives the Java invocation ending in
`-jar dmengine.jar` as its arguments. They run under `run_test_engine_shared_cli`
and the aggregate `run_test_engine_shared` target.
If the module exposes command-line functionality, provide a public static
`main(String[])` in its facade. The shared launcher can invoke it by class name
without changes to `EngineJni` or its tests.
Register its native methods from the single `JNI_OnLoad` in `engine_jni.cpp`.
Modules must not define their own `JNI_OnLoad`.

The explicit registration calls also ensure the linker retains each module's
bindings from its static library. Java facades call `EngineJni.Initialize()`
before native methods; they must not load the DLL in a static initializer,
since they are resolved during native registration.
