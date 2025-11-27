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

We currently do not have any native support for any form of rich text.
It is in our plans however.

### Runtime generation

For fonts that support it (currently only .ttf), we can generate distance fields at runtime. This helps keep the game bundle size to a minimum.

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

Final vertex data generation is handled by `dmRender::CreateFontVertexData` in `engine/render/src/render/font/default/font_default_vertex.cpp`.

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
At this stage, the file format is either an offline font (.glyphbankc), or a runtime font (.ttf).

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
In particular, you can get a `HFont` from a `HFontCollection` (.fontc) and associate it with another `HFontCollection`. This will increase the reference count of the underlying `HFont` resource (.ttf).

The scripting api will also allow for generating new glyphs on-the-fly. This operation is asynchronous, as the bitmap generation is time consuming.


