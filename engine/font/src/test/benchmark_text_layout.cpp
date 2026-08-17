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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <dlib/time.h>
#include <dlib/utf8.h>
#include <dlib/vmath.h>

#include "font.h"
#include "fontcollection.h"
#include "glyph_gen.h"
#include "layout_vertex.h"
#include "text_layout.h"

#if defined(FONT_BENCHMARK_MARKUP)
#include "markup.h"
#endif

#if !defined(FONT_BENCHMARK_BUILD_LABEL)
#define FONT_BENCHMARK_BUILD_LABEL "base"
#endif

// Run this executable from engine/font. It emits CSV so repeated Release builds
// can be compared without making timing-sensitive assertions part of unit tests.
// By default, each operation is calibrated independently to the target sample
// duration. Pass --iterations=N to replace calibration with a fixed iteration count.
static const uint32_t    BENCHMARK_TEXT_LENGTH = 32000;
static const uint32_t    DEFAULT_ITERATIONS = 0;
static const uint32_t    DEFAULT_SAMPLES = 11;
static const uint32_t    DEFAULT_TARGET_MILLISECONDS = 100;
static const uint32_t    DEFAULT_WARMUP_MILLISECONDS = 20;
static const uint32_t    MAX_ITERATIONS = 100000;

static volatile uint64_t g_BenchmarkChecksum = 0;

struct BenchmarkOptions
{
    const char* m_FontPath;
    const char* m_StyleFilter;
    uint32_t    m_Iterations;
    uint32_t    m_Samples;
    uint32_t    m_TargetMilliseconds;
    uint32_t    m_WarmupMilliseconds;
    uint32_t    m_OrderSeed;
    uint32_t    m_TagCountFilter;
};

struct Measurement
{
    double   m_MedianMicroseconds;
    double   m_P25Microseconds;
    double   m_P75Microseconds;
    double   m_MedianAbsoluteDeviationMicroseconds;
    uint32_t m_Iterations;
};

typedef bool (*FBenchmarkIteration)(void* context);

struct PlainLayoutContext
{
    HFontCollection    m_Collection;
    uint32_t*          m_Codepoints;
    uint32_t           m_CodepointCount;
    TextLayoutSettings m_Settings;
};

struct CachedVertexGlyph
{
    HFont     m_Font;
    uint32_t  m_GlyphIndex;
    float     m_FontSize;
    uint32_t  m_CellX;
    uint32_t  m_CellY;
    FontGlyph m_Glyph;
};

struct VertexGenerationContext
{
    HTextLayout                 m_Layout;
    dmArray<CachedVertexGlyph>* m_GlyphCache;
    dmArray<FontGlyphVertex>*   m_Vertices;
    FontLayoutVertexConfig      m_Config;
    FontLayoutVertexMetrics     m_Metrics;
    float                       m_DefaultFontSize;
    uint32_t                    m_CacheMaxAscent;
};

struct PlainLayoutAndVerticesContext
{
    PlainLayoutContext       m_Layout;
    VertexGenerationContext* m_Vertices;
};

#if defined(FONT_BENCHMARK_TAGS)
enum MarkupStyleType
{
    MARKUP_STYLE_NONE,
    MARKUP_STYLE_COLOR,
    MARKUP_STYLE_SIZE,
    MARKUP_STYLE_GRADIENT_HORIZONTAL,
    MARKUP_STYLE_GRADIENT_VERTICAL,
    MARKUP_STYLE_GRADIENT_QUAD,
    MARKUP_STYLE_SHAKE,
    MARKUP_STYLE_WAVE,
    MARKUP_STYLE_OUTLINE,
    MARKUP_STYLE_SHADOW,
    MARKUP_STYLE_UNDERLINE_SOLID,
    MARKUP_STYLE_UNDERLINE_DASHED,
    MARKUP_STYLE_STRIKE_SOLID,
    MARKUP_STYLE_STRIKE_DASHED,
    MARKUP_STYLE_LINK,
    MARKUP_STYLE_SPRITE,
};

struct MarkupSource
{
    dmArray<char> m_Bytes;
    uint32_t      m_SelectedWordCount;
    uint32_t      m_WordCount;
};

struct MarkupLayoutContext
{
    HFontCollection    m_Collection;
    HMarkup            m_Markup;
    TextLayoutSettings m_Settings;
};

struct MarkupParseContext
{
    const char* m_Source;
    uint32_t    m_SourceLength;
};

struct MarkupEndToEndContext
{
    HFontCollection          m_Collection;
    const char*              m_Source;
    uint32_t                 m_SourceLength;
    TextLayoutSettings       m_Settings;
    VertexGenerationContext* m_Vertices;
};
#endif

static int CompareDouble(const void* left, const void* right)
{
    double a = *(const double*)left;
    double b = *(const double*)right;

    return (a > b) - (a < b);
}

