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

#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <testmain/testmain.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/image.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/time.h>
#include <dlib/utf8.h>
#include <dlib/vmath.h>
#include <dmsdk/font/fontcollection.h>
#include <dmsdk/font/text_layout.h>
#include <dmsdk/graphics/graphics.h>
#include <graphics/graphics.h>
#include <platform/window.hpp>

#include <graphics_private.h>
#include <test/test_graphics_util.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb/stb_image_write.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include "NotoSans-Regular.ttf.embed.h"
#include "NotoSansArabic-Regular.ttf.embed.h"
#include "font-df.vp.embed.h"
#include "font-df.fp.embed.h"

#include "fontviewer_nuklear.h"
#include "fontviewer_macos.h"

#include <font.h>
#include <font_render.h>
#include <glyph_gen.h>
#include <glyph_vertex.h>
#include <text_layout.h>

using dmVMath::Matrix4;
using dmVMath::Vector3;
using dmVMath::Vector4;

namespace dmGraphics
{
    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);
}

static const uint32_t WINDOW_WIDTH = 1280;
static const uint32_t WINDOW_HEIGHT = 900;
static const uint32_t ATLAS_WIDTH = 2048;
static const uint32_t ATLAS_HEIGHT = 4096;
static const uint32_t CELL_PADDING = 1;
static const float    DEFAULT_FONT_SIZE = 24.0f;
static const uint64_t KEY_REPEAT_DELAY = 350000;
static const uint64_t KEY_REPEAT_INTERVAL = 45000;
static const dmhash_t TAG_LINK = dmHashString64("link");
static const dmhash_t TAG_SPRITE = dmHashString64("sprite");
static const dmhash_t STYLE_LINK_HOVER = dmHashString64("link:hover");
static const dmhash_t STYLE_LINK_ACTIVE = dmHashString64("link:active");

enum ArrowKey
{
    ARROW_KEY_NONE,
    ARROW_KEY_LEFT,
    ARROW_KEY_RIGHT,
    ARROW_KEY_UP,
    ARROW_KEY_DOWN,
};

static const char*    ENGLISH_TEXT =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et "
"dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
"ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu "
"fugiat nulla pariatur.";

static const char* ARABIC_TEXT =
"لكن لا بد أن أوضح لك أن كل هذه الأفكار المغلوطة حول استنكار  النشوة وتمجيد الألم نشأت بالفعل، وسأعرض لك "
"التفاصيل لتكتشف حقيقة وأساس تلك السعادة البشرية، فلا أحد يرفض أو يكره أو يتجنب الشعور بالسعادة، ولكن بفضل "
"هؤلاء الأشخاص الذين لا يدركون بأن السعادة لا بد أن نستشعرها بصورة أكثر عقلانية ومنطقية فيعرضهم هذا لمواجهة "
"الظروف الأليمة، وأكرر بأنه لا يوجد من يرغب في الحب ونيل المنال ويتلذذ بالآلام، الألم هو الألم ولكن نتيجة "
"لظروف ما قد تكمن السعاده فيما نتحمله من كد وأسي.";

static const char* JAPANESE_TEXT =
"吾輩は猫である。名前はまだ無い。どこで生れたかとんと見当がつかぬ。何でも薄暗いじめじめした所で"
"ニャーニャー泣いていた事だけは記憶している。";

struct CachedGlyph
{
    FontGlyph m_Glyph;
    HFont     m_Font;
    uint32_t  m_GlyphIndex;
    float     m_FontSize;
    uint16_t  m_X;
    uint16_t  m_Y;
    // UI glyphs intentionally ignore preview outline/shadow generation settings.
    // Keep them as separate cache entries even when font, glyph, and size match.
    bool m_ApplyProperties;
};

struct ColorVertex
{
    float m_Position[4];
    float m_Color[4];
    float m_Decoration[2];
};

struct SpriteVertex
{
    float m_Position[4];
    float m_UV[2];
};

struct ViewerImage
{
    dmGraphics::HTexture m_Texture;
};

struct SpriteDraw
{
    dmGraphics::HTexture m_Texture;
    uint32_t             m_VertexIndex;
};

struct Viewer
{
    Viewer()
        : m_Window(0)
        , m_Context(0)
        , m_Program(0)
        , m_ColorProgram(0)
        , m_SpriteProgram(0)
        , m_NuklearProgram(0)
        , m_ViewProjLocation(dmGraphics::INVALID_UNIFORM_LOCATION)
        , m_VertexBuffer(0)
        , m_ColorVertexBuffer(0)
        , m_SpriteVertexBuffer(0)
        , m_NuklearVertexBuffer(0)
        , m_VertexDeclaration(0)
        , m_ColorVertexDeclaration(0)
        , m_SpriteVertexDeclaration(0)
        , m_NuklearVertexDeclaration(0)
        , m_Texture(0)
        , m_NuklearTexture(0)
        , m_Collection(0)
        , m_LoadedFontDataSize(0)
        , m_VertexCount(0)
        , m_ColorBackgroundVertexCount(0)
        , m_ColorDecorationVertexCount(0)
        , m_ColorDebugVertexCount(0)
        , m_CellWidth(1)
        , m_CellHeight(1)
        , m_CellMaxAscent(0)
        , m_AtlasChannels(1)
        , m_Closed(false)
        , m_LegacyLayout(false)
        , m_ShowBaselines(false)
        , m_ShowQuads(false)
        , m_TextFieldFocused(false)
        , m_ScrollCaretIntoViewRequested(false)
        , m_RebuildRequested(false)
        , m_RenderUpdateRequested(false)
        , m_PreviousMouseDown(false)
        , m_PreviousBackspaceDown(false)
        , m_PreviousDeleteDown(false)
        , m_PreviousEnterDown(false)
        , m_PreviousEscapeDown(false)
        , m_BackspaceRepeatAt(0)
        , m_RepeatingArrowKey(ARROW_KEY_NONE)
        , m_ArrowRepeatAt(0)
        , m_PreviousMouseWheel(0)
        , m_HoveredLinkObject(0xffffffff)
        , m_PressedLinkObject(0xffffffff)
        , m_TextScrollY(0.0f)
        , m_EditorContentHeight(0.0f)
        , m_FontSize(DEFAULT_FONT_SIZE)
        , m_Caret(0)
        , m_SelectionAnchor(0)
        , m_PreferredCaretX(0.0f)
        , m_HasPreferredCaretX(false)
        , m_TextSelecting(false)
        , m_Zoom(1.0f)
        , m_PanX(0.0f)
        , m_PanY(0.0f)
        , m_LinkCursor(false)
        , m_PreviewDragging(false)
        , m_PreviousMouseX(0)
        , m_PreviousMouseY(0)
        , m_ExportStyle(0)
        , m_OutputPath(0)
        , m_RequestedFrameCount(0)
        , m_ExportCanvasWidth(0)
        , m_ExportCanvasHeight(0)
        , m_ExportMode(false)
        , m_NuklearInitialized(false)
    {
        memset(&m_NuklearLayout, 0, sizeof(m_NuklearLayout));
        m_Properties.m_Alpha = 1.0f;
        m_Properties.m_OutlineAlpha = 1.0f;
        m_Properties.m_OutlineWidth = 0.0f;
        m_Properties.m_ShadowAlpha = 1.0f;
        m_Properties.m_ShadowBlur = 0.0f;
        m_Properties.m_ShadowX = 0.0f;
        m_Properties.m_ShadowY = 0.0f;
        m_Properties.m_FaceColor[0] = 1.0f;
        m_Properties.m_FaceColor[1] = 1.0f;
        m_Properties.m_FaceColor[2] = 1.0f;
        m_Properties.m_OutlineColor[0] = 0.0f;
        m_Properties.m_OutlineColor[1] = 0.0f;
        m_Properties.m_OutlineColor[2] = 1.0f;
        m_Properties.m_ShadowColor[0] = 0.0f;
        m_Properties.m_ShadowColor[1] = 0.0f;
        m_Properties.m_ShadowColor[2] = 0.0f;
        m_Properties.m_BackgroundColor[0] = 0.25f;
        m_Properties.m_BackgroundColor[1] = 0.25f;
        m_Properties.m_BackgroundColor[2] = 0.25f;
    }

    HWindow                        m_Window;
    dmGraphics::HContext           m_Context;
    dmGraphics::HProgram           m_Program;
    dmGraphics::HProgram           m_ColorProgram;
    dmGraphics::HProgram           m_SpriteProgram;
    dmGraphics::HProgram           m_NuklearProgram;
    dmGraphics::HUniformLocation   m_ViewProjLocation;
    dmGraphics::HVertexBuffer      m_VertexBuffer;
    dmGraphics::HVertexBuffer      m_ColorVertexBuffer;
    dmGraphics::HVertexBuffer      m_SpriteVertexBuffer;
    dmGraphics::HVertexBuffer      m_NuklearVertexBuffer;
    dmGraphics::HVertexDeclaration m_VertexDeclaration;
    dmGraphics::HVertexDeclaration m_ColorVertexDeclaration;
    dmGraphics::HVertexDeclaration m_SpriteVertexDeclaration;
    dmGraphics::HVertexDeclaration m_NuklearVertexDeclaration;
    dmGraphics::HTexture           m_Texture;
    dmGraphics::HTexture           m_NuklearTexture;
    HFontCollection                m_Collection;
    dmArray<HFont>                 m_Fonts;
    dmArray<char>                  m_LoadedFontText;
    uint64_t                       m_LoadedFontDataSize;
    dmArray<const char*>           m_FontPaths;
    dmArray<const char*>           m_Texts;
    dmArray<char*>                 m_OwnedTexts;
    dmArray<char>                  m_EditorText;
    dmArray<char>                  m_MarkedText;
    dmArray<char>                  m_LayoutText;
    // Layout zero is always the main preview. The remaining layout is the
    // effect-free native layout for the editable text field.
    dmArray<HTextLayout>          m_Layouts;
    dmArray<float>                m_LayoutTops;
    dmArray<float>                m_LayoutXs;
    dmArray<float>                m_LayoutSizes;
    dmArray<float>                m_LayoutWidths;
    dmArray<FontViewerNuklearBox> m_LayoutClips;
    dmArray<uint8_t>              m_LayoutBold;
    dmArray<uint8_t>              m_LayoutLayerCounts;
    dmArray<uint32_t>             m_LayoutTextOffsets;
    dmArray<CachedGlyph>          m_Glyphs;
    // The preview and editable text share one CPU atlas and GPU texture.
    dmArray<uint8_t>                 m_Atlas;
    dmArray<FontGlyphVertex>         m_Vertices;
    dmArray<ColorVertex>             m_ColorVertices;
    dmArray<SpriteVertex>            m_SpriteVertices;
    dmArray<SpriteDraw>              m_SpriteDraws;
    dmArray<FontViewerNuklearVertex> m_NuklearVertices;
    FontViewerNuklearLayout          m_NuklearLayout;
    FontViewerProperties             m_Properties;
    uint32_t                         m_VertexCount;
    uint32_t                         m_ColorBackgroundVertexCount;
    uint32_t                         m_ColorDecorationVertexCount;
    uint32_t                         m_ColorDebugVertexCount;
    uint16_t                         m_CellWidth;
    uint16_t                         m_CellHeight;
    uint16_t                         m_CellMaxAscent;
    uint8_t                          m_AtlasChannels;
    bool                             m_Closed;
    bool                             m_LegacyLayout;
    bool                             m_ShowBaselines;
    bool                             m_ShowQuads;
    bool                             m_TextFieldFocused;
    bool                             m_ScrollCaretIntoViewRequested;
    bool                             m_RebuildRequested;
    bool                             m_RenderUpdateRequested;
    bool                             m_PreviousMouseDown;
    bool                             m_PreviousBackspaceDown;
    bool                             m_PreviousDeleteDown;
    bool                             m_PreviousEnterDown;
    bool                             m_PreviousEscapeDown;
    uint64_t                         m_BackspaceRepeatAt;
    ArrowKey                         m_RepeatingArrowKey;
    uint64_t                         m_ArrowRepeatAt;
    int32_t                          m_PreviousMouseWheel;
    uint32_t                         m_HoveredLinkObject;
    uint32_t                         m_PressedLinkObject;
    float                            m_TextScrollY;
    float                            m_EditorContentHeight;
    float                            m_FontSize;
    uint32_t                         m_Caret;
    uint32_t                         m_SelectionAnchor;
    float                            m_PreferredCaretX;
    bool                             m_HasPreferredCaretX;
    bool                             m_TextSelecting;
    float                            m_Zoom;
    float                            m_PanX;
    float                            m_PanY;
    bool                             m_LinkCursor;
    bool                             m_PreviewDragging;
    int32_t                          m_PreviousMouseX;
    int32_t                          m_PreviousMouseY;
    const char*                      m_ExportStyle;
    const char*                      m_OutputPath;
    uint32_t                         m_RequestedFrameCount;
    uint32_t                         m_ExportCanvasWidth;
    uint32_t                         m_ExportCanvasHeight;
    bool                             m_ExportMode;
    bool                             m_NuklearInitialized;
};

static int OnWindowClose(void* user_data)
{
    ((Viewer*)user_data)->m_Closed = true;
    return 1;
}

static bool ReadTextFile(const char* path, dmArray<char>* output)
{
    FILE* file = fopen(path, "rb");
    if (!file)
        return false;
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0)
    {
        fclose(file);
        return false;
    }
    output->SetCapacity((uint32_t)size + 1);
    output->SetSize((uint32_t)size + 1);
    const bool success = fread(output->Begin(), 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    (*output)[size] = 0;
    return success;
}

static void PushFontPath(Viewer* viewer, const char* path)
{
    if (viewer->m_FontPaths.Full())
        viewer->m_FontPaths.OffsetCapacity(4);
    viewer->m_FontPaths.Push(path);
}

static bool PushText(Viewer* viewer, const char* text, bool copy)
{
    const char* stored_text = text;
    if (copy)
    {
        const size_t size = strlen(text) + 1;
        char*        owned_text = (char*)malloc(size);
        if (!owned_text)
            return false;
        memcpy(owned_text, text, size);
        if (viewer->m_OwnedTexts.Full())
            viewer->m_OwnedTexts.OffsetCapacity(4);
        viewer->m_OwnedTexts.Push(owned_text);
        stored_text = owned_text;
    }
    if (viewer->m_Texts.Full())
        viewer->m_Texts.OffsetCapacity(4);
    viewer->m_Texts.Push(stored_text);
    return true;
}

static bool PushTextFile(Viewer* viewer, const char* path)
{
    dmArray<char> text;
    if (!ReadTextFile(path, &text))
    {
        dmLogError("Unable to read text file '%s'", path);
        return false;
    }
    return PushText(viewer, text.Begin(), true);
}

static void PrintUsage(const char* executable)
{
    fprintf(stderr, "Usage: %s [-f <font>]... [-t <text>]... [-tf <text-file>]... [-sz <size>]\n", executable);
    fprintf(stderr, "       %s -t <text> -style <opening-tags> -o <output.webp> [--frames <count>] [--canvas <width>x<height>] [-sz <size>]\n", executable);
    fprintf(stderr, "Export defaults to 30 frames per second over the shortest exact animation loop.\n");
    fprintf(stderr, "--frames overrides the calculated count; shake exports require an even count.\n");
    fprintf(stderr, "WebP export requires img2webp on PATH.\n");
}

static bool ParseArguments(Viewer* viewer, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const char* option = argv[i];
        if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0)
        {
            PrintUsage(argv[0]);
            return false;
        }
        if (i + 1 >= argc)
        {
            dmLogError("Missing value for '%s'", option);
            PrintUsage(argv[0]);
            return false;
        }
        const char* value = argv[++i];
        if (strcmp(option, "-f") == 0)
        {
            PushFontPath(viewer, value);
        }
        else if (strcmp(option, "-t") == 0)
        {
            if (!PushText(viewer, value, true))
                return false;
        }
        else if (strcmp(option, "-tf") == 0)
        {
            if (!PushTextFile(viewer, value))
                return false;
        }
        else if (strcmp(option, "-sz") == 0)
        {
            char*       end = 0;
            const float size = strtof(value, &end);
            if (!end || *end || !isfinite(size) || size <= 0.0f || size > 512.0f)
            {
                dmLogError("Invalid font size '%s'; expected a value greater than 0 and at most 512", value);
                return false;
            }
            viewer->m_FontSize = size;
        }
        else if (strcmp(option, "-style") == 0 || strcmp(option, "--style") == 0)
        {
            viewer->m_ExportStyle = value;
        }
        else if (strcmp(option, "-o") == 0 || strcmp(option, "--output") == 0)
        {
            viewer->m_OutputPath = value;
            viewer->m_ExportMode = true;
        }
        else if (strcmp(option, "--frames") == 0)
        {
            char*               end = 0;
            const unsigned long frame_count = strtoul(value, &end, 10);

            if (!end || *end || frame_count == 0 || frame_count > 1000)
            {
                dmLogError("Invalid frame count '%s'; expected an integer from 1 through 1000", value);

                return false;
            }

            viewer->m_RequestedFrameCount = (uint32_t)frame_count;
        }
        else if (strcmp(option, "--canvas") == 0)
        {
            char*               end = 0;
            const unsigned long canvas_width = strtoul(value, &end, 10);

            if (!end || *end != 'x')
            {
                dmLogError("Invalid canvas '%s'; expected <width>x<height>", value);

                return false;
            }

            char*               height_end = 0;
            const unsigned long canvas_height = strtoul(end + 1, &height_end, 10);

            if (!height_end || *height_end || canvas_width == 0 || canvas_height == 0 || canvas_width > 4096 || canvas_height > 4096)
            {
                dmLogError("Invalid canvas '%s'; dimensions must be from 1 through 4096", value);

                return false;
            }

            viewer->m_ExportCanvasWidth = (uint32_t)canvas_width;
            viewer->m_ExportCanvasHeight = (uint32_t)canvas_height;
        }
        else
        {
            dmLogError("Unknown option '%s'", option);
            PrintUsage(argv[0]);
            return false;
        }
    }
    if (viewer->m_RequestedFrameCount && !viewer->m_ExportMode)
    {
        dmLogError("--frames requires -o <output.webp>");

        return false;
    }

    if (viewer->m_ExportCanvasWidth && !viewer->m_ExportMode)
    {
        dmLogError("--canvas requires -o <output.webp>");

        return false;
    }

    if (viewer->m_ExportMode)
    {
        const size_t output_length = strlen(viewer->m_OutputPath);

        if (output_length < 5 || strcmp(viewer->m_OutputPath + output_length - 5, ".webp") != 0)
        {
            dmLogError("Documentation export output must use the .webp extension");

            return false;
        }
    }

    return true;
}

static bool BuildExportStyleClosingTags(const char* definition, dmArray<char>* closing_tags)
{
    if (!definition || !*definition)
    {
        return true;
    }

    HMarkup     markup = 0;
    MarkupError error;

    if (MarkupCreateStyleFragment(definition, (uint32_t)strlen(definition), &markup, &error) != MARKUP_RESULT_OK)
    {
        dmLogError("Invalid export style at byte %u (error %d); expected opening tags only", error.m_ByteOffset, error.m_Type);

        return false;
    }

    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    const uint32_t         node_count = MarkupGetStyleNodeCount(markup);
    const char*            source = MarkupGetSource(markup);
    uint32_t               size = 0;

    for (uint32_t i = 1; i < node_count; ++i)
    {
        size += nodes[i].m_Tag.m_Length + 3;
    }

    closing_tags->SetCapacity(size);
    closing_tags->SetSize(size);
    uint32_t offset = 0;

    for (uint32_t i = node_count; i > 1; --i)
    {
        const MarkupString& tag = nodes[i - 1].m_Tag;
        (*closing_tags)[offset++] = '<';
        (*closing_tags)[offset++] = '/';
        memcpy(closing_tags->Begin() + offset, source + tag.m_Offset, tag.m_Length);
        offset += tag.m_Length;
        (*closing_tags)[offset++] = '>';
    }

    MarkupDestroy(markup);
    return true;
}

static bool InitializeEditorText(Viewer* viewer)
{
    const bool     use_defaults = viewer->m_Texts.Empty();
    const uint32_t text_count = use_defaults ? 3 : viewer->m_Texts.Size();
    const char*    default_texts[] = { ENGLISH_TEXT, ARABIC_TEXT, JAPANESE_TEXT };
    const uint32_t style_length = viewer->m_ExportStyle ? (uint32_t)strlen(viewer->m_ExportStyle) : 0;
    dmArray<char>  closing_tags;

    if (!BuildExportStyleClosingTags(viewer->m_ExportStyle, &closing_tags))
    {
        return false;
    }

    uint32_t       size = style_length + closing_tags.Size() + 1;
    for (uint32_t i = 0; i < text_count; ++i)
        size += (uint32_t)strlen(use_defaults ? default_texts[i] : viewer->m_Texts[i]) + (i ? 2 : 0);
    viewer->m_EditorText.SetCapacity(size + 256);
    viewer->m_EditorText.SetSize(size);
    uint32_t offset = 0;

    if (style_length)
    {
        memcpy(viewer->m_EditorText.Begin(), viewer->m_ExportStyle, style_length);
        offset += style_length;
    }
    for (uint32_t i = 0; i < text_count; ++i)
    {
        if (i)
        {
            viewer->m_EditorText[offset++] = '\n';
            viewer->m_EditorText[offset++] = '\n';
        }
        const char*    text = use_defaults ? default_texts[i] : viewer->m_Texts[i];
        const uint32_t length = (uint32_t)strlen(text);
        memcpy(viewer->m_EditorText.Begin() + offset, text, length);
        offset += length;
    }
    if (!closing_tags.Empty())
    {
        memcpy(viewer->m_EditorText.Begin() + offset, closing_tags.Begin(), closing_tags.Size());
        offset += closing_tags.Size();
    }
    viewer->m_EditorText[offset] = 0;
    viewer->m_MarkedText.SetCapacity(32);
    viewer->m_MarkedText.SetSize(1);
    viewer->m_MarkedText[0] = 0;
    for (uint32_t i = 0; i < offset; ++i)
        viewer->m_Caret += ((uint8_t)viewer->m_EditorText[i] & 0xc0) != 0x80;
    viewer->m_SelectionAnchor = viewer->m_Caret;
    return true;
}

