// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "debug_render.h"
#include <stdarg.h>
#include <stdio.h>
#include "skb_image_atlas.h"
#include "skb_common.h"
#include "skb_layout.h"

static skb_vec2_t rot90(skb_vec2_t v)
{
	return (skb_vec2_t) { .x = v.y, .y = -v.x, };
}

static void render__line_strip(render_context_t* rc, const skb_vec2_t* pts, int32_t pts_count, skb_color_t col, float line_width)
{
	if (pts_count < 2) return;

	static skb_vec2_t dirs[64];
	pts_count = skb_mini(pts_count, 64);

	const bool is_loop = skb_vec2_equals(pts[0], pts[pts_count-1], 0.01f);

	// Calculate segment directions
	for (int32_t i = 0; i < pts_count - 1; i++)
		dirs[i] = skb_vec2_norm(skb_vec2_sub(pts[i+1], pts[i]));
	if (!is_loop)
		dirs[pts_count-1] = dirs[pts_count-2];
	else
		dirs[pts_count-1] = skb_vec2_norm(skb_vec2_sub(pts[1], pts[pts_count-1])); // First & last are the same, so pick the next point.

	const float hw = line_width * 0.5f;

	bool has_prev = false;
	skb_vec2_t prev_left = {0}, prev_right = {0}, left = {0}, right = {0};

	static render_vert_t verts[64 * 6];
	int32_t verts_count = 0;

	// Start cap
	if (!is_loop) {
		skb_vec2_t p = pts[0];
		skb_vec2_t dir = dirs[0];
		skb_vec2_t off = rot90(dir);
		prev_left.x = p.x - dir.x * hw + off.x * hw;
		prev_left.y = p.y - dir.y * hw + off.y * hw;
		prev_right.x = p.x - dir.x * hw - off.x * hw;
		prev_right.y = p.y - dir.y * hw - off.y * hw;
		has_prev = true;
	}

	int32_t start = 0;
	int32_t count = 0;
	int32_t pi = 0;
	if (!is_loop) {
		start = 1;
		count = pts_count - 1;
		pi = 0;
	} else {
		start = 0;
		count = pts_count;
		pi = pts_count - 2; // First & last are the same, so pick the previous point.
	}

	for (int32_t i = start; i < count; i++) {
		skb_vec2_t p1 = pts[i];
		skb_vec2_t dir0 = dirs[pi];
		skb_vec2_t dir1 = dirs[i];

		// Calculate extrusions
		skb_vec2_t off0 = rot90(dir0);
		skb_vec2_t off1 = rot90(dir1);
		skb_vec2_t off = skb_vec2_lerp(off0, off1, 0.5f);
		const float dmr2 = skb_vec2_dot(off, off);
		if (dmr2 > 0.000001f) {
			float scale = 1.0f / dmr2;
			if (scale > 20.0f)
				scale = 20.0f;
			off = skb_vec2_scale(off, scale);
		}

		left.x = p1.x + off.x * hw;
		left.y = p1.y + off.y * hw;
		right.x = p1.x - off.x * hw;
		right.y = p1.y - off.y * hw;

		if (has_prev) {
			verts[verts_count++] = (render_vert_t){ .pos = prev_left, .col = col };
			verts[verts_count++] = (render_vert_t){ .pos = left, .col = col };
			verts[verts_count++] = (render_vert_t){ .pos = right, .col = col };
			verts[verts_count++] = (render_vert_t){ .pos = prev_left, .col = col };
			verts[verts_count++] = (render_vert_t){ .pos = right, .col = col };
			verts[verts_count++] = (render_vert_t){ .pos = prev_right, .col = col };
		}

		prev_left = left;
		prev_right = right;
		has_prev = true;

		pi = i;
	}

	// End cap
	if (!is_loop) {
		skb_vec2_t p = pts[pts_count-1];
		skb_vec2_t dir = dirs[pts_count-2];
		skb_vec2_t off = rot90(dir);
		left.x = p.x + dir.x * hw + off.x * hw;
		left.y = p.y + dir.y * hw + off.y * hw;
		right.x = p.x + dir.x * hw - off.x * hw;
		right.y = p.y + dir.y * hw - off.y * hw;

		verts[verts_count++] = (render_vert_t){ .pos = prev_left, .col = col };
		verts[verts_count++] = (render_vert_t){ .pos = left, .col = col };
		verts[verts_count++] = (render_vert_t){ .pos = right, .col = col };
		verts[verts_count++] = (render_vert_t){ .pos = prev_left, .col = col };
		verts[verts_count++] = (render_vert_t){ .pos = right, .col = col };
		verts[verts_count++] = (render_vert_t){ .pos = prev_right, .col = col };
	}

	render_draw_debug_tris(rc, verts, verts_count);
}

void debug_render_tick(render_context_t* rc, float x, float y, float s, skb_color_t col, float line_width)
{
	if (line_width < 0.f) line_width = -line_width / render_get_transform_scale(rc);

	const float hw = line_width * 0.5f;
	float hs = s * 0.5f + hw;
	debug_render_filled_rect(rc, x-hs, y-hw, s + line_width, line_width, col);
	debug_render_filled_rect(rc, x-hw, y-hs, line_width, s + line_width, col);
}


