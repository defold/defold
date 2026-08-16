// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_TEXT_LAYOUT_H
#define DMSDK_TEXT_LAYOUT_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>

typedef struct Font*           HFont;
typedef struct FontCollection* HFontCollection;

/*# API for laying out complex text into format ready for display
 *
 * API for laying out complex text into format ready for display
 *
 * @document
 * @name TextLayout
 * @language C
 */

/*#
 * A handle representing a text layout
 * @typedef
 * @name HTextLayout
 */
typedef struct TextLayout* HTextLayout;

/*# Attribute belonging to a layout object
 *
 * Offsets and lengths are UTF-8 byte ranges in the string returned by
 * `TextLayoutGetObjectSource()`.
 *
 * @struct
 * @name TextLayoutObjectAttribute
 * @member m_NameOffset [type: uint32_t] Attribute-name byte offset in the object source.
 * @member m_ValueOffset [type: uint32_t] Attribute-value byte offset in the object source.
 * @member m_NameLength [type: uint16_t] Attribute-name length in bytes; zero for shorthand values.
 * @member m_ValueLength [type: uint16_t] Attribute-value length in bytes.
 */
struct TextLayoutObjectAttribute
{
    uint32_t m_NameOffset;
    uint32_t m_ValueOffset;
    uint16_t m_NameLength;
    uint16_t m_ValueLength;
};

/*# Object found while resolving markup for a layout
 *
 * Sprite objects have resolved dimensions and an opaque value owned by the
 * resolver. Their dimensions reserve inline layout space; rendering the
 * sprite is the caller's responsibility. Link objects describe their visible
 * UTF-32 text range and have zero dimensions until their geometry is queried.
 *
 * @struct
 * @name TextLayoutObject
 * @member m_Resource [type: uintptr_t] Opaque resolver-owned resource value.
 * @member m_Width [type: float] Resolved object width in layout units.
 * @member m_Height [type: float] Resolved object height in layout units.
 * @member m_TextOffset [type: uint32_t] Zero-based UTF-32 offset in visible text.
 * @member m_TextLength [type: uint32_t] Visible UTF-32 length; one for an inline sprite's U+FFFC object-replacement codepoint.
 * @member m_Id [type: uint64_t] Stable object identifier derived from the `id` attribute, or generated from its order in the layout source.
 * @member m_AttributeIndex [type: uint16_t] First object attribute in the layout attribute array.
 * @member m_AttributeCount [type: uint16_t] Number of consecutive object attributes.
 * @member m_Tag [type: dmhash_t] Hash of the markup tag name.
 */
struct TextLayoutObject
{
    uintptr_t            m_Resource;
    uint64_t             m_Id;
    float                m_Width;
    float                m_Height;
    uint32_t             m_TextOffset;
    uint32_t             m_TextLength;
    uint16_t             m_AttributeIndex;
    uint16_t             m_AttributeCount;
    dmhash_t             m_Tag;
};

/*# Resolve a resource-backed layout object
 *
 * Called once for every sprite, including cache misses. The proposed
 * dimensions already include explicit markup dimensions; each unspecified
 * dimension is one em. The callback must set `object->m_Width` and
 * `object->m_Height`, and may store an acquired resource handle in
 * `object->m_Resource`. Returning false aborts markup layout creation.
 *
 * `source` and `attributes` are borrowed and valid for the duration of the
 * callback. Equivalent data can later be obtained from the created layout.
 *
 * @typedef
 * @name FTextLayoutResolveObject
 * @param context [type: void*] User context from `TextLayoutSettings.m_ObjectContext`.
 * @param source [type: const char*] Null-terminated markup source owned by the layout being created.
 * @param attributes [type: const TextLayoutObjectAttribute*] Attribute array indexed by `object->m_AttributeIndex`.
 * @param proposed_width [type: float] Explicit markup width, or one em when omitted.
 * @param proposed_height [type: float] Explicit markup height, or one em when omitted.
 * @param object [type: TextLayoutObject*] Object to update with resolved dimensions and an optional resource.
 * @return success [type: uint8_t] Non-zero on success; zero aborts layout creation.
 */
