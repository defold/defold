# Font library image tests

Start `./scripts/build.py shell` at the repository root, then run from `engine/font`:

```sh
cmake -S ../.. -B ../build/arm64-macos -DBUILD_TESTS=ON
cmake --build ../build/arm64-macos --target generate_font_test_images
```

Four generator executables compile `src/test/test_font_bitmap_gen.cpp`: legacy/full
layout, each with the rich-text parser enabled/excluded. Each runs only its
supported font matrix by default, sharing one graphics context. The ordinary
`test_font` unit tests remain independent and require no graphics context.
The matrix is enumerated in `src/test/make_report.py`.

The tests use fontviewer's direct graphics approach: glyph atlas, font-library
layout/vertex packing, and font shaders. One hidden OpenGL context per process
renders directly into off-screen targets. Vector cases use the production
render library cache, vertex backend, and Slug shader. There is no engine
executable, generated project, component, frame delay or process per image. PNG output is normalized
from the backend's BGRA readback. Actual and accepted current captures use the same
fixed rectangles and text origins from `src/test/data/font_render/capture_geometry.json`.
No foreground-dependent cropping is applied; clipping is an assertion failure.

The matrix covers TTF/OTF distance-field, bitmap, and Vector output, glyph-bank
providers, BMFont, both bitmap layer modes, effect alpha/width/shadow settings,
rich markup overrides, and wrapped English/Arabic. Bitmap glyph-bank snapshots
exercise prebaked layout/fallback. Vector banks use the curve and effect payload
exported by Fontc. Java/Bob tests cover the resource compiler integration.
Labels and GUI components are not included here.

Editor Vector previews use the same Slug curve and band records, with an OpenGL 2
adapter that stores the two integer band fields as exact float32 values. Faces
remain analytical; outlines use the generated SDF channel and shadows use the
blurred bitmap channel. Numeric textures are finalized and uploaded once after
all entries in a batch have populated the glyph cache. Invisible glyphs retain
their layout advance without creating drawable quads.
`integration.vector-font-test` covers Label drawing and picking, GUI stencil
clipping, switching SDF/Vector in open views, texture uploads per batch, and
closing/reopening preview contexts. Run it
from `editor` with `lein test integration.vector-font-test` after rebuilding Bob's
font renderer library and Java bindings.

Vector contributes 56 captures across the four configurations:

| Sources | Scenarios | Layout/parser configurations |
| --- | --- | --- |
| TTF and OTF runtime glyphs | Face only; face + outline + shadow; 2× font size | All four |
| TTF and OTF Fontc glyph banks | Same three scenarios | All four |
| All four Vector sources | Rich text color and 150% size override | Legacy/full with rich text |

The Vector face uses analytical curves; outline and shadow sample the generated
SDF effect atlas. Glyph data is generated at size 40, including the cases drawn
at size 80. Captures assert that faces, blue outlines, and green shadows are
present and fit within the fixed capture rectangle. Vector uses separate layer
quads; manual Vector captures require `--layers multi`.

```sh
./build/arm64-macos/src/test/test_font_bitmap_gen --case otf_vector_bank_multi_effects \
  --output build/font-test-images
```

Images and per-configuration logs go into `build/font-test-images/`. The HTML,
JSON and Markdown report goes into `build/font-render-report/`, with one child
report per executable and stable case paths. Each `index.html` is a standalone
file with embedded images, case details, logs, and downloadable data; share
that file without its surrounding directories. CMake finishes the report before
failing on assertions, missing captures or likeness below 98%.
Options and reproduction commands remain in HTML; stdout prints them only for
failed cases.

Linux CI uploads the standalone HTML report even when the tests fail. Download
the `build-reports-<platform>-<attempt>` artifact from the engine job's Build
Reports summary or the workflow run's artifacts. Reports are retained for 14 days.

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
The CMake target runs all four (400 images) and then generates the report.

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