static void render__line(render_context_t* rc, skb_vec2_t p, skb_vec2_t dir, float d0, float d1, float hw, skb_color_t col)
{
	const skb_vec2_t off = rot90(dir);

	const skb_vec2_t p0_left = {
		.x = p.x + dir.x * d0 + off.x * hw,
		.y = p.y + dir.y * d0 + off.y * hw
	};
	const skb_vec2_t p0_right = {
		.x = p.x + dir.x * d0 - off.x * hw,
		.y = p.y + dir.y * d0 - off.y * hw
	};
	const skb_vec2_t p1_left = {
		.x = p.x + dir.x * d1 + off.x * hw,
		.y = p.y + dir.y * d1 + off.y * hw
	};
	const skb_vec2_t p1_right = {
		.x = p.x + dir.x * d1 - off.x * hw,
		.y = p.y + dir.y * d1 - off.y * hw
	};

	render_vert_t verts[] = {
		{ p0_left, col },
		{ p1_left, col },
		{ p1_right, col },
		{ p0_left, col },
		{ p1_right, col },
		{ p0_right, col },
	};
	render_draw_debug_tris(rc, verts, SKB_COUNTOF(verts));
}


void debug_render_line(render_context_t* rc, float x0, float y0, float x1, float y1, skb_color_t col, float line_width)
{
	if (line_width < 0.f) line_width = -line_width / render_get_transform_scale(rc);

	const skb_vec2_t p0 = { x0, y0 };
	const skb_vec2_t p1 = { x1, y1 };
	const skb_vec2_t diff = skb_vec2_sub(p1, p0);
	const skb_vec2_t dir = skb_vec2_norm(diff);
	const float len = skb_vec2_length(diff);
	const float hw = line_width * 0.5f;

	render__line(rc, p0, dir, -hw, len + hw, hw, col);
}

void debug_render_dashed_line(render_context_t* rc, float x0, float y0, float x1, float y1, float dash, skb_color_t col, float line_width)
{
	if (line_width < 0.f) line_width = -line_width / render_get_transform_scale(rc);
	if (dash < 0.f) dash = -dash / render_get_transform_scale(rc);

	const skb_vec2_t p0 = { x0, y0 };
	const skb_vec2_t p1 = { x1, y1 };
	const skb_vec2_t diff = skb_vec2_sub(p1, p0);
	const skb_vec2_t dir = skb_vec2_norm(diff);
	const float len = skb_vec2_length(diff) + line_width;
	const float hw = line_width * 0.5f;

	const int32_t tick_count = skb_clampi((int32_t)floorf(len / dash) | 1, 1, 1000);
	const float d = len / (float)tick_count;
	const skb_vec2_t p = { x0 - dir.x * hw, y0 - dir.x * hw };

	for (int i = 0; i < tick_count; i += 2) {
		const float d0 = (float)i * d;
		const float d1 = d0 + d;
		render__line(rc, p, dir, d0, d1, hw, col);
	}
}

void debug_render_stroked_rect(render_context_t* rc, float x, float y, float w, float h, skb_color_t col, float line_width)
{
	if (line_width < 0.f) line_width = -line_width / render_get_transform_scale(rc);
	const float hw = line_width * 0.5f;
	debug_render_filled_rect(rc, x-hw, y-hw, w+line_width, line_width, col);
	debug_render_filled_rect(rc, x-hw, y+hw, line_width, h-line_width, col);
	debug_render_filled_rect(rc, x+w-hw, y+hw, line_width, h-line_width, col);
	debug_render_filled_rect(rc, x-hw, y+h-hw, w+line_width, line_width, col);
}

void debug_render_dashed_rect(render_context_t* rc, float x, float y, float w, float h, float dash, skb_color_t col, float line_width)
{
	debug_render_dashed_line(rc, x, y, x+w, y, dash, col, line_width);
	debug_render_dashed_line(rc, x+w, y, x+w, y+h, dash, col, line_width);
	debug_render_dashed_line(rc, x+w, y+h, x, y+h, dash, col, line_width);
	debug_render_dashed_line(rc, x, y+h, x, y, dash, col, line_width);
}

void debug_render_filled_rect(render_context_t* rc, float x, float y, float w, float h, skb_color_t col)
{
	const render_vert_t verts[] = {
		{ {x,y}, col },
		{ {x+w,y}, col },
		{ {x+w,y+h}, col },
		{ {x,y}, col },
		{ {x+w,y+h}, col },
		{ {x,y+h}, col },
	};
	render_draw_debug_tris(rc, verts, SKB_COUNTOF(verts));
}

void debug_render_tri(render_context_t* rc, float x0, float y0, float x1, float y1, float x2, float y2, skb_color_t col)
{
	const render_vert_t verts[] = {
		{ {x0,y0}, col },
		{ {x1,y1}, col },
		{ {x2,y2}, col },
	};
	render_draw_debug_tris(rc, verts, SKB_COUNTOF(verts));
}

