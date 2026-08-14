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

#include "fontviewer_nuklear.h"
#include "fontviewer_macos.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nuklear v4.13.3, upstream commit 0dbc52f86404f9e1f26ce0df3015ed23ff54a726.
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

static struct nk_context            g_Context;
static struct nk_font_atlas         g_FontAtlas;
static struct nk_font*              g_Font;
static struct nk_draw_null_texture  g_NullTexture;
static struct nk_buffer             g_DrawCommands;
static struct nk_buffer             g_Vertices;
static struct nk_buffer             g_Indices;
static uint8_t                      g_DrawCommandMemory[64 * 1024];
static uint8_t                      g_VertexMemory[512 * 1024];
static uint8_t                      g_IndexMemory[128 * 1024];
static FontViewerNuklearDrawCommand g_RenderCommands[FONT_VIEWER_NUKLEAR_MAX_DRAW_COMMANDS];
static uint8_t*                     g_AtlasPixels;
static uint32_t                     g_AtlasWidth;
static uint32_t                     g_AtlasHeight;
static bool                         g_Initialized;

static void CopyToClipboard(nk_handle window, const char* text, int length)
{
    FontViewerMacOSSetClipboard((HWindow)window.ptr, text, (uint32_t)length);
}

static void                         SetFont(float height)
{
    g_Font->handle.height = height;
    nk_style_set_font(&g_Context, &g_Font->handle);
}