// Editing positions use Unicode codepoint indices because TextGlyph::m_Cluster
// uses codepoints. Convert to UTF-8 byte offsets only when mutating the buffer.
static uint32_t CodepointToByteOffset(const dmArray<char>& text, uint32_t codepoint_index)
{
    uint32_t       codepoint = 0;
    uint32_t       offset = 0;
    const uint32_t length = text.Size() - 1;
    while (offset < length && codepoint < codepoint_index)
    {
        ++offset;
        while (offset < length && ((uint8_t)text[offset] & 0xc0) == 0x80)
            ++offset;
        ++codepoint;
    }
    return offset;
}

static uint32_t EditorCodepointCount(const Viewer* viewer)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i + 1 < viewer->m_EditorText.Size(); ++i)
        count += ((uint8_t)viewer->m_EditorText[i] & 0xc0) != 0x80;
    return count;
}

static bool DeleteSelection(Viewer* viewer)
{
    if (viewer->m_Caret == viewer->m_SelectionAnchor)
        return false;
    const uint32_t first = dmMath::Min(viewer->m_Caret, viewer->m_SelectionAnchor);
    const uint32_t last = dmMath::Max(viewer->m_Caret, viewer->m_SelectionAnchor);
    const uint32_t first_byte = CodepointToByteOffset(viewer->m_EditorText, first);
    const uint32_t last_byte = CodepointToByteOffset(viewer->m_EditorText, last);
    memmove(viewer->m_EditorText.Begin() + first_byte,
            viewer->m_EditorText.Begin() + last_byte,
            viewer->m_EditorText.Size() - last_byte);
    viewer->m_EditorText.SetSize(viewer->m_EditorText.Size() - (last_byte - first_byte));
    viewer->m_Caret = first;
    viewer->m_SelectionAnchor = first;
    viewer->m_HasPreferredCaretX = false;
    viewer->m_ScrollCaretIntoViewRequested = true;
    viewer->m_RebuildRequested = true;
    return true;
}

static void SetMarkedText(Viewer* viewer, const char* text)
{
    const uint32_t size = (uint32_t)strlen(text) + 1;
    if (viewer->m_MarkedText.Capacity() < size)
        viewer->m_MarkedText.SetCapacity(size + 32);
    viewer->m_MarkedText.SetSize(size);
    memcpy(viewer->m_MarkedText.Begin(), text, size);
    viewer->m_RebuildRequested = true;
}

static void UpdateLayoutText(Viewer* viewer)
{
    const uint32_t editor_length = viewer->m_EditorText.Size() - 1;
    const uint32_t marked_length = viewer->m_MarkedText.Size() - 1;
    const uint32_t caret_byte = CodepointToByteOffset(viewer->m_EditorText, viewer->m_Caret);
    const uint32_t size = editor_length + marked_length + 1;
    if (viewer->m_LayoutText.Capacity() < size)
        viewer->m_LayoutText.SetCapacity(size + 256);
    viewer->m_LayoutText.SetSize(size);
    // Marked IME text is transient: shape it at the caret without committing it
    // to the editable buffer until the platform sends committed characters.
    memcpy(viewer->m_LayoutText.Begin(), viewer->m_EditorText.Begin(), caret_byte);
    memcpy(viewer->m_LayoutText.Begin() + caret_byte, viewer->m_MarkedText.Begin(), marked_length);
    memcpy(viewer->m_LayoutText.Begin() + caret_byte + marked_length, viewer->m_EditorText.Begin() + caret_byte, editor_length - caret_byte);
    viewer->m_LayoutText[size - 1] = 0;
}

static void AppendEditorBytes(Viewer* viewer, const char* bytes, uint32_t byte_count)
{
    DeleteSelection(viewer);
    const uint32_t old_size = viewer->m_EditorText.Size();
    const uint32_t caret_byte = CodepointToByteOffset(viewer->m_EditorText, viewer->m_Caret);
    if (viewer->m_EditorText.Capacity() < old_size + byte_count)
        viewer->m_EditorText.SetCapacity(old_size + byte_count + 256);
    viewer->m_EditorText.SetSize(old_size + byte_count);
    memmove(viewer->m_EditorText.Begin() + caret_byte + byte_count,
            viewer->m_EditorText.Begin() + caret_byte,
            old_size - caret_byte);
    memcpy(viewer->m_EditorText.Begin() + caret_byte, bytes, byte_count);
    for (uint32_t i = 0; i < byte_count; ++i)
        viewer->m_Caret += ((uint8_t)bytes[i] & 0xc0) != 0x80;
    viewer->m_SelectionAnchor = viewer->m_Caret;
    viewer->m_HasPreferredCaretX = false;
    viewer->m_ScrollCaretIntoViewRequested = true;
    viewer->m_RebuildRequested = true;
}