// Line based debug font
typedef struct draw__line_glyph_t {
	char num;
	char advance;
	char verts[2*18];
} draw__line_glyph_t;

static const draw__line_glyph_t g_glyphs[95] = {
    // Space ( 32)
    { .num = 0, .advance = 16, .verts = { 0 }, },
    // ! ( 33)
    { .num = 5, .advance = 10, .verts = { 1, -6, 1, -20, -1, -1, 1, -1, 1, 0, }, },
    // " ( 34)
    { .num = 5, .advance = 14, .verts = { 1, -14, 1, -20, -1, -1, 5, -14, 5, -20, }, },
    // # ( 35)
    { .num = 11, .advance = 22, .verts = { 2, -14, 13, -14, -1, -1, 1, -7, 12, -7, -1, -1, 5, -20, 3, 0, -1, -1, 11, -20, 9, 0, }, },
    // $ ( 36)
    { .num = 13, .advance = 22, .verts = { 7, -22, 7, 2, -1, -1, 1, -3, 4, 0, 10, 0, 13, -3, 13, -7, 1, -13, 1, -17, 4, -20, 10, -20, 13, -17, }, },
    // % ( 37)
    { .num = 14, .advance = 24, .verts = { 1, -2, 15, -18, -1, -1, 1, -20, 1, -14, 6, -14, 6, -20, 1, -20, -1, -1, 10, -6, 10, 0, 15, 0, 15, -6, 10, -6, }, },
    // & ( 38)
    { .num = 13, .advance = 24, .verts = { 12, -17, 9, -20, 6, -20, 3, -17, 3, -14, 15, 0, -1, -1, 15, -8, 7, 0, 4, 0, 1, -3, 1, -7, 5, -11, }, },
    // ' ( 39)
    { .num = 2, .advance = 12, .verts = { 2, -16, 2, -20, }, },
    // ( ( 40)
    { .num = 4, .advance = 12, .verts = { 4, -22, 1, -19, 1, 1, 4, 4, }, },
    // ) ( 41)
    { .num = 4, .advance = 12, .verts = { 1, -22, 4, -19, 4, 1, 1, 4, }, },
    // * ( 42)
    { .num = 8, .advance = 20, .verts = { 6, -6, 6, -16, -1, -1, 1, -8, 11, -14, -1, -1, 1, -14, 11, -8, }, },
    // + ( 43)
    { .num = 5, .advance = 20, .verts = { 2, -9, 12, -9, -1, -1, 7, -14, 7, -4, }, },
    // , ( 44)
    { .num = 2, .advance = 16, .verts = { 1, 4, 5, -2, }, },
    // - ( 45)
    { .num = 2, .advance = 20, .verts = { 2, -9, 12, -9, }, },
    // . ( 46)
    { .num = 5, .advance = 16, .verts = { 4, -1, 4, 0, 5, 0, 5, -1, 4, -1, }, },
    // / ( 47)
    { .num = 2, .advance = 18, .verts = { 1, 4, 9, -22, }, },
    // 0 ( 48)
    { .num = 9, .advance = 20, .verts = { 1, -17, 1, -3, 4, 0, 10, 0, 13, -3, 13, -17, 10, -20, 4, -20, 1, -17, }, },
    // 1 ( 49)
    { .num = 4, .advance = 20, .verts = { 3, -16, 7, -20, 8, -20, 8, 0, }, },
    // 2 ( 50)
    { .num = 8, .advance = 20, .verts = { 1, -17, 4, -20, 10, -20, 13, -17, 13, -13, 1, -3, 1, 0, 13, 0, }, },
    // 3 ( 51)
    { .num = 14, .advance = 20, .verts = { 1, -3, 4, 0, 10, 0, 13, -3, 13, -8, 10, -11, 13, -14, 13, -17, 10, -20, 4, -20, 1, -17, -1, -1, 5, -11, 10, -11, }, },
    // 4 ( 52)
    { .num = 6, .advance = 20, .verts = { 14, -8, 1, -8, 1, -10, 9, -20, 11, -20, 11, 0, }, },
    // 5 ( 53)
    { .num = 9, .advance = 20, .verts = { 1, -3, 4, 0, 10, 0, 13, -3, 13, -8, 10, -11, 1, -11, 1, -20, 11, -20, }, },
    // 6 ( 54)
    { .num = 15, .advance = 20, .verts = { 13, -17, 10, -20, 4, -20, 1, -17, 1, -8, -1, -1, 1, -8, 1, -3, 4, 0, 10, 0, 13, -3, 13, -8, 10, -11, 4, -11, 1, -8, }, },
    // 7 ( 55)
    { .num = 4, .advance = 20, .verts = { 1, -20, 13, -20, 13, -18, 3, 0, }, },
    // 8 ( 56)
    { .num = 18, .advance = 20, .verts = { 13, -8, 10, -11, 4, -11, 1, -8, 1, -3, 4, 0, 10, 0, 13, -3, 13, -8, -1, -1, 4, -12, 2, -14, 2, -17, 5, -20, 9, -20, 12, -17, 12, -14, 10, -12, }, },
    // 9 ( 57)
    { .num = 15, .advance = 20, .verts = { 1, -3, 4, 0, 10, 0, 13, -3, 13, -12, -1, -1, 13, -12, 13, -17, 10, -20, 4, -20, 1, -17, 1, -12, 4, -9, 10, -9, 13, -12, }, },
    // : ( 58)
    { .num = 11, .advance = 14, .verts = { 4, -1, 4, 0, 5, 0, 5, -1, 4, -1, -1, -1, 4, -13, 4, -12, 5, -12, 5, -13, 4, -13, }, },
    // ; ( 59)
    { .num = 8, .advance = 14, .verts = { 1, 4, 5, -2, -1, -1, 4, -13, 4, -12, 5, -12, 5, -13, 4, -13, }, },
    // < ( 60)
    { .num = 3, .advance = 20, .verts = { 11, -14, 3, -9, 11, -4, }, },
    // = ( 61)
    { .num = 5, .advance = 20, .verts = { 2, -7, 12, -7, -1, -1, 2, -13, 12, -13, }, },
    // > ( 62)
    { .num = 3, .advance = 20, .verts = { 3, -14, 11, -9, 3, -4, }, },
    // ? ( 63)
    { .num = 10, .advance = 22, .verts = { 1, -17, 4, -20, 10, -20, 13, -17, 13, -14, 7, -9, 7, -6, -1, -1, 7, -1, 7, 0, }, },
    // @ ( 64)
    { .num = 17, .advance = 20, .verts = { 11, 0, 5, 0, 1, -4, 1, -16, 5, -20, 10, -20, 13, -17, 13, -5, -1, -1, 12, -7, 10, -5, 9, -5, 7, -7, 7, -12, 9, -14, 10, -14, 12, -12, }, },
    // A ( 65)
    { .num = 7, .advance = 24, .verts = { 1, 0, 8, -20, 10, -20, 17, 0, -1, -1, 5, -8, 13, -8, }, },
    // B ( 66)
    { .num = 13, .advance = 22, .verts = { 1, -20, 1, 0, 10, 0, 13, -3, 13, -7, 10, -10, 13, -13, 13, -17, 10, -20, 1, -20, -1, -1, 1, -10, 10, -10, }, },
    // C ( 67)
    { .num = 8, .advance = 24, .verts = { 15, -17, 12, -20, 5, -20, 1, -16, 1, -4, 5, 0, 12, 0, 15, -3, }, },
    // D ( 68)
    { .num = 7, .advance = 24, .verts = { 1, -20, 1, 0, 11, 0, 15, -4, 15, -16, 11, -20, 1, -20, }, },
    // E ( 69)
    { .num = 7, .advance = 22, .verts = { 15, -20, 1, -20, 1, 0, 15, 0, -1, -1, 1, -10, 13, -10, }, },
    // F ( 70)
    { .num = 6, .advance = 20, .verts = { 1, 0, 1, -20, 12, -20, -1, -1, 1, -10, 11, -10, }, },
    // G ( 71)
    { .num = 10, .advance = 24, .verts = { 9, -10, 15, -10, 15, -3, 12, 0, 5, 0, 1, -4, 1, -16, 5, -20, 11, -20, 14, -17, }, },
    // H ( 72)
    { .num = 8, .advance = 22, .verts = { 1, -20, 1, 0, -1, -1, 15, -20, 15, 0, -1, -1, 1, -10, 15, -10, }, },
    // I ( 73)
    { .num = 2, .advance = 10, .verts = { 2, -20, 2, 0, }, },
    // J ( 74)
    { .num = 5, .advance = 20, .verts = { 1, -3, 4, 0, 8, 0, 12, -4, 12, -20, }, },
    // K ( 75)
    { .num = 9, .advance = 22, .verts = { 1, 0, 1, -20, -1, -1, 15, -20, 7, -10, 15, 0, -1, -1, 1, -10, 7, -10, }, },
    // L ( 76)
    { .num = 3, .advance = 20, .verts = { 1, -20, 1, 0, 13, 0, }, },
    // M ( 77)
    { .num = 8, .advance = 26, .verts = { 1, 0, 1, -20, 3, -20, 8, 0, 10, 0, 15, -20, 17, -20, 17, 0, }, },
    // N ( 78)
    { .num = 6, .advance = 24, .verts = { 1, 0, 1, -20, 3, -20, 13, 0, 15, 0, 15, -20, }, },
    // O ( 79)
    { .num = 9, .advance = 24, .verts = { 1, -16, 1, -4, 5, 0, 11, 0, 15, -4, 15, -16, 11, -20, 5, -20, 1, -16, }, },
    // P ( 80)
    { .num = 10, .advance = 20, .verts = { 1, 0, 1, -10, -1, -1, 1, -10, 1, -20, 10, -20, 13, -17, 13, -13, 10, -10, 1, -10, }, },
    // Q ( 81)
    { .num = 12, .advance = 24, .verts = { 1, -16, 1, -4, 5, 0, 11, 0, 15, -4, 15, -16, 11, -20, 5, -20, 1, -16, -1, -1, 16, 1, 10, -5, }, },
    // R ( 82)
    { .num = 13, .advance = 22, .verts = { 1, 0, 1, -10, -1, -1, 1, -10, 1, -20, 10, -20, 13, -17, 13, -13, 10, -10, 1, -10, -1, -1, 13, 0, 7, -9, }, },
    // S ( 83)
    { .num = 10, .advance = 20, .verts = { 1, -4, 5, 0, 11, 0, 15, -4, 15, -7, 1, -13, 1, -16, 5, -20, 11, -20, 14, -17, }, },
    // T ( 84)
    { .num = 5, .advance = 22, .verts = { 1, -20, 15, -20, -1, -1, 8, 0, 8, -20, }, },
    // U ( 85)
    { .num = 6, .advance = 22, .verts = { 1, -20, 1, -4, 5, 0, 11, 0, 15, -4, 15, -20, }, },
    // V ( 86)
    { .num = 4, .advance = 20, .verts = { 1, -20, 7, 0, 9, 0, 15, -20, }, },
    // W ( 87)
    { .num = 8, .advance = 28, .verts = { 1, -20, 5, 0, 7, 0, 11, -20, 13, -20, 17, 0, 19, 0, 23, -20, }, },
    // X ( 88)
    { .num = 5, .advance = 20, .verts = { 1, 0, 15, -20, -1, -1, 15, 0, 1, -20, }, },
    // Y ( 89)
    { .num = 6, .advance = 20, .verts = { 1, -20, 8, -10, 15, -20, -1, -1, 8, -10, 8, 0, }, },
    // Z ( 90)
    { .num = 6, .advance = 22, .verts = { 1, -20, 15, -20, 15, -18, 1, -2, 1, 0, 15, 0, }, },
    // [ ( 91)
    { .num = 4, .advance = 12, .verts = { 5, -22, 1, -22, 1, 4, 5, 4, }, },
    // \ ( 92)
    { .num = 2, .advance = 16, .verts = { 9, 4, 1, -22, }, },
    // ] ( 93)
    { .num = 4, .advance = 14, .verts = { 1, -22, 5, -22, 5, 4, 1, 4, }, },
    // ^ ( 94)
    { .num = 3, .advance = 20, .verts = { 1, -14, 7, -20, 13, -14, }, },
    // _ ( 95)
    { .num = 2, .advance = 20, .verts = { 2, 0, 14, 0, }, },
    // ` ( 96)
    { .num = 2, .advance = 14, .verts = { 5, -14, 1, -20, }, },
    // a ( 97)
    { .num = 14, .advance = 20, .verts = { 2, -12, 4, -14, 9, -14, 11, -12, 11, -3, 14, 0, -1, -1, 11, -7, 3, -7, 1, -5, 1, -2, 3, 0, 8, 0, 11, -3, }, },
    // b ( 98)
    { .num = 11, .advance = 19, .verts = { 1, 0, 1, -20, -1, -1, 2, -10, 6, -14, 10, -14, 13, -11, 13, -3, 10, 0, 6, 0, 2, -4, }, },
    // c ( 99)
    { .num = 8, .advance = 20, .verts = { 13, -3, 10, 0, 4, 0, 1, -3, 1, -11, 4, -14, 10, -14, 12, -12, }, },
    // d (100)
    { .num = 11, .advance = 20, .verts = { 13, 0, 13, -20, -1, -1, 12, -10, 8, -14, 4, -14, 1, -11, 1, -3, 4, 0, 8, 0, 12, -4, }, },
    // e (101)
    { .num = 13, .advance = 20, .verts = { 12, -2, 10, 0, 4, 0, 1, -3, 1, -7, -1, -1, 1, -7, 13, -7, 13, -11, 10, -14, 4, -14, 1, -11, 1, -7, }, },
    // f (102)
    { .num = 7, .advance = 16, .verts = { 4, 0, 4, -17, 7, -20, 9, -20, -1, -1, 1, -14, 9, -14, }, },
    // g (103)
    { .num = 14, .advance = 20, .verts = { 12, -10, 8, -14, 4, -14, 1, -11, 1, -3, 4, 0, 8, 0, 12, -4, -1, -1, 2, 4, 4, 6, 10, 6, 13, 3, 13, -14, }, },
    // h (104)
    { .num = 8, .advance = 20, .verts = { 2, -10, 6, -14, 10, -14, 13, -11, 13, 0, -1, -1, 1, 0, 1, -20, }, },
    // i (105)
    { .num = 5, .advance = 10, .verts = { 2, -14, 2, 0, -1, -1, 2, -20, 2, -19, }, },
    // j (106)
    { .num = 7, .advance = 14, .verts = { -2, 6, 1, 6, 4, 3, 4, -14, -1, -1, 4, -20, 4, -19, }, },
    // k (107)
    { .num = 9, .advance = 20, .verts = { 1, 0, 1, -20, -1, -1, 12, -14, 5, -7, 12, 0, -1, -1, 1, -7, 5, -7, }, },
    // l (108)
    { .num = 3, .advance = 12, .verts = { 0, -20, 3, -20, 3, 0, }, },
    // m (109)
    { .num = 17, .advance = 26, .verts = { 1, -14, 1, -10, -1, -1, 1, 0, 1, -10, -1, -1, 1, -10, 5, -14, 7, -14, 10, -11, 10, 0, -1, -1, 11, -11, 14, -14, 16, -14, 19, -11, 19, 0, }, },
    // n (110)
    { .num = 11, .advance = 18, .verts = { 1, -14, 1, -10, -1, -1, 1, 0, 1, -10, -1, -1, 1, -10, 5, -14, 8, -14, 11, -11, 11, 0, }, },
    // o (111)
    { .num = 9, .advance = 20, .verts = { 1, -11, 1, -3, 4, 0, 10, 0, 13, -3, 13, -11, 10, -14, 4, -14, 1, -11, }, },
    // p (112)
    { .num = 11, .advance = 20, .verts = { 1, -14, 1, 6, -1, -1, 2, -4, 6, 0, 10, 0, 13, -3, 13, -11, 10, -14, 6, -14, 2, -10, }, },
    // q (113)
    { .num = 11, .advance = 22, .verts = { 13, -14, 13, 6, -1, -1, 12, -4, 8, 0, 4, 0, 1, -3, 1, -11, 4, -14, 8, -14, 12, -10, }, },
    // r (114)
    { .num = 7, .advance = 14, .verts = { 1, 0, 1, -14, -1, -1, 2, -11, 5, -14, 7, -14, 9, -12, }, },
    // s (115)
    { .num = 10, .advance = 20, .verts = { 12, -12, 10, -14, 4, -14, 1, -11, 1, -8, 13, -6, 13, -3, 10, 0, 3, 0, 1, -2, }, },
    // t (116)
    { .num = 7, .advance = 18, .verts = { 4, -20, 4, -3, 7, 0, 10, 0, -1, -1, 0, -14, 10, -14, }, },
    // u (117)
    { .num = 11, .advance = 18, .verts = { 11, 0, 11, -4, -1, -1, 11, -14, 11, -4, -1, -1, 11, -4, 7, 0, 4, 0, 1, -3, 1, -14, }, },
    // v (118)
    { .num = 4, .advance = 20, .verts = { 1, -14, 6, 0, 8, 0, 13, -14, }, },
    // w (119)
    { .num = 8, .advance = 26, .verts = { 1, -14, 4, 0, 6, 0, 9, -14, 11, -14, 14, 0, 16, 0, 19, -14, }, },
    // x (120)
    { .num = 5, .advance = 20, .verts = { 1, 0, 13, -14, -1, -1, 1, -14, 13, 0, }, },
    // y (121)
    { .num = 7, .advance = 20, .verts = { 1, -14, 6, 0, 8, 0, -1, -1, 13, -14, 7, 6, 3, 6, }, },
    // z (122)
    { .num = 6, .advance = 20, .verts = { 1, -14, 13, -14, 13, -12, 1, -2, 1, 0, 13, 0, }, },
    // { (123)
    { .num = 12, .advance = 16, .verts = { 9, -22, 7, -22, 5, -20, 5, -11, 3, -9, 5, -7, 5, 2, 7, 4, 9, 4, -1, -1, 1, -9, 3, -9, }, },
    // | (124)
    { .num = 2, .advance = 10, .verts = { 2, 4, 2, -22, }, },
    // } (125)
    { .num = 12, .advance = 16, .verts = { 1, -22, 3, -22, 5, -20, 5, -11, 7, -9, 5, -7, 5, 2, 3, 4, 1, 4, -1, -1, 7, -9, 9, -9, }, },
    // ~ (126)
    { .num = 8, .advance = 20, .verts = { 1, -16, 1, -18, 3, -20, 5, -20, 9, -16, 11, -16, 13, -18, 13, -20, }, },
};

