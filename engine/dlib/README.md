File embedding
--------------

Use `defold_embed_to_header` in the module's `CMakeLists.txt`. Provide an input file, an output header and an output C++ file. Add the generated C++ file to the consuming target and include the generated header to access the data and its size. For `test.embed` the default symbols are `TEST_EMBED` and `TEST_EMBED_SIZE`.

The [dlib test target](src/test/CMakeLists.txt) embeds `data/test.embed` this way. After configuring the engine with tests enabled, run it from the repository root:

```sh
cmake --build engine/build/arm64-macos --target run_test_embed
```

Replace `arm64-macos` with your configured platform.

Memory profiling
----------------

Run dmengine_memprofile.sh instead of dmengine.

To enable tracing where every allocation is written to disk for analaysis:

    # DMMEMPROFILE_TRACE=1 dmengine_memprofile.sh

The log file `memprofile.trace` is written in the current directory.

Create a trace report using memprofile.sh

    memprofile.sh `which dmengine` memprofile.trace > report.html

