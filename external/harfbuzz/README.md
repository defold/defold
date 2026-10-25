
* Download latest release from https://github.com/harfbuzz/harfbuzz/releases

* Unpack to package/harfbuzz-<version>

* Remove all but the `package/harfbuzz-<version>/src` folder

* We maintain a `./config-override.h` file, in order to support SkriBidi text layout.

    * CMake uses this override directly and includes it in the common package.

* Build from the repository root with `./scripts/build.py build_external --package=harfbuzz --platform=<platform>`.

* Outline and color-table decoding live in `engine/font/src/truetype`; the runtime
  HarfBuzz build keeps `HB_NO_DRAW`, `HB_NO_PAINT`, `HB_NO_CFF`, and `HB_NO_VAR`.
  Fonts are used at their default variation coordinates.

* CMake patches a build copy of `hb-style.cc` to retain the small, sanitized
  `fvar` default-axis lookup used by Skribidi font selection. This does not enable
  variation interpolation. On version updates, review that patch and run the
  font library's `DefaultVariationStyle` test along with the outline/layout tests.