static float render__glyph_width(float size, char chr)
{
	const float scale = size / 30.f;
	const int32_t idx = chr - 32;
	if (idx >= 0 && idx < 95) {
		const draw__line_glyph_t* g = &g_glyphs[idx];
		return (float)g->advance * scale;
	}
	return 0.f;
}


float render__char(render_context_t* rc, float x, float y, float size, skb_color_t color, char chr)
{
	// Ascender 22
	// x-height 14
	// Descender -8

	float advance = 0.f;
	float scale = size / 30.f;

	float line_width = skb_maxf(0.5f, 2.5f * scale);

	int32_t idx = chr - 32;
	if (idx >= 0 && idx < 95) {
		const draw__line_glyph_t* g = &g_glyphs[idx];

		skb_vec2_t pts[32];
		int32_t pts_count = 0;

		for (int32_t j = 0; j < g->num; j++) {
			const int32_t cx = g->verts[j*2];
			const int32_t cy = g->verts[j*2+1];
			if (cx == -1 && cy == -1) {
				// Flush line
				render__line_strip(rc, pts, pts_count, color, line_width);
				pts_count = 0;
			} else {
				pts[pts_count].x = x + (float)cx * scale;
				pts[pts_count].y = y + (float)cy * scale;
				pts_count++;
			}
		}
		render__line_strip(rc, pts, pts_count, color, line_width);

		advance = (float)g->advance * scale;
	}

	return advance;
}