static bool Measure(FBenchmarkIteration iteration, void* context, const BenchmarkOptions& options, Measurement* measurement)
{
    const uint64_t warmup_duration = (uint64_t)options.m_WarmupMilliseconds * 1000;
    const uint64_t warmup_start = dmTime::GetMonotonicTime();
    uint32_t       warmup_iterations = 0;
    uint64_t       warmup_elapsed = 0;
    do
    {
        if (!iteration(context))
        {
            return false;
        }

        ++warmup_iterations;
        warmup_elapsed = dmTime::GetMonotonicTime() - warmup_start;
    } while (warmup_elapsed < warmup_duration && warmup_iterations < MAX_ITERATIONS);

    uint32_t iterations = options.m_Iterations;

    if (iterations == 0)
    {
        if (warmup_elapsed == 0)
        {
            warmup_elapsed = 1;
        }

        const double microseconds_per_iteration = (double)warmup_elapsed / warmup_iterations;
        iterations = (uint32_t)((options.m_TargetMilliseconds * 1000.0) / microseconds_per_iteration + 0.5);

        if (iterations == 0)
        {
            iterations = 1;
        }

        if (iterations > MAX_ITERATIONS)
        {
            iterations = MAX_ITERATIONS;
        }
    }

    double* sample_times = (double*)malloc(sizeof(double) * options.m_Samples);
    double* deviations = (double*)malloc(sizeof(double) * options.m_Samples);

    if (!sample_times || !deviations)
    {
        free(sample_times);
        free(deviations);

        return false;
    }

    for (uint32_t sample = 0; sample < options.m_Samples; ++sample)
    {
        uint64_t start = dmTime::GetMonotonicTime();

        for (uint32_t i = 0; i < iterations; ++i)
        {
            if (!iteration(context))
            {
                free(sample_times);
                free(deviations);

                return false;
            }
        }

        uint64_t elapsed = dmTime::GetMonotonicTime() - start;
        sample_times[sample] = (double)elapsed / iterations;
    }

    qsort(sample_times, options.m_Samples, sizeof(double), CompareDouble);
    measurement->m_MedianMicroseconds = sample_times[options.m_Samples / 2];
    measurement->m_P25Microseconds = sample_times[options.m_Samples / 4];
    measurement->m_P75Microseconds = sample_times[(options.m_Samples * 3) / 4];

    for (uint32_t sample = 0; sample < options.m_Samples; ++sample)
    {
        deviations[sample] = fabs(sample_times[sample] - measurement->m_MedianMicroseconds);
    }

    qsort(deviations, options.m_Samples, sizeof(double), CompareDouble);
    measurement->m_MedianAbsoluteDeviationMicroseconds = deviations[options.m_Samples / 2];
    measurement->m_Iterations = iterations;
    free(sample_times);
    free(deviations);

    return true;
}

static bool RunPlainLayout(void* context)
{
    PlainLayoutContext* plain = (PlainLayoutContext*)context;
    HTextLayout         layout = 0;
    TextResult          result = TextLayoutCreate(plain->m_Collection, plain->m_Codepoints, plain->m_CodepointCount, &plain->m_Settings, &layout);

    if (result != TEXT_RESULT_OK || !layout)
    {
        return false;
    }

    g_BenchmarkChecksum += TextLayoutGetGlyphCount(layout);
    TextLayoutRelease(layout);

    return true;
}

static const CachedVertexGlyph* FindCachedGlyph(const dmArray<CachedVertexGlyph>& glyph_cache,
                                                HFont                             font,
                                                uint32_t                          glyph_index,
                                                float                             font_size)
{
    for (uint32_t i = 0; i < glyph_cache.Size(); ++i)
    {
        const CachedVertexGlyph& glyph = glyph_cache[i];

        if (glyph.m_Font == font && glyph.m_GlyphIndex == glyph_index && glyph.m_FontSize == font_size)
        {
            return &glyph;
        }
    }

    return 0;
}

static bool ResolveCachedGlyph(void* context, const TextGlyph& text_glyph, FontLayoutCachedGlyph* output)
{
    VertexGenerationContext* context_data = (VertexGenerationContext*)context;
    const CachedVertexGlyph* cached = FindCachedGlyph(*context_data->m_GlyphCache, text_glyph.m_Font, text_glyph.m_GlyphIndex, context_data->m_DefaultFontSize);

    if (!cached)
    {
        return false;
    }

    output->m_Glyph = (FontGlyph*)&cached->m_Glyph;
    output->m_CellX = cached->m_CellX;
    output->m_CellY = cached->m_CellY;

    return true;
}

static bool AddCachedGlyph(dmArray<CachedVertexGlyph>& glyph_cache, HFont font, uint32_t glyph_index, float font_size, uint32_t* max_ascent)
{
    if (FindCachedGlyph(glyph_cache, font, glyph_index, font_size))
    {
        return true;
    }

    CachedVertexGlyph glyph;
    memset(&glyph, 0, sizeof(glyph));
    glyph.m_Font = font;
    glyph.m_GlyphIndex = glyph_index;
    glyph.m_FontSize = font_size;
    FontGlyphGenParams params;
    params.m_Scale = FontGetScaleFromSize(font, font_size);
    params.m_SdfPadding = 6.0f;

    if (FontGenerateGlyph(font, glyph_index, &params, &glyph.m_Glyph) != FONT_RESULT_OK)
    {
        return false;
    }

    glyph.m_CellX = glyph_cache.Size() * 64;
    glyph.m_CellY = 0;
    *max_ascent = dmMath::Max(*max_ascent, (uint32_t)glyph.m_Glyph.m_Ascent);

    if (glyph_cache.Full())
    {
        glyph_cache.OffsetCapacity(32);
    }

    glyph_cache.Push(glyph);

    return true;
}

