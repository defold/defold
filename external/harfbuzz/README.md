
* Download latest release from https://github.com/harfbuzz/harfbuzz/releases

* Unpack to package/harfbuzz-<version>

* Remove all but the `package/harfbuzz-<version>/src` folder

* We maintain a `./config-override.h` file, in order to support SkriBidi text layout.

    * CMake uses this override directly and includes it in the common package.

* Build from the repository root with `./scripts/build.py build_external --package=harfbuzz --platform=<platform>`.
