

# Model Importer

Build the library first:

```
$ cd engine/modelc
$ waf build
```

# Debugging / Iterating

## C++

Currently, it's easiest to invoke the unit test:

```
$ ./build/src/test/test_model <path_to_model>`
```

Or faster in combination:
```
$ waf --opt-level=0 --skip-tests --target=test_model && ./build/src/test/test_model <path_to_model>
```


It will load the model, call `dmModelImporter::DebugScene(scene);` then destroy the scene and exit.

## Java

### ModelImporter.java

You can test it like so:
```
$ ./scripts/test_model_importer.sh <path_to_model>
```

Or faster in combination:
```
$ waf --opt-level=0 --skip-tests && ./scripts/test_model_importer.sh <path_to_model>
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

