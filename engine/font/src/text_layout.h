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

#ifndef DM_TEXT_LAYOUT_H
#define DM_TEXT_LAYOUT_H

#include "markup.h"

#include <dmsdk/dlib/array.h>
#include <dmsdk/font/text_layout.h>

typedef void (*FTextLayoutDestroy)(HTextLayout layout);

enum TextRenderStyleFlags
{
    TEXT_RENDER_STYLE_FACE_COLOR    = 1 << 0,
    TEXT_RENDER_STYLE_FONT_SIZE     = 1 << 1,
    TEXT_RENDER_STYLE_OUTLINE_COLOR = 1 << 2,
    TEXT_RENDER_STYLE_OUTLINE_WIDTH = 1 << 3,
    TEXT_RENDER_STYLE_SHADOW_COLOR  = 1 << 4,
    TEXT_RENDER_STYLE_SHADOW_X      = 1 << 5,
    TEXT_RENDER_STYLE_SHADOW_Y      = 1 << 6,
    TEXT_RENDER_STYLE_SHADOW_BLUR   = 1 << 7,
    TEXT_RENDER_STYLE_OUTLINE_ALPHA = 1 << 8,
    TEXT_RENDER_STYLE_SHADOW_ALPHA  = 1 << 9,
};

struct TextRenderStyle
{
    float    m_FaceColor[4];
    float    m_OutlineColor[4];
    float    m_ShadowColor[4];
    float    m_FontSize;
    float    m_OutlineWidth;
    float    m_ShadowX;
    float    m_ShadowY;
    float    m_ShadowBlur;
    float    m_OutlineAlpha;
    float    m_ShadowAlpha;
    uint32_t m_Flags;
};

enum TextEffectType
{
    TEXT_EFFECT_GRADIENT,
    TEXT_EFFECT_WAVE,
    TEXT_EFFECT_SHAKE,
};

enum TextEffectFlags
{
    TEXT_EFFECT_AFFECTS_COLOR    = 1 << 0,
    TEXT_EFFECT_AFFECTS_POSITION = 1 << 1,
};

enum TextEffectFit
{
    TEXT_EFFECT_FIT_GLYPH,
    TEXT_EFFECT_FIT_SPAN,
    TEXT_EFFECT_FIT_TEXT,
};

enum TextGradientMode
{
    TEXT_GRADIENT_MODE_HORIZONTAL,
    TEXT_GRADIENT_MODE_VERTICAL,
    TEXT_GRADIENT_MODE_QUAD,
};

struct TextGradientEffect
{
    float   m_BottomLeft[4];
    float   m_BottomRight[4];
    float   m_TopLeft[4];
    float   m_TopRight[4];
    float   m_Hz;
    uint8_t m_Fit;
    uint8_t m_Mode;
};

struct TextWaveEffect
{
    float   m_Amplitude;
    float   m_Hz;
    float   m_Wavelength;
    uint8_t m_Fit;
};

struct TextShakeEffect
{
    float   m_Amplitude;
    float   m_Hz;
    uint8_t m_Fit;
};

struct TextEffect
{
    union
    {
        TextGradientEffect m_Gradient;
        TextWaveEffect     m_Wave;
        TextShakeEffect    m_Shake;
    };
    uint32_t m_TextOffset;
    uint32_t m_TextLength;
    uint16_t m_Type;
    uint16_t m_Flags;
};

struct TextResolvedSpan
{
    uint32_t m_TextOffset;
    uint32_t m_TextLength;
    uint16_t m_StyleIndex;
    uint16_t m_EffectIndex;
    uint16_t m_EffectCount;
    uint8_t  m_DecorationFlags;
    uint8_t  m_InlineDecorationFlags;
    uint8_t  m_UnderlinePattern;
    uint8_t  m_StrikePattern;
    uint8_t  m_HasObjectStyle;
};

enum TextResolvedDecorationFlags
{
    TEXT_RESOLVED_DECORATION_UNDERLINE = 1 << 0,
    TEXT_RESOLVED_DECORATION_STRIKE    = 1 << 1,
};