static float render__text_width(float size, const char* s)
{
	float w = 0.f;
	while (*s) {
		w += render__glyph_width(size, *s);
		s++;
	}
	return w;
}

float debug_render_text(render_context_t* rc, float x, float y, float size, render_align_t align, skb_color_t col, const char* fmt, ...)
{
	char buf[1025];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, args);
	va_end(args);

	float tw = render__text_width(size, buf);
	if (align == RENDER_ALIGN_CENTER)
		x -= tw * 0.5f;
	else if (align == RENDER_ALIGN_END)
		x -= tw;

	const char* s = buf;
	while (*s) {
		x += render__char(rc, x, y, size, col, *s);
		s++;
	}

	return x;
}

float debug_render_text_width(render_context_t* rc, float size, const char* fmt, ...)
{
	char s[1025];
	va_list args;
	va_start(args, fmt);
	vsnprintf(s, sizeof(s) - 1, fmt, args);
	va_end(args);

	return render__text_width(size, s);
}

typedef struct render__atlas_rect_context_t {
	float x;
	float y;
	float scale;
	skb_color_t color;
	render_context_t* rc;
} render__atlas_rect_context_t;

static void debug_render__atlas_rects(int32_t x, int32_t y, int32_t width, int32_t height, void* context)
{
	render__atlas_rect_context_t* ctx = (render__atlas_rect_context_t*)context;
	skb_rect2_t r = {
		.x = ctx->x + (float)x * ctx->scale,
		.y = ctx->y + (float)y * ctx->scale,
		.width = (float)width * ctx->scale - 1.f,
		.height = (float)height * ctx->scale - 1.f
	};

	debug_render_filled_rect(ctx->rc, r.x, r.y, r.width, r.height, ctx->color);
}

