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

The jar's entry point prints a deterministic slice-9 fixture as JSON:

```sh
java -Djava.library.path=/path/to/lib/arm64-macos -jar /path/to/share/java/dmengine.jar
```

The tests exercise both loading paths, JNI conversions, invalid inputs, and the
jar entry point. `-Xcheck:jni` is enabled for the Java integration tests.

## Adding a module

Keep its bindings and Java facade beside that module's native code. Add the
module's static target to `ENGINE_SHARED_MODULES` and list its generated and
handwritten Java files in the target's `DEFOLD_JAVA_SOURCES` property.
Register its native methods from the single `JNI_OnLoad` in `engine_jni.cpp`.
Modules must not define their own `JNI_OnLoad`.

The explicit registration calls also ensure the linker retains each module's
bindings from its static library. Java facades call `EngineJni.Initialize()`
before native methods; they must not load the DLL in a static initializer,
since they are resolved during native registration.