struct ResolvedMarkup
{
    dmArray<TextRenderStyle>           m_Styles;
    dmArray<TextEffect>                m_Effects;
    dmArray<uint16_t>                  m_SpanEffects;
    dmArray<TextResolvedSpan>          m_Spans;
    dmArray<char>                      m_ObjectSource;
    dmArray<TextLayoutObject>          m_Objects;
    dmArray<TextLayoutObjectAttribute> m_ObjectAttributes;
};

struct TextDecorationGeometry
{
    uint32_t m_GlyphIndex;
    float m_X;
    float m_Length;
};

// Retain shaped metrics for decorations that an object style can enable later.
struct TextDecorationSource
{
    TextDecoration m_Decoration;
    uint8_t        m_Flag;
};

struct TextLayoutHitTestParams
{
    dmhash_t m_Tag; // Zero matches all layout object types.
    float    m_X;
    float    m_Y;
    float    m_Width;
    float    m_Height;
    float    m_FontSize;
    float    m_MonospacePadding;
    uint32_t m_Align;
    uint32_t m_VAlign;
};

struct TextLayoutObjectBounds
{
    float    m_MinX;
    float    m_MinY;
    float    m_MaxX;
    float    m_MaxY;
    uint16_t m_Parent;
};

struct TextLayout
{
    FTextLayoutDestroy m_Destroy;
    uint32_t           m_RefCount;

    // TODO: Make these C arrays?
    dmArray<TextGlyph>                 m_Glyphs;
    dmArray<TextLine>                  m_Lines;
    dmArray<TextParagraph>             m_Paragraphs;
    dmArray<TextRenderStyle>           m_Styles;
    dmArray<TextEffect>                m_Effects;
    dmArray<uint16_t>                  m_SpanEffects;
    dmArray<TextResolvedSpan>          m_ResolvedSpans;
    dmArray<TextDecorationSource>      m_DecorationSources;
    dmArray<TextDecoration>            m_Decorations;
    dmArray<TextDecorationGeometry>    m_DecorationGeometry;
    dmArray<uint32_t>                  m_DecorationGeometryOffsets;
    dmArray<char>                      m_ObjectSource;
    dmArray<TextLayoutObject>          m_Objects;
    dmArray<TextLayoutObjectAttribute> m_ObjectAttributes;
    dmArray<dmhash_t>                  m_ObjectStyleOverrides;
    dmArray<TextLayoutObjectBounds>    m_ObjectBounds;
    TextLayoutHitTestParams            m_ObjectBoundsParams;

    HFontCollection                    m_FontCollection;
    dmhash_t                           m_BaseStyleName;

    uint32_t                           m_NamedStyleRevision;
    uint16_t                           m_BaseStyleCount;
    uint16_t                           m_BaseEffectCount;
    uint16_t                           m_BaseSpanEffectCount;
    uint16_t                           m_BaseResolvedSpanCount;

    uint16_t                           m_NumValidGlyphs; // TODO: Remove non renderable glyphs
    bool                               m_UseRichText;

    // Used for creating a cell in the glyph image cache texture
    float m_MaxGlyphWidth;
    float m_MaxGlyphHeight;

    // Bounds of the entire layout
    float                    m_Width;
    float                    m_Height;
    double                   m_ElapsedTime;
    FTextLayoutReleaseObject m_ReleaseObject;
    void*                    m_ObjectContext;
};

TextResult TextLayoutLegacyCreate(HFontCollection     collection,
                                  uint32_t*           codepoints,
                                  uint32_t            num_codepoints,
                                  TextLayoutSettings* settings,
                                  HTextLayout*        outlayout);

TextResult TextLayoutLegacyCreateMarkup(HFontCollection collection, HMarkup markup, TextLayoutSettings* settings, HTextLayout* outlayout);

TextResult TextLayoutSkribidiCreate(HFontCollection     collection,
                                    uint32_t*           codepoints,
                                    uint32_t            num_codepoints,
                                    TextLayoutSettings* settings,
                                    HTextLayout*        outlayout);

TextResult TextLayoutSkribidiCreateMarkup(HFontCollection collection, HMarkup markup, TextLayoutSettings* settings, HTextLayout* outlayout);

TextResult TextLayoutCreateMarkup(HFontCollection collection, HMarkup markup, TextLayoutSettings* settings, HTextLayout* outlayout);

// Resolves generic markup nodes into renderer-facing styles, effects, and objects.
bool TextLayoutResolveMarkup(HFontCollection collection, HMarkup markup, TextLayoutSettings* settings, ResolvedMarkup* resolved);

