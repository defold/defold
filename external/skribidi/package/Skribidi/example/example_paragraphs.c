// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "debug_render.h"
#include "utils.h"
#include "render.h"

#include "skb_common.h"
#include "skb_font_collection.h"
#include "skb_attribute_collection.h"
#include "skb_rasterizer.h"
#include "skb_layout.h"
#include "skb_rich_layout.h"
#include "skb_rich_text.h"


typedef struct paragraphs_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_attribute_collection_t* attribute_collection;

	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_rich_text_t* rich_text;
	skb_rich_layout_t* rich_layout;

	uint8_t text_overflow;

	view_t view;
	bool drag_view;
	bool drag_text;

	bool show_layout_details;
	float atlas_scale;

} paragraphs_context_t;


void paragraphs_destroy(void* ctx_ptr);
void paragraphs_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void paragraphs_on_char(void* ctx_ptr, unsigned int codepoint);
void paragraphs_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void paragraphs_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void paragraphs_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void paragraphs_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void paragraphs_update_layout(paragraphs_context_t* ctx);

void* paragraphs_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	paragraphs_context_t* ctx = skb_malloc(sizeof(paragraphs_context_t));
	memset(ctx, 0, sizeof(paragraphs_context_t));

	ctx->base.create = paragraphs_create;
	ctx->base.destroy = paragraphs_destroy;
	ctx->base.on_key = paragraphs_on_key;
	ctx->base.on_char = paragraphs_on_char;
	ctx->base.on_mouse_button = paragraphs_on_mouse_button;
	ctx->base.on_mouse_move = paragraphs_on_mouse_move;
	ctx->base.on_mouse_scroll = paragraphs_on_mouse_scroll;
	ctx->base.on_update = paragraphs_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.0f;
	ctx->show_layout_details = true;

	ctx->text_overflow = SKB_OVERFLOW_NONE;

	ctx->font_collection = skb_font_collection_create();
	assert(ctx->font_collection);

	const skb_font_create_params_t fake_italic_params = {
		.slant = SKB_DEFAULT_SLANT
	};

	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansCondensed-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Italic.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Bold.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_PARAMS_OR_FAIL("data/IBMPlexSans-Bold.ttf", SKB_FONT_FAMILY_DEFAULT, &fake_italic_params);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansArabic-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansJP-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansKR-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/IBMPlexSansDevanagari-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansBrahmi-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSerifBalinese-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansTamil-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansBengali-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoSansThai-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
	LOAD_FONT_OR_FAIL("data/NotoColorEmoji-Regular.ttf", SKB_FONT_FAMILY_EMOJI);

	ctx->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(ctx->temp_alloc);


	ctx->rich_text = skb_rich_text_create();

	const skb_attribute_t h1_attributes[] = {
		skb_attribute_make_font_size(48.f),
		skb_attribute_make_font_weight(SKB_WEIGHT_BOLD),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(96,96,96,255)),
		skb_attribute_make_paragraph_padding(0,0,10,5),
	};

	const skb_attribute_t h2_attributes[] = {
		skb_attribute_make_font_size(24.f),
		skb_attribute_make_font_weight(SKB_WEIGHT_BOLD),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(96,96,96,255)),
		skb_attribute_make_horizontal_align(SKB_ALIGN_END),
		skb_attribute_make_paragraph_padding(10,10,10,5),
	};

	const skb_attribute_t body_attributes[] = {
		skb_attribute_make_font_size(16.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(16,16,16,255)),
		skb_attribute_make_paragraph_padding(0,0,5,5),
		skb_attribute_make_indent_increment(0, 20.f),
	};
	const skb_attribute_t body_attributes_right[] = {
		skb_attribute_make_font_size(16.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(16,16,16,255)),
		skb_attribute_make_horizontal_align(SKB_ALIGN_END),
		skb_attribute_make_paragraph_padding(0,0,5,5),
	};

	const skb_attribute_t body_attributes_padding[] = {
		skb_attribute_make_font_size(16.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(16,16,16,255)),
		skb_attribute_make_paragraph_padding(80,20,5,5),
	};

	const skb_attribute_t list_attributes_l1[] = {
		skb_attribute_make_font_size(16.f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(16,16,16,255)),
		skb_attribute_make_paragraph_padding_with_spacing(0,0,10,10,5),
		skb_attribute_make_indent_increment(40.f, 0.f),
		skb_attribute_make_indent_level(0),
		skb_attribute_make_list_marker(SKB_LIST_MARKER_CODEPOINT, 40, 5, 0x2022),
		skb_attribute_make_group_tag(SKB_TAG_STR("list"))
	};

	const skb_attribute_t list_attributes_l2[] = {
		skb_attribute_make_font_size(16.f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(16,16,16,255)),
		skb_attribute_make_paragraph_padding_with_spacing(0,0,10,10,5),
		skb_attribute_make_indent_increment(40.f, 0.f),
		skb_attribute_make_indent_level(1),
		skb_attribute_make_list_marker(SKB_LIST_MARKER_COUNTER_DECIMAL, 40, 5, 0),
		skb_attribute_make_group_tag(SKB_TAG_STR("list"))
	};

	const char* ipsum_1 =
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam eget blandit purus, sit amet faucibus quam. Morbi vulputate tellus in nulla fermentum feugiat id eu diam. Sed id orci sapien. "
		"Donec sodales vitae odio dapibus pulvinar. Maecenas molestie lorem vulputate, gravida ex sed, dignissim erat. Suspendisse vel magna sed libero fringilla tincidunt id eget nisl. "
		"Suspendisse potenti. Maecenas fringilla magna sollicitudin, porta ipsum sed, rutrum magna. Sed ac semper magna. Phasellus porta nunc nulla, non dignissim magna pretium a. "
		"Aenean condimentum, nisi vitae sollicitudin ullamcorper, tellus elit suscipit risus, aliquet hendrerit sem velit in leo. Sed ut est pellentesque, vehicula ligula consectetur, tincidunt tellus. "
		"Aliquam erat volutpat. Etiam efficitur consequat turpis, vitae faucibus erat porta sed.";

	const char* ipsum_2 =
		"Aenean euismod ante sed mi pellentesque dictum. Ut dapibus, nisl at dapibus egestas, enim metus semper lectus, ut dictum sapien leo et ligula. In et lorem quis nunc rutrum aliquet eget non velit. "
		"Ut a luctus metus. Morbi vestibulum sapien vitae velit feugiat feugiat. Interdum et malesuada fames ac ante ipsum primis in faucibus. Donec sit amet sapien quam.";

	const char* ipsum_3 =
		"Donec at sodales est, sit amet rutrum ante. Cras tincidunt auctor nunc, id ullamcorper ligula facilisis non. Curabitur auctor mi at feugiat porta. Vestibulum aliquet molestie velit vehicula cursus. "
		"Donec vitae tristique libero. Etiam eget pellentesque nisi, in porta lectus. Donec accumsan ligula mauris. Nulla consectetur tortor at sem rutrum, non dapibus libero interdum. "
		"Nunc blandit molestie neque, quis porttitor lectus. Pellentesque consectetur augue sed velit suscipit pretium. In nec massa eros. Fusce non justo efficitur metus auctor pretium efficitur mattis enim.";

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(h1_attributes));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "The Header of the Text", -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(body_attributes));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, ipsum_1, -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(h2_attributes));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Another Header", -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(body_attributes_right));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, ipsum_2, -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(body_attributes_padding));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, ipsum_3, -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(list_attributes_l1));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Banana", -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(list_attributes_l1));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Cherry", -1, (skb_attribute_set_t){0});
	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(list_attributes_l2));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Cherry Coke", -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(list_attributes_l2));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam eget blandit purus, sit amet faucibus quam. Morbi vulputate tellus in nulla fermentum feugiat id eu diam. Sed id orci sapien. ", -1, (skb_attribute_set_t){0});

	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(list_attributes_l1));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Apple", -1, (skb_attribute_set_t){0});
	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(list_attributes_l2));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Pineapple", -1, (skb_attribute_set_t){0});
	skb_rich_text_append_paragraph(ctx->rich_text, SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(list_attributes_l2));
	skb_rich_text_append_utf8(ctx->rich_text, ctx->temp_alloc, "Blue cheese", -1, (skb_attribute_set_t){0});

	paragraphs_update_layout(ctx);

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	return ctx;

