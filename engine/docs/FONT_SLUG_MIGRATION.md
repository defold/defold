# Slug vector font path

This branch starts at the latest dev commit already merged into the R&D branch
(`0c9968ea2b5df64a895247dd16230078fc30ce6f`). It carries the production font,
resource, Bob and shader integration. Fontviewer experiments, benchmark tools,
reports, radial shadows and dilation experiments remain on `rnd-vector-fonts`.
The editor includes Vector/SDF mode, Static/Dynamic glyph generation, inferred
BMFont sizes, and separate label/GUI display sizes. Vector faces do not require
an authored size; enabled bitmap effects default to a generation size of 15 in
the editor. Explicit Static choices survive saving and reopening.

Editor builds use the Slug glyph banks and bitmap effects described below.
Editor previews use the existing SDF renderer at the selected display size;
they do not yet reproduce Slug's curve rendering or baked bitmap effects.

## Resources and rendering

Use `/builtins/fonts/font-vector.material` for GUI text, or
`/builtins/fonts/label-vector.material` for labels, with
`vector_font_mode: VECTOR_FONT_MODE_VECTOR` in the font resource. Both materials
use `font-vector.vp` and `font-vector.fp`. The shared shader renders three modes:
Slug vector face, SDF outline, and bitmap shadow. Layers are separate quads
using the same material; there is no additional shadow shader or material.
Existing SDF fonts remain available and retain their default materials.
Legacy scalable fonts using the built-in bitmap materials migrate to the
corresponding SDF materials. Labels resolve the matching material at build and
preview time, including when the font has unsaved edits. BMFont materials are
unchanged. Custom material paths are preserved; custom shaders must interpret
the generated distance-field data themselves.

Vector glyphs use the shared text layout's bounded layer counts and resolved
span styles. Size, corner colors, animated offsets, and outline/shadow colors
follow the text layout. Shadow blur remains baked at the font's authored size.

The face uses quadratic curves in RGBA16F and eight horizontal plus eight
vertical bands in R32UI. References are sorted for Slug's early exit. Glyph
instances share the font map's cached records. The internal reference fill
implementation is `font-vector-slug.glsl`, with its upstream license retained.

## Static and dynamic preparation

With `runtime: false`, Bob generates curves and optional RGB effect bitmaps for
the requested character set and serializes them in the glyph bank. The font
resource references that bank, not the source TTF/OTF. Runtime loading needs no
source font or generation job. The engine still packs normalized curves into
GPU band/curve records once per cached glyph and decompresses/uploads bitmaps;
static mode does not mean there is no CPU upload preparation.

With `runtime: true`, the font generation jobs use the same CPU bitmap helper.
The existing synchronous cache-miss fallback also uses that helper. No GPU pass
is used to generate or blur these bitmaps.

Bitmap channels are:

- R: antialiased face mask, retained in the initial RGB layout.
- G: signed distance, reconstructed into outline coverage in the shader.
- B: blurred shadow of the outline when enabled, otherwise of the face.

The initial production helper thresholds a CPU-generated signed-distance image
into a face mask and the shadow source, preserving the distance field for
outlines. It then applies three separable box filters to approximate
a Gaussian shadow. Sliding sums make filtering linear in pixel area, independent
of blur radius. The authored blur is sigma in glyph pixels, with 3-sigma padding.
This helper is not the viewer's experimental vector rasterizer, and viewer
preparation/performance numbers are not measurements of this runtime path.

Effects are baked at the authored font size, outline width and blur radius.
Scaling or zooming transforms those cached images together with the vector face.
A new static effect size requires a rebuilt font or a separate font variant;
changing a shader parameter cannot recover another blur from the bitmap. Bob's
cache key distinguishes outline/shadow enablement as well as dimensions. Font
map and glyph bank carry an explicit bitmap-format marker and reject mismatched
old vector-SDF data with a rebuild error.

## Memory and platform constraints

The initial numeric cache reserves two 4096 x 8 textures (262,144 bytes of
RGBA16F curves and 131,072 bytes of R32UI band records), plus CPU mirrors.
Occupied numeric records are counted separately by `m_CurveTexels * 8` and
`m_BandTexels * 4`; row padding and capacity are additional. Effect bitmaps use
the font's existing cell atlas, with three bytes per pixel before compression.
Lower-resolution effects and channel packing are follow-up optimizations.

A failed numeric append preserves previously cached glyphs. Atlas exhaustion
skips the incoming glyph and schedules a reset for the next frame so existing
vertices do not reference replaced records. A working set larger than the fixed
numeric capacity is not fully supported yet.

Bob uses its normal shader compiler pipeline. This path needs numeric RGBA16F
and unsigned R32UI textures. It is not a GLES 2 / WebGL 1 fallback. Sources are
packaged with builtins, but these materials are excluded from the connect
archive which explicitly forces GLES 1.00. Compilation alone does not establish
runtime support on every graphics backend.

## Validation scope

Tests cover static glyph-bank compilation without a source-font dependency,
bitmap-format markers, distinct curve/bitmap data, 0/4/16px CPU blur preparation,
cache reuse/overflow, dynamic job lifecycle and static resource loading. The
static runtime fixture includes a 4px outline contributing to a 16px shadow.
The CMake Release build and Bob tests are validated; legacy Waf source/library
lists are updated but a full Waf build has not been run. A static-only gamesys
test linked with `font_gen_null` passes without the TTF parser or generation
jobs. Editor font, label, GUI, and save-data integration tests cover property
defaults, glyph-generation persistence, compiled vector data, and GUI overrides.
Actual engine GPU performance, Slug editor previews, broader glyph quality and
non-Metal runtime validation remain separate work. Existing R&D report data has
not been relabeled as results for this branch.