struct TextNamedStyleDecoration;
// Compiles an opening-tag fragment into one named style and its effects.
// A decoration output enables underline and strikethrough; null rejects them.
bool TextLayoutCompileStyleFragment(const char* definition, uint32_t definition_length, TextRenderStyle* style, dmArray<TextEffect>* effects, TextNamedStyleDecoration* decoration, MarkupError* error);

// Transfers resolved markup storage and object callbacks into a layout.
// A null resolved pointer initializes a single span for plain styled text.
void TextLayoutAdoptResolvedMarkup(HTextLayout layout, ResolvedMarkup* resolved, TextLayoutSettings* settings, uint32_t text_length);

// Precomputes physically ordered, gap-free per-glyph decoration geometry.
void TextLayoutInitializeDecorationGeometry(HTextLayout layout);

// Reports whether a decoration has cached per-glyph geometry.
bool TextLayoutDecorationRequiresGlyphSegments(HTextLayout layout, const TextDecoration& decoration);

// Returns one precomputed glyph segment for a decoration.
const TextDecorationGeometry* TextLayoutGetDecorationGeometry(HTextLayout layout, const TextDecoration& decoration, uint32_t segment_index);

// Releases resources acquired for the layout's sprite objects.
void TextLayoutReleaseObjects(HTextLayout layout);

// Captures base glyph styles and applies named layout-object styles.
void TextLayoutInitializeObjectStyles(HTextLayout layout);

// Reapplies named object styles after overrides or font style definitions change.
bool TextLayoutRefreshObjectStyles(HTextLayout layout);

struct TextGlyphFaceColors
{
    float m_BottomLeft[4];
    float m_BottomRight[4];
    float m_TopLeft[4];
    float m_TopRight[4];
};

struct TextGlyphRenderData
{
    TextGlyphFaceColors m_FaceColors;
    float               m_OutlineColor[4];
    float               m_ShadowColor[4];
    float               m_OffsetX;
    float               m_OffsetY;
    float               m_OutlineWidth;
    float               m_ShadowX;
    float               m_ShadowY;
    float               m_ShadowBlur;
    uint32_t            m_StyleFlags;
};

// Returns the index of the tagged layout object under the local text-render
// position, or UINT32_MAX when no object was hit. Each object uses one AABB
// spanning all its glyphs and lines, including nested objects. Bounds follow
// layout positions, unaffected by animated styles. Nested/later objects win.
uint32_t TextLayoutHitTestObject(HTextLayout layout, const TextLayoutHitTestParams& params);

// Resolves the final four-corner face colors for a glyph.
void TextLayoutGetGlyphFaceColors(HTextLayout layout, const TextGlyph& glyph, const float base_color[4], TextGlyphFaceColors* colors);

// Resolves all static style and animated-effect data needed for vertex generation.
void TextLayoutGetGlyphRenderData(HTextLayout layout, const TextGlyph& glyph, const float base_color[4], TextGlyphRenderData* data);

// Reports whether the layout needs an outline vertex layer.
bool TextLayoutHasMarkupOutline(HTextLayout layout);

// Returns the largest markup outline width in layout units.
float TextLayoutGetMaxMarkupOutlineWidth(HTextLayout layout);

// Reports whether the layout needs a shadow vertex layer.
bool TextLayoutHasMarkupShadow(HTextLayout layout);

// Resolves line baselines and total height from shaped glyph metrics.
void TextLayoutFinalizeLineBaselines(HTextLayout layout, TextLayoutSettings* settings);

#if defined(FONT_USE_SKRIBIDI)
/** A paragraph-local Unicode script run prepared before Skribidi layout. */
struct TextLayoutRun
{
    uint32_t    m_Offset;
    uint32_t    m_Length;
    uint32_t    m_Script;
    const char* m_Language;
};

/** Splits UTF-32 text into paragraphs and then into language-tagged script runs. */
void TextLayoutSegmentRuns(uint32_t* codepoints, uint32_t num_codepoints, const char* default_language, dmArray<TextLayoutRun>& runs, dmArray<TextParagraph>& paragraphs);
#endif

uint32_t TextToCodePoints(const char* text, dmArray<uint32_t>& codepoints);

#endif // DM_TEXT_LAYOUT_H
