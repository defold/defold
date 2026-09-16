# Slug vector font path

The editor includes Vector/SDF mode, Static/Dynamic glyph generation, inferred
BMFont sizes, and separate label/GUI display sizes. Vector faces do not require
an authored size; enabled bitmap effects default to a generation size of 15 in
the editor. Explicit Static choices survive saving and reopening.

Editor builds and previews use the Slug curves and bitmap effects described below.

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

A failed numeric append preserves previously cached glyphs. Atlas exhaustion
skips the incoming glyph and schedules a reset for the next frame so existing
vertices do not reference replaced records. A working set larger than the fixed
numeric capacity is not fully supported yet.

Bob uses its normal shader compiler pipeline. This path needs numeric RGBA16F
and unsigned R32UI textures. It is not a GLES 2 / WebGL 1 fallback. Sources are
packaged with builtins, but these materials are excluded from the connect
archive which explicitly forces GLES 1.00. Compilation alone does not establish
runtime support on every graphics backend.
