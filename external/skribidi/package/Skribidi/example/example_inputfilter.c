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
#include "ime.h"
#include "render.h"
#include "utils.h"

#include "skb_common.h"
#include "skb_font_collection.h"
#include "skb_layout.h"
#include "skb_rasterizer.h"
#include "skb_editor.h"
#include "skb_rich_text.h"

typedef struct inputfilter_context_t {
	example_t base;

	skb_font_collection_t* font_collection;

	skb_temp_alloc_t* temp_alloc;
	GLFWwindow* window;
	render_context_t* rc;

	skb_editor_t* editor;

	bool allow_char;
	view_t view;
	bool drag_view;
	bool drag_text;

	skb_vec2_t mouse_pos;
	bool mouse_pressed;

	GLFWcursor* hand_cursor;

} inputfilter_context_t;



static void update_ime_rect(inputfilter_context_t* ctx)
{
	skb_caret_info_t caret_info = skb_editor_get_caret_info_at(ctx->editor, SKB_CURRENT_SELECTION_END);

	skb_rect2_t caret_rect = {
		.x = caret_info.x - caret_info.descender * caret_info.slope,
		.y = caret_info.y + caret_info.ascender,
		.width = (-caret_info.ascender + caret_info.descender) * caret_info.slope,
		.height = -caret_info.ascender + caret_info.descender,
	};

	skb_rect2i_t input_rect = {
		.x = (int32_t)(ctx->view.cx + caret_rect.x * ctx->view.scale),
		.y = (int32_t)(ctx->view.cy + caret_rect.y * ctx->view.scale),
		.width = (int32_t)(caret_rect.width * ctx->view.scale),
		.height = (int32_t)(caret_rect.height * ctx->view.scale),
	};
	ime_set_input_rect(input_rect);
}

static void ime_handler(ime_event_t event, const uint32_t* text, int32_t text_length, int32_t cursor, void* context)
{
	inputfilter_context_t* ctx = context;

	if (event == IME_EVENT_COMPOSITION)
		skb_editor_set_composition_utf32(ctx->editor, ctx->temp_alloc, text, text_length, cursor);
	else if (event == IME_EVENT_COMMIT)
		skb_editor_commit_composition_utf32(ctx->editor, ctx->temp_alloc, text, text_length);
	else if (event == IME_EVENT_CANCEL)
		skb_editor_clear_composition(ctx->editor, ctx->temp_alloc);

	update_ime_rect(ctx);
}

static bool text_contains(skb_editor_t* editor, uint32_t codepoint)
{
	int32_t count = skb_editor_get_paragraph_count(editor);
	for (int32_t pi = 0; pi < count; pi++) {
		const skb_text_t* text = skb_editor_get_paragraph_text(editor, pi);
		const uint32_t* utf32 = skb_text_get_utf32(text);
		const int32_t utf32_count = skb_text_get_utf32_count(text);
		for (int32_t i = 0; i < utf32_count; i++) {
			if (utf32[i] == codepoint)
				return true;
		}
	}
	return false;
}

typedef struct is_not_numeric_context_t {
	bool allow_period;
	bool allow_sign;
} is_not_numeric_context_t;

static bool is_not_numeric(uint32_t codepoint, int32_t paragraph_idx, int32_t text_offset, void* context)
{
	is_not_numeric_context_t* ctx = context;
	const bool is_first_char = paragraph_idx == 0 && text_offset == 0;
	return !((codepoint >= '0' && codepoint <= '9') || (ctx->allow_sign && is_first_char && codepoint == '-') || (ctx->allow_sign && is_first_char && codepoint == '+') || (ctx->allow_period && codepoint == '.'));
}

static void numeric_filter(skb_editor_t* editor, skb_rich_text_t* input_text, skb_text_range_t selection, void* context)
{
	is_not_numeric_context_t ctx = {0};

	// Only allow one period.
	ctx.allow_period = !text_contains(editor, '.');

	// Only allow one sign as first character
	ctx.allow_sign = selection.start.offset == 0 && !text_contains(editor, '+') && !text_contains(editor, '-');

	skb_rich_text_remove_if(input_text, is_not_numeric, &ctx);
}

