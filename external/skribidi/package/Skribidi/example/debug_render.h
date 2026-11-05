// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef DEBUG_RENDER_H
#define DEBUG_RENDER_H

#include "render.h"

/**
 * Debug shape rendering on top of the render_context_t.
 */

typedef enum {
	RENDER_ALIGN_START,
	RENDER_ALIGN_CENTER,
	RENDER_ALIGN_END,
} render_align_t;

/** Renders a tick (cross) shape. */
void debug_render_tick(render_context_t* rc, float x, float y, float s, skb_color_t col, float line_width);

/** Renders a line. */
void debug_render_line(render_context_t* rc, float x0, float y0, float x1, float y1, skb_color_t col, float line_width);

/** Renders a dashed line. */
void debug_render_dashed_line(render_context_t* rc, float x0, float y0, float x1, float y1, float dash, skb_color_t col, float line_width);

/** Renders a stroked rectangle. */
void debug_render_stroked_rect(render_context_t* rc, float x, float y, float w, float h, skb_color_t col, float line_width);

/** Renders a stroked rectangle. */
void debug_render_dashed_rect(render_context_t* rc, float x, float y, float w, float h, float dash, skb_color_t col, float line_width);

/** Renders a filled rectangle. */
void debug_render_filled_rect(render_context_t* rc, float x, float y, float w, float h, skb_color_t col);

/** Renders a triangle */
void debug_render_tri(render_context_t* rc, float x0, float y0, float x1, float y1, float x2, float y2, skb_color_t col);

/** Renders text in debug font. */
float debug_render_text(render_context_t* rc, float x, float y, float size, render_align_t align, skb_color_t col, const char* fmt, ...);

/** Returns width of text. */
float debug_render_text_width(render_context_t* rc, float size, const char* fmt, ...);

/** Renders debug overlay of the image atlas on render context. */
void debug_render_atlas_overlay(render_context_t* rc, float sx, float sy, float scale, int32_t columns);

typedef struct skb_layout_t skb_layout_t;

void debug_render_layout(render_context_t* rc, float x, float y, const skb_layout_t* layout);
void debug_render_layout_lines(render_context_t* rc, float x, float y, const skb_layout_t* layout);
void debug_render_layout_runs(render_context_t* rc, float x, float y, const skb_layout_t* layout);
void debug_render_layout_glyphs(render_context_t* rc, float x, float y, const skb_layout_t* layout);

#endif // DEBUG_RENDER_H