static void SetStyle(void)
{
    struct nk_color colors[NK_COLOR_COUNT];
    colors[NK_COLOR_TEXT] = nk_rgb(238, 240, 244);
    colors[NK_COLOR_WINDOW] = nk_rgb(30, 34, 41);
    colors[NK_COLOR_HEADER] = nk_rgb(38, 44, 53);
    colors[NK_COLOR_BORDER] = nk_rgb(75, 85, 99);
    colors[NK_COLOR_BUTTON] = nk_rgb(48, 55, 66);
    colors[NK_COLOR_BUTTON_HOVER] = nk_rgb(61, 72, 87);
    colors[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(46, 112, 184);
    colors[NK_COLOR_TOGGLE] = nk_rgb(22, 26, 32);
    colors[NK_COLOR_TOGGLE_HOVER] = nk_rgb(38, 45, 55);
    colors[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(72, 151, 255);
    colors[NK_COLOR_SELECT] = nk_rgb(72, 151, 255);
    colors[NK_COLOR_SELECT_ACTIVE] = nk_rgb(91, 164, 255);
    colors[NK_COLOR_SLIDER] = nk_rgb(18, 22, 28);
    colors[NK_COLOR_SLIDER_CURSOR] = nk_rgb(72, 151, 255);
    colors[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(91, 164, 255);
    colors[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(42, 127, 235);
    colors[NK_COLOR_PROPERTY] = nk_rgb(18, 22, 28);
    colors[NK_COLOR_EDIT] = nk_rgb(15, 18, 23);
    colors[NK_COLOR_EDIT_CURSOR] = nk_rgb(230, 233, 238);
    colors[NK_COLOR_COMBO] = nk_rgb(38, 44, 53);
    colors[NK_COLOR_CHART] = nk_rgb(22, 26, 32);
    colors[NK_COLOR_CHART_COLOR] = nk_rgb(72, 151, 255);
    colors[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(255, 90, 70);
    colors[NK_COLOR_SCROLLBAR] = nk_rgb(22, 26, 32);
    colors[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(76, 88, 104);
    colors[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(98, 113, 133);
    colors[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(72, 151, 255);
    colors[NK_COLOR_TAB_HEADER] = nk_rgb(38, 44, 53);
    colors[NK_COLOR_KNOB] = nk_rgb(72, 151, 255);
    colors[NK_COLOR_KNOB_CURSOR] = nk_rgb(230, 233, 238);
    colors[NK_COLOR_KNOB_CURSOR_HOVER] = nk_rgb(255, 255, 255);
    colors[NK_COLOR_KNOB_CURSOR_ACTIVE] = nk_rgb(255, 255, 255);
    nk_style_from_table(&g_Context, colors);
    g_Context.style.window.padding = nk_vec2(16.0f, 14.0f);
    g_Context.style.window.spacing = nk_vec2(8.0f, 7.0f);
    // The viewer lays out every control within the fixed-width side panel and
    // only needs vertical scrolling for the text editor and panel contents.
    g_Context.style.window.scrollbar_size.y = 0.0f;
    g_Context.style.window.border = 1.0f;
    g_Context.style.window.rounding = 0.0f;
    g_Context.style.button.rounding = 2.0f;
    g_Context.style.slider.rounding = 2.0f;
    g_Context.style.slider.cursor_size = nk_vec2(14.0f, 18.0f);
    g_Context.style.edit.rounding = 2.0f;
    g_Context.style.property.rounding = 2.0f;
}

bool FontViewerNuklearInitialize(HWindow window, uint32_t width, uint32_t height)
{
    (void)width;
    (void)height;
    nk_font_atlas_init_default(&g_FontAtlas);
    nk_font_atlas_begin(&g_FontAtlas);
    struct nk_font_config font_config = nk_font_config(18.0f);
    font_config.oversample_h = 2;
    font_config.oversample_v = 2;
    g_Font = nk_font_atlas_add_default(&g_FontAtlas, 18.0f, &font_config);
    int         atlas_width = 0;
    int         atlas_height = 0;
    const void* atlas_pixels = nk_font_atlas_bake(&g_FontAtlas, &atlas_width, &atlas_height, NK_FONT_ATLAS_RGBA32);
    if (!g_Font || !atlas_pixels || atlas_width <= 0 || atlas_height <= 0)
        return false;

    const size_t atlas_size = (size_t)atlas_width * atlas_height * 4;
    g_AtlasPixels = (uint8_t*)malloc(atlas_size);
    if (!g_AtlasPixels)
        return false;
    memcpy(g_AtlasPixels, atlas_pixels, atlas_size);
    g_AtlasWidth = (uint32_t)atlas_width;
    g_AtlasHeight = (uint32_t)atlas_height;
    nk_font_atlas_end(&g_FontAtlas, nk_handle_id(1), &g_NullTexture);

    g_Initialized = nk_init_default(&g_Context, &g_Font->handle) != 0;
    if (!g_Initialized)
        return false;
    g_Context.clip.userdata.ptr = window;
    g_Context.clip.copy = CopyToClipboard;
    nk_buffer_init_fixed(&g_DrawCommands, g_DrawCommandMemory, sizeof(g_DrawCommandMemory));
    nk_buffer_init_fixed(&g_Vertices, g_VertexMemory, sizeof(g_VertexMemory));
    nk_buffer_init_fixed(&g_Indices, g_IndexMemory, sizeof(g_IndexMemory));
    SetStyle();
    return true;
}

void FontViewerNuklearFinalize(void)
{
    if (g_Initialized)
    {
        nk_free(&g_Context);
    }
    nk_font_atlas_clear(&g_FontAtlas);
    free(g_AtlasPixels);
    memset(&g_Context, 0, sizeof(g_Context));
    memset(&g_FontAtlas, 0, sizeof(g_FontAtlas));
    g_Font = 0;
    g_AtlasPixels = 0;
    g_AtlasWidth = 0;
    g_AtlasHeight = 0;
    g_Initialized = false;
}

bool FontViewerNuklearGetAtlas(const void** pixels, uint32_t* width, uint32_t* height)
{
    if (!g_AtlasPixels)
        return false;
    *pixels = g_AtlasPixels;
    *width = g_AtlasWidth;
    *height = g_AtlasHeight;
    return true;
}

static FontViewerNuklearBox ToBox(struct nk_rect rect)
{
    FontViewerNuklearBox box = { rect.x, rect.y, rect.w, rect.h };
    return box;
}

static float EstimateTextContentHeight(const char* text, float width)
{
    const float glyph_width = 14.0f * 0.55f;
    float       line_width = 0.0f;
    uint32_t    line_count = 1;
    const char* cursor = text;
    while (*cursor)
    {
        if (*cursor == '\n')
        {
            ++line_count;
            line_width = 0.0f;
            ++cursor;
            continue;
        }
        const char* word_end = cursor;
        while (*word_end && *word_end != ' ' && *word_end != '\n')
            ++word_end;
        const float word_width = (float)(word_end - cursor) * glyph_width;
        if (line_width > 0.0f && line_width + word_width > width)
        {
            ++line_count;
            line_width = 0.0f;
        }
        line_width += word_width;
        if (*word_end == ' ')
        {
            line_width += glyph_width;
            cursor = word_end + 1;
        }
        else
        {
            cursor = word_end;
        }
    }
    return line_count * 19.0f;
}

static void DrawSlider(const char* label, float minimum, float* value, float maximum, float step)
{
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%.2f", *value);
    nk_layout_row_begin(&g_Context, NK_STATIC, 26.0f, 3);
    nk_layout_row_push(&g_Context, 104.0f);
    nk_label(&g_Context, label, NK_TEXT_LEFT);
    nk_layout_row_push(&g_Context, 154.0f);
    nk_slider_float(&g_Context, minimum, value, maximum, step);
    nk_layout_row_push(&g_Context, 54.0f);
    nk_label(&g_Context, value_text, NK_TEXT_RIGHT);
    nk_layout_row_end(&g_Context);
}

static void DrawColorPicker(const char* label, float* color)
{
    struct nk_colorf picker = { color[0], color[1], color[2], 1.0f };
    nk_layout_row_begin(&g_Context, NK_STATIC, 26.0f, 2);
    nk_layout_row_push(&g_Context, 104.0f);
    nk_label(&g_Context, label, NK_TEXT_LEFT);
    nk_layout_row_push(&g_Context, 64.0f);
    if (nk_combo_begin_color(&g_Context, nk_rgb_cf(picker), nk_vec2(220.0f, 180.0f)))
    {
        nk_layout_row_dynamic(&g_Context, 150.0f, 1);
        picker = nk_color_picker(&g_Context, picker, NK_RGB);
        nk_combo_end(&g_Context);
    }
    nk_layout_row_end(&g_Context);
    color[0] = picker.r;
    color[1] = picker.g;
    color[2] = picker.b;
}

static void ConvertCommands(FontViewerNuklearLayout* layout)
{
    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        { NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(FontViewerNuklearVertex, m_Position) },
        { NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(FontViewerNuklearVertex, m_TexCoord) },
        { NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(FontViewerNuklearVertex, m_Color) },
        { NK_VERTEX_ATTRIBUTE_COUNT, NK_FORMAT_COUNT, 0 },
    };
    struct nk_convert_config config;
    memset(&config, 0, sizeof(config));
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(FontViewerNuklearVertex);
    config.vertex_alignment = NK_ALIGNOF(FontViewerNuklearVertex);
    config.tex_null = g_NullTexture;
    config.circle_segment_count = 22;
    config.curve_segment_count = 22;
    config.arc_segment_count = 22;
    config.global_alpha = 1.0f;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;

    nk_buffer_clear(&g_DrawCommands);
    nk_buffer_clear(&g_Vertices);
    nk_buffer_clear(&g_Indices);
    const nk_flags result = nk_convert(&g_Context, &g_DrawCommands, &g_Vertices, &g_Indices, &config);
    if (result != NK_CONVERT_SUCCESS)
        return;

    layout->m_Vertices = (const FontViewerNuklearVertex*)nk_buffer_memory_const(&g_Vertices);
    layout->m_Indices = (const uint16_t*)nk_buffer_memory_const(&g_Indices);
    layout->m_DrawCommands = g_RenderCommands;
    layout->m_VertexDataSize = (uint32_t)g_Vertices.allocated;
    layout->m_IndexDataSize = (uint32_t)g_Indices.allocated;
    const struct nk_draw_command* command;
    nk_draw_foreach(command, &g_Context, &g_DrawCommands)
    {
        if (!command->elem_count || layout->m_DrawCommandCount == FONT_VIEWER_NUKLEAR_MAX_DRAW_COMMANDS)
            continue;
        FontViewerNuklearDrawCommand* output = &g_RenderCommands[layout->m_DrawCommandCount++];
        output->m_Clip = ToBox(command->clip_rect);
        output->m_ElementCount = command->elem_count;
    }
}

void FontViewerNuklearBuild(uint32_t width, uint32_t height, const char* text, float text_content_height, const FontViewerNuklearInput* input, float* text_scroll_y, float* font_size, float* zoom, FontViewerProperties* properties, FontViewerNuklearFonts* fonts, bool* legacy_layout, bool* show_baselines, bool* show_quads, FontViewerNuklearLayout* layout)
{
    const float panel_width = 380.0f;
    const float panel_x = (float)width - panel_width;

    memset(layout, 0, sizeof(*layout));
    nk_clear(&g_Context);
    nk_input_begin(&g_Context);
    if (input)
    {
        nk_input_motion(&g_Context, input->m_MouseX, input->m_MouseY);
        nk_input_button(&g_Context, NK_BUTTON_LEFT, input->m_MouseX, input->m_MouseY, input->m_LeftMouseDown);
        nk_input_key(&g_Context, NK_KEY_COPY, input->m_CopyDown);
        nk_input_key(&g_Context, NK_KEY_TEXT_SELECT_ALL, input->m_SelectAllDown);
        if (input->m_ScrollY != 0.0f)
            nk_input_scroll(&g_Context, nk_vec2(0.0f, input->m_ScrollY));
    }
    nk_input_end(&g_Context);

    SetFont(15.0f);
    if (nk_begin(&g_Context, "Font Viewer", nk_rect(panel_x, 0.0f, panel_width, (float)height), NK_WINDOW_BORDER))
    {
        struct nk_rect text_field;
        nk_uint        scroll_x = 0;
        nk_uint        scroll_y = (nk_uint)fmaxf(0.0f, *text_scroll_y);

        SetFont(16.0f);
        nk_layout_row_dynamic(&g_Context, 26.0f, 1);
        if (nk_tree_push(&g_Context, NK_TREE_TAB, "Text", NK_MINIMIZED))
        {
            SetFont(14.0f);
            nk_layout_row_dynamic(&g_Context, (float)height * 0.25f, 1);
            text_field = nk_widget_bounds(&g_Context);
            layout->m_TextField = ToBox(text_field);
            if (nk_group_scrolled_offset_begin(&g_Context, &scroll_x, &scroll_y, "Editor", NK_WINDOW_BORDER))
            {
                struct nk_rect content_region = nk_window_get_content_region(&g_Context);
                layout->m_TextViewportHeight = content_region.h;
                layout->m_TextContentHeight = text_content_height > 0.0f ? text_content_height : EstimateTextContentHeight(text, content_region.w);
                nk_layout_row_static(&g_Context, fmaxf(layout->m_TextViewportHeight, layout->m_TextContentHeight), 1.0f, 1);
                nk_group_scrolled_end(&g_Context);
            }
            *text_scroll_y = (float)scroll_y;
            nk_tree_pop(&g_Context);
        }

        SetFont(16.0f);
        nk_layout_row_dynamic(&g_Context, 26.0f, 1);
        if (nk_tree_push(&g_Context, NK_TREE_TAB, "Properties", NK_MAXIMIZED))
        {
            SetFont(15.0f);
            nk_layout_row_dynamic(&g_Context, 32.0f, 1);
            nk_property_float(&g_Context, "Size", 1.0f, font_size, 512.0f, 1.0f, 0.25f);

            SetFont(14.0f);
            DrawSlider("Alpha", 0.0f, &properties->m_Alpha, 1.0f, 0.01f);
            DrawSlider("Outline alpha", 0.0f, &properties->m_OutlineAlpha, 1.0f, 0.01f);
            DrawSlider("Outline width", 0.0f, &properties->m_OutlineWidth, 16.0f, 0.1f);
            DrawSlider("Shadow alpha", 0.0f, &properties->m_ShadowAlpha, 1.0f, 0.01f);
            DrawSlider("Shadow blur", 0.0f, &properties->m_ShadowBlur, 16.0f, 0.1f);
            DrawSlider("Shadow X", -64.0f, &properties->m_ShadowX, 64.0f, 0.25f);
            DrawSlider("Shadow Y", -64.0f, &properties->m_ShadowY, 64.0f, 0.25f);

            DrawColorPicker("Face color", properties->m_FaceColor);
            DrawColorPicker("Outline color", properties->m_OutlineColor);
            DrawColorPicker("Shadow color", properties->m_ShadowColor);
            nk_tree_pop(&g_Context);
        }

        SetFont(16.0f);
        nk_layout_row_dynamic(&g_Context, 26.0f, 1);
        if (nk_tree_push(&g_Context, NK_TREE_TAB, "Debug", NK_MAXIMIZED))
        {
            SetFont(14.0f);
            nk_layout_row_begin(&g_Context, NK_STATIC, 28.0f, 4);
            nk_layout_row_push(&g_Context, 42.0f);
            nk_label(&g_Context, "Zoom", NK_TEXT_LEFT);
            nk_layout_row_push(&g_Context, 160.0f);
            nk_slider_float(&g_Context, FONT_VIEWER_ZOOM_MIN, zoom, FONT_VIEWER_ZOOM_MAX, 0.01f);
            char zoom_text[32];
            snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", *zoom * 100.0f);
            nk_layout_row_push(&g_Context, 50.0f);
            nk_label(&g_Context, zoom_text, NK_TEXT_RIGHT);
            nk_layout_row_push(&g_Context, 58.0f);
            if (nk_button_label(&g_Context, "Reset"))
                *zoom = 1.0f;
            nk_layout_row_end(&g_Context);

            nk_bool legacy = *legacy_layout;
            nk_bool baselines = *show_baselines;
            nk_bool quads = *show_quads;
            nk_layout_row_dynamic(&g_Context, 28.0f, 1);
            nk_checkbox_label(&g_Context, "Legacy layout", &legacy);
            nk_layout_row_dynamic(&g_Context, 28.0f, 1);
            nk_checkbox_label(&g_Context, "Show base lines", &baselines);
            nk_layout_row_dynamic(&g_Context, 28.0f, 1);
            nk_checkbox_label(&g_Context, "Show glyph quads", &quads);
            *legacy_layout = legacy != 0;
            *show_baselines = baselines != 0;
            *show_quads = quads != 0;
            DrawColorPicker("Background", properties->m_BackgroundColor);

            nk_layout_row_dynamic(&g_Context, 24.0f, 1);
            if (nk_tree_push(&g_Context, NK_TREE_NODE, "Loaded fonts", NK_MAXIMIZED))
            {
                SetFont(12.0f);
                char summary[64];
                snprintf(summary, sizeof(summary), "%u fonts, %.1f MB", fonts->m_LoadedFontCount,
                         fonts->m_LoadedFontDataSize / (1024.0f * 1024.0f));
                nk_layout_row_dynamic(&g_Context, 18.0f, 1);
                nk_label(&g_Context, summary, NK_TEXT_LEFT);
                float field_height = fonts->m_LoadedFontCount * 18.0f + 8.0f;
                nk_layout_row_dynamic(&g_Context, field_height, 1);
                nk_edit_string_zero_terminated(&g_Context, NK_EDIT_BOX | NK_EDIT_READ_ONLY,
                                               fonts->m_LoadedFontText,
                                               (int)fonts->m_LoadedFontTextSize,
                                               nk_filter_default);
                nk_tree_pop(&g_Context);
            }
            nk_tree_pop(&g_Context);
        }
    }
    nk_end(&g_Context);
    layout->m_ContentWidth = panel_x;
    ConvertCommands(layout);
}