static bool PrepareVertexGeneration(HTextLayout layout, const TextLayoutSettings& settings, dmArray<CachedVertexGlyph>& glyph_cache, dmArray<FontGlyphVertex>& vertices, VertexGenerationContext* context)
{
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    uint32_t   max_ascent = 0;

    for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
    {
        const TextGlyph& glyph = glyphs[i];

        if (dmUtf8::IsWhiteSpace(glyph.m_Codepoint) || (glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT))
        {
            continue;
        }

        if (!AddCachedGlyph(glyph_cache, glyph.m_Font, glyph.m_GlyphIndex, settings.m_Size, &max_ascent))
        {
            return false;
        }
    }

    memset(context, 0, sizeof(*context));
    context->m_Layout = layout;
    context->m_GlyphCache = &glyph_cache;
    context->m_Vertices = &vertices;
    context->m_DefaultFontSize = settings.m_Size;
    context->m_CacheMaxAscent = max_ascent;
    context->m_Config.m_Layout = layout;
    context->m_Config.m_ResolveGlyph = ResolveCachedGlyph;
    context->m_Config.m_ResolveGlyphContext = context;
    context->m_Config.m_Transform = dmVMath::Matrix4::identity();
    context->m_Config.m_OutlineColor = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    context->m_Config.m_ShadowColor = dmVMath::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

    for (uint32_t i = 0; i < 4; ++i)
    {
        context->m_Config.m_FaceColor[i] = 1.0f;
    }

    context->m_Config.m_Width = settings.m_Width;
    context->m_Config.m_Height = 0.0f;
    context->m_Config.m_RecipAtlasWidth = 1.0f / 4096.0f;
    context->m_Config.m_RecipAtlasHeight = 1.0f / 4096.0f;
    context->m_Config.m_SdfEdge = 0.75f;
    context->m_Config.m_SdfOutline = 1.0f;
    context->m_Config.m_SdfSmoothing = 0.25f / 6.0f;
    context->m_Config.m_SdfShadow = 1.0f;
    context->m_Config.m_SdfSpread = 6.0f;
    context->m_Config.m_CacheCellMaxAscent = max_ascent;
    context->m_Config.m_CacheCellPadding = 1;
    context->m_Config.m_BaseLayerMask = FONT_RENDER_LAYER_FACE;
    context->m_Config.m_MetricsFromTtf = true;
    context->m_Config.m_RenderDecorations = true;
    context->m_Config.m_RenderObjectOutlines = false;
    context->m_Config.m_ResolveGlyphsForMetrics = false;

    if (!FontGetLayoutVertexMetrics(context->m_Config, &context->m_Metrics))
    {
        return false;
    }

    if (vertices.Capacity() < context->m_Metrics.m_VertexCount)
    {
        vertices.SetCapacity(context->m_Metrics.m_VertexCount);
    }

    vertices.SetSize(context->m_Metrics.m_VertexCount);

    return true;
}

static bool GenerateVertices(VertexGenerationContext* context)
{
    context->m_Config.m_Layout = context->m_Layout;

    if (!FontGetLayoutVertexMetrics(context->m_Config, &context->m_Metrics))
    {
        return false;
    }

    if (context->m_Metrics.m_VertexCount > context->m_Vertices->Capacity())
    {
        return false;
    }

    context->m_Vertices->SetSize(context->m_Metrics.m_VertexCount);
    const uint32_t vertex_count = FontCreateLayoutVertices(context->m_Config, context->m_Metrics, context->m_Vertices->Begin(), context->m_Vertices->Size());

    if (vertex_count != context->m_Metrics.m_VertexCount)
    {
        return false;
    }

    g_BenchmarkChecksum += vertex_count;

    if (vertex_count != 0)
    {
        g_BenchmarkChecksum += (uint64_t)(context->m_Vertices->Back().m_Position[0] * 1000.0f);
    }

    return true;
}

static bool RunVertices(void* context)
{
    return GenerateVertices((VertexGenerationContext*)context);
}

static bool RunPlainLayoutAndVertices(void* context)
{
    PlainLayoutAndVerticesContext* end_to_end = (PlainLayoutAndVerticesContext*)context;
    HTextLayout                    layout = 0;
    TextResult                     result = TextLayoutCreate(end_to_end->m_Layout.m_Collection,
                                         end_to_end->m_Layout.m_Codepoints,
                                         end_to_end->m_Layout.m_CodepointCount,
                                         &end_to_end->m_Layout.m_Settings,
                                         &layout);
    if (result != TEXT_RESULT_OK || !layout)
    {
        return false;
    }

    end_to_end->m_Vertices->m_Layout = layout;
    bool generated = GenerateVertices(end_to_end->m_Vertices);
    TextLayoutRelease(layout);
    end_to_end->m_Vertices->m_Layout = 0;

    return generated;
}

static void FreeCachedGlyphs(dmArray<CachedVertexGlyph>& glyph_cache)
{
    for (uint32_t i = 0; i < glyph_cache.Size(); ++i)
    {
        FontFreeGlyph(glyph_cache[i].m_Font, &glyph_cache[i].m_Glyph);
    }
}

static void BuildPlainText(dmArray<char>& text)
{
    static const char seed[] =
    "Lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor incididunt ut labore et dolore magna aliqua ";
    const uint32_t seed_length = sizeof(seed) - 1;
    text.SetCapacity(BENCHMARK_TEXT_LENGTH + 1);
    text.SetSize(0);

    for (uint32_t i = 0; i < BENCHMARK_TEXT_LENGTH; ++i)
    {
        text.Push(seed[i % seed_length]);
    }

    text.Push(0);
}