void inputfilter_destroy(void* ctx_ptr);
void inputfilter_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void inputfilter_on_char(void* ctx_ptr, unsigned int codepoint);
void inputfilter_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void inputfilter_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void inputfilter_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void inputfilter_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* inputfilter_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	inputfilter_context_t* ctx = skb_malloc(sizeof(inputfilter_context_t));
	memset(ctx, 0, sizeof(inputfilter_context_t));

	ctx->base.create = inputfilter_create;
	ctx->base.destroy = inputfilter_destroy;
	ctx->base.on_key = inputfilter_on_key;
	ctx->base.on_char = inputfilter_on_char;
	ctx->base.on_mouse_button = inputfilter_on_mouse_button;
	ctx->base.on_mouse_move = inputfilter_on_mouse_move;
	ctx->base.on_mouse_scroll = inputfilter_on_mouse_scroll;
	ctx->base.on_update = inputfilter_on_update;

	ctx->window = window;
	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->font_collection = skb_font_collection_create();
	assert(ctx->font_collection);

	const skb_font_create_params_t fake_italic_params = {
		.slant = SKB_DEFAULT_SLANT
	};

	LOAD_FONT_OR_FAIL("data/IBMPlexSans-Regular.ttf", SKB_FONT_FAMILY_DEFAULT);
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

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f, .zoom_level = 0.f, };

	ctx->hand_cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_text_overflow(SKB_OVERFLOW_SCROLL),
		skb_attribute_make_vertical_align(SKB_ALIGN_CENTER),
	};

	const skb_attribute_t text_attributes[] = {
		skb_attribute_make_font_size(64.f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(64,64,64,255)),
	};

	const skb_attribute_t composition_attributes[] = {
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,128,192,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_DOTTED, 0.f, 1.f, SKB_PAINT_TEXT),
	};

	skb_editor_params_t edit_params = {
		.font_collection = ctx->font_collection,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
		.paragraph_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text_attributes),
		.composition_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(composition_attributes),
		.editor_width = 200.f,
		.editor_height = 80.f,
	};

	ctx->editor = skb_editor_create(&edit_params);
	assert(ctx->editor);
	skb_editor_set_input_filter_callback(ctx->editor, numeric_filter, NULL);
	skb_editor_set_text_utf8(ctx->editor, ctx->temp_alloc, "1.123", -1);

	ime_set_handler(ime_handler, ctx);

	update_ime_rect(ctx);

	return ctx;

error:
	inputfilter_destroy(ctx);
	return NULL;
}

void inputfilter_destroy(void* ctx_ptr)
{
	inputfilter_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_font_collection_destroy(ctx->font_collection);
	skb_editor_destroy(ctx->editor);
	skb_temp_alloc_destroy(ctx->temp_alloc);

	glfwDestroyCursor(ctx->hand_cursor);

	memset(ctx, 0, sizeof(inputfilter_context_t));

	ime_cancel();
	ime_set_handler(NULL, NULL);

	skb_free(ctx);
}

void inputfilter_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	inputfilter_context_t* ctx = ctx_ptr;
	assert(ctx);

	uint32_t edit_mods = 0;
	if (mods & GLFW_MOD_SHIFT)
		edit_mods |= SKB_MOD_SHIFT;
	if (mods & GLFW_MOD_CONTROL)
		edit_mods |= SKB_MOD_CONTROL;

	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		ctx->allow_char = true;
		if (key == GLFW_KEY_V && (mods & GLFW_MOD_CONTROL)) {
			// Paste
			const char* clipboard_text = glfwGetClipboardString(window);
			skb_editor_insert_text_utf8(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, clipboard_text, -1);
			ctx->allow_char = false;
		}
		if (key == GLFW_KEY_Z && (mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT) == 0)
			skb_editor_undo(ctx->editor, ctx->temp_alloc);
		if (key == GLFW_KEY_Z && (mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT))
			skb_editor_redo(ctx->editor, ctx->temp_alloc);
		if (key == GLFW_KEY_LEFT)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_LEFT, edit_mods);
		if (key == GLFW_KEY_RIGHT)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_RIGHT, edit_mods);
		if (key == GLFW_KEY_UP)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_UP, edit_mods);
		if (key == GLFW_KEY_DOWN)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_DOWN, edit_mods);
		if (key == GLFW_KEY_HOME)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_HOME, edit_mods);
		if (key == GLFW_KEY_END)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_END, edit_mods);
		if (key == GLFW_KEY_BACKSPACE)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_BACKSPACE, edit_mods);
		if (key == GLFW_KEY_DELETE)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_DELETE, edit_mods);
		if (key == GLFW_KEY_ENTER)
			skb_editor_process_key_pressed(ctx->editor, ctx->temp_alloc, SKB_KEY_ENTER, edit_mods);

		update_ime_rect(ctx);
	}
	if (action == GLFW_PRESS) {
		ctx->allow_char = true;
		if (key == GLFW_KEY_A && (mods & GLFW_MOD_CONTROL)) {
			// Select all
			skb_editor_select_all(ctx->editor);
			ctx->allow_char = false;
		}
		if (key == GLFW_KEY_TAB) {
			skb_editor_insert_codepoint(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, '\t');
		}
		if (key == GLFW_KEY_ESCAPE) {
			// Clear selection
			if (skb_editor_get_text_utf32_count_in_range(ctx->editor, SKB_CURRENT_SELECTION) > 0)
				skb_editor_select_none(ctx->editor);
			else
				glfwSetWindowShouldClose(window, GL_TRUE);
		}
		if (key == GLFW_KEY_X && (mods & GLFW_MOD_CONTROL)) {
			// Cut
			int32_t text_len = skb_editor_get_text_utf8_in_range(ctx->editor, SKB_CURRENT_SELECTION, NULL, -1);
			char* text = SKB_TEMP_ALLOC(ctx->temp_alloc, char, text_len + 1);
			text_len = skb_editor_get_text_utf8_in_range(ctx->editor, SKB_CURRENT_SELECTION, text, text_len);
			text[text_len] = '\0';
			glfwSetClipboardString(window, text);
			SKB_TEMP_FREE(ctx->temp_alloc, text);
			skb_editor_insert_text_utf8(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, NULL, 0);
			ctx->allow_char = false;
		}
		if (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL)) {
			// Copy
			int32_t text_len = skb_editor_get_text_utf8_count_in_range(ctx->editor, SKB_CURRENT_SELECTION);
			char* text = SKB_TEMP_ALLOC(ctx->temp_alloc, char, text_len + 1);
			text_len = skb_editor_get_text_utf8_in_range(ctx->editor, SKB_CURRENT_SELECTION, text, text_len);
			text[text_len] = '\0';
			glfwSetClipboardString(window, text);
			SKB_TEMP_FREE(ctx->temp_alloc, text);
			ctx->allow_char = false;
		}

		update_ime_rect(ctx);
	}
}

