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
    uint8_t  m_UnderlinePattern;
    uint8_t  m_StrikePattern;
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
    dmArray<TextDecoration>            m_Decorations;
    dmArray<char>                      m_ObjectSource;
    dmArray<TextLayoutObject>          m_Objects;
    dmArray<TextLayoutObjectAttribute> m_ObjectAttributes;
    dmArray<dmhash_t>                  m_ObjectStyleOverrides;

    HFontCollection                    m_FontCollection;

    uint32_t                           m_NamedStyleRevision;
    uint16_t                           m_BaseStyleCount;
    uint16_t                           m_BaseEffectCount;
    uint16_t                           m_BaseSpanEffectCount;
    uint16_t                           m_BaseResolvedSpanCount;

    uint16_t                           m_NumValidGlyphs; // TODO: Remove non renderable glyphs

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
bool TextLayoutResolveMarkup(HMarkup markup, TextLayoutSettings* settings, ResolvedMarkup* resolved);

// Compiles an opening-tag fragment into one named style and its effects.
bool TextLayoutCompileStyleFragment(const char* definition, uint32_t definition_length, TextRenderStyle* style, dmArray<TextEffect>* effects, MarkupError* error);

// Transfers resolved markup storage and object callbacks into a layout.
void TextLayoutAdoptResolvedMarkup(HTextLayout layout, ResolvedMarkup* resolved, TextLayoutSettings* settings);

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