static uint32_t CountWords(const dmArray<char>& text)
{
    uint32_t word_count = 0;
    bool     in_word = false;

    for (uint32_t i = 0; i < BENCHMARK_TEXT_LENGTH; ++i)
    {
        bool is_word = text[i] != ' ';

        if (is_word && !in_word)
        {
            ++word_count;
        }

        in_word = is_word;
    }

    return word_count;
}

static void PrintResult(const char* operation, const char* style, uint32_t tag_count, uint32_t source_bytes, uint32_t word_count, uint32_t span_count, uint32_t style_node_count, uint32_t generated_vertices, const Measurement& measurement)
{
    printf("%s,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%.3f,%.3f,%.3f,%.3f,%u,%.6f\n",
           FONT_BENCHMARK_BUILD_LABEL,
           operation,
           style,
           tag_count,
           BENCHMARK_TEXT_LENGTH,
           source_bytes,
           word_count,
           span_count,
           style_node_count,
           generated_vertices,
           (uint32_t)sizeof(FontGlyphVertex),
           (unsigned long long)generated_vertices * sizeof(FontGlyphVertex),
           measurement.m_MedianMicroseconds,
           measurement.m_P25Microseconds,
           measurement.m_P75Microseconds,
           measurement.m_MedianAbsoluteDeviationMicroseconds,
           measurement.m_Iterations,
           measurement.m_MedianMicroseconds / BENCHMARK_TEXT_LENGTH);
}

#if defined(FONT_BENCHMARK_TAGS)
static const char* GetStyleName(MarkupStyleType style)
{
    switch (style)
    {
        case MARKUP_STYLE_COLOR:
            return "color";
        case MARKUP_STYLE_SIZE:
            return "size";
        case MARKUP_STYLE_GRADIENT_HORIZONTAL:
            return "gradient_horizontal";
        case MARKUP_STYLE_GRADIENT_VERTICAL:
            return "gradient_vertical";
        case MARKUP_STYLE_GRADIENT_QUAD:
            return "gradient_quad";
        case MARKUP_STYLE_SHAKE:
            return "shake";
        case MARKUP_STYLE_WAVE:
            return "wave";
        case MARKUP_STYLE_OUTLINE:
            return "outline";
        case MARKUP_STYLE_SHADOW:
            return "shadow";
        case MARKUP_STYLE_UNDERLINE_SOLID:
            return "underline_solid";
        case MARKUP_STYLE_UNDERLINE_DASHED:
            return "underline_dashed";
        case MARKUP_STYLE_STRIKE_SOLID:
            return "strike_solid";
        case MARKUP_STYLE_STRIKE_DASHED:
            return "strike_dashed";
        case MARKUP_STYLE_LINK:
            return "link";
        case MARKUP_STYLE_SPRITE:
            return "sprite";
        default:
            return "none";
    }
}

static const char* GetOpeningTag(MarkupStyleType style)
{
    switch (style)
    {
        case MARKUP_STYLE_COLOR:
            return "<color=#FF8040>";
        case MARKUP_STYLE_SIZE:
            return "<size=120%>";
        case MARKUP_STYLE_GRADIENT_HORIZONTAL:
            return "<gradient hz=0.25 fit=span left=#FF00FF right=#FFFFFF>";
        case MARKUP_STYLE_GRADIENT_VERTICAL:
            return "<gradient hz=0.25 fit=span bottom=#FF00FF top=#FFFFFF>";
        case MARKUP_STYLE_GRADIENT_QUAD:
            return "<gradient hz=0.25 fit=span tl=#FF0000 tr=#00FF00 bl=#0000FF br=#FFFFFF>";
        case MARKUP_STYLE_SHAKE:
            return "<shake hz=20 amplitude=0.5 fit=span>";
        case MARKUP_STYLE_WAVE:
            return "<wave hz=1 amplitude=4 wavelength=6 fit=span>";
        case MARKUP_STYLE_OUTLINE:
            return "<outline size=2 color=#FFFFFF>";
        case MARKUP_STYLE_SHADOW:
            return "<shadow x=2 y=-2 blur=2 color=#000000A0>";
        case MARKUP_STYLE_UNDERLINE_SOLID:
            return "<ul>";
        case MARKUP_STYLE_UNDERLINE_DASHED:
            return "<ul pattern=dashed>";
        case MARKUP_STYLE_STRIKE_SOLID:
            return "<strike>";
        case MARKUP_STYLE_STRIKE_DASHED:
            return "<strike pattern=dashed>";
        case MARKUP_STYLE_LINK:
            return "<link src=https://defold.com>";
        case MARKUP_STYLE_SPRITE:
            return "<sprite src=/benchmark.png/>";
        default:
            return "";
    }
}

