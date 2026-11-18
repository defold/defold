

# Model Importer

After the initial engine build described in [README_BUILD.md](../../README_BUILD.md), rebuild the library from the repository root. Replace `arm64-macos` with your configured platform:

```
$ cmake --build engine/build/arm64-macos --target model modelc_shared modelimporter_jar
```

# Debugging / Iterating

## C++

Currently, it's easiest to invoke the unit test:

```
$ cmake --build engine/build/arm64-macos --target test_model
$ cd engine/modelc
$ ./build/arm64-macos/src/test/test_model <path_to_model>
```

For subsequent rebuilds from `engine/modelc`:
```
$ cmake --build ../build/arm64-macos --target test_model && ./build/arm64-macos/src/test/test_model <path_to_model>
```


It will load the model, call `dmModelImporter::DebugScene(scene);` then destroy the scene and exit.

## Java

### ModelImporter.java

From the repository root, you can test it like so:
```
$ ./engine/modelc/scripts/test_model_importer.sh <path_to_model>
```

Or faster in combination:
```
$ cmake --build engine/build/arm64-macos --target modelc_shared modelimporter_jar && ./engine/modelc/scripts/test_model_importer.sh <path_to_model>
```


### ModelUtil.java

This is part of Bob, so we need to build bob light for this to work.

```
$ ./scripts/build.py build_bob_light --skip-tests

```

Then call the ModelUtil directly using:

```
$ ./com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/pipeline/test_model_util.sh <path_to_model_file>
```


### ModelViewer

The `modelview.py` is an old model viewer script. Depends on the python wheel package `pyglet` (tested with version 1.5.26).

## Model buffer compilation

The file layout belongs to gamesys: [`gamesys/modelbuffers.h`](../gamesys/src/gamesys/modelbuffers.h) defines the DMMB header, range descriptors, attribute masks and vertex layouts. The native compiler and runtime include this header. Fixed-width fields and compile-time size/offset checks keep the layout independent of pointer size. DMMB v2 remains little endian, with a 36-byte header and 28-byte descriptors.

Bob's geometry path is:

1. `MeshsetBuilder.create()` requests native packing through `ModelImporterJni.LoadFromBuffer()`.
2. `modelc_shared` parses/decompresses the scene and resolves external buffers through the existing Java resolver. `modelimporter_buffers.cpp` hashes and deduplicates native mesh arrays, assigns each mesh's `bufferIndex`, and writes the DMMB structs and GPU payloads before JNI destroys the native scene.
3. JNI returns the packed bytes and small model metadata. Bob's discovery pass omits Java vertex/index arrays. `ModelUtil.loadModelMetadata()` preserves names, transforms, material bindings, bounds, skeleton references and morph dependencies without constructing geometry protobufs.
4. `ModelBuffers.compile()` keeps runtime-reachable ranges, or one explicit mesh selection, and remaps metadata indices. Native code copies the selected ranges into the final file when a subset is needed.
5. Bob content-addresses the DMMB bytes as a generated `.modelbuffers` input. Its normal copy builder produces `.modelbuffersc`. The format version is part of the input bytes; no extra cache-version strings or protobuf serialize/parse round trip are needed.

When `model.split_large_meshes` is enabled, the existing Java splitting algorithm runs before native packing. The JNI adapter passes only GPU streams back to the native compiler, preserving the current splitting behavior.

`MeshsetBuilder.build()` still supplies raw topology, skeleton, animations and morph textures to existing build consumers. The editor requests native packing during import while also retaining raw arrays for previews and collision meshes. Its build targets select the same native-packed ranges. Each compiled model retains its own metadata and references exactly one shared buffer resource.

The runtime casts the aligned input blob to `ModelBuffersHeader` and its following descriptor array. It validates fields directly, uploads the payloads, and retains small header/descriptor copies after releasing the upload data. CPU streams are still recovered on demand. Metal uses the existing upload and draw paths.

### Import and packing measurements, 2026-11-04

Baseline: Java geometry preparation/writer at `84f2f9538f`. Both paths use the same rebuilt importer, codecs, OpenJDK 25.0.1, and a 1 GiB initial / 4 GiB maximum Java heap on an Apple M5 Max (arm64 macOS). The baseline loads the new importer bindings with native packing disabled. This isolates the geometry compiler change from importer/codec changes.

The corpus contains all 104 glTF models reachable from example-gltf-reference's bootstrap collection: 768 unique ranges within those loads, 2,710,702 vertices, and 215,528,554 bytes of baseline output. Range ordering can change, so validation compares every range's full vertex/index bytes, counts, stride, index width and attribute mask. All 768 ranges match. A separate validation pass ran with `-Xcheck:jni`.

Each process warms the entire corpus three times, then measures seven complete passes. Three before/after process pairs alternate order, producing 21 measured passes per path with warm filesystem caches. Checksums consume every output outside timing. Bob's generated-input content hash is included in both paths. Cache lookup, archive compression, the separate raw mesh-set producer, textures/shaders and GPU uploads are excluded.

| Timed geometry path for 104 models | Java before | Native production path | Reduction |
| --- | ---: | ---: | ---: |
| Read/import, prepare metadata/geometry, pack and content-hash | 770.9 ms | 205.7 ms | 73.3% (3.75x faster) |

The total interquartile ranges are 754.2–777.1 ms before and 204.3–207.6 ms after. Native packing now happens inside the import call, so individual phase boundaries differ; compare the total. This is a build/import benchmark, not a Metal runtime startup measurement.

The earlier standalone native prototype measured 110.6 ms for this corpus. It excluded JNI output copying, Java metadata and Bob's content hash. The production result above includes those integration costs.

### Reproduce the current path

Build `modelc_shared`, `modelimporter_jar`, and Bob, then run the explicit `benchmark_model_buffers` target and the companion script. The C++ benchmark now calls the production native compiler; it contains no separate file writer. It measures native import, mesh selection, and the combined canonicalization/packing operation. The Java benchmark measures the integrated Bob geometry path. Neither benchmark is part of normal builds or tests.

```sh
cmake --build engine/build/arm64-macos --target benchmark_model_buffers modelc_shared modelimporter_jar

python3 engine/modelc/scripts/benchmark_model_buffers.py \
    --project /path/to/example-gltf-reference \
    --native engine/modelc/build/arm64-macos/src/test/benchmark_model_buffers \
    --native-library engine/modelc/build/arm64-macos/libmodelc_shared.dylib \
    --bob-jar com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar \
    --java-home "$JAVA_HOME" \
    --output /tmp/model-buffer-benchmark
```

For the before/after comparison, build the baseline Java benchmark against the saved baseline Bob JAR and run it with the current `modelimporter.jar` first on the classpath and the same native importer. The baseline leaves native packing disabled. Alternate those runs with the production Java benchmark using the same manifest and JVM settings.
