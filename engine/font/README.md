# Font library image tests

Start `./scripts/build.py shell` at the repository root, then run from `engine/font`:

```sh
cmake -S ../.. -B ../build/arm64-macos -DBUILD_TESTS=ON
cmake --build ../build/arm64-macos --target generate_font_test_images
```

Four generator executables compile `src/test/test_font_bitmap_gen.cpp`: legacy/full
layout, each with the rich-text parser enabled/excluded. Each runs only its
supported bitmap matrix by default, sharing one graphics context. The ordinary
`test_font` unit tests remain independent and require no graphics context.
The matrix is enumerated in `src/test/make_report.py`.

The tests use fontviewer's direct graphics approach: glyph atlas, font-library
layout/vertex packing, and font shaders. One hidden OpenGL context per process
renders directly into off-screen targets. There is no engine, Bob, generated
project, component, frame delay or process per image. PNG output is normalized
from the backend's BGRA readback. Actual and accepted current captures use the same
fixed rectangles and text origins from `src/test/data/font_render/capture_geometry.json`.
No foreground-dependent cropping is applied; clipping is an assertion failure.

The matrix covers TTF/OTF distance-field and bitmap output, glyph-bank provider
snapshots, BMFont, both layer modes, effect alpha/width/shadow settings, rich
markup overrides, and wrapped English/Arabic. Glyph-bank snapshots exercise
prebaked layout/fallback; they do not test the font compiler. Compiler tests
remain in the existing Java/Bob test suites. Labels and GUI are not font-library
objects and are not included here.

Images and per-configuration logs go into `build/font-test-images/`. The HTML,
JSON and Markdown report goes into `build/font-render-report/`, with one child
report per executable and stable case paths. Each `index.html` is a standalone
file with embedded images, case details, logs, and downloadable data; share
that file without its surrounding directories. CMake finishes the report before
failing on assertions, missing captures or likeness below 98%.
Options and reproduction commands remain in HTML; stdout prints them only for
failed cases.

Generate every supported image in one executable, without comparison/reporting:

```sh
./build/arm64-macos/src/test/test_font_bitmap_gen --output build/font-test-images
```

Reproduce one matrix case:

```sh
./build/arm64-macos/src/test/test_font_bitmap_gen --case ttf_sdf_single_default \
  --output build/font-test-images
```

Or render a single custom image using defaults plus manual overrides:

```sh
./build/arm64-macos/src/test/test_font_bitmap_gen --source ttf_bitmap \
  --layers multi --size 50 --outline 4 --shadow-alpha 1 --shadow-blur 2 \
  --text 'ABCDEFGabcdefg 0123456789' --output build/font-manual
```

This writes `build/font-manual/legacy-rich/manual.png`. Use `--help` for all
options. Manual options cannot be combined with `--case`. Layout/parser support
is chosen by executable: `test_font_bitmap_gen_skribidi`,
`test_font_bitmap_gen_plain`, or `test_font_bitmap_gen_skribidi_plain`.
The CMake target runs all four (344 images) and then generates the report.

Rendered reference PNGs belong under
`src/test/data/reference/<configuration>/`. They require visual
review. Previous raw SDF/channel images and project screenshots are not
interchangeable with these shader-rendered unit images; missing references are marked skipped. Normal runs never promote their output to references.

Missing reference images skip comparison and remain visible in the report.
Skipped cases do not fail the build; missing or invalid captures, native
assertion failures and visual mismatches still do. A run with only skipped
comparisons is reported as skipped, never as passed.

The active Python files are `make_report.py` (matrix, C++ fixture emission,
comparison and reports) and `test_font_images.py` (comparison, report and CLI
tests). CMake runs the Python
tests before `generate_font_test_images`; they can also be run independently:

```sh
cmake --build ../build/arm64-macos --target run_test_font_images_python
```

RGB RMSE and foreground-mask comparison live in `scripts/likeness.py` at the
repository root. They use Pillow. The existing Python requirement is reused;
install Pillow through the normal offline dependency step at the repository root:

```sh
./scripts/build.py install_ext
./scripts/build.py shell
```

`install_ext` installs the pinned Pillow wheel from `packages/python` into
`$DYNAMO_HOME/ext/lib/python`. The build shell exposes it on `PYTHONPATH`.
CMake verifies PNG handling and comparison when configuring host tests with
`BUILD_TESTS=ON`; no separate Python or ImageMagick installation is required.
Cross-target and `BUILD_TESTS=OFF` checks do not require Pillow or a graphics
device. From the build shell, you can also run `python3 scripts/likeness.py --check`
at the repository root.

Rebuild a standalone report from existing results, from `engine/font`:

```sh
python3 src/test/make_report.py --results build/font-render-report/results.json \
  --output build/font-render-report
```

On Linux, the image generator sets `GALLIVM_PERF=no_aos_sampling` before
initializing graphics. This selects Mesa's floating-point texture filtering
instead of its optimized 8-bit AoS path, whose rounding differences across CPUs
are amplified by SDF edge smoothing. It applies to manual captures as well as
CMake runs and leaves the engine's production rendering configuration unchanged.