static const char* GetClosingTag(MarkupStyleType style)
{
    switch (style)
    {
        case MARKUP_STYLE_COLOR:
            return "</color>";
        case MARKUP_STYLE_SIZE:
            return "</size>";
        case MARKUP_STYLE_GRADIENT_HORIZONTAL:
        case MARKUP_STYLE_GRADIENT_VERTICAL:
        case MARKUP_STYLE_GRADIENT_QUAD:
            return "</gradient>";
        case MARKUP_STYLE_SHAKE:
            return "</shake>";
        case MARKUP_STYLE_WAVE:
            return "</wave>";
        case MARKUP_STYLE_OUTLINE:
            return "</outline>";
        case MARKUP_STYLE_SHADOW:
            return "</shadow>";
        case MARKUP_STYLE_UNDERLINE_SOLID:
        case MARKUP_STYLE_UNDERLINE_DASHED:
            return "</ul>";
        case MARKUP_STYLE_STRIKE_SOLID:
        case MARKUP_STYLE_STRIKE_DASHED:
            return "</strike>";
        case MARKUP_STYLE_LINK:
            return "</link>";
        default:
            return "";
    }
}

static void AppendBytes(dmArray<char>& destination, const char* source, uint32_t length)
{
    for (uint32_t i = 0; i < length; ++i)
    {
        destination.Push(source[i]);
    }
}

static void BuildMarkupSource(const dmArray<char>& plain_text, MarkupStyleType style, uint32_t tag_count, MarkupSource* source)
{
    const char* opening_tag = GetOpeningTag(style);
    const char* closing_tag = GetClosingTag(style);
    const uint32_t tag_bytes = (uint32_t)strlen(opening_tag) + (uint32_t)strlen(closing_tag);
    source->m_Bytes.SetCapacity(BENCHMARK_TEXT_LENGTH + tag_count * tag_bytes + 1);
    source->m_Bytes.SetSize(0);
    source->m_SelectedWordCount = 0;
    source->m_WordCount = CountWords(plain_text);

    uint32_t offset = 0;
    uint32_t word_index = 0;

    while (offset < BENCHMARK_TEXT_LENGTH)
    {
        if (plain_text[offset] == ' ')
        {
            source->m_Bytes.Push(' ');
            ++offset;
            continue;
        }

        uint32_t word_end = offset;

        while (word_end < BENCHMARK_TEXT_LENGTH && plain_text[word_end] != ' ')
        {
            ++word_end;
        }

        bool selected = style != MARKUP_STYLE_NONE &&
                        (word_index + 1) * tag_count / source->m_WordCount != word_index * tag_count / source->m_WordCount;
        if (selected)
        {
            AppendBytes(source->m_Bytes, opening_tag, (uint32_t)strlen(opening_tag));
            ++source->m_SelectedWordCount;
        }

        const bool sprite = selected && style == MARKUP_STYLE_SPRITE;
        const uint32_t text_offset = offset + (sprite ? 1 : 0);
        AppendBytes(source->m_Bytes, plain_text.Begin() + text_offset, word_end - text_offset);

        if (selected && !sprite)
        {
            AppendBytes(source->m_Bytes, closing_tag, (uint32_t)strlen(closing_tag));
        }

        ++word_index;
        offset = word_end;
    }

    source->m_Bytes.Push(0);
}

static uint8_t ResolveBenchmarkObject(void*, const char*, const TextLayoutObjectAttribute*, float proposed_width, float proposed_height, TextLayoutObject* object)
{
    object->m_Width = proposed_width;
    object->m_Height = proposed_height;
    object->m_Resource = 1;

    return 1;
}

static void ReleaseBenchmarkObject(void*, const TextLayoutObject*)
{
}

static bool RunMarkupLayout(void* context)
{
    MarkupLayoutContext* markup = (MarkupLayoutContext*)context;
    HTextLayout          layout = 0;
    TextResult           result = TextLayoutCreateMarkup(markup->m_Collection, markup->m_Markup, &markup->m_Settings, &layout);

    if (result != TEXT_RESULT_OK || !layout)
    {
        return false;
    }

    g_BenchmarkChecksum += TextLayoutGetGlyphCount(layout);
    TextLayoutRelease(layout);

    return true;
}

static bool RunMarkupParse(void* context)
{
    MarkupParseContext* parse = (MarkupParseContext*)context;
    HMarkup             markup = 0;
    MarkupResult        result = MarkupCreate(parse->m_Source, parse->m_SourceLength, &markup, 0);

    if (result != MARKUP_RESULT_OK || !markup)
    {
        return false;
    }

    g_BenchmarkChecksum += MarkupGetTextLength(markup);
    MarkupDestroy(markup);

    return true;
}

static bool RunMarkupEndToEnd(void* context)
{
    MarkupEndToEndContext* end_to_end = (MarkupEndToEndContext*)context;
    HMarkup                markup = 0;
    MarkupResult           markup_result = MarkupCreate(end_to_end->m_Source, end_to_end->m_SourceLength, &markup, 0);

    if (markup_result != MARKUP_RESULT_OK || !markup)
    {
        return false;
    }

    HTextLayout layout = 0;
    TextResult  layout_result = TextLayoutCreateMarkup(end_to_end->m_Collection, markup, &end_to_end->m_Settings, &layout);

    if (layout_result != TEXT_RESULT_OK || !layout)
    {
        MarkupDestroy(markup);

        return false;
    }

    g_BenchmarkChecksum += TextLayoutGetGlyphCount(layout);
    TextLayoutRelease(layout);
    MarkupDestroy(markup);

    return true;
}