typedef uint8_t (*FTextLayoutResolveObject)(void*                            context,
                                            const char*                      source,
                                            const TextLayoutObjectAttribute* attributes,
                                            float                            proposed_width,
                                            float                            proposed_height,
                                            TextLayoutObject*                object);

/*# Release a resource acquired by `FTextLayoutResolveObject`
 *
 * Called during the layout's final release for each resolved sprite.
 * The callback may be null when the resolver never acquires resources.
 *
 * @typedef
 * @name FTextLayoutReleaseObject
 * @param context [type: void*] User context from `TextLayoutSettings.m_ObjectContext`.
 * @param object [type: const TextLayoutObject*] Sprite object whose resource should be released.
 */
typedef void (*FTextLayoutReleaseObject)(void* context, const TextLayoutObject* object);

/*#
 * An enum representing text layout results
 * @enum
 * @name TextResult
 * @member TEXT_RESULT_OK
 * @member TEXT_RESULT_ERROR
 */
enum TextResult
{
    TEXT_RESULT_OK,
    TEXT_RESULT_ERROR,
};

/*#
 * An enum representing text layout directions
 * @enum
 * @name TextDirection
 * @member TEXT_DIRECTION_LTR   Left-to-right text direction
 * @member TEXT_DIRECTION_RTL   Right-to-left text direction
 */
enum TextDirection
{
    TEXT_DIRECTION_LTR = 0,
    TEXT_DIRECTION_RTL = 1,
};

/*#
 * An enum representing text layout features
 * Each font supports a layout type
 * The selected layout type it the minimum value of layout types
 * @enum
 * @name TextLayoutType
 * @member TEXT_LAYOUT_TYPE_LEGACY Legacy text shaping api
 * @member TEXT_LAYOUT_TYPE_FULL   Full text shaping api
 */
enum TextLayoutType
{
    TEXT_LAYOUT_TYPE_LEGACY = 0,
    TEXT_LAYOUT_TYPE_FULL   = 1,
};

/*# Text glyph flags
 * @enum
 * @name TextGlyphFlags
 * @member TEXT_GLYPH_FLAG_OBJECT The glyph reserves inline layout space for a layout object and must not be rasterized as text.
 */
enum TextGlyphFlags
{
    TEXT_GLYPH_FLAG_OBJECT = 1 << 0,
};

/*#
 * Glyph representing the final position within a layout
 * @struct
 * @name TextGlyph
 * @member m_Font [type: HFont] The font used for this glyph
 * @member m_X [type: float] the final x position, relative the top-left corner of the layout
 * @member m_Y [type: float] the final y position, relative the top-left corner of the layout
 * @member m_Width [type: float] the width of the glyph
 * @member m_Height [type: float] the height of the glyph
 * @member m_RenderScale [type: float] scale applied to the cached glyph geometry during vertex generation
 * @member m_Codepoint [type: uint32_t] original codepoint (if available)
 * @member m_GlyphIndex [type: uint16_t] the glyph index in the font
 * @member m_Cluster [type: uint32_t] the index in the visible UTF-32 text that this glyph corresponds to
 * @member m_StyleIndex [type: uint16_t] resolved render style index
 * @member m_BaseStyleIndex [type: uint16_t] render style before layout-object named styles are applied
 * @member m_MarkupSpanIndex [type: uint16_t] resolved markup span index, or 0xffff when no markup is present
 * @member m_BaseMarkupSpanIndex [type: uint16_t] markup span before layout-object named styles are applied
 * @member m_Flags [type: uint16_t] `TextGlyphFlags` describing non-text layout glyphs
 */
struct TextGlyph
{
    // The font is needed for actually using the glyph index (i.e. rasterizing the glyph bitmap)
    HFont    m_Font;

    float    m_X;           // X position inside the layout
    float    m_Y;           // Y position inside the layout
    // The bounding box is used to calculate the space required in the glyph cache texture
    float    m_Width;       // width of the glyph bounding box
    float    m_Height;      // height of the glyph bounding box
    float    m_RenderScale; // final geometry scale relative to the layout's base font size

