# Slug vector font algorithm

Defold renders Vector font faces directly from quadratic Bézier curves using
Slug. Outlines use a signed distance field (SDF), and shadows use a blurred
bitmap. The editor preview and runtime share the curve preparation and shader
algorithm.

## Rendering the face

1. **Prepare the curves.** Normalize each glyph's outline to its bounds and pack
   the quadratic control points into a curve texture.
2. **Build the bands.** Divide the glyph into horizontal and vertical bands
   (eight per axis by default). Each band stores a list of the curves that can
   intersect it. Sort the lists so the shader can stop once later curves cannot
   affect the pixel.
3. **Evaluate coverage.** For each pixel, look up its horizontal and vertical
   bands, solve the curve crossings, and combine their contributions into face
   coverage. Screen-space derivatives determine the pixel footprint for
   antialiasing.

Glyph instances share the font map's cached curve and band records. Changing
the display size or zoom transforms the glyph without regenerating its curves.

### Tuning the band count

Set **Render > Vector Font Bands** in `game.project` to choose the number of
bands per axis for the runtime glyph cache:

```ini
[render]
vector_font_bands = 32
```

The default is **8**, and the supported range is **1–32**. Invalid runtime values
log a warning and use 8. More bands can shorten the candidate lists evaluated
by the fragment shader, at the cost of larger band tables and potentially more
cache growth or resets. Measure with the project's fonts and target devices.
This changes lookup tables, not glyph geometry or antialiasing quality.

Both static glyph banks and dynamic fonts build these tables at runtime; no
different baked curve format is needed. The built-in vertex shader reads the
count from `face_color.x`, which the runtime supplies on vector glyph vertices.
Custom vector vertex shaders should use this count instead of a fixed band
maximum. Native editor previews and color glyph paint layers retain their
separate eight-band encoding.

## Textures

Each runtime Vector font map uses these textures:

| Texture | Format | Dimensions | Contents |
| --- | --- | --- | --- |
| Curves | `RGBA16F` | **4096 × 8–256 texels** | Quadratic control points and curve bounds |
| Bands | `R32UI` | **4096 × 8–256 texels** | Band headers and references to curves |
| Effects (when enabled) | `RGB` | Font cache width × height | SDF outlines and bitmap shadows |

The curve and band textures contain numeric data. Curve records and band lists
are kept within texture rows. The effects texture stores glyph images in cells
and follows the font's cache settings.

Font curve records store max X and max Y in the second texel's Z/W components.
These bounds use the same half-float coordinates as the three control points.
The shader reads the endpoint and bounds first, then loads the other control
points only if the curve can influence the pixel.

Band headers and references retain their existing 16-bit count/offset and X/Y
address fields. Font face shaders use `SlugRenderWithBounds`, since the font
encoder always stores bounds. `SlugRender` reads the control points directly
and supports both shared endpoint records and records with stored bounds.
Storing bounds prevents adjacent curves from sharing the endpoint texel and
increases occupied curve data. Texture growth follows the same row limits and
cache policy.

The runtime starts with eight rows in each numeric texture. When the atlas fills,
it doubles their height at the next frame boundary, up to 256 rows, and rebuilds
the cached glyph records. Queued text retains its texture handles for the current
frame. At the limit, overflow still resets the cache to reclaim unused glyphs.

The editor preview uses the same texture formats and shaders as the runtime.
Its curve and band textures are
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

Omitting `runtime` means Static. New font templates explicitly set `runtime: true`.

Both modes use the same CPU effect generator. Static fonts still pack their
curves into GPU records and decompress/upload effect images when cached.
Dynamic prewarming also runs on the CPU; it does not make the font static.

### Updating projects that used runtime generation

The project-wide `font.runtime_generation` setting is deprecated and has no
effect. Existing fonts that omit `runtime` now use Static generation, even if
the old project setting is enabled. To retain dynamic glyph generation:

1. Set **Glyph Generation** to **Dynamic** (`runtime: true`) in each SDF or Vector
   `.font` resource that should generate glyphs at runtime. Keep its existing
   font mode and material. Bitmap fonts remain Static.
2. Enable **Use full text layout system** in the project's App Manifest for
   target builds. Put representative text in **Prewarm Text** (`characters` in
   the `.font` file) to prepare common glyphs when the font loads.
3. Remove `font.runtime_generation` from `game.project`.

This is an explicit migration: loading a project does not change its font
generation choices. Fonts that remain Static only include the glyphs selected
at build time, so review fonts used with runtime font collections or text that
contains characters outside that set.

## Materials and text layout

Select `vector_font_mode: VECTOR_FONT_MODE_VECTOR` and use:

- `/builtins/fonts/font-vector.material` for GUI text.
- `/builtins/fonts/label-vector.material` for labels.

Both materials use `font-vector.vp` and `font-vector.fp`. Face, outline, and
shadow are separate quads rendered with the same material. The shared text
layout supplies display size, corner colors, animated offsets, and effect
colors. Face-only Vector fonts need no Bitmap Size.

### Monochrome vector materials

Use `/builtins/fonts/font-vector-monochrome.material` for GUI text or
`/builtins/fonts/label-vector-monochrome.material` for labels when the font does
not need color glyph paint. These materials retain rich-text colors, gradients,
outlines, shadows, and decorations. Color font glyphs use their ordinary outline
fallback. Removing the paint interpreter reduces the fragment program without
changing the number of text draws.

### Separate face, bitmap, and decoration programs

A custom `.render` resource can opt into specialized vector programs by adding
all three material resources:

```text
render_resources {
  name: "vector_font_face"
  path: "/builtins/fonts/font-vector-face.material"
}
render_resources {
  name: "vector_font_bitmap"
  path: "/builtins/fonts/font-vector-bitmap.material"
}
render_resources {
  name: "vector_font_decoration"
  path: "/builtins/fonts/font-vector-decoration.material"
}
```

The renderer divides monochrome vector batches into consecutive runs of face,
bitmap-effect, and decoration quads, preserving their order and vertex buffer.
Outlines and shadows share the bitmap program. This increases draw calls when
the layer changes, but lets each fragment program contain only the work needed
for its layer. Choose this using measurements on the target device.

The complete set belongs to its render script instance and is cleared when the
instance or its resources are replaced. Deleting a specialized material, including
a material hot reload, disables splitting until the `.render` resource rebinds
the complete set. Materials with a `paint_texture` sampler
retain the combined program, as do render-script material overrides and indexed
texture overrides. Named texture overrides follow sampler names. Custom materials
also retain it if the specialized
materials have incompatible vertex space, missing original tags, or missing
samplers. If either draw pool cannot fit every run, the intact original batch
is submitted. Custom specialized materials must accept the built-in vector
font vertex layout and preserve the original shader's colors and constants.

## Cache and platform limits

If a glyph does not fit, the runtime keeps existing records intact, skips that
glyph, and schedules numeric atlas growth and a cache reset for the next frame.
A glyph set that exceeds the maximum 4096 × 256 capacity cannot be fully cached
at once. Numeric and effect atlas growth is applied before render batches
capture texture handles.

Runtime rendering requires `RGBA16F` and unsigned `R32UI` texture support. It
does not provide a GLES 2 / WebGL 1 fallback. Projects using vector fonts must
explicitly enable **Shader > Exclude GLES 2.0** (`shader.exclude_gles_sm100`) in
`game.project`. Otherwise, compiling the GLES 2 shader variant reports an error.

The CPU packing code is in [font_vector_slug.cpp](../font/src/font_vector_slug.cpp).
The shared coverage shader is [font-vector-slug.glsl](../font/content/builtins/fonts/font-vector-slug.glsl),
adapted from Eric Lengyel's Slug reference implementation with its MIT license
retained in the source.
