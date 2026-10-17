# Overview

In the engine we have several concepts that together make up our text rendering system:

* fonts
* font collections
* text shaping
* text rendering.

The engine supports bitmap fonts and rasterized distance-field fonts. It currently doesn't support true vector rendering by default.

For text layout, we support both a very basic layout and also full text shaping, as an opt-in.

## Font library (`./engine/font`)

### HFont (`dmsdk/font/font.h`)

The font library defines the `HFont` handle which represents a single loaded font (.ttf, .otf, or .glyphbankc).

The font api allows you to get the information about a single glyph.

The api also allows you to request a `FontGlyph`, which holds the information about a single glyph, including the bitmap data.

Also, each `HFont` uses a `uint32_t` as a unique hash. This is later used to make a unique key when requesting the information for a `(font hash, glyph index)` pair.

### HFontCollection (`dmsdk/font/font_collection.h`)

As a .ttf font may be very large (>65k glyphs), it it often split into multiple fonts (e.g. different languages). In such cases, it's common to use the concept of a "font collection". In our case, it's represented by the `HFontCollection` handle.

A font collection always references at least one `HFont`.
During operations, lookups are performed across the HFonts to find the glyph index. The search order is implementation-specific; no guarantee is made.

### HTextLayout (`dmsdk/font/text_layout.h`)

A text layout holds all the information for the final result of the text shaping. such as placement of the glyphs with word wrapping, ligatures and so on.
At this point, it is ready for rendering (assuming that the bitmap data is ready).

We currently support two backends:
* Legacy layout
* Full layout (opt-in)

The legacy layout only gets glyphs based on the 32 bit unicode codepoints, and it doesn't do any pair kernings or any other advanced text shaping.

The full layout supports full text shaping featuring right-to-left and other layout rules.
Full text layout support requires building locally with `--enable-feature=font_layout` or using an app manifest.

The text layout api uses the `TextGlyph`.

### Rich text

Rich text parsing and style/effect resolution are provided by the `font_richtext`
library. The parser API remains link-compatible when markup is excluded: an
engine built with `--disable-feature=font_richtext` links `font_richtext_null`
instead, and markup creation returns `MARKUP_RESULT_UNSUPPORTED`. Plain text
layout and rendering continue to work through pass-through render-data helpers,
without linking the parser or style/effect implementation.

Markup can be applied through both the legacy and full layout paths. The parser
itself is independent and may be used without enabling `font_layout`.

#### GPU color effects

Built-in font materials use a shared `font_effects` RGBA8 data texture owned by
the fontmap. Each 72-byte font vertex carries `effect`: selected effect UVs,
signed record index, and a bounded animation phase. Gradients replace inherited
face RGB and cap its alpha, matching markup color precedence. Gradient records
and independent layout clocks do not split batches.

The shared shader helper `font_effect_color(effect)` takes the packed effect
vector explicitly: fit UVs in `xy`, metadata row in `z`, and animation phase in
`w`. Calling shaders declare their own varying or supply a computed vector.
The metadata sampler remains the shared `font_effects` uniform.

The texture stores four colors, four 16-bit stop positions, and type parameters.
The shader evaluates spatial constant color, linear, radial, and four-corner
effects per fragment. Radial parameters contain the pivot and an inverse ellipse
transform, including fit dimensions, rotation, and radii. Equal radii produce a
circle in text-local space. The six parameters fit in the existing 64-byte record.
Legacy directional/four-corner markup keeps its original fitting and vertex
interpolation: negative record indices select `font_effect_vertex_color`, which
evaluates animation and alpha caps in the vertex shader and clears the index
before rasterization. Positive indices select fragment evaluation. Both paths
use the same metadata texture.

Explicit `type=color|linear|radial|four-corner` selects spatial fitting with
`fit=glyph|span|block` (default: block). Glyph fitting uses unpadded glyph bounds;
radial glyph fitting uses unpadded line bounds vertically, so glyphs
share a Y center while keeping their individual X centers. Span bounds cover
the tagged range, including its line breaks and nested color overrides; block
bounds cover all visible glyphs after line alignment. Four-corner markup without
a `type` also supports `fit=block`. Nested gradients use the innermost gradient.

Color, linear, and radial types accept `color0` through `color3`, with one to
four consecutive colors. The color type takes only `color0`. Stops are evenly
spaced unless `stop0` through `stop3` override their positions in [0, 1]; positions
must be nondecreasing. Four-corner uses `bl`, `br`, `tl`, and `tr`.