    uint32_t m_Codepoint;           // Not always available if there was a substitution
    uint32_t m_Cluster;             // index into visible UTF-32 text
    uint16_t m_GlyphIndex;          // index into the font
    uint16_t m_StyleIndex;          // index into resolved render styles
    uint16_t m_BaseStyleIndex;      // style index before layout-object named styles
    uint16_t m_MarkupSpanIndex;     // index into resolved markup spans
    uint16_t m_BaseMarkupSpanIndex; // markup span before layout-object named styles
    uint16_t m_Flags;

    // private
    float m_Advance;     // LEGACY SHAPING ONLY! TODO: See if we can remove these
    float m_LeftBearing; // LEGACY SHAPING ONLY!
};

/*#
 * Represents a line of glyphs
 * @struct
 * @name TextLine
 * @member m_Width [type: float] Width of the line
 * @member m_Index [type: uint16_t] Index into the list of glyphs
 * @member m_Length [type: uint16_t] Number of glyphs to render
 * @member m_ParagraphIndex [type: uint16_t] Index of the paragraph containing the line
 * @member m_Baseline [type: float] Final baseline position measured from the bottom of the layout
 */
struct TextLine
{
    float    m_Width;
    float    m_Baseline;
    uint16_t m_Index;
    uint16_t m_Length;
    uint16_t m_ParagraphIndex;
};

/*# Text decoration pattern
 * @enum
 * @name TextDecorationPattern
 * @member TEXT_DECORATION_PATTERN_SOLID A continuous decoration line.
 * @member TEXT_DECORATION_PATTERN_DASHED A dashed decoration line.
 */
enum TextDecorationPattern
{
    TEXT_DECORATION_PATTERN_SOLID,
    TEXT_DECORATION_PATTERN_DASHED,
};

/*# Resolved line decoration
 *
 * Decorations contain backend-independent geometry after shaping, BiDi
 * reordering, and line wrapping. `m_Y` is relative to the line baseline. The
 * original underline or strike type is deliberately omitted because its font
 * metrics have already been resolved into `m_Y` and `m_Thickness`.
 *
 * @struct
 * @name TextDecoration
 * @member m_X [type: float] Start position in the shaped line coordinate system.
 * @member m_Y [type: float] Vertical offset from the line baseline.
 * @member m_Length [type: float] Decoration length.
 * @member m_Thickness [type: float] Resolved font-derived thickness.
 * @member m_PatternOffset [type: float] Stable pattern phase offset.
 * @member m_GlyphStart [type: uint32_t] First associated layout glyph.
 * @member m_GlyphCount [type: uint16_t] Number of associated layout glyphs.
 * @member m_LineIndex [type: uint16_t] Physical line containing the decoration.
 * @member m_Pattern [type: TextDecorationPattern] Line pattern.
 */
struct TextDecoration
{
    float    m_X;
    float    m_Y;
    float    m_Length;
    float    m_Thickness;
    float    m_PatternOffset;
    uint32_t m_GlyphStart;
    uint16_t m_GlyphCount;
    uint16_t m_LineIndex;
    uint8_t  m_Pattern;
};

/*#
 * Represents a paragraph of lines
 * @struct
 * @name TextParagraph
 * @member m_TextIndex [type: uint32_t] Index into the source codepoints
 * @member m_TextLength [type: uint32_t] Number of source codepoints, excluding the paragraph separator
 * @member m_LineIndex [type: uint16_t] Index into the list of lines
 * @member m_LineCount [type: uint16_t] Number of lines in the paragraph
 * @member m_Direction [type: TextDirection] Base direction of the paragraph
 */
struct TextParagraph
{
    uint32_t      m_TextIndex;
    uint32_t      m_TextLength;
    uint16_t      m_LineIndex;
    uint16_t      m_LineCount;
    TextDirection m_Direction;
};

/*#
 * Describes how to do a text layout
 * @struct
 * @name TextLayoutSettings
 * @member m_Size [type: float] The desired size of the font (in pixels)
 * @member m_Width [type: float] Max layout width. Used only when m_LineBreak is non-zero
 * @member m_Leading [type: float] The extra space between each line. Set 1.0f as default.
 * @member m_Tracking [type: float] The extra tracking between glyphs. Set 0 as default.
 * @member m_ResolveObject [type: FTextLayoutResolveObject] Resolver required when markup contains a sprite.
 * @member m_ReleaseObject [type: FTextLayoutReleaseObject] Optional finalizer for resources acquired by the resolver.
 * @member m_ObjectContext [type: void*] User context passed to both object callbacks.
 * @member m_Padding [type: uint32_t] Legacy: Padding for monospace, glyphbank fonts
 * @member m_LineBreak [type: uint8_t:1] Allow line breaks
 * @member m_Monospace [type: uint8_t:1] Legacy: Is the font a monospace font. Current: should be set on the font in the font collection!
 */