static void OnKeyboardChar(void* user_data, int codepoint)
{
    Viewer* viewer = (Viewer*)user_data;
    if (!viewer->m_TextFieldFocused || codepoint < 32 || codepoint > 0x10ffff)
        return;
    if (viewer->m_MarkedText.Size() > 1)
        SetMarkedText(viewer, "");
    char     bytes[4];
    uint32_t count = 0;
    if (codepoint <= 0x7f)
    {
        bytes[count++] = (char)codepoint;
    }
    else if (codepoint <= 0x7ff)
    {
        bytes[count++] = (char)(0xc0 | (codepoint >> 6));
        bytes[count++] = (char)(0x80 | (codepoint & 0x3f));
    }
    else if (codepoint <= 0xffff)
    {
        bytes[count++] = (char)(0xe0 | (codepoint >> 12));
        bytes[count++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[count++] = (char)(0x80 | (codepoint & 0x3f));
    }
    else
    {
        bytes[count++] = (char)(0xf0 | (codepoint >> 18));
        bytes[count++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        bytes[count++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[count++] = (char)(0x80 | (codepoint & 0x3f));
    }
    AppendEditorBytes(viewer, bytes, count);
}

static void OnKeyboardMarkedText(void* user_data, char* text)
{
    Viewer* viewer = (Viewer*)user_data;
    if (viewer->m_TextFieldFocused)
        SetMarkedText(viewer, text ? text : "");
}

static void RemoveLastCodepoint(Viewer* viewer)
{
    if (DeleteSelection(viewer) || viewer->m_Caret == 0)
        return;
    viewer->m_SelectionAnchor = viewer->m_Caret - 1;
    DeleteSelection(viewer);
}

static void RemoveNextCodepoint(Viewer* viewer)
{
    if (DeleteSelection(viewer) || viewer->m_Caret >= EditorCodepointCount(viewer))
        return;
    viewer->m_SelectionAnchor = viewer->m_Caret + 1;
    DeleteSelection(viewer);
    viewer->m_RebuildRequested = true;
}

static bool PointInBox(int32_t x, int32_t y, const FontViewerNuklearBox& box)
{
    return x >= box.m_X && x < box.m_X + box.m_Width && y >= box.m_Y && y < box.m_Y + box.m_Height;
}

static CachedGlyph* FindGlyph(Viewer* viewer, HFont font, uint32_t glyph_index, float font_size, bool apply_properties)
{
    for (uint32_t i = 0; i < viewer->m_Glyphs.Size(); ++i)
    {
        CachedGlyph& glyph = viewer->m_Glyphs[i];
        if (glyph.m_Font == font && glyph.m_GlyphIndex == glyph_index && glyph.m_FontSize == font_size && glyph.m_ApplyProperties == apply_properties)
            return &glyph;
    }
    return 0;
}

static bool AddLayoutGlyphs(Viewer* viewer, HTextLayout layout, float font_size, bool apply_properties)
{
    const float markup_outline_width = apply_properties ? TextLayoutGetMaxMarkupOutlineWidth(layout) : 0.0f;
    TextGlyph*     text_glyphs = TextLayoutGetGlyphs(layout);
    const uint32_t glyph_count = TextLayoutGetGlyphCount(layout);
    for (uint32_t i = 0; i < glyph_count; ++i)
    {
        TextGlyph& text_glyph = text_glyphs[i];

        if (dmUtf8::IsWhiteSpace(text_glyph.m_Codepoint) || (text_glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT))
        {
            continue;
        }

        if (FindGlyph(viewer, text_glyph.m_Font, text_glyph.m_GlyphIndex, font_size, apply_properties))
        {
            continue;
        }

        CachedGlyph glyph;
        memset(&glyph, 0, sizeof(glyph));
        glyph.m_Font = text_glyph.m_Font;
        glyph.m_GlyphIndex = text_glyph.m_GlyphIndex;
        glyph.m_FontSize = font_size;
        glyph.m_ApplyProperties = apply_properties;
        FontGlyphGenParams params;
        // Nuklear text always gets a plain SDF. Only preview glyphs bake the extra
        // padding and shadow channels required by the font properties.
        const bool  has_blurred_shadow = apply_properties && viewer->m_Properties.m_ShadowBlur > 0.0f;
        const float base_outline_width = apply_properties ? viewer->m_Properties.m_OutlineWidth : 0.0f;
        // Reserve enough SDF distance for the widest span, but do not bake that
        // width into every glyph's blurred-shadow silhouette.
        const float padding_outline_width = dmMath::Max(base_outline_width, markup_outline_width);
        params.m_Scale = FontGetScaleFromSize(glyph.m_Font, font_size);
        params.m_SdfPadding = 6.0f + padding_outline_width + (has_blurred_shadow ? viewer->m_Properties.m_ShadowBlur : 0.0f);
        params.m_OutlineWidth = base_outline_width;
        params.m_ShadowBlur = has_blurred_shadow ? viewer->m_Properties.m_ShadowBlur : 0.0f;
        if (FontGenerateGlyph(glyph.m_Font, glyph.m_GlyphIndex, &params, &glyph.m_Glyph) != FONT_RESULT_OK)
        {
            dmLogError("Unable to generate glyph %u", glyph.m_GlyphIndex);
            return false;
        }
        viewer->m_CellWidth = dmMath::Max(viewer->m_CellWidth, (uint16_t)(glyph.m_Glyph.m_Bitmap.m_Width + CELL_PADDING * 2));
        viewer->m_CellHeight = dmMath::Max(viewer->m_CellHeight, (uint16_t)(glyph.m_Glyph.m_Bitmap.m_Height + CELL_PADDING * 2));
        viewer->m_CellMaxAscent = dmMath::Max(viewer->m_CellMaxAscent, (uint16_t)glyph.m_Glyph.m_Ascent);
        if (viewer->m_Glyphs.Full())
            viewer->m_Glyphs.OffsetCapacity(32);
        viewer->m_Glyphs.Push(glyph);
    }
    return true;
}

static bool BuildAtlas(Viewer* viewer)
{
    const uint32_t columns = ATLAS_WIDTH / viewer->m_CellWidth;
    const uint32_t rows = ATLAS_HEIGHT / viewer->m_CellHeight;
    if (columns * rows < viewer->m_Glyphs.Size())
    {
        dmLogError("The %ux%u atlas cannot hold %u glyphs in %ux%u cells", ATLAS_WIDTH, ATLAS_HEIGHT, viewer->m_Glyphs.Size(), viewer->m_CellWidth, viewer->m_CellHeight);
        return false;
    }
    viewer->m_Atlas.SetCapacity(ATLAS_WIDTH * ATLAS_HEIGHT * viewer->m_AtlasChannels);
    viewer->m_Atlas.SetSize(viewer->m_Atlas.Capacity());
    memset(viewer->m_Atlas.Begin(), 0, viewer->m_Atlas.Size());
    for (uint32_t i = 0; i < viewer->m_Glyphs.Size(); ++i)
    {
        CachedGlyph& glyph = viewer->m_Glyphs[i];
        glyph.m_X = (i % columns) * viewer->m_CellWidth;
        glyph.m_Y = (i / columns) * viewer->m_CellHeight;
        const uint32_t image_x = glyph.m_X + CELL_PADDING;
        const uint32_t image_y = glyph.m_Y + CELL_PADDING + viewer->m_CellMaxAscent - (uint16_t)glyph.m_Glyph.m_Ascent;
        for (uint32_t y = 0; y < glyph.m_Glyph.m_Bitmap.m_Height; ++y)
        {
            uint8_t*       destination = viewer->m_Atlas.Begin() + ((image_y + y) * ATLAS_WIDTH + image_x) * viewer->m_AtlasChannels;
            const uint8_t* source = glyph.m_Glyph.m_Bitmap.m_Data + y * glyph.m_Glyph.m_Bitmap.m_Width * glyph.m_Glyph.m_Bitmap.m_Channels;
            if (glyph.m_Glyph.m_Bitmap.m_Channels == viewer->m_AtlasChannels)
            {
                memcpy(destination, source, glyph.m_Glyph.m_Bitmap.m_Width * viewer->m_AtlasChannels);
            }
            else
            {
                // A blurred preview makes the shared atlas RGB, while the
                // effect-free UI glyph generator still returns one channel.
                // Replication preserves the face SDF in every shader channel.
                for (uint32_t x = 0; x < glyph.m_Glyph.m_Bitmap.m_Width; ++x)
                {
                    destination[x * 3 + 0] = source[x];
                    destination[x * 3 + 1] = source[x];
                    destination[x * 3 + 2] = source[x];
                }
            }
        }
    }
    return true;
}

static const TextLayoutObjectAttribute* FindLayoutObjectAttribute(const char* source, const TextLayoutObjectAttribute* attributes, const TextLayoutObject* object, const char* name)
{
    const uint32_t name_length = (uint32_t)strlen(name);

    for (uint32_t i = 0; i < object->m_AttributeCount; ++i)
    {
        const TextLayoutObjectAttribute& attribute = attributes[object->m_AttributeIndex + i];

        if (attribute.m_NameLength == name_length && memcmp(source + attribute.m_NameOffset, name, name_length) == 0)
        {
            return &attribute;
        }
    }

    return 0;
}

static bool LoadViewerImage(Viewer* viewer, const char* path, ViewerImage** output)
{
    dmArray<char> encoded;

    if (!ReadTextFile(path, &encoded))
    {
        return false;
    }

    dmImage::Image image;

    if (dmImage::Load(encoded.Begin(), encoded.Size() - 1, false, true, &image) != dmImage::RESULT_OK)
    {
        return false;
    }

    const uint32_t bytes_per_pixel = dmImage::BytesPerPixel(image.m_Type);

    if (bytes_per_pixel == 0 || image.m_Width == 0 || image.m_Height == 0 || image.m_Width > 0xffff || image.m_Height > 0xffff)
    {
        dmImage::Free(&image);

        return false;
    }

    dmGraphics::TextureFormat format;

    switch (image.m_Type)
    {
        case dmImage::TYPE_LUMINANCE:
            format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
            break;
        case dmImage::TYPE_LUMINANCE_ALPHA:
            format = dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA;
            break;
        case dmImage::TYPE_RGB:
            format = dmGraphics::TEXTURE_FORMAT_RGB;
            break;
        case dmImage::TYPE_RGBA:
            format = dmGraphics::TEXTURE_FORMAT_RGBA;
            break;
        default:
            dmImage::Free(&image);

            return false;
    }

    ViewerImage* resource = new ViewerImage;
    resource->m_Texture = 0;
    dmGraphics::TextureCreationParams creation;
    creation.m_Width = (uint16_t)image.m_Width;
    creation.m_Height = (uint16_t)image.m_Height;
    creation.m_OriginalWidth = creation.m_Width;
    creation.m_OriginalHeight = creation.m_Height;
    resource->m_Texture = dmGraphics::NewTexture(viewer->m_Context, creation);

    if (resource->m_Texture)
    {
        dmGraphics::TextureParams texture;
        texture.m_Data = image.m_Buffer;
        texture.m_DataSize = image.m_Width * image.m_Height * bytes_per_pixel;
        texture.m_Width = creation.m_Width;
        texture.m_Height = creation.m_Height;
        texture.m_Depth = 1;
        texture.m_LayerCount = 1;
        texture.m_Format = format;
        texture.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        texture.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        dmGraphics::SetTexture(viewer->m_Context, resource->m_Texture, texture);
    }

    dmImage::Free(&image);

    if (!resource->m_Texture)
    {
        delete resource;

        return false;
    }

    *output = resource;

    return true;
}

static uint8_t ResolveViewerLayoutObject(void* context, const char* source, const TextLayoutObjectAttribute* attributes, float proposed_width, float proposed_height, TextLayoutObject* object)
{
    Viewer* viewer = (Viewer*)context;
    object->m_Width = proposed_width;
    object->m_Height = proposed_height;
    object->m_Resource = 0;

    if (object->m_Tag != TAG_SPRITE)
    {
        return 1;
    }

    const TextLayoutObjectAttribute* source_attribute = FindLayoutObjectAttribute(source, attributes, object, "src");

    if (!source_attribute || source_attribute->m_ValueLength == 0)
    {
        dmLogWarning("An inline sprite has no src attribute; using its proposed layout size without rendering it");

        return 1;
    }

    dmArray<char> path;
    path.SetCapacity(source_attribute->m_ValueLength + 1);
    path.SetSize(source_attribute->m_ValueLength + 1);
    memcpy(path.Begin(), source + source_attribute->m_ValueOffset, source_attribute->m_ValueLength);
    path[source_attribute->m_ValueLength] = 0;
    char* relative_path = path.Begin();

    while (*relative_path == '/')
    {
        ++relative_path;
    }

    ViewerImage* resource = 0;

    if (!*relative_path || !LoadViewerImage(viewer, relative_path, &resource))
    {
        dmLogWarning("Unable to load inline sprite '%s' relative to the fontviewer working directory; using its proposed layout size without rendering it", relative_path);

        return 1;
    }

    object->m_Resource = (uintptr_t)resource;

    return 1;
}

static void ReleaseViewerLayoutObject(void* context, const TextLayoutObject* object)
{
    ViewerImage* resource = (ViewerImage*)object->m_Resource;

    if (!resource)
    {
        return;
    }

    Viewer* viewer = (Viewer*)context;
    dmGraphics::DeleteTexture(viewer->m_Context, resource->m_Texture);
    delete resource;
}

static bool CreateLayout(Viewer* viewer, const char* text, uint32_t text_length, float font_size, float width, bool line_break, bool legacy_layout, bool markup, HTextLayout* out_layout)
{
    TextLayoutSettings settings = {};
    settings.m_Size = font_size;
    settings.m_Width = width;
    settings.m_Leading = 1.2f;
    settings.m_LineBreak = line_break ? 1 : 0;
    settings.m_ResolveObject = ResolveViewerLayoutObject;
    settings.m_ReleaseObject = ReleaseViewerLayoutObject;
    settings.m_ObjectContext = viewer;

    if (markup)
    {
        HMarkup      parsed_markup = 0;
        MarkupError  error;
        MarkupResult markup_result = MarkupCreateRecovering(text, text_length, &parsed_markup, &error);

        if (markup_result != MARKUP_RESULT_OK)
        {
            dmLogWarning("Unable to parse markup at byte %u (error %d); displaying the source as plain text", error.m_ByteOffset, error.m_Type);
        }
        else
        {
            if (error.m_Type != MARKUP_ERROR_NONE)
            {
                dmLogWarning("Recovered markup parsing error at byte %u (error %d)", error.m_ByteOffset, error.m_Type);
            }

            TextResult result = legacy_layout ? TextLayoutLegacyCreateMarkup(viewer->m_Collection, parsed_markup, &settings, out_layout) : TextLayoutCreateMarkup(viewer->m_Collection, parsed_markup, &settings, out_layout);
            MarkupDestroy(parsed_markup);

            if (result == TEXT_RESULT_OK)
            {
                return true;
            }

            dmLogWarning("Unable to resolve markup values; displaying the source as plain text");
        }
    }

    dmArray<uint32_t> codepoints;
    char*             copy = (char*)malloc(text_length + 1);
    if (!copy)
        return false;
    memcpy(copy, text, text_length);
    copy[text_length] = 0;
    TextToCodePoints(copy, codepoints);
    free(copy);
    TextResult result = legacy_layout
                      ? TextLayoutLegacyCreate(viewer->m_Collection, codepoints.Begin(), codepoints.Size(), &settings, out_layout)
                      : TextLayoutCreate(viewer->m_Collection, codepoints.Begin(), codepoints.Size(), &settings, out_layout);

    return result == TEXT_RESULT_OK;
}

static uint32_t CountVisibleGlyphs(HTextLayout layout)
{
    uint32_t   count = 0;
    TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
    for (uint32_t i = 0; i < TextLayoutGetGlyphCount(layout); ++i)
        count += dmUtf8::IsWhiteSpace(glyphs[i].m_Codepoint) || (glyphs[i].m_Flags & TEXT_GLYPH_FLAG_OBJECT) ? 0 : 1;
    return count;
}

static void ClipPackedQuad(Viewer* viewer, uint32_t vertex_index, const FontViewerNuklearBox& clip_box)
{
    // The centralized packer emits a complete quad. Clip in screen space and
    // adjust UVs proportionally so text cannot leak outside Nuklear's text field.
    float min_x = WINDOW_WIDTH;
    float min_y = WINDOW_HEIGHT;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float uv_at_min_x = 0.0f;
    float uv_at_max_x = 0.0f;
    float uv_at_min_y = 0.0f;
    float uv_at_max_y = 0.0f;
    for (uint32_t vertex_offset = 0; vertex_offset < 6; ++vertex_offset)
    {
        FontGlyphVertex& vertex = viewer->m_Vertices[vertex_index + vertex_offset];
        const float      x = (vertex.m_Position[0] + 1.0f) * WINDOW_WIDTH * 0.5f;
        const float      y = (vertex.m_Position[1] + 1.0f) * WINDOW_HEIGHT * 0.5f;
        if (x <= min_x)
        {
            min_x = x;
            uv_at_min_x = FontUnpackGlyphUV(vertex.m_UV[0]);
        }
        if (x >= max_x)
        {
            max_x = x;
            uv_at_max_x = FontUnpackGlyphUV(vertex.m_UV[0]);
        }
        if (y <= min_y)
        {
            min_y = y;
            uv_at_min_y = FontUnpackGlyphUV(vertex.m_UV[1]);
        }
        if (y >= max_y)
        {
            max_y = y;
            uv_at_max_y = FontUnpackGlyphUV(vertex.m_UV[1]);
        }
    }
    const float clip_max_x = clip_box.m_X + clip_box.m_Width;
    const float clip_max_y = clip_box.m_Y + clip_box.m_Height;
    if (max_x <= clip_box.m_X || min_x >= clip_max_x || max_y <= clip_box.m_Y || min_y >= clip_max_y)
    {
        for (uint32_t vertex_offset = 0; vertex_offset < 6; ++vertex_offset)
        {
            FontGlyphVertex& vertex = viewer->m_Vertices[vertex_index + vertex_offset];
            memset(vertex.m_FaceColor, 0, sizeof(vertex.m_FaceColor));
            memset(vertex.m_OutlineColor, 0, sizeof(vertex.m_OutlineColor));
            memset(vertex.m_ShadowColor, 0, sizeof(vertex.m_ShadowColor));
        }
        return;
    }
    for (uint32_t vertex_offset = 0; vertex_offset < 6; ++vertex_offset)
    {
        FontGlyphVertex& vertex = viewer->m_Vertices[vertex_index + vertex_offset];
        const float      x = (vertex.m_Position[0] + 1.0f) * WINDOW_WIDTH * 0.5f;
        const float      y = (vertex.m_Position[1] + 1.0f) * WINDOW_HEIGHT * 0.5f;
        const float      clipped_x = dmMath::Max(clip_box.m_X, dmMath::Min(clip_max_x, x));
        const float      clipped_y = dmMath::Max(clip_box.m_Y, dmMath::Min(clip_max_y, y));
        vertex.m_UV[0] = FontPackGlyphUV(uv_at_min_x + (clipped_x - min_x) * (uv_at_max_x - uv_at_min_x) / (max_x - min_x));
        vertex.m_UV[1] = FontPackGlyphUV(uv_at_min_y + (clipped_y - min_y) * (uv_at_max_y - uv_at_min_y) / (max_y - min_y));
        vertex.m_Position[0] = 2.0f * clipped_x / WINDOW_WIDTH - 1.0f;
        vertex.m_Position[1] = 2.0f * clipped_y / WINDOW_HEIGHT - 1.0f;
    }
}

static void PushTransformedClippedGradientRectangle(Viewer* viewer, const Matrix4& transform, const FontViewerNuklearBox& clip,
                                                     float x, float y, float width, float height, const TextGlyphFaceColors& colors,
                                                     float pattern_start, float pattern_end, float pattern_duty);

static void PackLayout(Viewer* viewer, HTextLayout layout, float paragraph_x, float paragraph_top, float font_size, float paragraph_width, const FontViewerNuklearBox& clip_box, bool bold, bool apply_properties, uint32_t layer_count, uint32_t layer_stride, uint32_t* vertex_index)
{
    TextGlyph*     text_glyphs = TextLayoutGetGlyphs(layout);
    TextLine*      lines = TextLayoutGetLines(layout);
    TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
    float layout_width;
    float layout_height;
    TextLayoutGetBounds(layout, &layout_width, &layout_height);
    (void)layout_width;
    Matrix4     transform = Matrix4::orthographic(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT, -1.0f, 1.0f);
    if (apply_properties)
    {
        // Zoom/pan is a preview transform only. Shaping and glyph generation
        // remain at the requested font size and therefore need no rebuild.
        const float   pivot_x = viewer->m_NuklearLayout.m_ContentWidth * 0.5f;
        const float   pivot_y = WINDOW_HEIGHT * 0.5f;
        const Matrix4 zoom_transform = Matrix4::translation(Vector3(pivot_x + viewer->m_PanX, pivot_y + viewer->m_PanY, 0.0f)) *
        Matrix4::scale(Vector3(viewer->m_Zoom, viewer->m_Zoom, 1.0f)) *
        Matrix4::translation(Vector3(-pivot_x, -pivot_y, 0.0f));
        transform = transform * zoom_transform;
    }
    const float markup_outline_width = apply_properties ? TextLayoutGetMaxMarkupOutlineWidth(layout) : 0.0f;
    const float base_outline_width = apply_properties ? viewer->m_Properties.m_OutlineWidth : 0.0f;
    const float padding_outline_width = dmMath::Max(base_outline_width, markup_outline_width);
    const float shadow_blur = apply_properties ? viewer->m_Properties.m_ShadowBlur : 0.0f;
    const float padding = 6.0f + padding_outline_width + shadow_blur;
    const float sdf_outline = (0.75f * 255.0f - (191.0f / padding) * base_outline_width) / 255.0f;
    const float sdf_shadow = shadow_blur > 0.0f ? (0.75f * 255.0f - (191.0f / padding) * shadow_blur) / 255.0f : 1.0f;
    // The editable field uses a slightly lower edge threshold and a narrower
    // transition, making small SDF text stronger while keeping it crisp.
    const bool    crisp_ui_text = !apply_properties && clip_box.m_Width < WINDOW_WIDTH;
    const float   sdf_face = bold ? 0.69f : (crisp_ui_text ? 0.72f : 0.75f);
    const float   sdf_smoothing = (crisp_ui_text ? 0.125f : 0.25f) / padding;
    const Vector4 face_color(apply_properties ? viewer->m_Properties.m_FaceColor[0] : 1.0f,
                             apply_properties ? viewer->m_Properties.m_FaceColor[1] : 1.0f,
                             apply_properties ? viewer->m_Properties.m_FaceColor[2] : 1.0f,
                             apply_properties ? viewer->m_Properties.m_Alpha : 1.0f);
    const float   base_face_color[4] = { face_color.getX(), face_color.getY(), face_color.getZ(), face_color.getW() };
    const float   overall_alpha = apply_properties ? viewer->m_Properties.m_Alpha : 1.0f;
    const float   outline_alpha = apply_properties && base_outline_width > 0.0f ? overall_alpha * viewer->m_Properties.m_OutlineAlpha : 0.0f;
    const float   shadow_alpha = apply_properties ? overall_alpha * viewer->m_Properties.m_ShadowAlpha : 0.0f;
    const Vector4 outline_color(viewer->m_Properties.m_OutlineColor[0], viewer->m_Properties.m_OutlineColor[1], viewer->m_Properties.m_OutlineColor[2], outline_alpha);
    const Vector4 shadow_color(viewer->m_Properties.m_ShadowColor[0], viewer->m_Properties.m_ShadowColor[1], viewer->m_Properties.m_ShadowColor[2], shadow_alpha);
    for (uint32_t line_index = 0; line_index < TextLayoutGetLineCount(layout); ++line_index)
    {
        TextLine& line = lines[line_index];
        if (!line.m_Length)
            continue;
        float first_x = text_glyphs[line.m_Index].m_X;
        for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
        {
            first_x = dmMath::Min(first_x, text_glyphs[i].m_X);
        }
        const bool  right_to_left = paragraphs[line.m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL;
        const float line_x = paragraph_x + (right_to_left ? paragraph_width - line.m_Width : 0.0f);
        const float first_y = text_glyphs[line.m_Index].m_Y;
        const float line_y = paragraph_top - layout_height + line.m_Baseline;
        for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
        {
            TextGlyph& text_glyph = text_glyphs[i];

            if (dmUtf8::IsWhiteSpace(text_glyph.m_Codepoint) || (text_glyph.m_Flags & TEXT_GLYPH_FLAG_OBJECT))
            {
                continue;
            }

            CachedGlyph* glyph = FindGlyph(viewer, text_glyph.m_Font, text_glyph.m_GlyphIndex, font_size, apply_properties);

            if (!glyph)
            {
                continue;
            }

            TextGlyphRenderData glyph_render_data;
            TextLayoutGetGlyphRenderData(layout, text_glyph, base_face_color, &glyph_render_data);
            const bool glyph_has_markup_outline = (glyph_render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) != 0 && glyph_render_data.m_OutlineWidth > 0.0f;
            const float glyph_outline_alpha = glyph_has_markup_outline ? overall_alpha * viewer->m_Properties.m_OutlineAlpha * glyph_render_data.m_OutlineColor[3]
                                                                        : outline_color.getW() * glyph_render_data.m_OutlineColor[3];
            const bool glyph_has_markup_outline_color = (glyph_render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_COLOR) != 0;
            const Vector4 glyph_outline_color(glyph_has_markup_outline_color ? glyph_render_data.m_OutlineColor[0] : outline_color.getX(),
                                              glyph_has_markup_outline_color ? glyph_render_data.m_OutlineColor[1] : outline_color.getY(),
                                              glyph_has_markup_outline_color ? glyph_render_data.m_OutlineColor[2] : outline_color.getZ(),
                                              glyph_outline_alpha);
            float glyph_sdf_outline = sdf_outline;

            if (glyph_render_data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_WIDTH)
            {
                glyph_sdf_outline = (0.75f * 255.0f - (191.0f / padding) * glyph_render_data.m_OutlineWidth / text_glyph.m_RenderScale) / 255.0f;
            }

            const uint32_t shadow_flags = TEXT_RENDER_STYLE_SHADOW_COLOR | TEXT_RENDER_STYLE_SHADOW_X | TEXT_RENDER_STYLE_SHADOW_Y | TEXT_RENDER_STYLE_SHADOW_BLUR;
            const bool glyph_has_markup_shadow = (glyph_render_data.m_StyleFlags & shadow_flags) != 0;
            const float glyph_shadow_alpha = glyph_has_markup_shadow ? overall_alpha * viewer->m_Properties.m_ShadowAlpha * glyph_render_data.m_ShadowColor[3]
                                                                      : shadow_color.getW() * glyph_render_data.m_ShadowColor[3];
            const bool glyph_has_markup_shadow_color = (glyph_render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_COLOR) != 0;
            const Vector4 glyph_shadow_color(glyph_has_markup_shadow_color ? glyph_render_data.m_ShadowColor[0] : shadow_color.getX(),
                                             glyph_has_markup_shadow_color ? glyph_render_data.m_ShadowColor[1] : shadow_color.getY(),
                                             glyph_has_markup_shadow_color ? glyph_render_data.m_ShadowColor[2] : shadow_color.getZ(),
                                             glyph_shadow_alpha);
            float glyph_sdf_shadow = glyph_has_markup_shadow && shadow_blur <= 0.0f ? 1.0f : sdf_shadow;

            if (glyph_render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_BLUR)
            {
                const float requested_shadow_blur = glyph_render_data.m_ShadowBlur / text_glyph.m_RenderScale;

                if (requested_shadow_blur <= 0.0f)
                {
                    glyph_sdf_shadow = 1.0f;
                }
                else if (shadow_blur > 0.0f && requested_shadow_blur < shadow_blur)
                {
                    glyph_sdf_shadow = (0.75f * 255.0f - (191.0f / padding) * requested_shadow_blur) / 255.0f;
                }
            }

            const float glyph_shadow_x = glyph_render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_X ? glyph_render_data.m_ShadowX : viewer->m_Properties.m_ShadowX;
            const float glyph_shadow_y = glyph_render_data.m_StyleFlags & TEXT_RENDER_STYLE_SHADOW_Y ? glyph_render_data.m_ShadowY : viewer->m_Properties.m_ShadowY;
            const uint32_t layer_mask = apply_properties ? FONT_RENDER_LAYER_FACE | FONT_RENDER_LAYER_OUTLINE | FONT_RENDER_LAYER_SHADOW : FONT_RENDER_LAYER_FACE;
            uint32_t face_colors[4];
            FontPackGlyphFaceColors(glyph_render_data.m_FaceColors, face_colors);

            FontGlyphVertexParams glyph_params;
            glyph_params.m_Glyph = &glyph->m_Glyph;
            glyph_params.m_RecipAtlasWidth = 1.0f / ATLAS_WIDTH;
            glyph_params.m_RecipAtlasHeight = 1.0f / ATLAS_HEIGHT;
            glyph_params.m_X = line_x + text_glyph.m_X - first_x + glyph_render_data.m_OffsetX;
            glyph_params.m_Y = line_y + text_glyph.m_Y - first_y + glyph_render_data.m_OffsetY;
            glyph_params.m_RenderScale = text_glyph.m_RenderScale;
            glyph_params.m_CellX = glyph->m_X;
            glyph_params.m_CellY = glyph->m_Y;
            glyph_params.m_CacheCellMaxAscent = viewer->m_CellMaxAscent;
            glyph_params.m_CacheCellPadding = CELL_PADDING;
            glyph_params.m_MetricsFromTtf = true;

            FontVertexLayerParams layers;
            layers.m_Transform = &transform;
            layers.m_FaceColors = face_colors;
            layers.m_FaceVertices = viewer->m_Vertices.Begin() + *vertex_index + layer_stride * (layer_count - 1);
            layers.m_OutlineVertices = (layer_mask & FONT_RENDER_LAYER_OUTLINE) ? viewer->m_Vertices.Begin() + *vertex_index + layer_stride * (layer_count - 2) : 0;
            layers.m_ShadowVertices = (layer_mask & FONT_RENDER_LAYER_SHADOW) ? viewer->m_Vertices.Begin() + *vertex_index : 0;
            layers.m_OutlineColor = FontPackColor(glyph_outline_color);
            layers.m_ShadowColor = FontPackColor(glyph_shadow_color);
            layers.m_SdfEdge = sdf_face;
            layers.m_SdfOutline = glyph_sdf_outline;
            layers.m_SdfSmoothing = sdf_smoothing / text_glyph.m_RenderScale;
            layers.m_SdfShadow = glyph_sdf_shadow;
            layers.m_ShadowX = glyph_shadow_x;
            layers.m_ShadowY = glyph_shadow_y;
            layers.m_LayerCount = layer_count;
            FontPackGlyphVertices(glyph_params, layers);
            for (uint32_t layer = 0; layer < layer_count; ++layer)
                ClipPackedQuad(viewer, *vertex_index + layer * layer_stride, clip_box);
            *vertex_index += 6;
        }
    }
}

static void PackLayoutDecorations(Viewer* viewer, HTextLayout layout, float paragraph_x, float paragraph_top, float paragraph_width, const FontViewerNuklearBox& clip_box)
{
    const uint32_t decoration_count = TextLayoutGetDecorationCount(layout);

    if (decoration_count == 0)
    {
        return;
    }

    TextGlyph*            glyphs = TextLayoutGetGlyphs(layout);
    TextLine*             lines = TextLayoutGetLines(layout);
    TextParagraph*        paragraphs = TextLayoutGetParagraphs(layout);
    const TextDecoration* decorations = TextLayoutGetDecorations(layout);
    const uint32_t        glyph_count = TextLayoutGetGlyphCount(layout);
    float layout_width;
    float layout_height;
    TextLayoutGetBounds(layout, &layout_width, &layout_height);
    (void)layout_width;

    Matrix4 transform = Matrix4::orthographic(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT, -1.0f, 1.0f);
    const float   pivot_x = viewer->m_NuklearLayout.m_ContentWidth * 0.5f;
    const float   pivot_y = WINDOW_HEIGHT * 0.5f;
    const Matrix4 zoom_transform = Matrix4::translation(Vector3(pivot_x + viewer->m_PanX, pivot_y + viewer->m_PanY, 0.0f)) *
        Matrix4::scale(Vector3(viewer->m_Zoom, viewer->m_Zoom, 1.0f)) *
        Matrix4::translation(Vector3(-pivot_x, -pivot_y, 0.0f));
    transform = transform * zoom_transform;

    const float base_face_color[4] = { viewer->m_Properties.m_FaceColor[0], viewer->m_Properties.m_FaceColor[1], viewer->m_Properties.m_FaceColor[2], viewer->m_Properties.m_Alpha };

    for (uint32_t decoration_index = 0; decoration_index < decoration_count; ++decoration_index)
    {
        const TextDecoration& decoration = decorations[decoration_index];

        if (decoration.m_LineIndex >= TextLayoutGetLineCount(layout) || decoration.m_GlyphCount == 0 || decoration.m_GlyphStart + decoration.m_GlyphCount > glyph_count)
        {
            continue;
        }

        const TextLine& line = lines[decoration.m_LineIndex];

        if (line.m_Length == 0)
        {
            continue;
        }

        float first_x = glyphs[line.m_Index].m_X;

        for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
        {
            first_x = dmMath::Min(first_x, glyphs[i].m_X);
        }

        const bool  right_to_left = paragraphs[line.m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL;
        const float line_x = paragraph_x + (right_to_left ? paragraph_width - line.m_Width : 0.0f);
        const float line_y = paragraph_top - layout_height + line.m_Baseline;
        const bool glyph_segments = FontDecorationRequiresGlyphSegments(layout, decoration);
        const uint32_t segment_count = glyph_segments ? decoration.m_GlyphCount : 1;
        const float segment_length = decoration.m_Length / segment_count;
        TextGlyphFaceColors decoration_colors;

        if (!glyph_segments)
        {
            FontGetDecorationFaceColors(layout, decoration, base_face_color, &decoration_colors);
        }

        for (uint32_t segment = 0; segment < segment_count; ++segment)
        {
            TextGlyphFaceColors segment_colors;

            if (glyph_segments)
            {
                TextGlyphRenderData render_data;
                TextLayoutGetGlyphRenderData(layout, glyphs[decoration.m_GlyphStart + segment], base_face_color, &render_data);
                segment_colors = render_data.m_FaceColors;
            }
            else
            {
                segment_colors = decoration_colors;
            }

            const float piece_x0 = line_x + decoration.m_X - first_x + segment_length * segment;
            const float piece_x1 = piece_x0 + segment_length;
            const float piece_y = line_y + decoration.m_Y;
            FontDecorationPattern pattern;
            FontGetDecorationPattern(decoration, segment, segment_count, &pattern);
            PushTransformedClippedGradientRectangle(viewer, transform, clip_box, piece_x0, piece_y - decoration.m_Thickness * 0.5f,
                                                    piece_x1 - piece_x0, decoration.m_Thickness, segment_colors,
                                                    pattern.m_Start, pattern.m_End, pattern.m_Duty);
        }
    }
}

static bool GetLayoutObjectPosition(HTextLayout layout, const TextLayoutObject& object, float paragraph_x, float paragraph_top, float paragraph_width, float* x, float* y)
{
    return TextLayoutGetObjectPosition(layout, &object, paragraph_x, paragraph_top, paragraph_width, x, y) != 0;
}

static void PushSpriteVertex(Viewer* viewer, const Matrix4& transform, float x, float y, float u, float v)
{
    const Vector4 position = transform * Vector4(x, y, 0.0f, 1.0f);
    SpriteVertex  vertex = { { position.getX(), position.getY(), position.getZ(), position.getW() }, { u, v } };

    if (viewer->m_SpriteVertices.Full())
    {
        viewer->m_SpriteVertices.OffsetCapacity(24);
    }

    viewer->m_SpriteVertices.Push(vertex);
}

static void PackLayoutObjects(Viewer* viewer, HTextLayout layout, float paragraph_x, float paragraph_top, float paragraph_width, bool apply_properties)
{
    Matrix4 transform = Matrix4::orthographic(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT, -1.0f, 1.0f);

    if (apply_properties)
    {
        const float   pivot_x = viewer->m_NuklearLayout.m_ContentWidth * 0.5f;
        const float   pivot_y = WINDOW_HEIGHT * 0.5f;
        const Matrix4 zoom_transform = Matrix4::translation(Vector3(pivot_x + viewer->m_PanX, pivot_y + viewer->m_PanY, 0.0f)) *
        Matrix4::scale(Vector3(viewer->m_Zoom, viewer->m_Zoom, 1.0f)) *
        Matrix4::translation(Vector3(-pivot_x, -pivot_y, 0.0f));
        transform = transform * zoom_transform;
    }

    const TextLayoutObject* objects = TextLayoutGetObjects(layout);

    for (uint32_t i = 0; i < TextLayoutGetObjectCount(layout); ++i)
    {
        const TextLayoutObject& object = objects[i];
        const ViewerImage*      resource = (const ViewerImage*)object.m_Resource;
        float                   x;
        float                   y;

        if (object.m_Tag != TAG_SPRITE || !resource || !GetLayoutObjectPosition(layout, object, paragraph_x, paragraph_top, paragraph_width, &x, &y))
        {
            continue;
        }

        SpriteDraw draw = { resource->m_Texture, viewer->m_SpriteVertices.Size() };

        if (viewer->m_SpriteDraws.Full())
        {
            viewer->m_SpriteDraws.OffsetCapacity(8);
        }

        viewer->m_SpriteDraws.Push(draw);
        const float x1 = x + object.m_Width;
        const float y1 = y + object.m_Height;
        PushSpriteVertex(viewer, transform, x, y, 0.0f, 0.0f);
        PushSpriteVertex(viewer, transform, x1, y, 1.0f, 0.0f);
        PushSpriteVertex(viewer, transform, x1, y1, 1.0f, 1.0f);
        PushSpriteVertex(viewer, transform, x, y, 0.0f, 0.0f);
        PushSpriteVertex(viewer, transform, x1, y1, 1.0f, 1.0f);
        PushSpriteVertex(viewer, transform, x, y1, 0.0f, 1.0f);
    }
}

static uint32_t HitTestPreviewLink(Viewer* viewer, float mouse_x, float mouse_y)
{
    if (viewer->m_Layouts.Empty() || mouse_x < 0.0f || mouse_x >= viewer->m_NuklearLayout.m_ContentWidth)
    {
        return 0xffffffff;
    }

    const float             pivot_x = viewer->m_NuklearLayout.m_ContentWidth * 0.5f;
    const float             pivot_y = WINDOW_HEIGHT * 0.5f;
    const float             layout_x = pivot_x + (mouse_x - pivot_x - viewer->m_PanX) / viewer->m_Zoom;
    const float             render_y = WINDOW_HEIGHT - mouse_y;
    const float             layout_y = pivot_y + (render_y - pivot_y - viewer->m_PanY) / viewer->m_Zoom;

    HTextLayout             layout = viewer->m_Layouts[0];
    const TextLayoutObject* objects = TextLayoutGetObjects(layout);
    TextGlyph*              glyphs = TextLayoutGetGlyphs(layout);
    TextLine*               lines = TextLayoutGetLines(layout);
    TextParagraph*          paragraphs = TextLayoutGetParagraphs(layout);
    float                   layout_width;
    float                   layout_height;
    TextLayoutGetBounds(layout, &layout_width, &layout_height);
    (void)layout_width;

    for (uint32_t object_index = 0; object_index < TextLayoutGetObjectCount(layout); ++object_index)
    {
        const TextLayoutObject& object = objects[object_index];

        if (object.m_Tag != TAG_LINK || object.m_TextLength == 0)
        {
            continue;
        }

        const uint32_t link_end = object.m_TextOffset + object.m_TextLength;

        for (uint32_t line_index = 0; line_index < TextLayoutGetLineCount(layout); ++line_index)
        {
            const TextLine& line = lines[line_index];

            if (line.m_Length == 0)
            {
                continue;
            }

            float first_x = glyphs[line.m_Index].m_X;

            for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
            {
                first_x = dmMath::Min(first_x, glyphs[i].m_X);
            }

            const TextParagraph& paragraph = paragraphs[line.m_ParagraphIndex];
            const bool           right_to_left = paragraph.m_Direction == TEXT_DIRECTION_RTL;
            const float          line_x = viewer->m_LayoutXs[0] + (right_to_left ? viewer->m_LayoutWidths[0] - line.m_Width : 0.0f);
            const float          line_y = viewer->m_LayoutTops[0] - layout_height + line.m_Baseline;

            for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
            {
                const TextGlyph& glyph = glyphs[i];

                if (glyph.m_Cluster < object.m_TextOffset || glyph.m_Cluster >= link_end)
                {
                    continue;
                }

                const float         glyph_size = viewer->m_LayoutSizes[0] * glyph.m_RenderScale;
                const float         scale = FontGetScaleFromSize(glyph.m_Font, glyph_size);
                float               ascent = FontGetAscent(glyph.m_Font, scale);
                float               descent = fabsf(FontGetDescent(glyph.m_Font, scale));
                const float         white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                TextGlyphRenderData render_data;
                TextLayoutGetGlyphRenderData(layout, glyph, white, &render_data);
                float        x = line_x + glyph.m_X - first_x + render_data.m_OffsetX;
                float        width = dmMath::Max(glyph.m_Width, glyph_size * 0.35f);
                CachedGlyph* cached_glyph = FindGlyph(viewer, glyph.m_Font, glyph.m_GlyphIndex, viewer->m_LayoutSizes[0], true);

                if (cached_glyph)
                {
                    const FontGlyph& font_glyph = cached_glyph->m_Glyph;
                    const float      source_width = font_glyph.m_Bitmap.m_Width;
                    const float      source_size_difference = source_width - font_glyph.m_Width;
                    x += (font_glyph.m_LeftBearing - source_size_difference * 0.5f) * glyph.m_RenderScale;
                    width = source_width * glyph.m_RenderScale;
                    ascent = font_glyph.m_Ascent * glyph.m_RenderScale;
                    descent = font_glyph.m_Descent * glyph.m_RenderScale;
                }

                const float y = line_y + render_data.m_OffsetY;

                if (layout_x >= x && layout_x <= x + width && layout_y >= y - descent && layout_y <= y + ascent)
                {
                    return object_index;
                }
            }
        }
    }

    return 0xffffffff;
}

static bool SetPreviewLinkStyle(Viewer* viewer, uint32_t object_index, dmhash_t style)
{
    if (viewer->m_Layouts.Empty())
    {
        return false;
    }

    HTextLayout             layout = viewer->m_Layouts[0];
    const TextLayoutObject* objects = TextLayoutGetObjects(layout);

    if (object_index >= TextLayoutGetObjectCount(layout) || objects[object_index].m_Tag != TAG_LINK)
    {
        return false;
    }

    return TextLayoutSetObjectStyle(layout, objects[object_index].m_Id, style) != 0;
}

static bool IsSupportedBrowserURL(const char* url, uint32_t length)
{
    const bool http = length > 7 && memcmp(url, "http://", 7) == 0;
    const bool https = length > 8 && memcmp(url, "https://", 8) == 0;

    if (!http && !https)
    {
        return false;
    }

    for (uint32_t i = 0; i < length; ++i)
    {
        const uint8_t c = (uint8_t)url[i];

        if (c <= 0x20 || c == 0x7f || c == '"' || c == '\'' || c == '\\' || c == '`')
        {
            return false;
        }
#if defined(DM_PLATFORM_LINUX)
        // dmSys::OpenURL currently passes Linux URLs through a shell command.

        if (strchr("$&;|<>*?()[]{}!#", c))
        {
            return false;
        }
#endif
    }

    return true;
}

static void OpenPreviewLink(Viewer* viewer, uint32_t object_index)
{
    if (viewer->m_Layouts.Empty())
    {
        return;
    }

    HTextLayout                      layout = viewer->m_Layouts[0];
    const TextLayoutObject*          objects = TextLayoutGetObjects(layout);
    const TextLayoutObjectAttribute* attributes = TextLayoutGetObjectAttributes(layout);
    const char*                      source = TextLayoutGetObjectSource(layout);

    if (object_index >= TextLayoutGetObjectCount(layout) || objects[object_index].m_Tag != TAG_LINK)
    {
        return;
    }

    const TextLayoutObjectAttribute* source_attribute = FindLayoutObjectAttribute(source, attributes, &objects[object_index], "src");

    if (!source_attribute || !IsSupportedBrowserURL(source + source_attribute->m_ValueOffset, source_attribute->m_ValueLength))
    {
        dmLogWarning("Ignoring rich-text link without a supported, safe http(s) src");

        return;
    }

    dmArray<char> url;
    url.SetCapacity(source_attribute->m_ValueLength + 1);
    url.SetSize(source_attribute->m_ValueLength + 1);
    memcpy(url.Begin(), source + source_attribute->m_ValueOffset, source_attribute->m_ValueLength);
    url[source_attribute->m_ValueLength] = 0;

    if (dmSys::OpenURL(url.Begin(), 0) != dmSys::RESULT_OK)
    {
        dmLogWarning("Unable to open rich-text link '%s'", url.Begin());
    }
}

static void UpdateLoadedFontText(Viewer* viewer)
{
    uint32_t text_size = 1;
    viewer->m_LoadedFontDataSize = 0;

    for (uint32_t i = 0; i < viewer->m_Fonts.Size(); ++i)
    {
        text_size += (uint32_t)strlen(FontGetPath(viewer->m_Fonts[i])) + (i ? 1 : 0);
        viewer->m_LoadedFontDataSize += FontGetResourceSize(viewer->m_Fonts[i]);
    }

    viewer->m_LoadedFontText.SetCapacity(text_size);
    viewer->m_LoadedFontText.SetSize(text_size);
    char* cursor = viewer->m_LoadedFontText.Begin();

    for (uint32_t i = 0; i < viewer->m_Fonts.Size(); ++i)
    {
        if (i)
        {
            *cursor++ = '\n';
        }

        const char* path = FontGetPath(viewer->m_Fonts[i]);
        uint32_t path_length = (uint32_t)strlen(path);
        memcpy(cursor, path, path_length);
        cursor += path_length;
    }

    *cursor = 0;
}

static bool LoadFonts(Viewer* viewer)
{
    viewer->m_Collection = FontCollectionCreate();

    if (!viewer->m_Collection)
    {
        return false;
    }

    if (viewer->m_FontPaths.Empty())
    {
        HFont english_font = FontLoadFromMemory("NotoSans-Regular.ttf", FONTVIEWER_NOTO_SANS, FONTVIEWER_NOTO_SANS_SIZE, true);
        HFont arabic_font = FontLoadFromMemory("NotoSansArabic-Regular.ttf", FONTVIEWER_NOTO_SANS_ARABIC, FONTVIEWER_NOTO_SANS_ARABIC_SIZE, true);

        if (viewer->m_Fonts.Capacity() < 2)
        {
            viewer->m_Fonts.SetCapacity(2);
        }

        viewer->m_Fonts.Push(english_font);
        viewer->m_Fonts.Push(arabic_font);
    }
    else
    {
        viewer->m_Fonts.SetCapacity(viewer->m_FontPaths.Size());

        for (uint32_t i = 0; i < viewer->m_FontPaths.Size(); ++i)
        {
            viewer->m_Fonts.Push(FontLoadFromPath(viewer->m_FontPaths[i]));
        }
    }

    for (uint32_t i = 0; i < viewer->m_Fonts.Size(); ++i)
    {
        if (!viewer->m_Fonts[i] || FontCollectionAddFont(viewer->m_Collection, viewer->m_Fonts[i]) != FONT_RESULT_OK)
        {
            dmLogError("Unable to load font %u", i + 1);

            return false;
        }
    }

    UpdateLoadedFontText(viewer);

    return true;
}

static void ClearGeneratedFontData(Viewer* viewer)
{
    for (uint32_t i = 0; i < viewer->m_Glyphs.Size(); ++i)
    {
        FontFreeGlyph(viewer->m_Glyphs[i].m_Font, &viewer->m_Glyphs[i].m_Glyph);
    }

    for (uint32_t i = 0; i < viewer->m_Layouts.Size(); ++i)
    {
        TextLayoutRelease(viewer->m_Layouts[i]);
    }

    viewer->m_Glyphs.SetSize(0);
    viewer->m_Layouts.SetSize(0);
    viewer->m_LayoutTops.SetSize(0);
    viewer->m_LayoutXs.SetSize(0);
    viewer->m_LayoutSizes.SetSize(0);
    viewer->m_LayoutWidths.SetSize(0);
    viewer->m_LayoutClips.SetSize(0);
    viewer->m_LayoutBold.SetSize(0);
    viewer->m_LayoutLayerCounts.SetSize(0);
    viewer->m_LayoutTextOffsets.SetSize(0);
    viewer->m_Atlas.SetSize(0);
    viewer->m_Vertices.SetSize(0);
    viewer->m_ColorVertices.SetSize(0);
    viewer->m_SpriteVertices.SetSize(0);
    viewer->m_SpriteDraws.SetSize(0);
    viewer->m_CellWidth = 1;
    viewer->m_CellHeight = 1;
    viewer->m_CellMaxAscent = 0;
    viewer->m_VertexCount = 0;
    viewer->m_ColorBackgroundVertexCount = 0;
    viewer->m_ColorDecorationVertexCount = 0;
    viewer->m_ColorDebugVertexCount = 0;
    viewer->m_HoveredLinkObject = 0xffffffff;
    viewer->m_PressedLinkObject = 0xffffffff;
}

static void PushColorVertex(Viewer* viewer, float x, float y, float red, float green, float blue, float alpha)
{
    if (viewer->m_ColorVertices.Full())
    {
        viewer->m_ColorVertices.OffsetCapacity(128);
    }

    ColorVertex vertex = { { 2.0f * x / WINDOW_WIDTH - 1.0f, 2.0f * y / WINDOW_HEIGHT - 1.0f, 0.0f, 1.0f },
                           { red, green, blue, alpha },
                           { 0.0f, 0.0f } };
    viewer->m_ColorVertices.Push(vertex);
}

static void PushDecorationVertex(Viewer* viewer, float x, float y, const float color[4], float pattern, float pattern_duty)
{
    if (viewer->m_ColorVertices.Full())
    {
        viewer->m_ColorVertices.OffsetCapacity(128);
    }

    ColorVertex vertex = { { 2.0f * x / WINDOW_WIDTH - 1.0f, 2.0f * y / WINDOW_HEIGHT - 1.0f, 0.0f, 1.0f },
                           { color[0], color[1], color[2], color[3] },
                           { pattern, pattern_duty } };
    viewer->m_ColorVertices.Push(vertex);
}

static void PushColorRectangle(Viewer* viewer, float x, float y, float width, float height, float red, float green, float blue, float alpha)
{
    const float x1 = x + width;
    const float y1 = y + height;
    PushColorVertex(viewer, x, y, red, green, blue, alpha);
    PushColorVertex(viewer, x1, y, red, green, blue, alpha);
    PushColorVertex(viewer, x1, y1, red, green, blue, alpha);
    PushColorVertex(viewer, x, y, red, green, blue, alpha);
    PushColorVertex(viewer, x1, y1, red, green, blue, alpha);
    PushColorVertex(viewer, x, y1, red, green, blue, alpha);
}

static void InterpolateColor(const float* bottom_left, const float* bottom_right, const float* top_left, const float* top_right, float x, float y, float* color)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        const float bottom = bottom_left[i] + (bottom_right[i] - bottom_left[i]) * x;
        const float top = top_left[i] + (top_right[i] - top_left[i]) * x;
        color[i] = bottom + (top - bottom) * y;
    }
}

static void PushTransformedClippedGradientRectangle(Viewer* viewer, const Matrix4& transform, const FontViewerNuklearBox& clip,
                                                     float x, float y, float width, float height, const TextGlyphFaceColors& colors,
                                                     float pattern_start, float pattern_end, float pattern_duty)
{
    const Vector4 p0 = transform * Vector4(x, y, 0.0f, 1.0f);
    const Vector4 p1 = transform * Vector4(x + width, y + height, 0.0f, 1.0f);
    const float min_x = (p0.getX() + 1.0f) * WINDOW_WIDTH * 0.5f;
    const float min_y = (p0.getY() + 1.0f) * WINDOW_HEIGHT * 0.5f;
    const float max_x = (p1.getX() + 1.0f) * WINDOW_WIDTH * 0.5f;
    const float max_y = (p1.getY() + 1.0f) * WINDOW_HEIGHT * 0.5f;
    const float clipped_x0 = dmMath::Max(min_x, clip.m_X);
    const float clipped_y0 = dmMath::Max(min_y, clip.m_Y);
    const float clipped_x1 = dmMath::Min(max_x, clip.m_X + clip.m_Width);
    const float clipped_y1 = dmMath::Min(max_y, clip.m_Y + clip.m_Height);

    if (clipped_x1 <= clipped_x0 || clipped_y1 <= clipped_y0)
    {
        return;
    }

    const float tx0 = (clipped_x0 - min_x) / (max_x - min_x);
    const float tx1 = (clipped_x1 - min_x) / (max_x - min_x);
    const float ty0 = (clipped_y0 - min_y) / (max_y - min_y);
    const float ty1 = (clipped_y1 - min_y) / (max_y - min_y);
    float bottom_left[4];
    float bottom_right[4];
    float top_left[4];
    float top_right[4];
    InterpolateColor(colors.m_BottomLeft, colors.m_BottomRight, colors.m_TopLeft, colors.m_TopRight, tx0, ty0, bottom_left);
    InterpolateColor(colors.m_BottomLeft, colors.m_BottomRight, colors.m_TopLeft, colors.m_TopRight, tx1, ty0, bottom_right);
    InterpolateColor(colors.m_BottomLeft, colors.m_BottomRight, colors.m_TopLeft, colors.m_TopRight, tx0, ty1, top_left);
    InterpolateColor(colors.m_BottomLeft, colors.m_BottomRight, colors.m_TopLeft, colors.m_TopRight, tx1, ty1, top_right);

    const float clipped_pattern_start = pattern_start + (pattern_end - pattern_start) * tx0;
    const float clipped_pattern_end = pattern_start + (pattern_end - pattern_start) * tx1;
    PushDecorationVertex(viewer, clipped_x0, clipped_y0, bottom_left, clipped_pattern_start, pattern_duty);
    PushDecorationVertex(viewer, clipped_x1, clipped_y0, bottom_right, clipped_pattern_end, pattern_duty);
    PushDecorationVertex(viewer, clipped_x0, clipped_y1, top_left, clipped_pattern_start, pattern_duty);
    PushDecorationVertex(viewer, clipped_x0, clipped_y1, top_left, clipped_pattern_start, pattern_duty);
    PushDecorationVertex(viewer, clipped_x1, clipped_y0, bottom_right, clipped_pattern_end, pattern_duty);
    PushDecorationVertex(viewer, clipped_x1, clipped_y1, top_right, clipped_pattern_end, pattern_duty);
}

static void PushColorOutline(Viewer* viewer, float x, float y, float width, float height, float red, float green, float blue, float alpha)
{
    PushColorRectangle(viewer, x, y, width, 1.0f, red, green, blue, alpha);
    PushColorRectangle(viewer, x, y + height - 1.0f, width, 1.0f, red, green, blue, alpha);
    PushColorRectangle(viewer, x, y, 1.0f, height, red, green, blue, alpha);
    PushColorRectangle(viewer, x + width - 1.0f, y, 1.0f, height, red, green, blue, alpha);
}

static void PushClippedColorRectangle(Viewer* viewer, const FontViewerNuklearBox& clip, float x, float y, float width, float height, float red, float green, float blue, float alpha)
{
    const float clipped_x = dmMath::Max(x, clip.m_X);
    const float clipped_y = dmMath::Max(y, clip.m_Y);
    const float clipped_x1 = dmMath::Min(x + width, clip.m_X + clip.m_Width);
    const float clipped_y1 = dmMath::Min(y + height, clip.m_Y + clip.m_Height);
    if (clipped_x1 > clipped_x && clipped_y1 > clipped_y)
        PushColorRectangle(viewer, clipped_x, clipped_y, clipped_x1 - clipped_x, clipped_y1 - clipped_y, red, green, blue, alpha);
}

static bool PushLayout(Viewer* viewer, const char* text, uint32_t text_length, uint32_t text_offset, float x, float top, float font_size, float width, bool line_break, const FontViewerNuklearBox& clip_box, bool bold, bool apply_properties, bool legacy_layout, bool markup)
{
    HTextLayout layout = 0;

    if (!CreateLayout(viewer, text, text_length, font_size, width, line_break, legacy_layout, markup, &layout))
    {
        return false;
    }

    if (viewer->m_ExportMode && apply_properties)
    {
        TextLayout* internal = (TextLayout*)layout;

        for (uint32_t i = 0; i < internal->m_Styles.Size(); ++i)
        {
            const TextRenderStyle& style = internal->m_Styles[i];

            if ((style.m_Flags & TEXT_RENDER_STYLE_SHADOW_BLUR) && style.m_ShadowBlur > viewer->m_Properties.m_ShadowBlur)
            {
                viewer->m_Properties.m_ShadowBlur = style.m_ShadowBlur;
                viewer->m_AtlasChannels = 3;
            }
        }
    }
    if (viewer->m_Layouts.Full())
    {
        viewer->m_Layouts.OffsetCapacity(16);
        viewer->m_LayoutTops.OffsetCapacity(16);
        viewer->m_LayoutXs.OffsetCapacity(16);
        viewer->m_LayoutSizes.OffsetCapacity(16);
        viewer->m_LayoutWidths.OffsetCapacity(16);
        viewer->m_LayoutClips.OffsetCapacity(16);
        viewer->m_LayoutBold.OffsetCapacity(16);
        viewer->m_LayoutLayerCounts.OffsetCapacity(16);
        viewer->m_LayoutTextOffsets.OffsetCapacity(16);
    }
    viewer->m_Layouts.Push(layout);
    viewer->m_LayoutTops.Push(top);
    viewer->m_LayoutXs.Push(x);
    viewer->m_LayoutSizes.Push(font_size);
    viewer->m_LayoutWidths.Push(width);
    viewer->m_LayoutClips.Push(clip_box);
    viewer->m_LayoutBold.Push(bold ? 1 : 0);
    const uint8_t layer_count = apply_properties ? 3 : 1;
    viewer->m_LayoutLayerCounts.Push(layer_count);
    viewer->m_LayoutTextOffsets.Push(text_offset);
    viewer->m_VertexCount += CountVisibleGlyphs(layout) * 6 * layer_count;
    return AddLayoutGlyphs(viewer, layout, font_size, apply_properties);
}

static bool IsEditorLayout(const Viewer* viewer, uint32_t layout_index)
{
    return layout_index > 0 && viewer->m_LayoutClips[layout_index].m_Width < WINDOW_WIDTH;
}

static uint32_t GetLineStartCluster(TextGlyph* glyphs, const TextLine& line)
{
    uint32_t cluster = glyphs[line.m_Index].m_Cluster;
    for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
        cluster = dmMath::Min(cluster, (uint32_t)glyphs[i].m_Cluster);
    return cluster;
}

static uint32_t GetLineEndCluster(TextGlyph* glyphs, TextLine* lines, uint32_t line_index, uint32_t line_count, uint32_t codepoint_count)
{
    const uint32_t next_line = line_index + 1;
    for (uint32_t i = next_line; i < line_count; ++i)
    {
        if (lines[i].m_Length)
        {
            const uint32_t next_line_start = GetLineStartCluster(glyphs, lines[i]);
            const uint32_t empty_line_count = i - next_line;
            return next_line_start >= empty_line_count ? next_line_start - empty_line_count : 0;
        }
    }
    return codepoint_count;
}

static uint32_t GetGlyphEndCluster(TextGlyph* glyphs, const TextLine& line, uint32_t cluster, uint32_t line_end)
{
    uint32_t cluster_end = line_end;
    for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
    {
        const uint32_t candidate = glyphs[i].m_Cluster;
        if (candidate > cluster)
            cluster_end = dmMath::Min(cluster_end, candidate);
    }
    return cluster_end;
}

static uint32_t GetEmptyLineCluster(TextGlyph* glyphs, TextLine* lines, uint32_t line_index, uint32_t line_count, uint32_t codepoint_count)
{
    for (uint32_t i = line_index + 1; i < line_count; ++i)
    {
        if (lines[i].m_Length)
        {
            const uint32_t next_line_start = GetLineStartCluster(glyphs, lines[i]);
            const uint32_t empty_line_count = i - line_index;
            return next_line_start >= empty_line_count ? next_line_start - empty_line_count : 0;
        }
    }
    return codepoint_count;
}

static uint32_t HitTestEditorText(Viewer* viewer, float mouse_x, float mouse_y)
{
    uint32_t layout_index = 0;
    float    closest_layout_distance = FLT_MAX;
    for (uint32_t i = 1; i < viewer->m_Layouts.Size(); ++i)
    {
        if (!IsEditorLayout(viewer, i) || !TextLayoutGetLineCount(viewer->m_Layouts[i]))
            continue;
        TextGlyph*  layout_glyphs = TextLayoutGetGlyphs(viewer->m_Layouts[i]);
        HFont       layout_font = TextLayoutGetGlyphCount(viewer->m_Layouts[i]) ? layout_glyphs[0].m_Font : viewer->m_Fonts[0];
        const float layout_scale = FontGetScaleFromSize(layout_font, viewer->m_LayoutSizes[i]);
        const float layout_line_height = (FontGetAscent(layout_font, layout_scale) + fabsf(FontGetDescent(layout_font, layout_scale))) * 1.2f;
        const float layout_top = WINDOW_HEIGHT - viewer->m_LayoutTops[i];
        const float layout_bottom = layout_top + TextLayoutGetLineCount(viewer->m_Layouts[i]) * layout_line_height;
        const float distance = mouse_y < layout_top ? layout_top - mouse_y : (mouse_y > layout_bottom ? mouse_y - layout_bottom : 0.0f);
        if (distance < closest_layout_distance)
        {
            layout_index = i;
            closest_layout_distance = distance;
        }
    }
    if (!layout_index)
        return EditorCodepointCount(viewer);
    HTextLayout layout = viewer->m_Layouts[layout_index];
    TextGlyph*     glyphs = TextLayoutGetGlyphs(layout);
    TextLine*      lines = TextLayoutGetLines(layout);
    TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
    if (!TextLayoutGetLineCount(layout))
        return 0;
    HFont       font = TextLayoutGetGlyphCount(layout) ? glyphs[0].m_Font : viewer->m_Fonts[0];
    const float scale = FontGetScaleFromSize(font, viewer->m_LayoutSizes[layout_index]);
    const float ascent = FontGetAscent(font, scale);
    const float descent = fabsf(FontGetDescent(font, scale));
    const float line_height = (ascent + descent) * 1.2f;
    const float text_top = WINDOW_HEIGHT - viewer->m_LayoutTops[layout_index];
    uint32_t    line_index = (uint32_t)dmMath::Max(0.0f, floorf((mouse_y - text_top) / line_height));
    line_index = dmMath::Min(line_index, TextLayoutGetLineCount(layout) - 1);
    TextLine& line = lines[line_index];
    if (!line.m_Length)
    {
        const uint32_t text_offset = viewer->m_LayoutTextOffsets[layout_index];
        return text_offset + GetEmptyLineCluster(glyphs, lines, line_index, TextLayoutGetLineCount(layout), EditorCodepointCount(viewer) - text_offset);
    }
    float first_x = glyphs[line.m_Index].m_X;
    for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
    {
        first_x = dmMath::Min(first_x, glyphs[i].m_X);
    }
    const bool  right_to_left = paragraphs[line.m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL;
    const float line_x = viewer->m_LayoutXs[layout_index] + (right_to_left ? viewer->m_LayoutWidths[layout_index] - line.m_Width : 0.0f);
    const uint32_t text_offset = viewer->m_LayoutTextOffsets[layout_index];
    const uint32_t line_end = GetLineEndCluster(glyphs, lines, line_index, TextLayoutGetLineCount(layout), EditorCodepointCount(viewer) - text_offset);
    uint32_t       closest_caret = GetLineStartCluster(glyphs, line);
    float          closest_distance = FLT_MAX;
    for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
    {
        const uint32_t cluster = glyphs[i].m_Cluster;
        const uint32_t cluster_end = GetGlyphEndCluster(glyphs, line, cluster, line_end);
        const float width = dmMath::Max(glyphs[i].m_Width, viewer->m_LayoutSizes[layout_index] * 0.35f);
        const float x = line_x + glyphs[i].m_X - first_x;
        const float cluster_x = right_to_left ? x + width : x;
        const float cluster_end_x = right_to_left ? x : x + width;
        const float cluster_distance = fabsf(mouse_x - cluster_x);
        if (cluster_distance < closest_distance)
        {
            closest_caret = cluster;
            closest_distance = cluster_distance;
        }
        const float cluster_end_distance = fabsf(mouse_x - cluster_end_x);
        if (cluster_end_distance < closest_distance)
        {
            closest_caret = cluster_end;
            closest_distance = cluster_end_distance;
        }
    }
    return dmMath::Min(text_offset + closest_caret, EditorCodepointCount(viewer));
}

static bool GetEditorCaretScreenPosition(Viewer* viewer, float* caret_x, float* caret_y, float* line_height)
{
    for (uint32_t layout_index = 1; layout_index < viewer->m_Layouts.Size(); ++layout_index)
    {
        if (!IsEditorLayout(viewer, layout_index))
            continue;
        HTextLayout layout = viewer->m_Layouts[layout_index];
        TextGlyph*     glyphs = TextLayoutGetGlyphs(layout);
        TextLine*      lines = TextLayoutGetLines(layout);
        TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
        const uint32_t line_count = TextLayoutGetLineCount(layout);
        if (!line_count || !TextLayoutGetGlyphCount(layout))
            continue;
        HFont       font = glyphs[0].m_Font;
        const float scale = FontGetScaleFromSize(font, viewer->m_LayoutSizes[layout_index]);
        const float ascent = FontGetAscent(font, scale);
        const float descent = fabsf(FontGetDescent(font, scale));
        *line_height = (ascent + descent) * 1.2f;
        for (uint32_t line_index = 0; line_index < line_count; ++line_index)
        {
            TextLine& line = lines[line_index];
            if (!line.m_Length)
            {
                const uint32_t text_offset = viewer->m_LayoutTextOffsets[layout_index];
                const uint32_t empty_line_cluster = text_offset + GetEmptyLineCluster(glyphs, lines, line_index, line_count, EditorCodepointCount(viewer) - text_offset);
                if (viewer->m_Caret != empty_line_cluster)
                    continue;
                *caret_x = viewer->m_LayoutXs[layout_index];
                const float text_top = WINDOW_HEIGHT - viewer->m_LayoutTops[layout_index];
                *caret_y = text_top + (line_index + 0.5f) * *line_height;
                return true;
            }
            const uint32_t text_offset = viewer->m_LayoutTextOffsets[layout_index];
            const uint32_t line_start = text_offset + GetLineStartCluster(glyphs, line);
            const uint32_t line_end = text_offset + GetLineEndCluster(glyphs, lines, line_index, line_count, EditorCodepointCount(viewer) - text_offset);
            if (viewer->m_Caret < line_start || viewer->m_Caret > line_end)
                continue;
            if (viewer->m_Caret == line_end && line_index + 1 < line_count)
                continue;
            float first_x = glyphs[line.m_Index].m_X;
            for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
            {
                first_x = dmMath::Min(first_x, glyphs[i].m_X);
            }
            const bool  right_to_left = paragraphs[line.m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL;
            const float line_x = viewer->m_LayoutXs[layout_index] + (right_to_left ? viewer->m_LayoutWidths[layout_index] - line.m_Width : 0.0f);
            bool        caret_found = false;
            for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
            {
                const uint32_t cluster = text_offset + glyphs[i].m_Cluster;
                const uint32_t cluster_end = text_offset + GetGlyphEndCluster(glyphs, line, glyphs[i].m_Cluster, line_end - text_offset);
                if (viewer->m_Caret < cluster || viewer->m_Caret > cluster_end)
                    continue;
                const float width = dmMath::Max(glyphs[i].m_Width, viewer->m_LayoutSizes[layout_index] * 0.35f);
                const float x = line_x + glyphs[i].m_X - first_x;
                const float cluster_width = (float)dmMath::Max(1u, cluster_end - cluster);
                const float position = (viewer->m_Caret - cluster) / cluster_width;
                *caret_x = right_to_left ? x + width * (1.0f - position) : x + width * position;
                caret_found = true;
                break;
            }
            if (!caret_found)
                continue;
            const float text_top = WINDOW_HEIGHT - viewer->m_LayoutTops[layout_index];
            *caret_y = text_top + (line_index + 0.5f) * *line_height;
            return true;
        }
    }
    return false;
}

static bool ScrollEditorCaretIntoView(Viewer* viewer)
{
    float caret_x;
    float caret_y;
    float line_height;
    if (!GetEditorCaretScreenPosition(viewer, &caret_x, &caret_y, &line_height))
        return false;
    (void)caret_x;
    const FontViewerNuklearBox& text_field = viewer->m_NuklearLayout.m_TextField;
    const float visible_top = text_field.m_Y + 10.0f;
    const float visible_bottom = text_field.m_Y + text_field.m_Height - 10.0f;
    const float caret_top = caret_y - line_height * 0.5f;
    const float caret_bottom = caret_y + line_height * 0.5f;
    float       scroll_y = viewer->m_TextScrollY;
    if (caret_top < visible_top)
        scroll_y -= visible_top - caret_top;
    else if (caret_bottom > visible_bottom)
        scroll_y += caret_bottom - visible_bottom;
    const float max_scroll_y = fmaxf(0.0f, viewer->m_NuklearLayout.m_TextContentHeight - viewer->m_NuklearLayout.m_TextViewportHeight);
    scroll_y = dmMath::Max(0.0f, dmMath::Min(max_scroll_y, scroll_y));
    if (fabsf(scroll_y - viewer->m_TextScrollY) < 0.5f)
        return false;
    viewer->m_TextScrollY = scroll_y;
    return true;
}

static void MoveEditorCaret(Viewer* viewer, ArrowKey arrow_key, bool extend_selection)
{
    const uint32_t codepoint_count = EditorCodepointCount(viewer);
    uint32_t       caret = viewer->m_Caret;
    if (arrow_key == ARROW_KEY_LEFT || arrow_key == ARROW_KEY_RIGHT)
        viewer->m_HasPreferredCaretX = false;
    if (!extend_selection && viewer->m_Caret != viewer->m_SelectionAnchor && arrow_key == ARROW_KEY_LEFT)
    {
        caret = dmMath::Min(viewer->m_Caret, viewer->m_SelectionAnchor);
    }
    else if (!extend_selection && viewer->m_Caret != viewer->m_SelectionAnchor && arrow_key == ARROW_KEY_RIGHT)
    {
        caret = dmMath::Max(viewer->m_Caret, viewer->m_SelectionAnchor);
    }
    else if (arrow_key == ARROW_KEY_LEFT && caret > 0)
    {
        --caret;
    }
    else if (arrow_key == ARROW_KEY_RIGHT && caret < codepoint_count)
    {
        ++caret;
    }
    else if (arrow_key == ARROW_KEY_UP || arrow_key == ARROW_KEY_DOWN)
    {
        float caret_x;
        float caret_y;
        float line_height;
        if (GetEditorCaretScreenPosition(viewer, &caret_x, &caret_y, &line_height))
        {
            if (!viewer->m_HasPreferredCaretX)
            {
                viewer->m_PreferredCaretX = caret_x;
                viewer->m_HasPreferredCaretX = true;
            }
            caret = HitTestEditorText(viewer, viewer->m_PreferredCaretX, caret_y + (arrow_key == ARROW_KEY_UP ? -line_height : line_height));
        }
    }
    viewer->m_Caret = caret;
    if (!extend_selection)
        viewer->m_SelectionAnchor = caret;
    viewer->m_ScrollCaretIntoViewRequested = true;
    viewer->m_RenderUpdateRequested = true;
}

static ArrowKey GetPressedArrowKey(const FontViewerMacOSInput& input)
{
    if (input.m_LeftDown)
        return ARROW_KEY_LEFT;
    if (input.m_RightDown)
        return ARROW_KEY_RIGHT;
    if (input.m_UpDown)
        return ARROW_KEY_UP;
    if (input.m_DownDown)
        return ARROW_KEY_DOWN;
    return ARROW_KEY_NONE;
}

static void BuildEditorSelectionGeometry(Viewer* viewer)
{
    if (!viewer->m_TextFieldFocused)
        return;
    const uint32_t       selection_first = dmMath::Min(viewer->m_Caret, viewer->m_SelectionAnchor);
    const uint32_t       selection_last = dmMath::Max(viewer->m_Caret, viewer->m_SelectionAnchor);
    for (uint32_t layout_index = 1; layout_index < viewer->m_Layouts.Size(); ++layout_index)
    {
        if (!IsEditorLayout(viewer, layout_index))
            continue;
        HTextLayout                 layout = viewer->m_Layouts[layout_index];
        TextGlyph*                  glyphs = TextLayoutGetGlyphs(layout);
        TextLine*                   lines = TextLayoutGetLines(layout);
        TextParagraph*              paragraphs = TextLayoutGetParagraphs(layout);
        HFont                       font = TextLayoutGetGlyphCount(layout) ? glyphs[0].m_Font : viewer->m_Fonts[0];
        const float                 scale = FontGetScaleFromSize(font, viewer->m_LayoutSizes[layout_index]);
        const float                 ascent = FontGetAscent(font, scale);
        const float                 descent = fabsf(FontGetDescent(font, scale));
        const float                 line_height = (ascent + descent) * 1.2f;
        const FontViewerNuklearBox& clip = viewer->m_LayoutClips[layout_index];
        for (uint32_t line_index = 0; line_index < TextLayoutGetLineCount(layout); ++line_index)
        {
            TextLine& line = lines[line_index];
            if (!line.m_Length)
                continue;
            float first_x = glyphs[line.m_Index].m_X;
            for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
            {
                first_x = dmMath::Min(first_x, glyphs[i].m_X);
            }
            const bool  right_to_left = paragraphs[line.m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL;
            const float line_x = viewer->m_LayoutXs[layout_index] + (right_to_left ? viewer->m_LayoutWidths[layout_index] - line.m_Width : 0.0f);
            const float line_y = viewer->m_LayoutTops[layout_index] - ascent - line_index * line_height;
            for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
            {
                const uint32_t cluster = viewer->m_LayoutTextOffsets[layout_index] + glyphs[i].m_Cluster;
                const float    width = dmMath::Max(glyphs[i].m_Width, viewer->m_LayoutSizes[layout_index] * 0.35f);
                const float    x = line_x + glyphs[i].m_X - first_x;
                if (cluster >= selection_first && cluster < selection_last)
                    PushClippedColorRectangle(viewer, clip, x, line_y - descent, width, line_height, 0.28f, 0.59f, 1.0f, 0.38f);
            }
        }
    }
    float caret_x;
    float caret_y;
    float caret_height;
    if (GetEditorCaretScreenPosition(viewer, &caret_x, &caret_y, &caret_height))
    {
        for (uint32_t layout_index = 1; layout_index < viewer->m_Layouts.Size(); ++layout_index)
        {
            if (IsEditorLayout(viewer, layout_index))
            {
                const float caret_bottom = WINDOW_HEIGHT - caret_y - caret_height * 0.5f;
                PushClippedColorRectangle(viewer, viewer->m_LayoutClips[layout_index], caret_x, caret_bottom, 2.0f, caret_height, 0.9f, 0.93f, 1.0f, 1.0f);
                break;
            }
        }
    }
}

static void BuildDebugGeometry(Viewer* viewer, uint32_t main_face_start, uint32_t main_face_count)
{
    if (viewer->m_ShowBaselines && !viewer->m_Layouts.Empty())
    {
        HTextLayout layout = viewer->m_Layouts[0];
        TextGlyph*  glyphs = TextLayoutGetGlyphs(layout);
        TextLine*   lines = TextLayoutGetLines(layout);
        HFont       font = TextLayoutGetGlyphCount(layout) ? glyphs[0].m_Font : viewer->m_Fonts[0];
        const float scale = FontGetScaleFromSize(font, viewer->m_FontSize);
        const float ascent = FontGetAscent(font, scale);
        const float descent = fabsf(FontGetDescent(font, scale));
        const float line_height = (ascent + descent) * 1.2f;
        const float pivot_x = viewer->m_NuklearLayout.m_ContentWidth * 0.5f;
        const float pivot_y = WINDOW_HEIGHT * 0.5f;
        for (uint32_t i = 0; i < TextLayoutGetLineCount(layout); ++i)
        {
            const float y = viewer->m_LayoutTops[0] - ascent - i * line_height;
            const float transformed_x = pivot_x + (viewer->m_LayoutXs[0] - pivot_x) * viewer->m_Zoom + viewer->m_PanX;
            const float transformed_y = pivot_y + (y - pivot_y) * viewer->m_Zoom + viewer->m_PanY;
            PushColorRectangle(viewer, transformed_x, transformed_y, lines[i].m_Width * viewer->m_Zoom, 1.0f, 1.0f, 0.25f, 0.25f, 0.9f);
        }
    }
    if (viewer->m_ShowQuads)
    {
        for (uint32_t i = main_face_start; i + 5 < main_face_start + main_face_count; i += 6)
        {
            float min_x = WINDOW_WIDTH;
            float min_y = WINDOW_HEIGHT;
            float max_x = 0.0f;
            float max_y = 0.0f;
            for (uint32_t j = 0; j < 6; ++j)
            {
                const FontGlyphVertex& vertex = viewer->m_Vertices[i + j];
                const float            x = (vertex.m_Position[0] + 1.0f) * WINDOW_WIDTH * 0.5f;
                const float            y = (vertex.m_Position[1] + 1.0f) * WINDOW_HEIGHT * 0.5f;
                min_x = dmMath::Min(min_x, x);
                min_y = dmMath::Min(min_y, y);
                max_x = dmMath::Max(max_x, x);
                max_y = dmMath::Max(max_y, y);
            }
            PushColorOutline(viewer, min_x, min_y, max_x - min_x, max_y - min_y, 0.2f, 0.9f, 0.45f, 0.8f);
        }
    }
    BuildEditorSelectionGeometry(viewer);
    viewer->m_ColorDebugVertexCount = viewer->m_ColorVertices.Size() - viewer->m_ColorBackgroundVertexCount - viewer->m_ColorDecorationVertexCount;
}

static void BuildNuklearData(Viewer* viewer, const FontViewerNuklearInput* input)
{
    FontViewerNuklearFonts fonts;
    fonts.m_LoadedFontText = viewer->m_LoadedFontText.Begin();
    fonts.m_LoadedFontCount = viewer->m_Fonts.Size();
    fonts.m_LoadedFontTextSize = viewer->m_LoadedFontText.Size();
    fonts.m_LoadedFontDataSize = viewer->m_LoadedFontDataSize;
    FontViewerNuklearBuild(WINDOW_WIDTH, WINDOW_HEIGHT, viewer->m_LayoutText.Begin(), viewer->m_EditorContentHeight, input, &viewer->m_TextScrollY, &viewer->m_FontSize, &viewer->m_Zoom, &viewer->m_Properties, &fonts, &viewer->m_LegacyLayout, &viewer->m_ShowBaselines, &viewer->m_ShowQuads, &viewer->m_NuklearLayout);
    const uint32_t index_count = viewer->m_NuklearLayout.m_IndexDataSize / sizeof(uint16_t);
    if (viewer->m_NuklearVertices.Capacity() < index_count)
        viewer->m_NuklearVertices.SetCapacity(index_count);
    viewer->m_NuklearVertices.SetSize(index_count);
    for (uint32_t i = 0; i < index_count; ++i)
        viewer->m_NuklearVertices[i] = viewer->m_NuklearLayout.m_Vertices[viewer->m_NuklearLayout.m_Indices[i]];
}

static bool PushEditorLayout(Viewer* viewer)
{
    // Nuklear owns the field rectangle and scrollbar. Render the editor string
    // with the native engine using the selected layout path.
    const FontViewerNuklearBox& text_field = viewer->m_NuklearLayout.m_TextField;
    if (text_field.m_Width <= 0.0f || text_field.m_Height <= 0.0f)
        return true;
    FontViewerNuklearBox        editor_clip;
    editor_clip.m_X = text_field.m_X + 10.0f;
    editor_clip.m_Y = WINDOW_HEIGHT - text_field.m_Y - text_field.m_Height + 10.0f;
    editor_clip.m_Width = text_field.m_Width - 28.0f;
    editor_clip.m_Height = text_field.m_Height - 20.0f;
    const float editor_top = WINDOW_HEIGHT - text_field.m_Y - 10.0f + viewer->m_TextScrollY;

    if (!PushLayout(viewer, viewer->m_LayoutText.Begin(), viewer->m_LayoutText.Size() - 1, 0, editor_clip.m_X, editor_top, 14.0f, editor_clip.m_Width, true, editor_clip, false, false, viewer->m_LegacyLayout, false))
    {
        return false;
    }
    float editor_width;
    TextLayoutGetBounds(viewer->m_Layouts.Back(), &editor_width, &viewer->m_EditorContentHeight);
    (void)editor_width;
    return true;
}

static bool PackAllLayouts(Viewer* viewer)
{
    if (viewer->m_Vertices.Capacity() < viewer->m_VertexCount)
        viewer->m_Vertices.SetCapacity(viewer->m_VertexCount);
    viewer->m_Vertices.SetSize(viewer->m_VertexCount);
    const uint32_t main_layer_stride = CountVisibleGlyphs(viewer->m_Layouts[0]) * 6;
    uint32_t       vertex_start = 0;
    viewer->m_ColorVertices.SetSize(0);
    viewer->m_ColorBackgroundVertexCount = 0;
    viewer->m_ColorDecorationVertexCount = 0;
    viewer->m_SpriteVertices.SetSize(0);
    viewer->m_SpriteDraws.SetSize(0);
    for (uint32_t i = 0; i < viewer->m_Layouts.Size(); ++i)
    {
        const uint32_t layer_stride = CountVisibleGlyphs(viewer->m_Layouts[i]) * 6;
        uint32_t       vertex_index = vertex_start;
        PackLayout(viewer, viewer->m_Layouts[i], viewer->m_LayoutXs[i], viewer->m_LayoutTops[i], viewer->m_LayoutSizes[i], viewer->m_LayoutWidths[i], viewer->m_LayoutClips[i], viewer->m_LayoutBold[i] != 0, i == 0, viewer->m_LayoutLayerCounts[i], layer_stride, &vertex_index);

        if (i == 0)
        {
            PackLayoutDecorations(viewer, viewer->m_Layouts[i], viewer->m_LayoutXs[i], viewer->m_LayoutTops[i], viewer->m_LayoutWidths[i], viewer->m_LayoutClips[i]);
        }

        PackLayoutObjects(viewer, viewer->m_Layouts[i], viewer->m_LayoutXs[i], viewer->m_LayoutTops[i], viewer->m_LayoutWidths[i], i == 0);
        vertex_start += layer_stride * viewer->m_LayoutLayerCounts[i];
    }
    viewer->m_ColorDecorationVertexCount = viewer->m_ColorVertices.Size() - viewer->m_ColorBackgroundVertexCount;
    BuildDebugGeometry(viewer, main_layer_stride * 2, main_layer_stride);
    return vertex_start == viewer->m_VertexCount;
}

static bool BuildFontData(Viewer* viewer)
{
    // Full rebuild path: text/property changes that affect shaping or SDF pixels.
    // Render-only properties and scrolling use the lighter refresh paths below.
    UpdateLayoutText(viewer);
    viewer->m_AtlasChannels = viewer->m_Properties.m_ShadowBlur > 0.0f ? 3 : 1;
    if (viewer->m_NuklearLayout.m_ContentWidth == 0.0f)
        BuildNuklearData(viewer, 0);

    const float                paragraph_width = dmMath::Max(100.0f, viewer->m_NuklearLayout.m_ContentWidth - 100.0f);
    const FontViewerNuklearBox main_clip = { 0.0f, 0.0f, viewer->m_NuklearLayout.m_ContentWidth, WINDOW_HEIGHT };

    if (!PushLayout(viewer, viewer->m_LayoutText.Begin(), viewer->m_LayoutText.Size() - 1, 0, 50.0f, WINDOW_HEIGHT - 60.0f, viewer->m_FontSize, paragraph_width, true, main_clip, false, true, viewer->m_LegacyLayout, true))
    {
        return false;
    }

    if (!PushEditorLayout(viewer))
        return false;

    HTextLayout       preload_layout = 0;
    static const char preload_text[] = "0123456789.+-%";

    if (!CreateLayout(viewer, preload_text, sizeof(preload_text) - 1, 13.0f, 200.0f, false, false, false, &preload_layout))
    {
        return false;
    }
    const bool preload_ok = AddLayoutGlyphs(viewer, preload_layout, 13.0f, false);
    TextLayoutRelease(preload_layout);
    if (!preload_ok)
        return false;

    if (!BuildAtlas(viewer))
        return false;

    return PackAllLayouts(viewer);
}

static bool CreateGraphicsResources(Viewer* viewer)
{
    dmGraphics::ShaderDescBuilder shaders;
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, (const char*)FONTVIEWER_FONT_DF_VP, FONTVIEWER_FONT_DF_VP_SIZE);
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, (const char*)FONTVIEWER_FONT_DF_FP, FONTVIEWER_FONT_DF_FP_SIZE);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "texcoord0", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "face_color", 2, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "outline_color", 3, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "shadow_color", 4, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "sdf_params", 5, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "layer_mask", 6, dmGraphics::ShaderDesc::SHADER_TYPE_VEC3);
    shaders.AddTypeMember("view_proj", dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
    shaders.AddUniform("vs_uniforms", 0, 0);
    shaders.AddTexture("texture_sampler", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    char error[1024] = {};
    viewer->m_Program = dmGraphics::NewProgram(viewer->m_Context, shaders.Get(), error, sizeof(error));
    if (!viewer->m_Program)
    {
        dmLogError("Unable to create fontviewer shader: %s", error);
        return false;
    }
    viewer->m_ViewProjLocation = dmGraphics::FindUniformLocation(viewer->m_Program, "view_proj");
    if (viewer->m_ViewProjLocation == dmGraphics::INVALID_UNIFORM_LOCATION)
    {
        dmLogError("The default font shader does not expose view_proj");
        return false;
    }

    static const char* color_vertex_shader =
    "#version 140\n"
    "in vec4 position;\n"
    "in vec4 color;\n"
    "in vec2 decoration;\n"
    "out vec4 var_color;\n"
    "out vec2 var_decoration;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = position;\n"
    "    var_color = color;\n"
    "    var_decoration = decoration;\n"
    "}\n";
    static const char* color_fragment_shader =
    "#version 140\n"
    "in vec4 var_color;\n"
    "in vec2 var_decoration;\n"
    "out vec4 out_fragColor;\n"
    "void main()\n"
    "{\n"
    "    float mask = var_decoration.y > 0.0 ? 1.0 - step(var_decoration.y, fract(var_decoration.x)) : 1.0;\n"
    "    out_fragColor = var_color * mask;\n"
    "}\n";
    dmGraphics::ShaderDescBuilder color_shaders;
    color_shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, color_vertex_shader, (uint32_t)strlen(color_vertex_shader));
    color_shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, color_fragment_shader, (uint32_t)strlen(color_fragment_shader));
    color_shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    color_shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "color", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    color_shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "decoration", 2, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);
    memset(error, 0, sizeof(error));
    viewer->m_ColorProgram = dmGraphics::NewProgram(viewer->m_Context, color_shaders.Get(), error, sizeof(error));
    if (!viewer->m_ColorProgram)
    {
        dmLogError("Unable to create fontviewer color shader: %s", error);
        return false;
    }

    static const char* sprite_vertex_shader =
    "#version 140\n"
    "in vec4 position;\n"
    "in vec2 texcoord0;\n"
    "out vec2 var_texcoord0;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = position;\n"
    "    var_texcoord0 = texcoord0;\n"
    "}\n";
    static const char* sprite_fragment_shader =
    "#version 140\n"
    "in vec2 var_texcoord0;\n"
    "out vec4 out_fragColor;\n"
    "uniform sampler2D texture_sampler;\n"
    "void main()\n"
    "{\n"
    "    out_fragColor = texture(texture_sampler, var_texcoord0);\n"
    "}\n";
    dmGraphics::ShaderDescBuilder sprite_shaders;
    sprite_shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, sprite_vertex_shader, (uint32_t)strlen(sprite_vertex_shader));
    sprite_shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, sprite_fragment_shader, (uint32_t)strlen(sprite_fragment_shader));
    sprite_shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    sprite_shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "texcoord0", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);
    sprite_shaders.AddTexture("texture_sampler", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    memset(error, 0, sizeof(error));
    viewer->m_SpriteProgram = dmGraphics::NewProgram(viewer->m_Context, sprite_shaders.Get(), error, sizeof(error));

    if (!viewer->m_SpriteProgram)
    {
        dmLogError("Unable to create fontviewer sprite shader: %s", error);

        return false;
    }

    viewer->m_VertexBuffer = dmGraphics::NewVertexBuffer(viewer->m_Context,
                                                         viewer->m_Vertices.Size() * sizeof(FontGlyphVertex),
                                                         viewer->m_Vertices.Begin(),
                                                         dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    viewer->m_VertexDeclaration = FontCreateGlyphVertexDeclaration(viewer->m_Context);

    viewer->m_ColorVertexBuffer = dmGraphics::NewVertexBuffer(viewer->m_Context,
                                                              viewer->m_ColorVertices.Size() * sizeof(ColorVertex),
                                                              viewer->m_ColorVertices.Begin(),
                                                              dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(viewer->m_Context);
    dmGraphics::AddVertexStream(streams, "position", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "color", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "decoration", 2, dmGraphics::TYPE_FLOAT, false);
    viewer->m_ColorVertexDeclaration = dmGraphics::NewVertexDeclaration(viewer->m_Context, streams, sizeof(ColorVertex));
    dmGraphics::DeleteVertexStreamDeclaration(streams);

    SpriteVertex   empty_sprite_vertex = {};
    const uint32_t sprite_vertex_count = dmMath::Max(1u, viewer->m_SpriteVertices.Size());
    const void*    sprite_vertex_data = viewer->m_SpriteVertices.Empty() ? (const void*)&empty_sprite_vertex : viewer->m_SpriteVertices.Begin();
    viewer->m_SpriteVertexBuffer = dmGraphics::NewVertexBuffer(viewer->m_Context,
                                                               sprite_vertex_count * sizeof(SpriteVertex),
                                                               sprite_vertex_data,
                                                               dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    streams = dmGraphics::NewVertexStreamDeclaration(viewer->m_Context);
    dmGraphics::AddVertexStream(streams, "position", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
    viewer->m_SpriteVertexDeclaration = dmGraphics::NewVertexDeclaration(viewer->m_Context, streams, sizeof(SpriteVertex));
    dmGraphics::DeleteVertexStreamDeclaration(streams);

    dmGraphics::TextureCreationParams creation;
    creation.m_Width = ATLAS_WIDTH;
    creation.m_Height = ATLAS_HEIGHT;
    creation.m_OriginalWidth = ATLAS_WIDTH;
    creation.m_OriginalHeight = ATLAS_HEIGHT;
    viewer->m_Texture = dmGraphics::NewTexture(viewer->m_Context, creation);
    dmGraphics::TextureParams texture;
    texture.m_Data = viewer->m_Atlas.Begin();
    texture.m_DataSize = viewer->m_Atlas.Size();
    texture.m_Width = ATLAS_WIDTH;
    texture.m_Height = ATLAS_HEIGHT;
    texture.m_Depth = 1;
    texture.m_LayerCount = 1;
    texture.m_Format = viewer->m_AtlasChannels == 3 ? dmGraphics::TEXTURE_FORMAT_RGB : dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    texture.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    texture.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    dmGraphics::SetTexture(viewer->m_Context, viewer->m_Texture, texture);
    return viewer->m_VertexBuffer && viewer->m_VertexDeclaration && viewer->m_Texture &&
    viewer->m_ColorVertexBuffer && viewer->m_ColorVertexDeclaration && viewer->m_SpriteProgram &&
    viewer->m_SpriteVertexBuffer && viewer->m_SpriteVertexDeclaration;
}

static bool CreateNuklearGraphicsResources(Viewer* viewer)
{
    static const char* vertex_shader =
    "#version 140\n"
    "in vec4 position;\n"
    "in vec2 texcoord0;\n"
    "in vec4 color;\n"
    "out vec2 var_texcoord0;\n"
    "out vec4 var_color;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(position.x / 640.0 - 1.0, 1.0 - position.y / 450.0, 0.0, 1.0);\n"
    "    var_texcoord0 = texcoord0;\n"
    "    var_color = color;\n"
    "}\n";
    static const char* fragment_shader =
    "#version 140\n"
    "in vec2 var_texcoord0;\n"
    "in vec4 var_color;\n"
    "out vec4 out_fragColor;\n"
    "uniform sampler2D texture_sampler;\n"
    "void main()\n"
    "{\n"
    "    out_fragColor = var_color * texture(texture_sampler, var_texcoord0);\n"
    "}\n";
    dmGraphics::ShaderDescBuilder shaders;
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vertex_shader, (uint32_t)strlen(vertex_shader));
    shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment_shader, (uint32_t)strlen(fragment_shader));
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "texcoord0", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);
    shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "color", 2, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shaders.AddTexture("texture_sampler", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
    char error[1024] = {};
    viewer->m_NuklearProgram = dmGraphics::NewProgram(viewer->m_Context, shaders.Get(), error, sizeof(error));
    if (!viewer->m_NuklearProgram)
    {
        dmLogError("Unable to create Nuklear shader: %s", error);
        return false;
    }
    viewer->m_NuklearVertexBuffer = dmGraphics::NewVertexBuffer(viewer->m_Context,
                                                                viewer->m_NuklearVertices.Size() * sizeof(FontViewerNuklearVertex),
                                                                viewer->m_NuklearVertices.Begin(),
                                                                dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(viewer->m_Context);
    dmGraphics::AddVertexStream(streams, "position", 2, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "color", 4, dmGraphics::TYPE_UNSIGNED_BYTE, true);
    viewer->m_NuklearVertexDeclaration = dmGraphics::NewVertexDeclaration(viewer->m_Context, streams, sizeof(FontViewerNuklearVertex));
    dmGraphics::DeleteVertexStreamDeclaration(streams);

    const void* atlas_pixels = 0;
    uint32_t    atlas_width = 0;
    uint32_t    atlas_height = 0;
    if (!FontViewerNuklearGetAtlas(&atlas_pixels, &atlas_width, &atlas_height))
        return false;
    dmGraphics::TextureCreationParams creation;
    creation.m_Width = atlas_width;
    creation.m_Height = atlas_height;
    creation.m_OriginalWidth = atlas_width;
    creation.m_OriginalHeight = atlas_height;
    viewer->m_NuklearTexture = dmGraphics::NewTexture(viewer->m_Context, creation);
    dmGraphics::TextureParams texture;
    texture.m_Data = atlas_pixels;
    texture.m_DataSize = atlas_width * atlas_height * 4;
    texture.m_Width = atlas_width;
    texture.m_Height = atlas_height;
    texture.m_Depth = 1;
    texture.m_LayerCount = 1;
    texture.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    texture.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    texture.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    dmGraphics::SetTexture(viewer->m_Context, viewer->m_NuklearTexture, texture);
    return viewer->m_NuklearVertexBuffer && viewer->m_NuklearVertexDeclaration && viewer->m_NuklearTexture;
}

static void UpdateNuklearGraphicsData(Viewer* viewer)
{
    if (!viewer->m_NuklearVertexBuffer)
        return;
    dmGraphics::SetVertexBufferData(viewer->m_NuklearVertexBuffer,
                                    viewer->m_NuklearVertices.Size() * sizeof(FontViewerNuklearVertex),
                                    viewer->m_NuklearVertices.Begin(),
                                    dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
}

static void DestroyNuklearGraphicsResources(Viewer* viewer)
{
    if (viewer->m_NuklearTexture)
        dmGraphics::DeleteTexture(viewer->m_Context, viewer->m_NuklearTexture);
    if (viewer->m_NuklearVertexDeclaration)
        dmGraphics::DeleteVertexDeclaration(viewer->m_NuklearVertexDeclaration);
    if (viewer->m_NuklearVertexBuffer)
        dmGraphics::DeleteVertexBuffer(viewer->m_NuklearVertexBuffer);
    if (viewer->m_NuklearProgram)
        dmGraphics::DeleteProgram(viewer->m_Context, viewer->m_NuklearProgram);
    viewer->m_NuklearTexture = 0;
    viewer->m_NuklearVertexDeclaration = 0;
    viewer->m_NuklearVertexBuffer = 0;
    viewer->m_NuklearProgram = 0;
}

static void DestroyGraphicsResources(Viewer* viewer)
{
    if (viewer->m_Texture)
        dmGraphics::DeleteTexture(viewer->m_Context, viewer->m_Texture);
    if (viewer->m_VertexDeclaration)
        dmGraphics::DeleteVertexDeclaration(viewer->m_VertexDeclaration);
    if (viewer->m_ColorVertexDeclaration)
        dmGraphics::DeleteVertexDeclaration(viewer->m_ColorVertexDeclaration);
    if (viewer->m_SpriteVertexDeclaration)
    {
        dmGraphics::DeleteVertexDeclaration(viewer->m_SpriteVertexDeclaration);
    }
    if (viewer->m_VertexBuffer)
        dmGraphics::DeleteVertexBuffer(viewer->m_VertexBuffer);
    if (viewer->m_ColorVertexBuffer)
        dmGraphics::DeleteVertexBuffer(viewer->m_ColorVertexBuffer);
    if (viewer->m_SpriteVertexBuffer)
    {
        dmGraphics::DeleteVertexBuffer(viewer->m_SpriteVertexBuffer);
    }
    if (viewer->m_Program)
        dmGraphics::DeleteProgram(viewer->m_Context, viewer->m_Program);
    if (viewer->m_ColorProgram)
        dmGraphics::DeleteProgram(viewer->m_Context, viewer->m_ColorProgram);
    if (viewer->m_SpriteProgram)
    {
        dmGraphics::DeleteProgram(viewer->m_Context, viewer->m_SpriteProgram);
    }
    viewer->m_Texture = 0;
    viewer->m_VertexDeclaration = 0;
    viewer->m_ColorVertexDeclaration = 0;
    viewer->m_SpriteVertexDeclaration = 0;
    viewer->m_VertexBuffer = 0;
    viewer->m_ColorVertexBuffer = 0;
    viewer->m_SpriteVertexBuffer = 0;
    viewer->m_Program = 0;
    viewer->m_ColorProgram = 0;
    viewer->m_SpriteProgram = 0;
}

static bool Rebuild(Viewer* viewer)
{
    DestroyGraphicsResources(viewer);
    ClearGeneratedFontData(viewer);
    return BuildFontData(viewer) && CreateGraphicsResources(viewer);
}

static bool UploadRenderData(Viewer* viewer)
{
    if (!PackAllLayouts(viewer))
    {
        return false;
    }

    dmGraphics::SetVertexBufferData(viewer->m_VertexBuffer,
                                    viewer->m_Vertices.Size() * sizeof(FontGlyphVertex),
                                    viewer->m_Vertices.Begin(),
                                    dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    dmGraphics::SetVertexBufferData(viewer->m_ColorVertexBuffer,
                                    viewer->m_ColorVertices.Size() * sizeof(ColorVertex),
                                    viewer->m_ColorVertices.Begin(),
                                    dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    if (!viewer->m_SpriteVertices.Empty())
    {
        dmGraphics::SetVertexBufferData(viewer->m_SpriteVertexBuffer,
                                        viewer->m_SpriteVertices.Size() * sizeof(SpriteVertex),
                                        viewer->m_SpriteVertices.Begin(),
                                        dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    }

    return true;
}

static bool HasAnimatedEffects(HTextLayout layout)
{
    const TextLayout* internal = (const TextLayout*)layout;

    for (uint32_t i = 0; i < internal->m_Effects.Size(); ++i)
    {
        if ((internal->m_Effects[i].m_Type == TEXT_EFFECT_SHAKE && internal->m_Effects[i].m_Shake.m_Hz != 0.0f) ||
            (internal->m_Effects[i].m_Type == TEXT_EFFECT_WAVE && internal->m_Effects[i].m_Wave.m_Hz != 0.0f) ||
            (internal->m_Effects[i].m_Type == TEXT_EFFECT_GRADIENT && internal->m_Effects[i].m_Gradient.m_Hz != 0.0f))

            return true;
    }

    return false;
}

static bool RefreshRenderData(Viewer* viewer)
{
    // Preserve the main shaped layout, glyph cache, atlas, shaders, and texture.
    // Only the editable text layout and render transforms need repacking.
    if (viewer->m_Layouts.Empty())
        return false;

    for (uint32_t i = 1; i < viewer->m_Layouts.Size(); ++i)
        TextLayoutRelease(viewer->m_Layouts[i]);
    viewer->m_Layouts.SetSize(1);
    viewer->m_LayoutTops.SetSize(1);
    viewer->m_LayoutXs.SetSize(1);
    viewer->m_LayoutSizes.SetSize(1);
    viewer->m_LayoutWidths.SetSize(1);
    viewer->m_LayoutClips.SetSize(1);
    viewer->m_LayoutBold.SetSize(1);
    viewer->m_LayoutLayerCounts.SetSize(1);
    viewer->m_LayoutTextOffsets.SetSize(1);
    viewer->m_VertexCount = CountVisibleGlyphs(viewer->m_Layouts[0]) * 6 * viewer->m_LayoutLayerCounts[0];

    const uint32_t glyph_count = viewer->m_Glyphs.Size();

    if (!PushEditorLayout(viewer) || viewer->m_Glyphs.Size() != glyph_count)
    {
        return false;
    }

    return UploadRenderData(viewer);
}

static void HandleInput(Viewer* viewer)
{
    FontViewerMacOSInput input;
    FontViewerMacOSPollInput(viewer->m_Window, WINDOW_WIDTH, WINDOW_HEIGHT, &input);
    const bool             window_active = dmPlatform::GetWindowStateParam(viewer->m_Window, WINDOW_STATE_ACTIVE) != 0;
    const int32_t          mouse_x = input.m_MouseX;
    const int32_t          mouse_y = input.m_MouseY;
    const bool             mouse_down = window_active && input.m_LeftMouseDown;
    const uint32_t         hovered_link = window_active ? HitTestPreviewLink(viewer, (float)mouse_x, (float)mouse_y) : 0xffffffff;

    if (hovered_link != viewer->m_HoveredLinkObject)
    {
        SetPreviewLinkStyle(viewer, viewer->m_HoveredLinkObject, 0);
        SetPreviewLinkStyle(viewer, hovered_link, STYLE_LINK_HOVER);
        viewer->m_HoveredLinkObject = hovered_link;
        viewer->m_RenderUpdateRequested = true;
    }

    const bool             link_cursor = hovered_link != 0xffffffff;

    if (link_cursor != viewer->m_LinkCursor)
    {
        FontViewerMacOSSetLinkCursor(viewer->m_Window, link_cursor);
        viewer->m_LinkCursor = link_cursor;
    }
    const int32_t          wheel_delta = window_active ? input.m_MouseWheel - viewer->m_PreviousMouseWheel : 0;
    const float            previous_font_size = viewer->m_FontSize;
    const float            previous_outline_width = viewer->m_Properties.m_OutlineWidth;
    const float            previous_shadow_blur = viewer->m_Properties.m_ShadowBlur;
    const bool             previous_legacy_layout = viewer->m_LegacyLayout;
    const bool             previous_show_baselines = viewer->m_ShowBaselines;
    const bool             previous_show_quads = viewer->m_ShowQuads;
    const bool             previous_text_visible = viewer->m_NuklearLayout.m_TextField.m_Width > 0.0f;
    FontViewerNuklearInput nuklear_input;
    nuklear_input.m_MouseX = mouse_x;
    nuklear_input.m_MouseY = mouse_y;
    nuklear_input.m_ScrollY = mouse_x >= viewer->m_NuklearLayout.m_ContentWidth ? (float)wheel_delta : 0.0f;
    nuklear_input.m_LeftMouseDown = mouse_down;
    nuklear_input.m_CopyDown = input.m_CopyDown;
    nuklear_input.m_SelectAllDown = input.m_SelectAllDown;
    BuildNuklearData(viewer, &nuklear_input);
    UpdateNuklearGraphicsData(viewer);
    const bool text_visible = viewer->m_NuklearLayout.m_TextField.m_Width > 0.0f;

    if (fabsf(previous_font_size - viewer->m_FontSize) > 0.0001f ||
        fabsf(previous_outline_width - viewer->m_Properties.m_OutlineWidth) > 0.0001f ||
        fabsf(previous_shadow_blur - viewer->m_Properties.m_ShadowBlur) > 0.0001f ||
        previous_legacy_layout != viewer->m_LegacyLayout ||
        previous_show_baselines != viewer->m_ShowBaselines ||
        previous_show_quads != viewer->m_ShowQuads ||
        previous_text_visible != text_visible)
    {
        viewer->m_RebuildRequested = true;
    }
    if (!text_visible)
        viewer->m_TextFieldFocused = false;
    // Native widget hover/active states are part of Nuklear's command stream,
    // so upload the regenerated UI geometry every frame.
    viewer->m_RenderUpdateRequested = true;

    if (!window_active)
    {
        SetPreviewLinkStyle(viewer, viewer->m_PressedLinkObject, 0);
        viewer->m_PreviousMouseDown = false;
        viewer->m_PressedLinkObject = 0xffffffff;
        viewer->m_RepeatingArrowKey = ARROW_KEY_NONE;
        viewer->m_PreviousMouseWheel = input.m_MouseWheel;
        return;
    }

    if (mouse_down && !viewer->m_PreviousMouseDown)
    {
        const FontViewerNuklearBox& text_field = viewer->m_NuklearLayout.m_TextField;

        if (hovered_link != 0xffffffff)
        {
            viewer->m_TextFieldFocused = false;
            viewer->m_TextSelecting = false;
            viewer->m_PreviewDragging = false;
            viewer->m_PressedLinkObject = hovered_link;
            SetPreviewLinkStyle(viewer, hovered_link, STYLE_LINK_ACTIVE);
        }
        else if (PointInBox(mouse_x, mouse_y, text_field) && mouse_x < text_field.m_X + text_field.m_Width - 14.0f)
        {
            viewer->m_TextFieldFocused = true;
            viewer->m_Caret = HitTestEditorText(viewer, (float)mouse_x, (float)mouse_y);
            viewer->m_SelectionAnchor = viewer->m_Caret;
            viewer->m_HasPreferredCaretX = false;
            viewer->m_TextSelecting = true;
            viewer->m_RenderUpdateRequested = true;
        }
        else if (mouse_x >= 0 && mouse_x < viewer->m_NuklearLayout.m_ContentWidth)
        {
            // Dragging the preview pans its render transform. UI interaction and
            // text selection are confined to the Nuklear panel on the right.
            viewer->m_TextFieldFocused = false;
            viewer->m_TextSelecting = false;
            viewer->m_PreviewDragging = true;
            viewer->m_PreviousMouseX = mouse_x;
            viewer->m_PreviousMouseY = mouse_y;
            viewer->m_RenderUpdateRequested = true;
        }
        else
        {
            viewer->m_TextFieldFocused = false;
            viewer->m_TextSelecting = false;
            viewer->m_RenderUpdateRequested = true;
            if (viewer->m_MarkedText.Size() > 1)
                SetMarkedText(viewer, "");
        }
    }
    if (viewer->m_TextSelecting)
    {
        if (mouse_down)
        {
            const uint32_t caret = HitTestEditorText(viewer, (float)mouse_x, (float)mouse_y);
            if (caret != viewer->m_Caret)
            {
                viewer->m_Caret = caret;
                viewer->m_HasPreferredCaretX = false;
                viewer->m_ScrollCaretIntoViewRequested = true;
                viewer->m_RenderUpdateRequested = true;
            }
        }
        else
        {
            viewer->m_TextSelecting = false;
        }
    }
    if (viewer->m_PreviewDragging)
    {
        if (mouse_down)
        {
            viewer->m_PanX += mouse_x - viewer->m_PreviousMouseX;
            viewer->m_PanY -= mouse_y - viewer->m_PreviousMouseY;
            viewer->m_PreviousMouseX = mouse_x;
            viewer->m_PreviousMouseY = mouse_y;
            viewer->m_RenderUpdateRequested = true;
        }
        else
        {
            viewer->m_PreviewDragging = false;
        }
    }
    if (viewer->m_PressedLinkObject != 0xffffffff)
    {
        if (mouse_down && hovered_link != viewer->m_PressedLinkObject)
        {
            SetPreviewLinkStyle(viewer, viewer->m_PressedLinkObject, 0);
            viewer->m_PressedLinkObject = 0xffffffff;
        }
        else if (!mouse_down)
        {
            SetPreviewLinkStyle(viewer, viewer->m_PressedLinkObject,
                                hovered_link == viewer->m_PressedLinkObject ? STYLE_LINK_HOVER : 0);
            if (hovered_link == viewer->m_PressedLinkObject)
            {
                OpenPreviewLink(viewer, viewer->m_PressedLinkObject);
            }

            viewer->m_PressedLinkObject = 0xffffffff;
        }
    }
    viewer->m_PreviousMouseDown = mouse_down;

    if (wheel_delta && mouse_x >= 0 && mouse_x < viewer->m_NuklearLayout.m_ContentWidth)
    {
        // Multiplicative wheel zoom remains useful across the 1%-2000% range.
        const float previous_zoom = viewer->m_Zoom;
        const float zoom = dmMath::Max(FONT_VIEWER_ZOOM_MIN,
                                       dmMath::Min(FONT_VIEWER_ZOOM_MAX, previous_zoom * powf(1.1f, (float)wheel_delta)));
        const float zoom_ratio = zoom / previous_zoom;
        const float pivot_x = viewer->m_NuklearLayout.m_ContentWidth * 0.5f;
        const float pivot_y = WINDOW_HEIGHT * 0.5f;
        const float mouse_render_y = WINDOW_HEIGHT - mouse_y;
        viewer->m_PanX = mouse_x - pivot_x - (mouse_x - pivot_x - viewer->m_PanX) * zoom_ratio;
        viewer->m_PanY = mouse_render_y - pivot_y - (mouse_render_y - pivot_y - viewer->m_PanY) * zoom_ratio;
        viewer->m_Zoom = zoom;
        viewer->m_RenderUpdateRequested = true;
    }
    viewer->m_PreviousMouseWheel = input.m_MouseWheel;

    ArrowKey arrow_key = viewer->m_TextFieldFocused && viewer->m_MarkedText.Size() == 1
                       ? GetPressedArrowKey(input)
                       : ARROW_KEY_NONE;
    if (arrow_key == ARROW_KEY_NONE)
    {
        viewer->m_RepeatingArrowKey = ARROW_KEY_NONE;
    }
    else
    {
        const uint64_t now = dmTime::GetMonotonicTime();
        if (arrow_key != viewer->m_RepeatingArrowKey)
        {
            MoveEditorCaret(viewer, arrow_key, input.m_ShiftDown);
            viewer->m_RepeatingArrowKey = arrow_key;
            viewer->m_ArrowRepeatAt = now + KEY_REPEAT_DELAY;
        }
        else if (now >= viewer->m_ArrowRepeatAt)
        {
            MoveEditorCaret(viewer, arrow_key, input.m_ShiftDown);
            viewer->m_ArrowRepeatAt = now + KEY_REPEAT_INTERVAL;
        }
    }

    const bool backspace_down = input.m_BackspaceDown;
    if (viewer->m_TextFieldFocused && viewer->m_MarkedText.Size() == 1 && backspace_down)
    {
        const uint64_t now = dmTime::GetMonotonicTime();
        if (!viewer->m_PreviousBackspaceDown)
        {
            RemoveLastCodepoint(viewer);
            viewer->m_BackspaceRepeatAt = now + KEY_REPEAT_DELAY;
        }
        else if (now >= viewer->m_BackspaceRepeatAt)
        {
            RemoveLastCodepoint(viewer);
            viewer->m_BackspaceRepeatAt = now + KEY_REPEAT_INTERVAL;
        }
    }
    viewer->m_PreviousBackspaceDown = backspace_down;

    const bool delete_down = input.m_DeleteDown;
    if (viewer->m_TextFieldFocused && viewer->m_MarkedText.Size() == 1 && delete_down && !viewer->m_PreviousDeleteDown)
        RemoveNextCodepoint(viewer);
    viewer->m_PreviousDeleteDown = delete_down;

    const bool enter_down = input.m_EnterDown;
    if (viewer->m_TextFieldFocused && enter_down && !viewer->m_PreviousEnterDown)
        AppendEditorBytes(viewer, "\n", 1);
    viewer->m_PreviousEnterDown = enter_down;

    const bool escape_down = input.m_EscapeDown;
    if (escape_down && !viewer->m_PreviousEscapeDown)
        viewer->m_Closed = true;
    viewer->m_PreviousEscapeDown = escape_down;
}

static bool Initialize(Viewer* viewer)
{
    viewer->m_Window = dmPlatform::NewWindow();
    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = WINDOW_WIDTH;
    window_params.m_Height = WINDOW_HEIGHT;
    window_params.m_Title = "Defold Font Viewer";
    window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    window_params.m_CloseCallback = OnWindowClose;
    window_params.m_CloseCallbackUserData = viewer;
    window_params.m_Hidden = viewer->m_ExportMode ? 1 : 0;
    if (dmPlatform::OpenWindow(viewer->m_Window, window_params) != WINDOW_RESULT_OK)
        return false;
    if (!viewer->m_ExportMode)
    {
        FontViewerMacOSInstallInput(viewer->m_Window, OnKeyboardChar, OnKeyboardMarkedText, viewer);
        dmPlatform::ShowWindow(viewer->m_Window);
    }

    dmGraphics::ContextParams context_params;
    context_params.m_Window = viewer->m_Window;
    context_params.m_Width = WINDOW_WIDTH;
    context_params.m_Height = WINDOW_HEIGHT;
    context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
    viewer->m_Context = dmGraphics::NewContext(context_params);

    if (!viewer->m_Context)
    {
        return false;
    }

    if (viewer->m_ExportMode)
    {
        viewer->m_NuklearLayout.m_ContentWidth = WINDOW_WIDTH;

        return LoadFonts(viewer) && BuildFontData(viewer) && CreateGraphicsResources(viewer);
    }

    if (!FontViewerNuklearInitialize(viewer->m_Window, WINDOW_WIDTH, WINDOW_HEIGHT))
    {
        return false;
    }

    viewer->m_NuklearInitialized = true;

    return LoadFonts(viewer) && BuildFontData(viewer) && CreateGraphicsResources(viewer) && CreateNuklearGraphicsResources(viewer);
}

static void Finalize(Viewer* viewer)
{
    if (viewer->m_Context)
    {
        DestroyNuklearGraphicsResources(viewer);
        DestroyGraphicsResources(viewer);
    }
    ClearGeneratedFontData(viewer);

    if (viewer->m_NuklearInitialized)
    {
        FontViewerNuklearFinalize();
    }
    if (viewer->m_Collection)
        FontCollectionDestroy(viewer->m_Collection);
    for (uint32_t i = 0; i < viewer->m_Fonts.Size(); ++i)
    {
        if (viewer->m_Fonts[i])
            FontDestroy(viewer->m_Fonts[i]);
    }
    for (uint32_t i = 0; i < viewer->m_OwnedTexts.Size(); ++i)
        free(viewer->m_OwnedTexts[i]);
    if (viewer->m_Context)
    {
        dmGraphics::CloseWindow(viewer->m_Context);
        dmGraphics::DeleteContext(viewer->m_Context);
        dmGraphics::Finalize();
    }
    if (viewer->m_Window)
    {
        if (!viewer->m_ExportMode)
        {
            FontViewerMacOSDestroyLinkCursor(viewer->m_Window);
        }
        dmPlatform::CloseWindow(viewer->m_Window);
        dmPlatform::DeleteWindow(viewer->m_Window);
    }
}

static void DrawNuklear(Viewer* viewer)
{
    dmGraphics::EnableProgram(viewer->m_Context, viewer->m_NuklearProgram);
    dmGraphics::EnableTexture(viewer->m_Context, 0, 0, viewer->m_NuklearTexture);
    dmGraphics::EnableVertexBuffer(viewer->m_Context, viewer->m_NuklearVertexBuffer, 0);
    dmGraphics::EnableVertexDeclaration(viewer->m_Context, viewer->m_NuklearVertexDeclaration, 0, 0, viewer->m_NuklearProgram);
    dmGraphics::EnableState(viewer->m_Context, dmGraphics::STATE_SCISSOR_TEST);
    const float framebuffer_scale_x = (float)dmGraphics::GetWindowWidth(viewer->m_Context) / WINDOW_WIDTH;
    const float framebuffer_scale_y = (float)dmGraphics::GetWindowHeight(viewer->m_Context) / WINDOW_HEIGHT;
    uint32_t    vertex_offset = 0;
    for (uint32_t i = 0; i < viewer->m_NuklearLayout.m_DrawCommandCount; ++i)
    {
        const FontViewerNuklearDrawCommand& command = viewer->m_NuklearLayout.m_DrawCommands[i];
        const int32_t                       x = dmMath::Max(0, (int32_t)floorf(command.m_Clip.m_X));
        const int32_t                       y = dmMath::Max(0, (int32_t)floorf(WINDOW_HEIGHT - command.m_Clip.m_Y - command.m_Clip.m_Height));
        const int32_t                       right = dmMath::Min((int32_t)WINDOW_WIDTH, (int32_t)ceilf(command.m_Clip.m_X + command.m_Clip.m_Width));
        const int32_t                       top = dmMath::Min((int32_t)WINDOW_HEIGHT, (int32_t)ceilf(WINDOW_HEIGHT - command.m_Clip.m_Y));
        if (right > x && top > y)
        {
            dmGraphics::SetScissor(viewer->m_Context,
                                   (int32_t)floorf(x * framebuffer_scale_x),
                                   (int32_t)floorf(y * framebuffer_scale_y),
                                   (int32_t)ceilf((right - x) * framebuffer_scale_x),
                                   (int32_t)ceilf((top - y) * framebuffer_scale_y));
            dmGraphics::Draw(viewer->m_Context, dmGraphics::PRIMITIVE_TRIANGLES, vertex_offset, command.m_ElementCount, 1);
        }
        vertex_offset += command.m_ElementCount;
    }
    dmGraphics::DisableState(viewer->m_Context, dmGraphics::STATE_SCISSOR_TEST);
    dmGraphics::DisableVertexDeclaration(viewer->m_Context, viewer->m_NuklearVertexDeclaration);
    dmGraphics::DisableVertexBuffer(viewer->m_Context, viewer->m_NuklearVertexBuffer);
    dmGraphics::DisableTexture(viewer->m_Context, 0, viewer->m_NuklearTexture);
}

static void DrawSprites(Viewer* viewer)
{
    if (viewer->m_SpriteDraws.Empty())
    {
        return;
    }

    dmGraphics::EnableProgram(viewer->m_Context, viewer->m_SpriteProgram);
    dmGraphics::EnableVertexBuffer(viewer->m_Context, viewer->m_SpriteVertexBuffer, 0);
    dmGraphics::EnableVertexDeclaration(viewer->m_Context, viewer->m_SpriteVertexDeclaration, 0, 0, viewer->m_SpriteProgram);
    dmGraphics::HTexture bound_texture = 0;

    for (uint32_t i = 0; i < viewer->m_SpriteDraws.Size(); ++i)
    {
        const SpriteDraw& draw = viewer->m_SpriteDraws[i];

        if (draw.m_Texture != bound_texture)
        {
            if (bound_texture)
            {
                dmGraphics::DisableTexture(viewer->m_Context, 0, bound_texture);
            }

            dmGraphics::EnableTexture(viewer->m_Context, 0, 0, draw.m_Texture);
            bound_texture = draw.m_Texture;
        }

        dmGraphics::Draw(viewer->m_Context, dmGraphics::PRIMITIVE_TRIANGLES, draw.m_VertexIndex, 6, 1);
    }

    if (bound_texture)
    {
        dmGraphics::DisableTexture(viewer->m_Context, 0, bound_texture);
    }

    dmGraphics::DisableVertexDeclaration(viewer->m_Context, viewer->m_SpriteVertexDeclaration);
    dmGraphics::DisableVertexBuffer(viewer->m_Context, viewer->m_SpriteVertexBuffer);
}

static void Draw(Viewer* viewer, void* capture_buffer, uint32_t capture_buffer_size)
{
    dmGraphics::BeginFrame(viewer->m_Context);
    dmGraphics::SetViewport(viewer->m_Context, 0, 0, dmGraphics::GetWindowWidth(viewer->m_Context), dmGraphics::GetWindowHeight(viewer->m_Context));
    dmGraphics::Clear(viewer->m_Context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, (uint8_t)(viewer->m_Properties.m_BackgroundColor[0] * 255.0f), (uint8_t)(viewer->m_Properties.m_BackgroundColor[1] * 255.0f), (uint8_t)(viewer->m_Properties.m_BackgroundColor[2] * 255.0f), 255, 1.0f, 0);
    dmGraphics::EnableState(viewer->m_Context, dmGraphics::STATE_BLEND);
    dmGraphics::SetBlendFunc(viewer->m_Context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);

    dmGraphics::EnableProgram(viewer->m_Context, viewer->m_ColorProgram);
    dmGraphics::EnableVertexBuffer(viewer->m_Context, viewer->m_ColorVertexBuffer, 0);
    dmGraphics::EnableVertexDeclaration(viewer->m_Context, viewer->m_ColorVertexDeclaration, 0, 0, viewer->m_ColorProgram);
    dmGraphics::Draw(viewer->m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, viewer->m_ColorBackgroundVertexCount, 1);
    dmGraphics::DisableVertexDeclaration(viewer->m_Context, viewer->m_ColorVertexDeclaration);
    dmGraphics::DisableVertexBuffer(viewer->m_Context, viewer->m_ColorVertexBuffer);

    if (!viewer->m_ExportMode)
    {
        DrawNuklear(viewer);
    }

    DrawSprites(viewer);

    dmGraphics::EnableProgram(viewer->m_Context, viewer->m_Program);
    const Matrix4 identity = Matrix4::identity();
    dmGraphics::SetConstantM4(viewer->m_Context, (const Vector4*)&identity, 1, viewer->m_ViewProjLocation);
    dmGraphics::EnableTexture(viewer->m_Context, 0, 0, viewer->m_Texture);
    dmGraphics::EnableVertexBuffer(viewer->m_Context, viewer->m_VertexBuffer, 0);
    dmGraphics::EnableVertexDeclaration(viewer->m_Context, viewer->m_VertexDeclaration, 0, 0, viewer->m_Program);
    dmGraphics::Draw(viewer->m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, viewer->m_VertexCount, 1);
    dmGraphics::DisableVertexDeclaration(viewer->m_Context, viewer->m_VertexDeclaration);
    dmGraphics::DisableVertexBuffer(viewer->m_Context, viewer->m_VertexBuffer);

    if (viewer->m_ColorDecorationVertexCount || viewer->m_ColorDebugVertexCount)
    {
        dmGraphics::EnableProgram(viewer->m_Context, viewer->m_ColorProgram);
        dmGraphics::EnableVertexBuffer(viewer->m_Context, viewer->m_ColorVertexBuffer, 0);
        dmGraphics::EnableVertexDeclaration(viewer->m_Context, viewer->m_ColorVertexDeclaration, 0, 0, viewer->m_ColorProgram);
        dmGraphics::Draw(viewer->m_Context, dmGraphics::PRIMITIVE_TRIANGLES, viewer->m_ColorBackgroundVertexCount,
                         viewer->m_ColorDecorationVertexCount + viewer->m_ColorDebugVertexCount, 1);
        dmGraphics::DisableVertexDeclaration(viewer->m_Context, viewer->m_ColorVertexDeclaration);
        dmGraphics::DisableVertexBuffer(viewer->m_Context, viewer->m_ColorVertexBuffer);
    }
    if (capture_buffer)
    {
        dmGraphics::ReadPixels(viewer->m_Context,
                               0,
                               0,
                               dmGraphics::GetWindowWidth(viewer->m_Context),
                               dmGraphics::GetWindowHeight(viewer->m_Context),
                               capture_buffer,
                               capture_buffer_size);
    }
    dmGraphics::Flip(viewer->m_Context);
}

struct ExportBounds
{
    ExportBounds()
        : m_MinX(0xffffffff)
        , m_MinY(0xffffffff)
        , m_MaxX(0)
        , m_MaxY(0)
    {
    }

    uint32_t m_MinX;
    uint32_t m_MinY;
    uint32_t m_MaxX;
    uint32_t m_MaxY;
};

struct ExportTiming
{
    uint32_t m_FrameCount;
    float    m_Duration;
    bool     m_Animated;
    bool     m_PingPong;
};

struct ExportCrop
{
    uint32_t m_X;
    uint32_t m_Y;
    uint32_t m_Width;
    uint32_t m_Height;
};

static uint32_t GreatestCommonDivisor(uint32_t a, uint32_t b)
{
    while (b)
    {
        const uint32_t remainder = a % b;
        a = b;
        b = remainder;
    }

    return a;
}

static ExportTiming GetExportTiming(HTextLayout layout, uint32_t requested_frame_count)
{
    const TextLayout* internal = (const TextLayout*)layout;
    uint32_t          periodic_frequency_millihz = 0;
    bool              has_shake = false;

    for (uint32_t i = 0; i < internal->m_Effects.Size(); ++i)
    {
        const TextEffect& effect = internal->m_Effects[i];
        float             hz = 0.0f;

        if (effect.m_Type == TEXT_EFFECT_SHAKE)
        {
            hz = effect.m_Shake.m_Hz;
            has_shake |= hz != 0.0f && effect.m_Shake.m_Amplitude != 0.0f;
        }
        else if (effect.m_Type == TEXT_EFFECT_WAVE)
        {
            hz = effect.m_Wave.m_Hz;
        }
        else if (effect.m_Type == TEXT_EFFECT_GRADIENT)
        {
            hz = effect.m_Gradient.m_Hz;
        }

        hz = fabsf(hz);

        if (hz == 0.0f)
        {
            continue;
        }

        if (effect.m_Type == TEXT_EFFECT_SHAKE)
        {
            continue;
        }

        const uint32_t effect_millihz = dmMath::Max(1U, (uint32_t)floorf(hz * 1000.0f + 0.5f));
        periodic_frequency_millihz = periodic_frequency_millihz ? GreatestCommonDivisor(periodic_frequency_millihz, effect_millihz) : effect_millihz;
    }

    ExportTiming timing;
    timing.m_Animated = has_shake || periodic_frequency_millihz != 0;
    timing.m_PingPong = has_shake;

    if (!timing.m_Animated)
    {
        timing.m_FrameCount = 1;
        timing.m_Duration = 1.0f;

        return timing;
    }

    const float forward_duration = periodic_frequency_millihz ? 1000.0f / periodic_frequency_millihz : 1.0f;
    timing.m_Duration = timing.m_PingPong ? forward_duration * 2.0f : forward_duration;

    if (requested_frame_count)
    {
        timing.m_FrameCount = requested_frame_count;
    }
    else
    {
        timing.m_FrameCount = dmMath::Max(2U, dmMath::Min(300U, (uint32_t)ceilf(timing.m_Duration * 30.0f)));

        if (timing.m_PingPong && timing.m_FrameCount & 1)
        {
            ++timing.m_FrameCount;
        }
    }

    return timing;
}

static void ConvertBgraToRgbaAndUpdateBounds(Viewer* viewer, uint8_t* pixels, uint32_t width, uint32_t height, ExportBounds* bounds)
{
    const uint8_t background_red = (uint8_t)(viewer->m_Properties.m_BackgroundColor[0] * 255.0f);
    const uint8_t background_green = (uint8_t)(viewer->m_Properties.m_BackgroundColor[1] * 255.0f);
    const uint8_t background_blue = (uint8_t)(viewer->m_Properties.m_BackgroundColor[2] * 255.0f);

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            uint8_t* pixel = pixels + (y * width + x) * 4;
            const uint8_t blue = pixel[0];
            pixel[0] = pixel[2];
            pixel[2] = blue;

            if (abs((int)pixel[0] - background_red) <= 2 &&
                abs((int)pixel[1] - background_green) <= 2 &&
                abs((int)pixel[2] - background_blue) <= 2)
                continue;
            bounds->m_MinX = dmMath::Min(bounds->m_MinX, x);
            bounds->m_MinY = dmMath::Min(bounds->m_MinY, y);
            bounds->m_MaxX = dmMath::Max(bounds->m_MaxX, x);
            bounds->m_MaxY = dmMath::Max(bounds->m_MaxY, y);
        }
    }
}

static std::string ShellQuote(const char* value)
{
#if defined(_WIN32)
    std::string result = "\"";

    for (const char* cursor = value; *cursor; ++cursor)
    {
        if (*cursor == '"')
        {
            result += "\"\"";
        }
        else
        {
            result += *cursor;
        }
    }

    result += '"';
#else
    std::string result = "'";

    for (const char* cursor = value; *cursor; ++cursor)
    {
        if (*cursor == '\'')
        {
            result += "'\\''";
        }
        else
        {
            result += *cursor;
        }
    }

    result += '\'';
#endif

    return result;
}

static bool GetExportCrop(const ExportBounds& bounds, uint32_t width, uint32_t height, uint32_t requested_width, uint32_t requested_height, ExportCrop* crop)
{
    static const uint32_t padding = 24;

    if (requested_width || requested_height)
    {
        const uint32_t bounds_width = bounds.m_MaxX - bounds.m_MinX + 1;
        const uint32_t bounds_height = bounds.m_MaxY - bounds.m_MinY + 1;

        if (!requested_width || !requested_height || requested_width > width || requested_height > height || bounds_width > requested_width || bounds_height > requested_height)
        {
            return false;
        }

        const uint32_t center_x = bounds.m_MinX + bounds_width / 2;
        const uint32_t center_y = bounds.m_MinY + bounds_height / 2;
        crop->m_Width = requested_width;
        crop->m_Height = requested_height;
        crop->m_X = center_x > requested_width / 2 ? center_x - requested_width / 2 : 0;
        crop->m_Y = center_y > requested_height / 2 ? center_y - requested_height / 2 : 0;
        crop->m_X = dmMath::Min(crop->m_X, width - requested_width);
        crop->m_Y = dmMath::Min(crop->m_Y, height - requested_height);

        return true;
    }

    crop->m_X = bounds.m_MinX > padding ? bounds.m_MinX - padding : 0;
    crop->m_Y = bounds.m_MinY > padding ? bounds.m_MinY - padding : 0;
    const uint32_t right = dmMath::Min(width, bounds.m_MaxX + padding + 1);
    const uint32_t bottom = dmMath::Min(height, bounds.m_MaxY + padding + 1);
    crop->m_Width = right - crop->m_X;
    crop->m_Height = bottom - crop->m_Y;

    return true;
}

static bool WriteExportFrame(const char* directory, uint32_t frame_index, const uint8_t* pixels, uint32_t source_width, const ExportCrop& crop)
{
    dmArray<uint8_t> cropped_pixels;
    cropped_pixels.SetCapacity(crop.m_Width * crop.m_Height * 4);
    cropped_pixels.SetSize(crop.m_Width * crop.m_Height * 4);

    for (uint32_t y = 0; y < crop.m_Height; ++y)
    {
        memcpy(cropped_pixels.Begin() + y * crop.m_Width * 4,
               pixels + ((crop.m_Y + y) * source_width + crop.m_X) * 4,
               crop.m_Width * 4);
    }

    char path[1024];
    dmSnPrintf(path, sizeof(path), "%s/frame-%06u.png", directory, frame_index);

    return stbi_write_png(path, crop.m_Width, crop.m_Height, 4, cropped_pixels.Begin(), crop.m_Width * 4) != 0;
}

static bool EncodeExportFrames(const Viewer* viewer, const char* directory, const ExportTiming& timing)
{
    const uint32_t total_duration_ms = dmMath::Max(timing.m_FrameCount, (uint32_t)floorf(timing.m_Duration * 1000.0f + 0.5f));
    std::string    command = "img2webp -loop 0";
    char           option[128];

    for (uint32_t i = 0; i < timing.m_FrameCount; ++i)
    {
        const uint32_t frame_start_ms = (uint32_t)(((uint64_t)i * total_duration_ms + timing.m_FrameCount / 2) / timing.m_FrameCount);
        const uint32_t frame_end_ms = (uint32_t)(((uint64_t)(i + 1) * total_duration_ms + timing.m_FrameCount / 2) / timing.m_FrameCount);
        const uint32_t frame_duration_ms = dmMath::Max(1U, frame_end_ms - frame_start_ms);
        char frame_path[1024];
        dmSnPrintf(frame_path, sizeof(frame_path), "%s/frame-%06u.png", directory, i);
        dmSnPrintf(option, sizeof(option), " -d %u -lossless -m 6 ", frame_duration_ms);
        command += option;
        command += ShellQuote(frame_path);
    }

    command += " -o ";
    command += ShellQuote(viewer->m_OutputPath);
    const int result = system(command.c_str());

    if (result != 0)
    {
        dmLogError("Unable to encode '%s'. Install img2webp and ensure the output directory exists", viewer->m_OutputPath);

        return false;
    }

    return true;
}

static bool RenderExportPass(Viewer* viewer, const ExportTiming& timing, uint8_t* pixels, uint32_t pixel_buffer_size,
                             uint32_t width, uint32_t height, ExportBounds* output_bounds,
                             const char* output_directory, const ExportCrop* crop)
{
    ((TextLayout*)viewer->m_Layouts[0])->m_ElapsedTime = 0.0;

    if (!UploadRenderData(viewer))
    {
        return false;
    }

    ExportBounds ignored_bounds;
    ExportBounds* bounds = output_bounds ? output_bounds : &ignored_bounds;

    if (timing.m_PingPong)
    {
        const uint32_t half_frame_count = timing.m_FrameCount / 2;
        const float    delta_time = (timing.m_Duration * 0.5f) / half_frame_count;

        for (uint32_t i = 0; i <= half_frame_count; ++i)
        {
            if (i)
            {
                TextLayoutUpdate(viewer->m_Layouts[0], delta_time);

                if (!UploadRenderData(viewer))
                {
                    return false;
                }
            }

            Draw(viewer, pixels, pixel_buffer_size);
            ConvertBgraToRgbaAndUpdateBounds(viewer, pixels, width, height, bounds);

            if (crop && !WriteExportFrame(output_directory, i, pixels, width, *crop))
            {
                return false;
            }

            if (crop && i > 0 && i < half_frame_count &&
                !WriteExportFrame(output_directory, timing.m_FrameCount - i, pixels, width, *crop))

                return false;
        }
    }
    else
    {
        const float delta_time = timing.m_Duration / timing.m_FrameCount;

        for (uint32_t i = 0; i < timing.m_FrameCount; ++i)
        {
            if (i)
            {
                TextLayoutUpdate(viewer->m_Layouts[0], delta_time);

                if (!UploadRenderData(viewer))
                {
                    return false;
                }
            }

            Draw(viewer, pixels, pixel_buffer_size);
            ConvertBgraToRgbaAndUpdateBounds(viewer, pixels, width, height, bounds);

            if (crop && !WriteExportFrame(output_directory, i, pixels, width, *crop))
            {
                return false;
            }
        }
    }

    return true;
}

static bool ExportDocumentationImage(Viewer* viewer)
{
    if (viewer->m_Layouts.Empty())
    {
        return false;
    }

    ExportTiming timing = GetExportTiming(viewer->m_Layouts[0], viewer->m_RequestedFrameCount);

    if (timing.m_Animated && timing.m_FrameCount < 2)
    {
        dmLogError("Animated exports require at least two frames");

        return false;
    }

    if (timing.m_PingPong && timing.m_FrameCount & 1)
    {
        dmLogError("Shake exports require an even --frames value so the deterministic motion can play forward and backward without a seam");

        return false;
    }

    if (!timing.m_Animated && viewer->m_RequestedFrameCount > 1)
    {
        dmLogWarning("The style has no active animation; exporting one frame");
    }

    char temporary_directory[256];
    dmSnPrintf(temporary_directory, sizeof(temporary_directory), ".fontviewer-export-%llu", (unsigned long long)dmTime::GetMonotonicTime());

    if (dmSys::Mkdir(temporary_directory, 0755) != dmSys::RESULT_OK)
    {
        dmLogError("Unable to create temporary export directory '%s'", temporary_directory);

        return false;
    }

    const uint32_t width = dmGraphics::GetWindowWidth(viewer->m_Context);
    const uint32_t height = dmGraphics::GetWindowHeight(viewer->m_Context);
    const uint32_t pixel_buffer_size = width * height * 4;
    dmArray<uint8_t> pixels;
    pixels.SetCapacity(pixel_buffer_size);
    pixels.SetSize(pixel_buffer_size);
    ExportBounds bounds;
    bool         success = RenderExportPass(viewer, timing, pixels.Begin(), pixel_buffer_size, width, height, &bounds, 0, 0);

    if (success && bounds.m_MinX == 0xffffffff)
    {
        dmLogError("The rendered export contains no pixels that differ from the background");
        success = false;
    }

    if (success)
    {
        ExportCrop crop;

        if (!GetExportCrop(bounds, width, height, viewer->m_ExportCanvasWidth, viewer->m_ExportCanvasHeight, &crop))
        {
            dmLogError("The requested export canvas does not fit the rendered content or framebuffer");
            success = false;
        }
        else
        {
            success = RenderExportPass(viewer, timing, pixels.Begin(), pixel_buffer_size, width, height, 0, temporary_directory, &crop) &&
                      EncodeExportFrames(viewer, temporary_directory, timing);
        }
    }

    dmSys::RmTree(temporary_directory);

    if (success)
    {
        dmLogInfo("Exported %u frame%s over %.3f seconds to '%s'", timing.m_FrameCount, timing.m_FrameCount == 1 ? "" : "s", timing.m_Duration, viewer->m_OutputPath);
    }

    return success;
}

extern "C" void dmExportedSymbols();

int             main(int argc, char** argv)
{
    TestMainPlatformInit();
    dmExportedSymbols();
    dmLog::LogParams log_params;
    dmLog::LogInitialize(&log_params);
    if (!dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL))
    {
        dmLogError("Unable to install the OpenGL graphics adapter");
        return 1;
    }

    Viewer viewer;
    if (!ParseArguments(&viewer, argc, argv) || !InitializeEditorText(&viewer))
    {
        Finalize(&viewer);
        return 1;
    }
    if (!Initialize(&viewer))
    {
        dmLogError("Unable to initialize fontviewer");
        Finalize(&viewer);
        return 1;
    }
    if (viewer.m_ExportMode)
    {
        const bool export_result = ExportDocumentationImage(&viewer);
        Finalize(&viewer);

        return export_result ? 0 : 1;
    }
    const bool auto_exit = getenv("DEFOLD_TEST_AUTO_EXIT") != 0;
    uint32_t   frame_count = 0;
    uint64_t   previous_frame_time = dmTime::GetMonotonicTime();
    while (!viewer.m_Closed)
    {
        const uint64_t frame_time = dmTime::GetMonotonicTime();
        const float    delta_time = (frame_time - previous_frame_time) / 1000000.0f;
        previous_frame_time = frame_time;
        dmPlatform::PollEvents(viewer.m_Window);
        HandleInput(&viewer);
        // Process only the heaviest requested update. Each heavier path also
        // subsumes and clears the lighter pending work.
        if (viewer.m_RebuildRequested)
        {
            viewer.m_RebuildRequested = false;
            viewer.m_RenderUpdateRequested = false;
            if (!Rebuild(&viewer))
            {
                dmLogError("Unable to rebuild edited text");
                viewer.m_Closed = true;
                continue;
            }
        }
        else if (viewer.m_RenderUpdateRequested)
        {
            viewer.m_RenderUpdateRequested = false;
            if (!RefreshRenderData(&viewer) && !Rebuild(&viewer))
            {
                dmLogError("Unable to refresh font rendering");
                viewer.m_Closed = true;
                continue;
            }
        }
        if (viewer.m_ScrollCaretIntoViewRequested)
        {
            viewer.m_ScrollCaretIntoViewRequested = false;
            if (ScrollEditorCaretIntoView(&viewer) && !RefreshRenderData(&viewer) && !Rebuild(&viewer))
            {
                dmLogError("Unable to scroll edited text to the caret");
                viewer.m_Closed = true;
                continue;
            }
        }
        if (!viewer.m_Layouts.Empty() && HasAnimatedEffects(viewer.m_Layouts[0]))
        {
            TextLayoutUpdate(viewer.m_Layouts[0], delta_time);

            if (!UploadRenderData(&viewer))
            {
                dmLogError("Unable to animate font rendering");
                viewer.m_Closed = true;
                continue;
            }
        }

        Draw(&viewer, 0, 0);
        if (auto_exit && ++frame_count == 2)
            viewer.m_Closed = true;
    }
    Finalize(&viewer);
    return 0;
}