Linear `angle` is in counterclockwise degrees: 0 runs bottom-to-top, 90 runs
right-to-left, 180 runs top-to-bottom, and 270 runs left-to-right. Stop 0 is at
the starting edge; the final stop is at the ending edge.
Projection uses local distances, so a 45-degree axis stays at 45 degrees on a
wide block. Radial `pivot-x` and `pivot-y` default to 0.5. Radial `radius` defaults
to 0.5 times the longer fit dimension and supplies both ellipse radii. Positive
`radius-x` and `radius-y` override their respective axes, regardless of attribute
order. Both use the longer fit dimension, so equal values produce a circle.
Radial `angle` defaults to 0 and rotates the ellipse counterclockwise around its
pivot; at 0 degrees the radii follow the text's X/Y axes, and at 90 degrees those
axes exchange directions. Rotation uses local text units, preserving the ellipse
on rectangular fits. Component transforms can subsequently stretch the text.
`hz` and `direction=forward|reverse` animate the sampled coordinate using the
layout clock and mirrored wrapping. Static gradients clamp at their endpoints.

For example, `<gradient type=linear fit=block angle=45 color0=#FF4050
color1=#FFD23F color2=#35D07F color3=#468BFF>text</gradient>` uses four stops
across the block. Separate tags around each line give independently fitted
and angled span gradients. Named font styles preserve the same metadata.

For a tilted ellipse, use `<gradient type=radial fit=block radius-x=0.5
radius-y=0.25 angle=45 color0=#FFFF00 color1=#FF8000 color2=#FF0000
color3=#8000FF>text</gradient>`. Existing radial markup that only specifies
`radius` remains circular.

The cache starts with 256 fixed-size records in a 16 KiB texture and doubles
when all rows are in use. Allocated capacity is retained between frames. Row
zero stores reciprocal texture height; vertices keep stable integer indices
when the texture grows, without splitting batches. Records used during a frame
remain pinned across its render passes; unused records can be reused later.
Growth is limited by the graphics backend and a maximum of 16,384 rows. Reaching
that limit logs an error before using approximate vertex colors for overflow.
Changes upload before the corresponding draws. The editor keeps a texture
snapshot with each cached vertex batch and grows its native metadata likewise.
Both caches index records by content hash and compare complete records on hash
collisions. Lookups avoid scanning all previously used effects; runtime eviction
scans each pinned row at most once per frame.
Materials without the effect sampler use CPU color evaluation at vertices for
compatibility. This approximates spatial gradients: radial centers and stops
inside a glyph require the built-in GPU shader for accurate rendering.

Spatial bounds reuse layout storage. One glyph traversal accumulates block and
line bounds and, when needed, a range tree keyed by source cluster. Span bounds
query the authored range in that tree, including glyphs with overridden colors
and reordered glyphs in shaped text.
Adjacent glyphs sharing an effect and fit dimensions reuse its projection and
resolved metadata record. Compiled fonts retain unpadded metrics so their
fits match editor previews; old glyph banks retain their existing bounds.

#### Underline and strikethrough

The markup resolver records `<ul>` and `<strike>` as decoration flags and a
solid or dashed pattern on each affected text span. The selected layout backend
then creates a `TextDecoration` for every part of the span on a physical line.
At this point underline and strikethrough use the same representation: the
backend has resolved their difference into a baseline-relative Y position and
thickness. The full backend gets these metrics from Skribidi; the legacy backend
derives them from the configured font size.

Vertex generation normally emits one six-vertex quad for a complete decoration
segment. It splits the segment at glyph boundaries only when this is necessary
to preserve different styles, markup spans, or glyph-fitted gradients. The
decoration colors are resolved from the geometrically outermost glyphs so that
gradients also follow the visual direction of right-to-left text. Glyph-position
effects such as shake and wave are deliberately not applied to decorations;
they remain attached to the line baseline.

Decoration quads sample a single opaque texel in the font atlas and render only
in the face layer. A solid line needs no additional shader work. For a dashed
line, normalized pattern position and duty are packed into otherwise unused
decoration `layer_mask` components and interpolated across the quad. The built-in
font shaders decode these values into the repeating dash mask. Custom font
materials must apply the same decoding to render dashed decorations; otherwise
the line appears solid.

### Runtime generation

For `.ttf` and `.otf` fonts, we can generate distance fields at runtime. This helps keep the game bundle size to a minimum.

This is an opt-in feature that requires building the engine locally with `--enable-feature=font_layout` or using an app manifest.