struct TextLayoutSettings
{
    float                    m_Size;
    float                    m_Width;
    float                    m_Leading;
    float                    m_Tracking;

    FTextLayoutResolveObject m_ResolveObject;
    FTextLayoutReleaseObject m_ReleaseObject;
    void*                    m_ObjectContext;

    uint32_t                 m_Padding;
    uint8_t                  m_LineBreak : 1;
    uint8_t                  m_Monospace : 1;
};

/*#
 * Create a text layout using a font collection
 * if successful, the caller owns the returned layout and must call TextLayoutRelease()
 * @name TextLayoutCreate
 * @param collection [type: HFontCollection] the font collection
 * @param codepoints [type: uint32_t*] an array of codepoints
 * @param num_codepoints [type: uint32_t] number of codepoints in the array
 * @param settings [type: TextLayoutSettings*] the settings used for rendering
 * @param layout [type: HTextLayout*] (out) the output text layout
 * @return result [type: TextResult] the result. TEXT_RESULT_OK if successful
 */
TextResult TextLayoutCreate(HFontCollection     collection,
                            uint32_t*           codepoints,
                            uint32_t            num_codepoints,
                            TextLayoutSettings* settings,
                            HTextLayout*        layout);

/*#
 * Acquire a shared reference to a previously created layout
 * @name TextLayoutAcquire
 * @param layout [type: HTextLayout] the text layout
 */
void TextLayoutAcquire(HTextLayout layout);

/*#
 * Release a previously created layout
 * @name TextLayoutRelease
 * @param layout [type: HTextLayout] the text layout
 */
void TextLayoutRelease(HTextLayout layout);

/*#
 * Advance the animation clock used by markup effects in a text layout.
 *
 * This function only changes the layout's accumulated effect time. It does not
 * reshape text or alter the base glyph positions, lines, paragraphs, or bounds.
 * Animated offsets are applied later when glyph vertices are generated.
 * Call this once per frame with the elapsed time since the previous frame.
 * Non-finite and non-positive values are ignored.
 *
 * @name TextLayoutUpdate
 * @param layout [type: HTextLayout] the text layout
 * @param delta_time [type: float] elapsed time in seconds
 */
void TextLayoutUpdate(HTextLayout layout, float delta_time);

/*#
 * Get the glyph count in the layout
 * @name TextLayoutGetGlyphCount
 * @param layout [type: HTextLayout] the text layout
 * @return count [type: uint32_t] the number of glyphs in the layout
 */
uint32_t TextLayoutGetGlyphCount(HTextLayout layout);

/*#
 * Get the glyphs in the layout
 * @name TextLayoutGetGlyphs
 * @param layout [type: HTextLayout] the text layout
 * @return glyphs [type: TextGlyph*] the array of glyphs in the layout
 */
TextGlyph* TextLayoutGetGlyphs(HTextLayout layout);

/*#
 * Get the line count in the layout
 * @name TextLayoutGetLineCount
 * @param layout [type: HTextLayout] the text layout
 * @return count [type: uint32_t] the number of lines in the layout
 */
uint32_t TextLayoutGetLineCount(HTextLayout layout);

/*#
 * Get the lines in the layout
 * @name TextLayoutGetLines
 * @param layout [type: HTextLayout] the text layout
 * @return lines [type: TextLine*] the array of lines in the layout
 */
TextLine* TextLayoutGetLines(HTextLayout layout);

/*#
 * Get the paragraph count in the layout
 * @name TextLayoutGetParagraphCount
 * @param layout [type: HTextLayout] the text layout
 * @return count [type: uint32_t] the number of paragraphs in the layout
 */
uint32_t TextLayoutGetParagraphCount(HTextLayout layout);