error:
	paragraphs_destroy(ctx);
	return NULL;
}

void paragraphs_destroy(void* ctx_ptr)
{
	paragraphs_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_rich_text_destroy(ctx->rich_text);
	skb_rich_layout_destroy(ctx->rich_layout);
	skb_font_collection_destroy(ctx->font_collection);
	skb_attribute_collection_destroy(ctx->attribute_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	SKB_ZERO_STRUCT(ctx);

	skb_free(ctx);
}

void paragraphs_update_layout(paragraphs_context_t* ctx)
{
	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_text_overflow(ctx->text_overflow),
	};
	skb_layout_params_t layout_params = {
		.font_collection = ctx->font_collection,
		.layout_width = 600.f,
		.layout_height = 600.f,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
	};

	ctx->rich_layout = skb_rich_layout_create();
	skb_rich_layout_set_from_rich_text(ctx->rich_layout, ctx->temp_alloc, &layout_params, ctx->rich_text, 0, NULL);
}

static uint8_t inc_wrap(uint8_t n, uint8_t max)
{
	if ((n+1) >= max) return 0;
	return n + 1;
}

void paragraphs_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	paragraphs_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_F9) {
			ctx->show_layout_details = !ctx->show_layout_details;
		}
		if (key == GLFW_KEY_F6) {
			ctx->text_overflow = inc_wrap(ctx->text_overflow, 3);
			paragraphs_update_layout(ctx);
		}
		if (key == GLFW_KEY_F10) {
			ctx->atlas_scale += 0.25f;
			if (ctx->atlas_scale > 1.01f)
				ctx->atlas_scale = 0.0f;
		}
	}

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
	}
}

