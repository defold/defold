

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