static bool RunMarkupParseLayoutAndVertices(void* context)
{
    MarkupEndToEndContext* end_to_end = (MarkupEndToEndContext*)context;
    HMarkup                markup = 0;
    MarkupResult           markup_result = MarkupCreate(end_to_end->m_Source, end_to_end->m_SourceLength, &markup, 0);

    if (markup_result != MARKUP_RESULT_OK || !markup)
    {
        return false;
    }

    HTextLayout layout = 0;
    TextResult  layout_result = TextLayoutCreateMarkup(end_to_end->m_Collection, markup, &end_to_end->m_Settings, &layout);

    if (layout_result != TEXT_RESULT_OK || !layout)
    {
        MarkupDestroy(markup);

        return false;
    }

    end_to_end->m_Vertices->m_Layout = layout;
    bool generated = GenerateVertices(end_to_end->m_Vertices);
    TextLayoutRelease(layout);
    MarkupDestroy(markup);
    end_to_end->m_Vertices->m_Layout = 0;

    return generated;
}

static bool BenchmarkMarkupSource(HFontCollection collection, const TextLayoutSettings& settings, const MarkupSource& source, MarkupStyleType style, const BenchmarkOptions& options)
{
    uint32_t source_length = source.m_Bytes.Size() - 1;
    HMarkup  markup = 0;

    if (MarkupCreate(source.m_Bytes.Begin(), source_length, &markup, 0) != MARKUP_RESULT_OK)
    {
        return false;
    }

    if (MarkupGetTextLength(markup) != BENCHMARK_TEXT_LENGTH)
    {
        MarkupDestroy(markup);

        return false;
    }

    uint32_t           span_count = MarkupGetSpanCount(markup);
    uint32_t           style_node_count = MarkupGetStyleNodeCount(markup);

    HTextLayout        vertex_layout = 0;
    TextLayoutSettings vertex_settings = settings;

    if (TextLayoutCreateMarkup(collection, markup, &vertex_settings, &vertex_layout) != TEXT_RESULT_OK)
    {
        MarkupDestroy(markup);

        return false;
    }

    dmArray<CachedVertexGlyph> glyph_cache;
    dmArray<FontGlyphVertex>   vertices;
    VertexGenerationContext    vertex_context;

    if (!PrepareVertexGeneration(vertex_layout, settings, glyph_cache, vertices, &vertex_context))
    {
        TextLayoutRelease(vertex_layout);
        MarkupDestroy(markup);

        return false;
    }

    Measurement         layout_measurement;
    MarkupLayoutContext layout_context = { collection, markup, settings };
    bool                result = Measure(RunMarkupLayout, &layout_context, options, &layout_measurement);

    if (!result)
    {
        TextLayoutRelease(vertex_layout);
        MarkupDestroy(markup);
        FreeCachedGlyphs(glyph_cache);

        return false;
    }

    PrintResult("layout_markup", GetStyleName(style), source.m_SelectedWordCount, source_length, source.m_WordCount, span_count, style_node_count, 0, layout_measurement);

    Measurement vertex_measurement;

    if (!Measure(RunVertices, &vertex_context, options, &vertex_measurement))
    {
        TextLayoutRelease(vertex_layout);
        MarkupDestroy(markup);
        FreeCachedGlyphs(glyph_cache);

        return false;
    }

    PrintResult("vertices", GetStyleName(style), source.m_SelectedWordCount, source_length, source.m_WordCount, span_count, style_node_count, vertex_context.m_Metrics.m_VertexCount, vertex_measurement);

    Measurement        parse_measurement;
    MarkupParseContext parse_context = { source.m_Bytes.Begin(), source_length };

    if (!Measure(RunMarkupParse, &parse_context, options, &parse_measurement))
    {
        TextLayoutRelease(vertex_layout);
        MarkupDestroy(markup);
        FreeCachedGlyphs(glyph_cache);

        return false;
    }

    PrintResult("parse", GetStyleName(style), source.m_SelectedWordCount, source_length, source.m_WordCount, span_count, style_node_count, 0, parse_measurement);

    Measurement           end_to_end_measurement;
    MarkupEndToEndContext end_to_end_context = { collection, source.m_Bytes.Begin(), source_length, settings, &vertex_context };

    if (!Measure(RunMarkupEndToEnd, &end_to_end_context, options, &end_to_end_measurement))
    {
        TextLayoutRelease(vertex_layout);
        MarkupDestroy(markup);
        FreeCachedGlyphs(glyph_cache);

        return false;
    }

    PrintResult("parse_and_layout", GetStyleName(style), source.m_SelectedWordCount, source_length, source.m_WordCount, span_count, style_node_count, 0, end_to_end_measurement);

    Measurement full_measurement;

    if (!Measure(RunMarkupParseLayoutAndVertices, &end_to_end_context, options, &full_measurement))
    {
        TextLayoutRelease(vertex_layout);
        MarkupDestroy(markup);
        FreeCachedGlyphs(glyph_cache);

        return false;
    }

    PrintResult("parse_layout_vertices", GetStyleName(style), source.m_SelectedWordCount, source_length, source.m_WordCount, span_count, style_node_count, vertex_context.m_Metrics.m_VertexCount, full_measurement);

    TextLayoutRelease(vertex_layout);
    MarkupDestroy(markup);
    FreeCachedGlyphs(glyph_cache);

    return true;
}
#endif