void paragraphs_on_char(void* ctx_ptr, unsigned int codepoint)
{
	paragraphs_context_t* ctx = ctx_ptr;
	assert(ctx);
}

void paragraphs_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	paragraphs_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			if (!ctx->drag_view) {
				view_drag_start(&ctx->view, mouse_x, mouse_y);
				ctx->drag_view = true;
			}
		}
		if (action == GLFW_RELEASE) {
			if (ctx->drag_view) {
				ctx->drag_view = false;
			}
		}
	}
}

void paragraphs_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	paragraphs_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
	}
}

void paragraphs_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	paragraphs_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void paragraphs_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	paragraphs_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	// Draw visual result
	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	for (int32_t pi = 0; pi < skb_rich_layout_get_paragraphs_count(ctx->rich_layout); pi++) {
		const skb_layout_t* layout = skb_rich_layout_get_layout(ctx->rich_layout, pi);
		const skb_vec2_t layout_offset = skb_rich_layout_get_layout_offset(ctx->rich_layout, pi);
		render_draw_layout(ctx->rc, NULL, layout_offset.x, layout_offset.y, layout, SKB_RASTERIZE_ALPHA_SDF);
	}

	if (ctx->show_layout_details) {

		// Input size
		const skb_layout_params_t* layout_params = skb_rich_layout_get_params(ctx->rich_layout);
		debug_render_dashed_rect(ctx->rc, 0, 0, layout_params->layout_width, layout_params->layout_height, 5, skb_rgba(0,0,0,128), -1.0f);

		// Layout
		const skb_rect2_t rich_layout_bounds = skb_rich_layout_get_bounds(ctx->rich_layout);
		debug_render_stroked_rect(ctx->rc, rich_layout_bounds.x, rich_layout_bounds.y, rich_layout_bounds.width, rich_layout_bounds.height, skb_rgba(255,128,64,128), -2.0f);

		// Paragraphs
		for (int32_t pi = 0; pi < skb_rich_layout_get_paragraphs_count(ctx->rich_layout); pi++) {
			const skb_layout_t* layout = skb_rich_layout_get_layout(ctx->rich_layout, pi);
			const skb_vec2_t layout_offset = skb_rich_layout_get_layout_offset(ctx->rich_layout, pi);

			debug_render_layout(ctx->rc, layout_offset.x, layout_offset.y, layout);
			debug_render_layout_lines(ctx->rc, layout_offset.x, layout_offset.y, layout);
			debug_render_layout_runs(ctx->rc, layout_offset.x, layout_offset.y, layout);
		}
	}

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	const char* overflow_labels[] = { "None", "Clip", "Ellipsis" };

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13.f, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"Text Overflow (F6) %s   F9: Layout details %s   F10: Atlas %.1f%%",
		overflow_labels[ctx->text_overflow], ctx->show_layout_details ? "ON" : "OFF", ctx->atlas_scale * 100.f);
}