static void debug_render__used_rects(int32_t x, int32_t y, int32_t width, int32_t height, void* context)
{
	render__atlas_rect_context_t* ctx = (render__atlas_rect_context_t*)context;
	skb_rect2_t r = {
		.x = ctx->x + (float)x * ctx->scale,
		.y = ctx->y + (float)y * ctx->scale,
		.width = (float)width * ctx->scale - 1.f,
		.height = (float)height * ctx->scale - 1.f
	};

	debug_render_stroked_rect(ctx->rc, r.x, r.y, r.width, r.height, ctx->color, 1.f);
}

void debug_render_atlas_overlay(render_context_t* rc, float sx, float sy, float scale, int32_t columns)
{
	if (scale < 0.01f)
		return;

	skb_image_atlas_t* atlas = render_get_atlas(rc);
	float row_y = sy;

	for (int32_t i = 0; i < skb_image_atlas_get_texture_count(atlas); i += columns) {

		float row_height = 0.f;
		float col_x = sx;

		for (int32_t j = 0; j < columns; j++) {
			int32_t texture_idx = i+j;
			if (texture_idx >= skb_image_atlas_get_texture_count(atlas))
				break;
			const skb_image_t* image = skb_image_atlas_get_texture(atlas, texture_idx);

			float ax = col_x;
			float ay = row_y;

			debug_render_text(rc, ax, ay+12,13, RENDER_ALIGN_START, skb_rgba(0,0,0,255), "[%d] %s (%d x %d)", texture_idx, image->bpp == 4 ? "RGBA" : "A", image->width, image->height);
			ay += 20.f;

			const float img_width = (float)image->width * scale;
			const float img_height = (float)image->height * scale;

			debug_render_filled_rect(rc, ax, ay, img_width, img_height, skb_rgba(0,0,0,255));

			// Make quad describing the whole atlas.
			skb_quad_t quad = {
				.geom = { ax,ay, img_width, img_height },
				.pattern = { 0.f, 0.f, 1.f, 1.f },
				.texture = { 0, 0, (float)image->width, (float)image->height },
				.color = skb_rgba(255,255,255,255),
				.texture_idx = (uint8_t)texture_idx,
			};
			render_draw_quad(rc, &quad);

			render__atlas_rect_context_t context = {
				.x = ax,
				.y = ay,
				.scale = scale,
				.color = skb_rgba(96,96,128,192),
				.rc = rc,
			};
			skb_image_atlas_debug_iterate_free_rects(atlas, texture_idx, debug_render__atlas_rects, &context);

			context.color = skb_rgba(32,192,255,255);
			skb_image_atlas_debug_iterate_used_rects(atlas, texture_idx, debug_render__used_rects, &context);

			// previous updated bounds
			skb_rect2i_t dirty_bounds = skb_image_atlas_debug_get_texture_prev_dirty_bounds(atlas, texture_idx);
			context.color = skb_rgba(255,220,32,255);
			debug_render__used_rects(dirty_bounds.x, dirty_bounds.y, dirty_bounds.width, dirty_bounds.height, &context);

			row_height = skb_maxf(row_height, img_height + 20.f);
			col_x += img_width + 20.f;
		}

		row_y += row_height + 20.f;
	}

}