static bool ParsePositiveOption(const char* argument, const char* prefix, uint32_t* value)
{
    uint32_t prefix_length = (uint32_t)strlen(prefix);

    if (strncmp(argument, prefix, prefix_length) != 0)
    {
        return false;
    }

    char*         end = 0;
    unsigned long parsed = strtoul(argument + prefix_length, &end, 10);

    if (!end || *end != 0 || parsed == 0 || parsed > 100000)
    {
        return false;
    }

    *value = (uint32_t)parsed;

    return true;
}

static bool ParseOptions(int argc, char** argv, BenchmarkOptions* options)
{
    options->m_FontPath = "src/test/data/vera_mo_bd.ttf";
    options->m_StyleFilter = 0;
    options->m_Iterations = DEFAULT_ITERATIONS;
    options->m_Samples = DEFAULT_SAMPLES;
    options->m_TargetMilliseconds = DEFAULT_TARGET_MILLISECONDS;
    options->m_WarmupMilliseconds = DEFAULT_WARMUP_MILLISECONDS;
    options->m_OrderSeed = 1;
    options->m_TagCountFilter = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "--font=", 7) == 0)
        {
            options->m_FontPath = argv[i] + 7;
        }
        else if (strncmp(argv[i], "--style=", 8) == 0)
        {
            options->m_StyleFilter = argv[i] + 8;
        }
        else if (strncmp(argv[i], "--tag-count=", 12) == 0)
        {
            if (!ParsePositiveOption(argv[i], "--tag-count=", &options->m_TagCountFilter))
            {
                return false;
            }
        }
        else if (strncmp(argv[i], "--iterations=", 13) == 0)
        {
            if (!ParsePositiveOption(argv[i], "--iterations=", &options->m_Iterations))
            {
                return false;
            }
        }
        else if (strncmp(argv[i], "--samples=", 10) == 0)
        {
            if (!ParsePositiveOption(argv[i], "--samples=", &options->m_Samples))
            {
                return false;
            }
        }
        else if (strncmp(argv[i], "--target-ms=", 12) == 0)
        {
            if (!ParsePositiveOption(argv[i], "--target-ms=", &options->m_TargetMilliseconds))
            {
                return false;
            }
        }
        else if (strncmp(argv[i], "--warmup-ms=", 12) == 0)
        {
            if (!ParsePositiveOption(argv[i], "--warmup-ms=", &options->m_WarmupMilliseconds))
            {
                return false;
            }
        }
        else if (strncmp(argv[i], "--order-seed=", 13) == 0)
        {
            if (!ParsePositiveOption(argv[i], "--order-seed=", &options->m_OrderSeed))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

int main(int argc, char** argv)
{
    BenchmarkOptions options;

    if (!ParseOptions(argc, argv, &options))
    {
        fprintf(stderr, "Usage: %s [--font=PATH] [--style=NAME] [--tag-count=N] [--iterations=N] [--samples=N] [--target-ms=N] [--warmup-ms=N] [--order-seed=N]\n", argv[0]);

        return 1;
    }

    HFont font = FontLoadFromPath(options.m_FontPath);

    if (!font)
    {
        fprintf(stderr, "Failed to load font: %s\n", options.m_FontPath);

        return 1;
    }

    HFontCollection collection = FontCollectionCreate();

    if (!collection || FontCollectionAddFont(collection, font) != FONT_RESULT_OK)
    {
        fprintf(stderr, "Failed to create font collection\n");

        if (collection)
        {
            FontCollectionDestroy(collection);
        }

        FontDestroy(font);

        return 1;
    }

    dmArray<char> plain_text;
    BuildPlainText(plain_text);
    dmArray<uint32_t> codepoints;
    TextToCodePoints(plain_text.Begin(), codepoints);

    if (codepoints.Size() != BENCHMARK_TEXT_LENGTH)
    {
        fprintf(stderr, "Generated text length mismatch\n");
        FontCollectionDestroy(collection);
        FontDestroy(font);

        return 1;
    }

    TextLayoutSettings settings = {};
    settings.m_Size = 32.0f;
    settings.m_Width = 800.0f;
    settings.m_Leading = 1.0f;
    settings.m_LineBreak = 1;
#if defined(FONT_BENCHMARK_TAGS)
    settings.m_ResolveObject = ResolveBenchmarkObject;
    settings.m_ReleaseObject = ReleaseBenchmarkObject;
#endif

    printf("build,operation,style,tag_count,visible_characters,source_bytes,total_words,spans,style_nodes,generated_vertices,vertex_stride_bytes,generated_vertex_bytes,median_us,p25_us,p75_us,mad_us,iterations,median_us_per_character\n");

    PlainLayoutContext plain_context = { collection, codepoints.Begin(), codepoints.Size(), settings };
    Measurement        plain_measurement;
    bool               success = Measure(RunPlainLayout, &plain_context, options, &plain_measurement);

    if (success)
    {
        PrintResult("layout_plain", "none", 0, BENCHMARK_TEXT_LENGTH, CountWords(plain_text), 0, 0, 0, plain_measurement);
    }

    HTextLayout        plain_layout = 0;
    TextLayoutSettings plain_vertex_settings = settings;

    if (success)
    {
        success = TextLayoutCreate(collection, codepoints.Begin(), codepoints.Size(), &plain_vertex_settings, &plain_layout) == TEXT_RESULT_OK;
    }

    dmArray<CachedVertexGlyph> plain_glyph_cache;
    dmArray<FontGlyphVertex>   plain_vertices;
    VertexGenerationContext    plain_vertex_context;

    if (success)
    {
        success = PrepareVertexGeneration(plain_layout, settings, plain_glyph_cache, plain_vertices, &plain_vertex_context);
    }

    Measurement plain_vertex_measurement;

    if (success)
    {
        success = Measure(RunVertices, &plain_vertex_context, options, &plain_vertex_measurement);
    }

    if (success)
    {
        PrintResult("vertices_plain", "none", 0, BENCHMARK_TEXT_LENGTH, CountWords(plain_text), 0, 0, plain_vertex_context.m_Metrics.m_VertexCount, plain_vertex_measurement);
    }

    PlainLayoutAndVerticesContext plain_full_context = { plain_context, &plain_vertex_context };
    Measurement                   plain_full_measurement;

    if (success)
    {
        success = Measure(RunPlainLayoutAndVertices, &plain_full_context, options, &plain_full_measurement);
    }

    if (success)
    {
        PrintResult("layout_vertices_plain", "none", 0, BENCHMARK_TEXT_LENGTH, CountWords(plain_text), 0, 0, plain_vertex_context.m_Metrics.m_VertexCount, plain_full_measurement);
    }

    if (plain_layout)
    {
        TextLayoutRelease(plain_layout);
    }

    FreeCachedGlyphs(plain_glyph_cache);

#if defined(FONT_BENCHMARK_TAGS)
    if (success)
    {
        MarkupSource source;
        BuildMarkupSource(plain_text, MARKUP_STYLE_NONE, 0, &source);
        success = BenchmarkMarkupSource(collection, settings, source, MARKUP_STYLE_NONE, options);
    }

    const uint32_t        tag_counts[] = { 1, 100, 500, 1000, 2000, 4000 };
    const MarkupStyleType styles[] = {
        MARKUP_STYLE_COLOR,
        MARKUP_STYLE_SIZE,
        MARKUP_STYLE_GRADIENT_HORIZONTAL,
        MARKUP_STYLE_GRADIENT_VERTICAL,
        MARKUP_STYLE_GRADIENT_QUAD,
        MARKUP_STYLE_SHAKE,
        MARKUP_STYLE_WAVE,
        MARKUP_STYLE_OUTLINE,
        MARKUP_STYLE_SHADOW,
        MARKUP_STYLE_UNDERLINE_SOLID,
        MARKUP_STYLE_UNDERLINE_DASHED,
        MARKUP_STYLE_STRIKE_SOLID,
        MARKUP_STYLE_STRIKE_DASHED,
        MARKUP_STYLE_LINK,
        MARKUP_STYLE_SPRITE,
    };
    struct BenchmarkCase
    {
        MarkupStyleType m_Style;
        uint32_t        m_TagCount;
    };
    BenchmarkCase cases[DM_ARRAY_SIZE(styles) * DM_ARRAY_SIZE(tag_counts)];
    uint32_t case_count = 0;

    for (uint32_t style_index = 0; style_index < DM_ARRAY_SIZE(styles); ++style_index)
    {
        for (uint32_t tag_count_index = 0; tag_count_index < DM_ARRAY_SIZE(tag_counts); ++tag_count_index)
        {
            if (options.m_StyleFilter && strcmp(options.m_StyleFilter, GetStyleName(styles[style_index])) != 0)
            {
                continue;
            }

            if (options.m_TagCountFilter != 0 && options.m_TagCountFilter != tag_counts[tag_count_index])
            {
                continue;
            }

            cases[case_count].m_Style = styles[style_index];
            cases[case_count].m_TagCount = tag_counts[tag_count_index];
            ++case_count;
        }
    }

    if (case_count == 0)
    {
        fprintf(stderr, "No tag benchmark matches the requested filters\n");
        success = false;
    }

    uint32_t order_state = options.m_OrderSeed;

    for (uint32_t count = case_count; count > 1; --count)
    {
        order_state = order_state * 1664525u + 1013904223u;
        const uint32_t index = count - 1;
        const uint32_t other = order_state % count;
        const BenchmarkCase temporary = cases[index];
        cases[index] = cases[other];
        cases[other] = temporary;
    }

    for (uint32_t case_index = 0; success && case_index < case_count; ++case_index)
    {
        MarkupSource source;
        BuildMarkupSource(plain_text, cases[case_index].m_Style, cases[case_index].m_TagCount, &source);

        if (source.m_SelectedWordCount != cases[case_index].m_TagCount)
        {
            fprintf(stderr, "Requested %u tags, but the generated text only supports %u\n",
                    cases[case_index].m_TagCount, source.m_SelectedWordCount);
            success = false;
            break;
        }

        success = BenchmarkMarkupSource(collection, settings, source, cases[case_index].m_Style, options);
    }
#endif

    FontCollectionDestroy(collection);
    FontDestroy(font);

    if (!success)
    {
        fprintf(stderr, "Benchmark operation failed\n");

        return 1;
    }

    fflush(stdout);
    fprintf(stderr, "checksum=%llu iterations=%s samples=%u target_ms=%u warmup_ms=%u order_seed=%u\n",
            (unsigned long long)g_BenchmarkChecksum,
            options.m_Iterations == 0 ? "adaptive" : "fixed",
            options.m_Samples,
            options.m_TargetMilliseconds,
            options.m_WarmupMilliseconds,
            options.m_OrderSeed);

    return 0;
}