void inputfilter_on_char(void* ctx_ptr, unsigned int codepoint)
{
	inputfilter_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->allow_char)
		skb_editor_insert_codepoint(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, codepoint);
}

static skb_vec2_t transform_mouse_pos(inputfilter_context_t* ctx, float mouse_x, float mouse_y)
{
	return (skb_vec2_t) {
		.x = (mouse_x - ctx->view.cx) / ctx->view.scale,
		.y = (mouse_y - ctx->view.cy) / ctx->view.scale,
	};
}

void inputfilter_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	inputfilter_context_t* ctx = ctx_ptr;
	assert(ctx);

	int32_t mouse_mods = 0;
	if (mods & GLFW_MOD_SHIFT)
		mouse_mods |= SKB_MOD_SHIFT;
	if (mods & GLFW_MOD_CONTROL)
		mouse_mods |= SKB_MOD_CONTROL;

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

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		// caret hit testing
		if (action == GLFW_PRESS) {
			if (!ctx->drag_text) {
				ime_cancel();
				ctx->drag_text = true;
				skb_vec2_t pos = transform_mouse_pos(ctx, mouse_x, mouse_y);
				skb_editor_process_mouse_click(ctx->editor, pos.x, pos.y, mouse_mods, glfwGetTime());
			}
		}

		if (action == GLFW_RELEASE) {
			if (ctx->drag_text) {
				ctx->drag_text = false;
			}
		}
	}

	update_ime_rect(ctx);
}

void inputfilter_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	inputfilter_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->drag_view) {
		view_drag_move(&ctx->view, mouse_x, mouse_y);
		update_ime_rect(ctx);
	}

	if (ctx->drag_text) {
		skb_vec2_t pos = transform_mouse_pos(ctx, mouse_x, mouse_y);
		skb_editor_process_mouse_drag(ctx->editor, pos.x, pos.y);
		update_ime_rect(ctx);
	}
}

void inputfilter_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	inputfilter_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void inputfilter_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	inputfilter_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	// Draw visual result
	{
		skb_color_t sel_color = skb_rgba(255,192,192,255);
		skb_color_t caret_color = skb_rgba(255,128,128,255);

		skb_vec2_t view_offset = skb_editor_get_view_offset(ctx->editor);
		const skb_rect2_t editor_view_bounds = skb_editor_get_view_bounds(ctx->editor);

		debug_render_stroked_rect(ctx->rc, editor_view_bounds.x - 5, editor_view_bounds.y - 5, editor_view_bounds.width + 10, editor_view_bounds.height + 10, skb_rgba(0,0,0,128), 1.f);
		debug_render_text(ctx->rc, editor_view_bounds.x - 5, editor_view_bounds.y - 20, 13, RENDER_ALIGN_START, skb_rgba(0,0,0,128), "Numeric Input");

		render_push_scissor(ctx->rc, editor_view_bounds.x, editor_view_bounds.y, editor_view_bounds.width, editor_view_bounds.height);

		skb_text_range_t edit_selection = skb_editor_get_current_selection(ctx->editor);
		if (skb_editor_get_text_range_count(ctx->editor, edit_selection) > 0)
			render_draw_text_range_background(ctx->rc, NULL, view_offset.x, view_offset.y, skb_editor_get_rich_layout(ctx->editor), edit_selection, sel_color);

		render_draw_rich_layout(ctx->rc, NULL, view_offset.x, view_offset.y, skb_editor_get_rich_layout(ctx->editor), SKB_RASTERIZE_ALPHA_SDF);

		render_pop_scissor(ctx->rc);

		// Caret is generally drawn only when there is no selection.
		if (skb_editor_get_text_range_count(ctx->editor, edit_selection) == 0) {
			const skb_caret_info_t caret_info = skb_editor_get_caret_info_at(ctx->editor, SKB_CURRENT_SELECTION_END);
			render_draw_caret(ctx->rc, NULL, view_offset.x, view_offset.y, &caret_info, 2.f, caret_color);
		}
	}

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
}