static void debug_draw__padding(render_context_t* rc, float x, float y, skb_rect2_t bounds, skb_padding2_t padding, skb_color_t color)
{
	const float left = x + bounds.x;
	const float right = left + bounds.width;
	const float top = y + bounds.y;
	const float bottom = top + bounds.height;
	const float inner_width = skb_maxf(0.f, bounds.width - padding.left - padding.right);

	if (padding.left > 0)
		debug_render_filled_rect(rc, left, top, padding.left, bounds.height, color);
	if (padding.right > 0)
		debug_render_filled_rect(rc, right - padding.right, top, padding.right, bounds.height, color);
	if (padding.top > 0)
		debug_render_filled_rect(rc, left + padding.left, top, inner_width, padding.top, color);
	if (padding.bottom > 0)
		debug_render_filled_rect(rc, left + padding.left, bottom - padding.bottom, inner_width, padding.bottom, color);
}

void debug_render_layout(render_context_t* rc, float x, float y, const skb_layout_t* layout)
{
	const skb_rect2_t layout_bounds = skb_layout_get_bounds(layout);
	const skb_padding2_t layout_padding = skb_layout_get_padding(layout);

	debug_draw__padding(rc, x, y, layout_bounds, layout_padding, skb_rgba(220, 192, 0,32));
	debug_render_stroked_rect(rc,
		x + layout_bounds.x, layout_bounds.y + y, layout_bounds.width, layout_bounds.height,
		skb_rgba(220, 192, 0,128), 1.f);
}


