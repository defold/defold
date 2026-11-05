# Font library image tests

## Fontviewer rendering modes and color emojis

Select **SDF**, **Bitmap**, or **Vector** under Properties → Render mode.
Vector is the default. The selection applies to ordinary text and emojis, and is
also available as `--mode sdf|bitmap|vector` for interactive use and exports.

Vector uses `src/test/data/NotoColorEmoji-Regular.ttf` as an emoji fallback.
It contains COLRv1 vector layers and a CPAL palette. SDF and Bitmap use the
neighboring monochrome `NotoEmoji-Regular.ttf`; only Vector renders the font's
palette colors. Both fixtures are unmodified copies from
`external/skribidi/package/Skribidi/example/data`, with their SIL Open Font
License in `src/test/data/NotoEmoji-LICENSE.md`.
Rebuild the HarfBuzz external package with the smaller shaping-only configuration,
then build `fontviewer`. From the repository root, open the rich text sample with:

```sh
<fontviewer> --mode vector -tf engine/font/src/test/data/markup.txt
```

The shared engine reader in `src/truetype/font_colr.cpp` reads COLR/CPAL without
HarfBuzz paint callbacks. The standalone TrueType/CFF parser extracts outlines;
the production Slug encoder and coverage shader render them. Solid colors and linear/radial gradients are evaluated
on the GPU using premultiplied linear-light colors. Palette conversion happens
when caching; the shader converts the completed glyph back to sRGB for rendering.
Each color glyph uses one quad; the shader composites its layers before
applying text opacity, preserving colors when text fades. The numeric textures hold curves, bands and paint parameters, with
no raster emoji atlas. Ordinary Vector text uses the same analytical Slug
coverage with the rich text face color; its outlines and shadows use the SDF
effect atlas. Bitmap uses generated coverage with baked outlines and shadows.
Color emoji outlines and shadows are not generated. Variable COLR paints, sweep
gradients, nested outline clips and compositing beyond source-over are unsupported
and fall back to the glyph's monochrome outline or `.notdef` without interrupting
surrounding text. This includes some flags in the bundled font.

Dynamic Vector fonts in game Label/GUI rendering use the same paint cache in
`src/font_vector_paint.cpp` and gradient shader in `font-vector-paint.glsl`.
Full-layout collections register both color and outline fonts for emoji selection.
The font-map material enables COLR emoji only when it has `curve_texture`,
`band_texture` and `paint_texture` samplers. SDF, Bitmap and older Vector materials
without `paint_texture` select a supplied monochrome fallback. Changing this policy
affects newly created layouts. Color paint data is uploaded only when glyphs are added. The cache shares the existing
bounded Slug atlas; it does not invalidate geometry already submitted in a frame.
Prebaked font banks and editor previews continue to use their existing monochrome
payload; they do not yet serialize COLR paint graphs.

The focused rendering checks require Pillow and a desktop OpenGL context.
They run through `run_test_fontviewer_color` and the normal `run_tests` target
on local desktops and native Linux CI. They export PNG frames, so no external
WebP encoder is required. Run from `engine/font` in the build shell:

```sh
python3 src/test/test_fontviewer_color.py <fontviewer>
```

They check all three modes, monochrome emoji fallback, bitmap span outlines,
palette colors, gradients, composed-glyph opacity, rich text sizes, ZWJ and skin-tone
sequences, animation, unsupported-glyph fallback, and the complete rich text sample.
The small `src/test/data/color-test.ttf` fixture provides exact gradient and
composition values, foreground colors, and an outline reused from a clipped
color glyph. Triangular outlines check that shared composite fills apply edge
coverage once, while independent clips and layer lists retain separate coverage.
Regenerate it with `python3 src/test/make_color_test_font.py`.
Use `-o output.png` for a PNG export; animations also write
`output-000001.png`, `output-000002.png`, etc. WebP export still uses `img2webp`.

## Font parsing and shaping

Both legacy and full-layout engines use `src/truetype/font_truetype.cpp` for
glyf, CFF1 and CFF2 outlines at default variation coordinates. Full layout still
uses HarfBuzz for shaping, mapping and OpenType metrics. The adapter supplies
our glyph bounds to HarfBuzz, including COLRv1 clip boxes for color glyphs.
When a clip box is absent, COLRv0/v1 bounds come from the transformed paint
outlines, including nested clipping.

The external build keeps `HB_NO_DRAW`, `HB_NO_PAINT`, `HB_NO_CFF` and `HB_NO_VAR`.
A small build-time patch preserves default-axis style metadata (`fvar`) for
Skribidi's font selection. There are no runtime variation-axis controls.

`test_font_truetype` covers the decoder, CFF bounds supplied to shaping, and
variable-font default styles. `test_font_colr` covers palette/foreground layers,
gradients, transforms, color glyph bounds, invalid offsets and cyclic graphs.
The color reader is compiled into both engine font libraries and has no
HarfBuzz or graphics dependency. The shared Slug paint cache supplies callbacks
for both fontviewer and the production renderer. Tests pin the supported glyph
set in the emoji fixture, cover all static transforms and clipping, and compare
all 1,464 CFF2 paths against a saved HarfBuzz reference checksum.

## Font image matrix

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

Editor Vector previews use the same Slug curve and band records, with a float-atlas
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

Vector contributes 115 captures across the four configurations:

| Sources | Scenarios | Layout/parser configurations |
| --- | --- | --- |
| TTF and OTF runtime glyphs | Face only; face + outline + shadow; 2× font size | All four |
| TTF and OTF Fontc glyph banks | Same three scenarios | All four |
| All four Vector sources | Rich text color and 150% size override | Legacy/full with rich text |
| All four Vector sources | Overlapping text at different world depths | All four |
| All four Vector sources | Solid and dashed underline/strikethrough | Legacy/full with rich text |
| All four Vector sources | Outlined underline/strikethrough at ½× and 2× font size | Legacy/full with rich text |
| Stress TTF | All 64 visible glyphs render after numeric atlas growth | All four |
| Arabic TTF | Wrapped RTL placement with trailing whitespace | Full with rich/plain text |
| COLR runtime emoji | Palette/gradient colors; unsupported-glyph fallback; text-color preservation | Full layout, plain/rich (tint requires rich text) |

The Vector face uses analytical curves; outline and shadow sample the generated
SDF effect atlas. Glyph data is generated at size 40, including the cases drawn
at size 80. Captures assert that faces, blue outlines, and green shadows are
present and fit within the fixed capture rectangle. Vector uses separate layer
quads; manual Vector captures require `--layers multi`.
The depth case draws coincident red foreground and green background text through
a depth buffer and requires only the red text to remain. Decoration cases enable
face culling and check pixels inside word spaces, independently of reference
images. The outlined cases also check that the outline limit scales with font
size. Their references gate the full rendered result at 98% likeness.

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
The CMake target runs all four (543 images) and then generates the report.

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
