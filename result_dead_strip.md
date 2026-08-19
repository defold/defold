

Results dead-strip:

macOS

Before - '-flto', '-fvisibility=hidden'

    7298304  engine/engine/build/src/dmengine_release

After - + '-dead_strip', '-Wl,-dead_strip_dylibs', '-fvisibility-inlines-hidden'

    6708016  engine/engine/build/src/dmengine_release


Diff:

    6708016 - 7298304 = -590288 = -576kb 

* -576kb
* 8% saving

Wasm

Before

     267133  engine/engine/build/src/dmengine_release.js
     321862  engine/engine/build/src/dmengine_release.js.symbols
    2353970  engine/engine/build/src/dmengine_release.wasm

After (CMake -O3 + '-Wl,--gc-sections')

     267133  engine/engine/build/src/dmengine_release.js
     321697  engine/engine/build/src/dmengine_release.js.symbols
    2355962  engine/engine/build/src/dmengine_release.wasm

Diff:

    2355962 - 2353970 = 1992 = ~2kb



Android

Before

    4724648  engine/engine/build/src/libdmengine.so

After (+ '-fdata-sections', '-Wl,--gc-sections')

    4362208  engine/engine/build/src/libdmengine.so


Diff:

    4362208 - 4724648 = -362440 = -354kb
    4362208 / 4724648 = 0,9232874068

* -354kb
* 7.7% saving