void debug_render_layout_lines(render_context_t* rc, float x, float y, const skb_layout_t* layout)
{
	const skb_layout_line_t* layout_lines = skb_layout_get_lines(layout);
	const int32_t layout_lines_count = skb_layout_get_lines_count(layout);
	for (int32_t i = 0; i < layout_lines_count; i++) {
		const skb_layout_line_t* layout_line = &layout_lines[i];

		const skb_padding2_t line_padding = { .left = layout_line->padding_left, .right = layout_line->padding_right };
		debug_draw__padding(rc, x, y, layout_line->bounds, line_padding, skb_rgba(192,220,100,92));

		debug_render_stroked_rect(rc, x + layout_line->bounds.x, y + layout_line->bounds.y, layout_line->bounds.width, layout_line->bounds.height, skb_rgba(192,220,100,196), 1.f);
	}
}

void debug_render_layout_runs(render_context_t* rc, float x, float y, const skb_layout_t* layout)
{
	const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(layout);
	const int32_t layout_runs_count = skb_layout_get_layout_runs_count(layout);
	for (int32_t i = 0; i < layout_runs_count; i++) {
		const skb_layout_run_t* layout_run = &layout_runs[i];

		const skb_color_t col = skb_is_rtl(layout_run->direction) ? skb_rgba(255,100,128,128) : skb_rgba(128,100,255,128);

		debug_draw__padding(rc, x, y, layout_run->bounds, layout_run->padding, skb_color_with_alpha(col, 64));

		debug_render_stroked_rect(rc,
			x + layout_run->bounds.x, y + layout_run->bounds.y, layout_run->bounds.width, layout_run->bounds.height,
			col, 1.f);

		float mid_x = layout_run->bounds.x + layout_run->bounds.width * 0.5f;
		debug_render_text(rc, x + mid_x, y + layout_run->bounds.y + layout_run->bounds.height - 2, 5, RENDER_ALIGN_CENTER, col,
			"%d%c", i, skb_is_rtl(layout_run->direction) ? '<' : '>');
	}
}

void debug_render_layout_glyphs(render_context_t* rc, float x, float y, const skb_layout_t* layout)
{
	const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(layout);
	const int32_t layout_runs_count = skb_layout_get_layout_runs_count(layout);
	const skb_glyph_t* glyphs = skb_layout_get_glyphs(layout);
	for (int32_t i = 0; i < layout_runs_count; i++) {
		const skb_layout_run_t* layout_run = &layout_runs[i];
		for (int32_t gi = layout_run->glyph_range.start; gi < layout_run->glyph_range.end; gi++) {
			const skb_glyph_t* glyph = &glyphs[gi];
			debug_render_tick(rc, x + glyph->offset_x, y + glyph->offset_y, 2.f, skb_rgba(0,0,0,128), -1.f);
		}
	}
}
