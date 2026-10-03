# Slug vector font algorithm

Defold renders Vector font faces directly from quadratic Bézier curves using
Slug. Outlines use a signed distance field (SDF), and shadows use a blurred
bitmap. The editor preview and runtime share the curve preparation and shader
algorithm.

## Rendering the face

1. **Prepare the curves.** Normalize each glyph's outline to its bounds and pack
   the quadratic control points into a curve texture.
2. **Build the bands.** Divide the glyph into eight horizontal and eight vertical
   bands. Each band stores a list of the curves that can intersect it. Sort the
   lists so the shader can stop once later curves cannot affect the pixel.
3. **Evaluate coverage.** For each pixel, look up its horizontal and vertical
   bands, solve the curve crossings, and combine their contributions into face
   coverage. Screen-space derivatives determine the pixel footprint for
   antialiasing.

Glyph instances share the font map's cached curve and band records. Changing
the display size or zoom transforms the glyph without regenerating its curves.

## Textures

Each runtime Vector font map uses these textures:

| Texture | Format | Dimensions | Contents |
| --- | --- | --- | --- |
| Curves | `RGBA16F` | **4096 × 8 texels** | Quadratic control points |
| Bands | `R32UI` | **4096 × 8 texels** | Band headers and references to curves |
| Effects (when enabled) | `RGB` | Font cache width × height | SDF outlines and bitmap shadows |

The curve and band textures contain numeric data. Curve records and band lists
are kept within texture rows. The effects texture stores glyph images in cells
and follows the font's cache settings.

The editor's GL 2 preview uses the same curve records, but stores band fields in
the R and G channels of an `RGBA32F` texture. Its curve and band textures are
**4096 texels wide**, with each height determined by the occupied rows, up to
**256 rows**.

## Outlines and shadows

Effects are generated on the CPU at the font's Bitmap Size. Their channels are:

| Channel | Contents | Use |
| --- | --- | --- |
| R | Antialiased face mask | Retained in the effect image; the face shader uses curves |
| G | Signed distance field | Reconstructed as outline coverage in the shader |
| B | Blurred coverage | Shadow of the outline when enabled, otherwise of the face |

The generator derives face and shadow coverage from a signed-distance image,
then approximates a Gaussian shadow with three separable box filters. Blur is
specified as sigma in glyph pixels, with padding of three times sigma.

Outline width and shadow blur are baked at the generation size. Scaling the
text also scales these cached effects. Changing a static font's Bitmap Size or
blur requires rebuilding its glyph data.

## Static and dynamic generation

| Mode | When glyph data is generated | Compiled font dependency |
| --- | --- | --- |
| Static (`runtime: false`) | Bob generates curves and optional effect images for the requested characters at build time | Glyph bank |
| Dynamic (`runtime: true`) | Prewarming and later glyph requests generate data at runtime | Source TTF/OTF |

Both modes use the same CPU effect generator. Static fonts still pack their
curves into GPU records and decompress/upload effect images when cached.
Dynamic prewarming also runs on the CPU; it does not make the font static.

## Materials and text layout

Select `vector_font_mode: VECTOR_FONT_MODE_VECTOR` and use:

- `/builtins/fonts/font-vector.material` for GUI text.
- `/builtins/fonts/label-vector.material` for labels.

Both materials use `font-vector.vp` and `font-vector.fp`. Face, outline, and
shadow are separate quads rendered with the same material. The shared text
layout supplies display size, corner colors, animated offsets, and effect
colors. Face-only Vector fonts need no Bitmap Size.

## Cache and platform limits

The runtime curve and band textures have fixed dimensions. If a glyph does not
fit, the cache keeps existing records intact, skips that glyph, and schedules
a reset for the next frame. A glyph set that exceeds this capacity cannot be
fully cached at once. Effect atlas growth is applied before render batches
capture texture handles.

Runtime rendering requires `RGBA16F` and unsigned `R32UI` texture support. It
does not provide a GLES 2 / WebGL 1 fallback. The editor's GL 2 representation
is a separate preview path.

The CPU packing code is in [font_vector_slug.cpp](../font/src/font_vector_slug.cpp).
The shared coverage shader is [font-vector-slug.glsl](../font/content/builtins/fonts/font-vector-slug.glsl),
adapted from Eric Lengyel's Slug reference implementation with its MIT license
retained in the source.