/*#
 * Get the paragraphs in the layout
 * @name TextLayoutGetParagraphs
 * @param layout [type: HTextLayout] the text layout
 * @return paragraphs [type: TextParagraph*] the array of paragraphs in the layout
 */
TextParagraph* TextLayoutGetParagraphs(HTextLayout layout);

/*# Get resolved decoration count
 * @name TextLayoutGetDecorationCount
 * @param layout [type: HTextLayout] the text layout
 * @return count [type: uint32_t] number of underline and strike segments
 */
uint32_t TextLayoutGetDecorationCount(HTextLayout layout);

/*# Get resolved decorations
 * @name TextLayoutGetDecorations
 * @param layout [type: HTextLayout] the text layout
 * @return decorations [type: const TextDecoration*] borrowed decoration array
 */
const TextDecoration* TextLayoutGetDecorations(HTextLayout layout);

/*# Get layout object count
 * @name TextLayoutGetObjectCount
 * @param layout [type: HTextLayout] the text layout
 * @return count [type: uint32_t] number of sprites and links
 */
uint32_t TextLayoutGetObjectCount(HTextLayout layout);

/*# Get layout objects
 *
 * The borrowed array remains valid until the layout is released.
 *
 * @name TextLayoutGetObjects
 * @param layout [type: HTextLayout] the text layout
 * @return objects [type: const TextLayoutObject*] layout object array
 */
const TextLayoutObject* TextLayoutGetObjects(HTextLayout layout);

/*# Get the rendered position of a layout object
 *
 * Resolves the lower-left corner of an object using the same shaped-line
 * coordinate normalization as text rendering. `paragraph_x` and
 * `paragraph_top` define the upper-left layout origin. `paragraph_width` is
 * used to place right-to-left lines.
 *
 * @name TextLayoutGetObjectPosition
 * @param layout [type: HTextLayout] the text layout
 * @param object [type: const TextLayoutObject*] object borrowed from `TextLayoutGetObjects`
 * @param paragraph_x [type: float] left layout origin
 * @param paragraph_top [type: float] top layout origin
 * @param paragraph_width [type: float] layout width
 * @param x [type: float*] resolved lower-left x coordinate (out)
 * @param y [type: float*] resolved lower-left y coordinate (out)
 * @return found [type: uint8_t] non-zero when the object belongs to a layout line
 */
uint8_t TextLayoutGetObjectPosition(HTextLayout layout, const TextLayoutObject* object, float paragraph_x, float paragraph_top, float paragraph_width, float* x, float* y);

/*# Get layout object attributes
 * @name TextLayoutGetObjectAttributes
 * @param layout [type: HTextLayout] the text layout
 * @return attributes [type: const TextLayoutObjectAttribute*] attribute array
 */
const TextLayoutObjectAttribute* TextLayoutGetObjectAttributes(HTextLayout layout);

/*# Get the UTF-8 source referenced by layout object attributes
 * @name TextLayoutGetObjectSource
 * @param layout [type: HTextLayout] the text layout
 * @return source [type: const char*] null-terminated markup source copy
 */
const char* TextLayoutGetObjectSource(HTextLayout layout);

/*# Set a layout object's named style override
 *
 * The named style is applied after the object's default style. The default is
 * the markup `style` attribute when present, otherwise the object's tag name.
 * Pass zero to restore the default style. This affects rendering only and
 * never reshapes or reflows text.
 *
 * @name TextLayoutSetObjectStyle
 * @param layout [type: HTextLayout] the text layout
 * @param object_id [type: uint64_t] ID returned in `TextLayoutObject.m_Id`
 * @param style [type: dmhash_t] named style hash, or zero to restore the default
 * @return changed [type: uint8_t] non-zero when the object style changed
 */
uint8_t TextLayoutSetObjectStyle(HTextLayout layout, uint64_t object_id, dmhash_t style);

/*#
 * Get the lines in the layout
 * @name TextLayoutGetBounds
 * @param layout [type: HTextLayout] the text layout
 * @return width [type: float*] the total width of the layout (out)
 * @return height [type: float*] the total height of the layout (out)
 */
void TextLayoutGetBounds(HTextLayout layout, float* width, float* height);

#endif // DMSDK_TEXT_LAYOUT_H