See the [documentation](https://defold.com/manuals/font/#enabling-runtime-fonts) on more detailed instructions.

### Offline generation

This is the default, and for TrueType fonts, it also means we generate a distance field for each glyph into a "glyph bank" (`.glyphbankc`). This can be sizeable, as a game may need many glyphs from each language, and each font rasterizes into its designated size.
Offline generated fonts does not perform shaping (converting sequences of code points into final glyph indices), which is needed for more complex languages (e.g., Arabic).

## Render library

This library handles the high-level logic of rendering various types of objects, such as text.
A text object is registered with `dmRender::DrawTextParams`, and represented by a `dmRender::TextEntry`.

The main text render loop is in `engine/render/src/render/font/font_renderer.cpp`. It handles missing glyphs and requests more data from the registered callback (see `engine/gamesys/src/gamesys/resources/res_font.cpp`). Any missing glyphs are represented by the character `~` (codepoint value == `126`).

The render library also manages the texture cache, via its `HFontMap`

Final vertex data generation is coordinated by `dmRender::CreateFontVertexData` in `engine/render/src/render/font/font_renderer.cpp`. The `font` library handles fonts, glyph generation, and text layout without depending on graphics. The `font_render` library in `engine/font/src/render` owns the fixed `FontGlyphVertex` layout, packs glyph vertices, and creates the matching graphics vertex declaration. The `fontc_shared` C/FFM boundary lives separately in `engine/font/src/fontc` and links the full-text-shaping font implementation with `font_render`.

For distance-field fonts, SDF smoothing is computed in screen space. The renderer derives a pixels-per-local-unit scale from the current view-projection matrix and viewport and uses it to keep edge thickness stable under camera zoom and perspective. If the projection is invalid for a text entry (e.g. clip `w` near zero or a zero-sized viewport), it falls back to the local/world scale.

### dmRender::HFontMap

The font map holds cached information such as glyph metrics and glyph bitmap data.
It also owns and handles the "glyph cache", which is an instance of `dmGraphics::HTexture`.

The font map holds callbacks in case data is missing (See `res_font.cpp`).
To identify the correct font and glyph index, we use a key created like so:

```c++
glyph_key = (font_hash << 32 | glyph_index)
```

#### The Glyph Cache

The glyph cache texture may grow dynamically up to a maximum size of `4096x2048`. If a preset size is set by the developer, it will use that and will not grow.

If the glyph cache is full, it evicts the oldest glyph to make room for a new one.

*NOTE:* Currently, if a glyph is evicted from the cache texture, it isn't deallocated, but still resides within the font map.

*NOTE:* The texture is currently using a cell size which is the size of the maximum of the currently loaded glyphs. In the future, we may use a more efficient bin packing, and/or more texture pages.

Each glyph in the cache is represented by a dmRender::CacheGlyph (see `render/font/fontmap.h`)

## Gamesys library

This library handles file formats, component logic, and scripting.

### FontResource

In `engine/gamesys/src/gamesys/resources/res_font.cpp` we load a `.fontc` and create a `FontResource*`. This represents an `HFontCollection`.
At this stage, the file format is either an offline font (`.glyphbankc`), or a runtime font (`.ttf` or `.otf`).

`res_font.cpp` is responsible for producing data for a glyph index: glyph info or glyph distance-field bitmap. See `OnGlyphCacheMiss()` in `res_font.cpp`.

Multiple `HFont` instances may be associated with a font collection. See `AddFontInternal` for implementation details. This only works for runtime fonts.

#### Prewarming text

When the `.fontc` resource is loaded, and if it's a runtime font, we check the `characters` field of the DDF struct to help figure out which glyphs to load immediately.

We do this by creating an `HTextLayout` using this text, and then sending the resulting glyph indices to our glyph-generation thread.

### Glyph generation

For runtime fonts, we use a separate thread to generate the distance-field bitmap data.
Requests come from `res_font.cpp`; when generation completes, a callback is issued and `dmRender::HFontMap` is populated with the new glyph data.

### Components

The text renderer is used by the
* Label component
* GUI component's text node
* Engine debug text renderer (which isn't part of the gamesys lib)

They register a text entry with the renderer each frame.

### Scripting

Scripting is handled by the `font.*` Lua module (see `script_font.cpp`).

The scripting module allows for associating a compatible `HFont` with a `HFontCollection`.
In particular, you can get a `HFont` from a `HFontCollection` (.fontc) and associate it with another `HFontCollection`. This will increase the reference count of the underlying `HFont` resource (`.ttf` or `.otf`).

The scripting api will also allow for generating new glyphs on-the-fly. This operation is asynchronous, as the bitmap generation is time consuming.
