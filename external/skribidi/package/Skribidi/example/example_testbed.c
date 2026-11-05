// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "render.h"
#include "debug_render.h"
#include "utils.h"

#include "hb.h"
#include "skb_common.h"
#include "skb_font_collection.h"
#include "skb_rasterizer.h"
#include "skb_layout.h"
#include "skb_editor.h"
#include "skb_image_atlas.h"
#include "ime.h"
#include "skb_rich_text.h"

typedef struct testbed_context_t {
	example_t base;

	skb_font_collection_t* font_collection;
	skb_temp_alloc_t* temp_alloc;
	render_context_t* rc;

	skb_editor_t* editor;

	skb_rich_text_t* rich_text_clipboard;
	uint64_t rich_text_clipboard_hash;

	bool allow_char;
	view_t view;
	bool drag_view;
	bool drag_text;

	float atlas_scale;
	bool show_glyph_details;
	bool show_caret_details;
	bool show_baseline_details;
	bool show_run_details;

} testbed_context_t;

static void update_ime_rect(testbed_context_t* ctx)
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
	testbed_context_t* ctx = context;

	if (event == IME_EVENT_COMPOSITION)
		skb_editor_set_composition_utf32(ctx->editor, ctx->temp_alloc, text, text_length, cursor);
	else if (event == IME_EVENT_COMMIT)
		skb_editor_commit_composition_utf32(ctx->editor, ctx->temp_alloc, text, text_length);
	else if (event == IME_EVENT_CANCEL)
		skb_editor_clear_composition(ctx->editor, ctx->temp_alloc);

	update_ime_rect(ctx);
}

void testbed_destroy(void* ctx_ptr);
void testbed_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods);
void testbed_on_char(void* ctx_ptr, unsigned int codepoint);
void testbed_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods);
void testbed_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y);
void testbed_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods);
void testbed_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height);

void* testbed_create(GLFWwindow* window, render_context_t* rc)
{
	assert(rc);

	testbed_context_t* ctx = skb_malloc(sizeof(testbed_context_t));
	memset(ctx, 0, sizeof(testbed_context_t));

	ctx->base.create = testbed_create;
	ctx->base.destroy = testbed_destroy;
	ctx->base.on_key = testbed_on_key;
	ctx->base.on_char = testbed_on_char;
	ctx->base.on_mouse_button = testbed_on_mouse_button;
	ctx->base.on_mouse_move = testbed_on_mouse_move;
	ctx->base.on_mouse_scroll = testbed_on_mouse_scroll;
	ctx->base.on_update = testbed_on_update;

	ctx->rc = rc;
	render_reset_atlas(rc, NULL);

	ctx->atlas_scale = 0.0f;
	ctx->show_glyph_details = false;
	ctx->show_caret_details = true;
	ctx->show_baseline_details = false;
	ctx->show_run_details = false;

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
//	LOAD_FONT_OR_FAIL("data/OpenMoji-color-glyf_colr_1.ttf", SKB_FONT_FAMILY_EMOJI);

	// These snippets have been useful for at some point in developing the library.
	// Leaving them here for future tests.
//	const char* bidi_text = "یہ ایک )cargfi( ہے۔";
//	const char* bidi_text = "Koffi";
//	const char* bidi_text = "nǐn hǎo¿Qué tal?Привет你好안녕하세요こんにちは";
//	const char* bidi_text = "a\u0308o\u0308u\u0308";
//	const char* bidi_text = "\uE0B0\u2588Öy";
//	const char* bidi_text = "एक गांव -- में मोहन नाम का लड़का रहता था। उसके पिताजी एक मामूली मजदूर थे";
//	const char* bidi_text = "ᬓ ᬓᬸ ᬓᭀ ᬓᬿ";

//	const char* bidi_text = "ᬓᭀ ᬓᬿ ہے۔ kofi یہ ایک";

//	const char* bidi_text = "ᬓᭀ ᬓᬿ ہے۔ [kofi] یہ ایک";

//	const char* bidi_text = "ᬓᭀ ᬓᬿ (ہے۔) [kofi] (یہ ایک)";

//	const char* bidi_text = "ہے۔ kofi یہ ایک"; // rlt line
//	const char* bidi_text = "asd ہے۔ kofi یہ ایک";
//	const char* bidi_text = "سلام در حال تست";

//	const char* bidi_text = "123سلام در حال تست";

//	const char* bidi_text = "123.456";

//	const char* bidi_text = "١١رس"; // arabic numerals

//	const char* bidi_text = "såppa";

//	const char* bidi_text = "لا"; // ligature
//	const char* bidi_text = "این یک تست است"; // this is a test

//	const char* bidi_text = "ltr این یک تست است"; // this is a test

//	const char* bidi_text = "aa این یک تست\nاست"; // this is a test

//	const char* bidi_text = "ہے۔ kofi یہ ایک";
//	const char* bidi_text = "私はその人を常に先生と 呼んでいた。";
//	const char* bidi_text = "วันนี้อากาศดี";
//	const char* bidi_text = "今天天气晴朗。";
//	const char* bidi_text ="Hamburgerfontstiv";

//	const char* bidi_text = "🤣moikka 🥰💀✌️🌴🐢🐐🍄⚽🍻👑📸😬foo 👀🚨🏡🕊️🏆😻🌟🧿🍀🎨🍜 bar 🥳🧁🍰🎁🎂🎈🎺🎉🎊📧〽️🧿🌶️🔋 😂❤️😍😊🥺🙏💕😭😘👍😅👏😁";

//	const char* bidi_text = "این یک 😬👀🚨 تست است"; // this is a test

//	const char* bidi_text = "い😍";

//	const char* bidi_text = "🤦🏼‍♂️ Ä था ᬓᬿ";

//	const char* bidi_text = "A, B, C, kissa kävelee, tikapuita pitkin taivaaseen.";

//	const char* bidi_text = "\nsorsa juo \r\n\r\nkaf  fia\n";
//	const char* bidi_text = "sorsa juo \nkaffia thisiverylongwordandstuff and more";
//	const char* bidi_text = "शकति शक्ति";
//	const char* bidi_text = "हिन्दी हि न्दी";
//	const char* bidi_text = "யாவற்றையும்"; // tamil, does not work correctly!
//	const char* bidi_text = "ঝিল্লি ঝি ল্লি"; // bengali
//	const char* bidi_text = "";

	const char* bidi_text = "Hamburgerfontstiv 🤣🥰💀✌️🌴🐢🐐🍄⚽🍻👑📸 این یک تست است 😬👀🚨🏡🕊️🏆😻🌟私はその人を常に先生と 呼んでいた。";

	ctx->temp_alloc = skb_temp_alloc_create(512*1024);
	assert(ctx->temp_alloc);

	skb_color_t ink_color = skb_rgba(64,64,64,255);

	const skb_attribute_t layout_attributes[] = {
		skb_attribute_make_tab_stop_increment(92.f * 2.f),
		skb_attribute_make_lang("zh-hans"),
		skb_attribute_make_text_wrap(SKB_WRAP_WORD_CHAR),
		skb_attribute_make_tab_stop_increment(92.f * 2.f),
		skb_attribute_make_line_height(SKB_LINE_HEIGHT_METRICS_RELATIVE, 1.3f),
	};

	const skb_attribute_t text_attributes[] = {
		skb_attribute_make_font_size(92.f),
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, ink_color),
	};

	const skb_attribute_t composition_attributes[] = {
		skb_attribute_make_paint_color(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, skb_rgba(0,128,192,255)),
		skb_attribute_make_decoration(SKB_DECORATION_LINE_UNDER, SKB_DECORATION_STYLE_DOTTED, 0.f, 1.f, SKB_PAINT_TEXT),
	};

	skb_editor_params_t edit_params = {
		.editor_width = 1200.f,
		.font_collection = ctx->font_collection,
		.layout_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(layout_attributes),
		.paragraph_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(text_attributes),
		.composition_attributes = SKB_ATTRIBUTE_SET_FROM_STATIC_ARRAY(composition_attributes),
	};

	ctx->editor = skb_editor_create(&edit_params);
	assert(ctx->editor);
	skb_editor_set_text_utf8(ctx->editor, ctx->temp_alloc, bidi_text, -1);

	ctx->rich_text_clipboard = skb_rich_text_create();
	ctx->rich_text_clipboard_hash = 0;

	ctx->view = (view_t) { .cx = 400.f, .cy = 120.f, .scale = 1.f };

	ime_set_handler(ime_handler, ctx);

	update_ime_rect(ctx);

	return ctx;

error:
	testbed_destroy(ctx);
	return NULL;
}

void testbed_destroy(void* ctx_ptr)
{
	testbed_context_t* ctx = ctx_ptr;
	assert(ctx);

	skb_editor_destroy(ctx->editor);
	skb_font_collection_destroy(ctx->font_collection);
	skb_temp_alloc_destroy(ctx->temp_alloc);
	skb_rich_text_destroy(ctx->rich_text_clipboard);

	memset(ctx, 0, sizeof(testbed_context_t));

	ime_cancel();
	ime_set_handler(NULL, NULL);

	skb_free(ctx);
}

void testbed_on_key(void* ctx_ptr, GLFWwindow* window, int key, int action, int mods)
{
	testbed_context_t* ctx = ctx_ptr;
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
			const uint64_t clipboard_hash = skb_hash64_append_str(skb_hash64_empty(), clipboard_text);
			if (clipboard_hash == ctx->rich_text_clipboard_hash) {
				// The text matches what we copied, paste the rich text version instead.
				skb_editor_insert_rich_text(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, ctx->rich_text_clipboard);
			} else {
				// Paste plain text from clipboard.
				skb_editor_insert_text_utf8(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, clipboard_text, -1);
			}
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
		if (key == GLFW_KEY_B && (mods & GLFW_MOD_CONTROL)) {
			// Bold
			skb_editor_toggle_attribute(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, skb_attribute_make_font_weight(SKB_WEIGHT_BOLD));
			ctx->allow_char = false;
		}
		if (key == GLFW_KEY_I && (mods & GLFW_MOD_CONTROL)) {
			// Italic
			skb_editor_toggle_attribute(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, skb_attribute_make_font_style(SKB_STYLE_ITALIC));
			ctx->allow_char = false;
		}
		if (key == GLFW_KEY_TAB) {
			skb_editor_insert_codepoint(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, '\t');
		}
		if (key == GLFW_KEY_ESCAPE) {
			// Clear selection
			if (skb_editor_get_text_range_count(ctx->editor, SKB_CURRENT_SELECTION) > 0)
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

			// Keep copy of the selection as rich text, so that we can paste as rich text.
			skb_editor_get_rich_text_in_range(ctx->editor, SKB_CURRENT_SELECTION, ctx->rich_text_clipboard);
			ctx->rich_text_clipboard_hash = skb_hash64_append_str(skb_hash64_empty(), text);

			SKB_TEMP_FREE(ctx->temp_alloc, text);
			skb_editor_insert_text_utf8(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, NULL, 0);
			ctx->allow_char = false;
		}
		if (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL)) {
			// Copy
			skb_text_range_t selection = skb_editor_get_current_selection(ctx->editor);
			int32_t text_len = skb_editor_get_text_utf8_count_in_range(ctx->editor, selection);
			char* text = SKB_TEMP_ALLOC(ctx->temp_alloc, char, text_len + 1);
			text_len = skb_editor_get_text_utf8_in_range(ctx->editor, selection, text, text_len);
			text[text_len] = '\0';
			glfwSetClipboardString(window, text);

			// Keep copy of the selection as rich text, so that we can paste as rich text.
			skb_editor_get_rich_text_in_range(ctx->editor, selection, ctx->rich_text_clipboard);
			ctx->rich_text_clipboard_hash = skb_hash64_append_str(skb_hash64_empty(), text);

			SKB_TEMP_FREE(ctx->temp_alloc, text);
			ctx->allow_char = false;
		}

		update_ime_rect(ctx);

		if (key == GLFW_KEY_F6) {
			ctx->show_run_details = !ctx->show_run_details;
		}
		if (key == GLFW_KEY_F7) {
			ctx->show_baseline_details = !ctx->show_baseline_details;
		}
		if (key == GLFW_KEY_F8) {
			ctx->show_caret_details = !ctx->show_caret_details;
		}
		if (key == GLFW_KEY_F9) {
			ctx->show_glyph_details = !ctx->show_glyph_details;
		}
		if (key == GLFW_KEY_F10) {
			ctx->atlas_scale += 0.25f;
			if (ctx->atlas_scale > 1.01f)
				ctx->atlas_scale = 0.0f;
		}
	}
}

void testbed_on_char(void* ctx_ptr, unsigned int codepoint)
{
	testbed_context_t* ctx = ctx_ptr;
	assert(ctx);

	if (ctx->allow_char)
		skb_editor_insert_codepoint(ctx->editor, ctx->temp_alloc, SKB_CURRENT_SELECTION, codepoint);
}

static skb_vec2_t transform_mouse_pos(testbed_context_t* ctx, float mouse_x, float mouse_y)
{
	return (skb_vec2_t) {
		.x = (mouse_x - ctx->view.cx) / ctx->view.scale,
		.y = (mouse_y - ctx->view.cy) / ctx->view.scale,
	};
}

void testbed_on_mouse_button(void* ctx_ptr, float mouse_x, float mouse_y, int button, int action, int mods)
{
	testbed_context_t* ctx = ctx_ptr;
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

void testbed_on_mouse_move(void* ctx_ptr, float mouse_x, float mouse_y)
{
	testbed_context_t* ctx = ctx_ptr;
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

void testbed_on_mouse_scroll(void* ctx_ptr, float mouse_x, float mouse_y, float delta_x, float delta_y, int mods)
{
	testbed_context_t* ctx = ctx_ptr;
	assert(ctx);

	const float zoom_speed = 0.2f;
	view_scroll_zoom(&ctx->view, mouse_x, mouse_y, delta_y * zoom_speed);
}

void testbed_on_update(void* ctx_ptr, int32_t view_width, int32_t view_height)
{
	testbed_context_t* ctx = ctx_ptr;
	assert(ctx);

	{
		skb_temp_alloc_stats_t stats = skb_temp_alloc_stats(ctx->temp_alloc);
		debug_render_text(ctx->rc, (float)view_width - 20,20, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)stats.used / 1024.f, (float)stats.allocated / 1024.f);
		skb_temp_alloc_stats_t render_stats = skb_temp_alloc_stats(render_get_temp_alloc(ctx->rc));
		debug_render_text(ctx->rc, (float)view_width - 20,40, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,220), "Render Temp alloc  used:%.1fkB  allocated:%.1fkB", (float)render_stats.used / 1024.f, (float)render_stats.allocated / 1024.f);
	}

	// Draw visual result
	render_push_transform(ctx->rc, ctx->view.cx, ctx->view.cy, ctx->view.scale);

	skb_color_t log_color = skb_rgba(32,128,192,255);
	skb_color_t caret_color = skb_rgba(255,128,128,255);
	skb_color_t caret_color_dark = skb_rgba(192,96,96,255);
	skb_color_t caret2_color = skb_rgba(128,128,255,255);
	skb_color_t caret_color_trans = skb_rgba(255,128,128,32);
	skb_color_t sel_color = skb_rgba(255,192,192,255);
	skb_color_t ink_color = skb_rgba(64,64,64,255);
	skb_color_t ink_color_trans = skb_rgba(32,32,32,128);

	float layout_height = 0.f;
	float layout_width = 0.f;
	for (int32_t pi = 0; pi < skb_editor_get_paragraph_count(ctx->editor); pi++) {
		const skb_layout_t* edit_layout = skb_editor_get_paragraph_layout(ctx->editor, pi);
		const skb_rect2_t layout_bounds = skb_layout_get_bounds(edit_layout);
		layout_height += layout_bounds.height;
		layout_width = skb_maxf(layout_width, layout_bounds.width);
	}

	const char* affinity_str[] = {
		"-",
		"TR", //SKB_AFFINITY_TRAILING,
		"LE", //SKB_AFFINITY_LEADING,
		"SOL", //SKB_AFFINITY_SOL, // Start of line
		"EOL", //SKB_AFFINITY_EOL, // End of line
	};

	{
		// line break boundaries
		const float line_break_width = skb_editor_get_params(ctx->editor)->editor_width;
		debug_render_dashed_line(ctx->rc, 0, -50, 0, layout_height+50, 6, ink_color_trans, -1.f);
		debug_render_dashed_line(ctx->rc, line_break_width, 50, line_break_width, layout_height+50, 6, ink_color_trans, -1.f);

		if (skb_editor_get_text_range_count(ctx->editor, SKB_CURRENT_SELECTION) > 0)
			render_draw_text_range_background(ctx->rc, NULL, 0.f, 0.f, skb_editor_get_rich_layout(ctx->editor), skb_editor_get_current_selection(ctx->editor), sel_color);

		for (int32_t pi = 0; pi < skb_editor_get_paragraph_count(ctx->editor); pi++) {
			const skb_layout_t* edit_layout = skb_editor_get_paragraph_layout(ctx->editor, pi);
			const skb_vec2_t edit_layout_offset = skb_editor_get_paragraph_offset(ctx->editor, pi);
			const skb_layout_line_t* lines = skb_layout_get_lines(edit_layout);
			const int32_t lines_count = skb_layout_get_lines_count(edit_layout);
			const skb_layout_run_t* layout_runs = skb_layout_get_layout_runs(edit_layout);
			const skb_glyph_t* glyphs = skb_layout_get_glyphs(edit_layout);
			const skb_cluster_t* clusters = skb_layout_get_clusters(edit_layout);
			const skb_layout_params_t* layout_params = skb_layout_get_params(edit_layout);
			const int32_t decorations_count = skb_layout_get_decorations_count(edit_layout);
			const skb_decoration_t* decorations = skb_layout_get_decorations(edit_layout);

			// Draw underlines
			for (int32_t i = 0; i < decorations_count; i++) {
				const skb_decoration_t* decoration = &decorations[i];
				if (decoration->layer == SKB_DECORATION_UNDER) {
					const skb_layout_run_t* run = &layout_runs[decoration->line.layout_run_idx];
					const skb_attribute_set_t run_attributes = skb_layout_get_layout_run_attributes(edit_layout, run);
					const skb_attribute_paint_t paint = skb_attributes_get_paint(decoration->line.paint_tag, SKB_PAINT_STATE_DEFAULT, run_attributes, layout_params->attribute_collection);
					render_draw_decoration(ctx->rc, edit_layout_offset.x, edit_layout_offset.y, decoration, paint.color, SKB_RASTERIZE_ALPHA_SDF);
				}
			}

			for (int li = 0; li < lines_count; li++) {
				const skb_layout_line_t* line = &lines[li];

				float rox = edit_layout_offset.x + line->bounds.x;
				float roy = edit_layout_offset.y + line->baseline;

				float top_y = roy + line->ascender;
				float bot_y = roy + line->descender;
				float baseline_y = roy;

				// Line info
				debug_render_line(ctx->rc, rox - 25, baseline_y,rox,baseline_y, ink_color, -1.f);
				debug_render_text(ctx->rc, rox - 12, baseline_y - 4,13,RENDER_ALIGN_CENTER, ink_color, "L%d", li);

				if (skb_is_rtl(skb_layout_get_resolved_direction(edit_layout)))
					debug_render_text(ctx->rc, rox - 10, bot_y - 5.f,13,RENDER_ALIGN_END, log_color, "< RTL");
				else
					debug_render_text(ctx->rc, rox - 10, bot_y - 5.f,13,RENDER_ALIGN_END, log_color, "LTR >");

				// Draw glyphs
				float pen_x = line->bounds.x;
				float run_start_x = pen_x;
				skb_rect2_t run_bounds = skb_rect2_make_undefined();

				for (int32_t ri = line->layout_run_range.start; ri < line->layout_run_range.end; ri++) {
					const skb_layout_run_t* layout_run = &layout_runs[ri];
					const skb_attribute_set_t layout_run_attributes = skb_layout_get_layout_run_attributes(edit_layout, layout_run);
					const skb_attribute_paint_t paint = skb_attributes_get_paint(SKB_PAINT_TEXT, SKB_PAINT_STATE_DEFAULT, layout_run_attributes, layout_params->attribute_collection);
					const float font_size = layout_run->font_size;

					if (ctx->show_run_details) {
						const skb_color_t col = skb_rgba(0,64,220,128);
						debug_render_stroked_rect(ctx->rc,
							edit_layout_offset.x + layout_run->bounds.x+2, edit_layout_offset.y + layout_run->bounds.y-1,
							layout_run->bounds.width-4, layout_run->bounds.height+2,
							col, -2.f);

						debug_render_text(ctx->rc,
							edit_layout_offset.x + layout_run->bounds.x+2, edit_layout_offset.y + layout_run->bounds.y + layout_run->bounds.height + 10,
							10, RENDER_ALIGN_START, col,
							"%c%c%c%c", SKB_UNTAG(skb_script_to_iso15924_tag(layout_run->script)));

						debug_render_text(ctx->rc,
							edit_layout_offset.x + layout_run->bounds.x+2, edit_layout_offset.y + layout_run->bounds.y + layout_run->bounds.height + 20,
							10, RENDER_ALIGN_START, col,
							"F%d", layout_run->font_handle & 0xffff);
					}

					for (int32_t gi = layout_run->glyph_range.start; gi < layout_run->glyph_range.end; gi++) {
						const skb_glyph_t* glyph = &glyphs[gi];

						float gx = edit_layout_offset.x + glyph->offset_x;
						float gy = edit_layout_offset.y + glyph->offset_y;

						if (ctx->show_glyph_details) {
							// Glyph pen position
							debug_render_tick(ctx->rc, gx, gy, 5.f, ink_color_trans, -1.f);

							// Glyph bounds
							skb_rect2_t bounds = skb_font_get_glyph_bounds(layout_params->font_collection, layout_run->font_handle, glyph->gid, font_size);
							debug_render_stroked_rect(ctx->rc, gx + bounds.x, gy + bounds.y, bounds.width, bounds.height, ink_color_trans, -1.f);

							// Visual index
							debug_render_text(ctx->rc, gx + bounds.x +2.f +0.5f, gy + bounds.y-8+0.5f,13, RENDER_ALIGN_START, ink_color, "%d", gi);

							// Keep track of run of glyphs that map to same text range.
							if (!skb_rect2_is_empty(bounds))
								run_bounds = skb_rect2_union(run_bounds, skb_rect2_translate(bounds, skb_vec2_make(gx,gy)));
						}

						if (layout_run->type == SKB_CONTENT_RUN_UTF8 || layout_run->type == SKB_CONTENT_RUN_UTF32) {
							// Text
							render_draw_glyph(ctx->rc, gx, gy,
								layout_params->font_collection, layout_run->font_handle, glyph->gid, font_size,
								paint.color, SKB_RASTERIZE_ALPHA_SDF);
						}

						if (ctx->show_baseline_details) {
							const skb_text_direction_t dir = layout_run->direction;
							const uint8_t script = layout_run->script;
							skb_baseline_set_t baseline_set = skb_font_get_baseline_set(layout_params->font_collection, layout_run->font_handle, dir, script, font_size);
							skb_font_metrics_t metrics = skb_font_get_metrics(layout_params->font_collection, layout_run->font_handle);

							const float rx = roundf(gx);
							const float ry = roundf(gy);

							debug_render_line(ctx->rc, rx, ry + metrics.ascender * font_size, rx + glyph->advance_x * 0.5f, ry + metrics.ascender * font_size, skb_rgba(0,0,0,255), -1.f);
							debug_render_line(ctx->rc, rx, ry + metrics.descender * font_size, rx + glyph->advance_x * 0.5f, ry + metrics.descender * font_size, skb_rgba(0,0,0,255), -1.f);

							debug_render_line(ctx->rc, rx, ry + baseline_set.alphabetic, rx + glyph->advance_x, ry + baseline_set.alphabetic, skb_rgba(255,64,0,255), -1.f);
							debug_render_line(ctx->rc, rx, ry + baseline_set.ideographic, rx + glyph->advance_x, ry + baseline_set.ideographic, skb_rgba(0,64,255,255), -1.f);
							debug_render_line(ctx->rc, rx, ry + baseline_set.hanging, rx + glyph->advance_x, ry + baseline_set.hanging, skb_rgba(0,192,255,255), -1.f);
							debug_render_line(ctx->rc, rx, ry + baseline_set.central, rx + glyph->advance_x, ry + baseline_set.central, skb_rgba(64,255,0,255), -1.f);
						}

						pen_x += glyph->advance_x;

						if (ctx->show_glyph_details) {
							const skb_cluster_t* cluster = &clusters[glyph->cluster_idx];
							int32_t last_glyph_in_cluster = cluster->glyphs_offset + cluster->glyphs_count - 1;
							if (gi == last_glyph_in_cluster) {
								// Glyph run bounds

								if (cluster->text_count > 1 && !skb_rect2_is_empty(run_bounds))
									debug_render_stroked_rect(ctx->rc, run_bounds.x - 4.f, run_bounds.y - 4.f, run_bounds.width + 8.f, run_bounds.height + 8.f, ink_color_trans, -1.f);

								// Logical id
								float run_end_x = pen_x;
								debug_render_stroked_rect(ctx->rc, run_start_x + 2.f + 0.5f, bot_y + 0.5f - 18, (run_end_x - run_start_x) - 4.f,  18.f, log_color, -1.f);
								if (cluster->text_count > 1)
									debug_render_text(ctx->rc, run_start_x + 5.f, bot_y - 5.f, 11, RENDER_ALIGN_START, log_color, "L%d - L%d", cluster->text_offset, cluster->text_offset + cluster->text_count - 1);
								else
									debug_render_text(ctx->rc, run_start_x + 5.f, bot_y - 5.f,11,RENDER_ALIGN_START, log_color, "L%d", cluster->text_offset);

								// Reset
								run_bounds = skb_rect2_make_undefined();
								run_start_x = pen_x;
							}
						}
					}
				}

				if (ctx->show_caret_details) {
					float left_text_offset = 0.f;

					skb_caret_iterator_t caret_iter = skb_caret_iterator_make(edit_layout, li);

					float caret_x = 0.f;
					float caret_advance = 0.f;
					float caret_mid_point = 0.f;
					skb_caret_iterator_result_t left = {0};
					skb_caret_iterator_result_t right = {0};

					while (skb_caret_iterator_next(&caret_iter, &caret_x, &caret_advance, &caret_mid_point, &left, &right)) {

						float cx = caret_x;
						debug_render_line(ctx->rc, cx, bot_y, cx, top_y + 5, caret_color, -1.f);

						if (left.direction != right.direction) {
							debug_render_tri(ctx->rc, cx, top_y+5, cx-5, top_y+5, cx, top_y+5+5, caret2_color);
							debug_render_tri(ctx->rc, cx, top_y+5, cx+5, top_y+5, cx, top_y+5+5, caret_color);
							debug_render_text(ctx->rc, cx-3,top_y + 20 + left_text_offset, 11, RENDER_ALIGN_END, caret2_color, "%s%d", affinity_str[left.text_position.affinity], left.text_position.offset);
							debug_render_text(ctx->rc, cx+3,top_y + 20, 11,RENDER_ALIGN_START, caret_color, "%s%d", affinity_str[right.text_position.affinity], right.text_position.offset);
							left_text_offset = caret_advance < 40.f ? 15 : 0;
						} else {
							if (right.text_position.affinity == SKB_AFFINITY_TRAILING) { // || caret_iter.right.affinity == SKB_AFFINITY_EOL) {
								debug_render_tri(ctx->rc, cx, top_y+5, cx + (skb_is_rtl(right.direction) ? -5 : 5), top_y+5, cx, top_y+5+5, caret_color);
								debug_render_text(ctx->rc, cx+3,top_y + 20,11, RENDER_ALIGN_START, caret_color, "%s%d", affinity_str[right.text_position.affinity], right.text_position.offset);
								left_text_offset = caret_advance < 40.f ? 15 : 0;
							} else {
								debug_render_tri(ctx->rc, cx, top_y+5, cx + (skb_is_rtl(left.direction) ? -5 : 5), top_y+5, cx, top_y+5+5, caret2_color);
								debug_render_text(ctx->rc, cx-3,top_y + 20+left_text_offset, 11, RENDER_ALIGN_END, caret2_color, "%s%d", affinity_str[left.text_position.affinity], left.text_position.offset);
								left_text_offset = 0.f;
							}
						}
					}
				}
			}

			// Draw through lines
			for (int32_t i = 0; i < decorations_count; i++) {
				const skb_decoration_t* decoration = &decorations[i];
				if (decoration->layer == SKB_DECORATION_OVER) {
					const skb_layout_run_t* run = &layout_runs[decoration->line.layout_run_idx];
					const skb_attribute_set_t run_attributes = skb_layout_get_layout_run_attributes(edit_layout, run);
					const skb_attribute_paint_t paint = skb_attributes_get_paint(decoration->line.paint_tag, SKB_PAINT_STATE_DEFAULT, run_attributes, layout_params->attribute_collection);
					render_draw_decoration(ctx->rc, edit_layout_offset.x, edit_layout_offset.y, decoration, paint.color, SKB_RASTERIZE_ALPHA_SDF);
				}
			}

		}

		// Caret & selection info
		{
			float cx = 0.f;

			skb_text_range_t edit_selection = skb_editor_get_current_selection(ctx->editor);

			// Caret
			cx = debug_render_text(ctx->rc, cx + 5,layout_height + 30, 13, RENDER_ALIGN_START, caret_color_dark, "Caret: %s%d",  affinity_str[edit_selection.end.affinity], edit_selection.end.offset);

			// Caret location
			int32_t line_idx = skb_editor_get_line_index_at(ctx->editor, SKB_CURRENT_SELECTION_END);
			int32_t col_idx = skb_editor_get_column_index_at(ctx->editor, SKB_CURRENT_SELECTION_END);

			cx = debug_render_text(ctx->rc, cx + 20,layout_height + 30, 13, RENDER_ALIGN_START, log_color, "Ln %d, Col %d", line_idx+1, col_idx+1);

			// Selection count
			const int32_t selection_count = skb_editor_get_text_range_count(ctx->editor, SKB_CURRENT_SELECTION);
			if (selection_count > 0) {
				cx = debug_render_text(ctx->rc, cx + 20,layout_height + 30, 13, RENDER_ALIGN_START, ink_color, "Selection %d - %d, (%d chars)", edit_selection.start.offset, edit_selection.end.offset, selection_count);
			}

			cx = debug_render_text(ctx->rc, cx + 20,layout_height + 30, 13, RENDER_ALIGN_START, ink_color, "text_offset %d", edit_selection.end.offset);

			// Active attributes
			const int32_t active_attributes_count = skb_editor_get_active_attributes_count(ctx->editor);
			const skb_attribute_t* active_attributes = skb_editor_get_active_attributes(ctx->editor);
			cx = debug_render_text(ctx->rc, cx + 20,layout_height + 30, 13, RENDER_ALIGN_START, ink_color, "Active attributes (%d):", active_attributes_count);
			for (int32_t i = 0; i < active_attributes_count; i++)
				cx = debug_render_text(ctx->rc, cx + 5,layout_height + 30, 13, RENDER_ALIGN_START, ink_color, "%c%c%c%c", SKB_UNTAG(active_attributes[i].kind));
		}

		// Caret is generally drawn only when there is no selection.
		if (skb_editor_get_text_range_count(ctx->editor, SKB_CURRENT_SELECTION) == 0) {

			// Visual caret
			const skb_caret_info_t caret_info = skb_editor_get_caret_info_at(ctx->editor, SKB_CURRENT_SELECTION_END);
			render_draw_caret(ctx->rc, NULL, 0.f, 0.f, &caret_info, 4.f, caret_color);

			// Caret affinity text
			const float affinity_x = caret_info.x + (caret_info.descender) * caret_info.slope;
			const float affinity_y = caret_info.y + (caret_info.descender);
			skb_text_range_t edit_selection = skb_editor_get_current_selection(ctx->editor);
			float dir = (edit_selection.end.affinity == SKB_AFFINITY_LEADING || edit_selection.end.affinity == SKB_AFFINITY_SOL) ? -1.f : 1.f;
			bool caret_is_rtl = skb_is_rtl(caret_info.direction);
			if (caret_is_rtl) dir = -dir;
			debug_render_text(ctx->rc, affinity_x + dir*7.f, affinity_y, 11, dir > 0.f ? RENDER_ALIGN_START : RENDER_ALIGN_END, caret_color, affinity_str[edit_selection.end.affinity]);
		}
	}

	// Draw logical string info
	{
		const skb_editor_params_t* edit_params = skb_editor_get_params(ctx->editor);
		const uint8_t font_family = skb_attributes_get_font_family(edit_params->paragraph_attributes, edit_params->attribute_collection);
		const float font_size = skb_attributes_get_font_size(edit_params->paragraph_attributes, edit_params->attribute_collection);
		const skb_weight_t font_weight = skb_attributes_get_font_weight(edit_params->paragraph_attributes, edit_params->attribute_collection);
		const skb_style_t font_style = skb_attributes_get_font_style(edit_params->paragraph_attributes, edit_params->attribute_collection);
		const skb_stretch_t font_stretch = skb_attributes_get_font_stretch(edit_params->paragraph_attributes, edit_params->attribute_collection);

		float ox = 0.f;
		float oy = 30.f + layout_height + 80.f;
		float sz = 80.f;
		float font_scale = (sz * 0.5f) / font_size;

		bool prev_is_emoji = false;
		uint8_t prev_script = 0;
		skb_font_handle_t font_handle = 0;

		skb_text_range_t edit_selection = skb_editor_get_current_selection(ctx->editor);
		skb_range_t caret_selection_range = skb_editor_get_offset_range_from_text_range(ctx->editor, edit_selection);
		int32_t caret_insert_idx = skb_editor_get_text_offset_from_text_position(ctx->editor, edit_selection.end);


/*		int32_t caret_selection_start_idx = -1;
		int32_t caret_selection_end_idx = -1;
		if (skb_editor_get_text_range_count(ctx->editor, SKB_CURRENT_SELECTION) > 0) {
			int32_t caret_start_idx = skb_editor_get_text_offset_from_text_position(ctx->editor, edit_selection.start);
			caret_selection_start_idx = skb_mini(caret_start_idx, caret_insert_idx);
			caret_selection_end_idx = skb_maxi(caret_start_idx, caret_insert_idx);
		}*/

		const int32_t edit_text_count = skb_editor_get_text_utf32(ctx->editor, NULL, 0);
		const int32_t edit_layout_count = skb_editor_get_paragraph_count(ctx->editor);

		for (int32_t pi = 0; pi < edit_layout_count; pi++) {
			const skb_layout_t* edit_layout = skb_editor_get_paragraph_layout(ctx->editor, pi);
			const int32_t edit_text_offset = skb_editor_get_paragraph_global_text_offset(ctx->editor, pi);
			const bool is_last_edit_line = pi == edit_layout_count - 1;

			const skb_layout_line_t* lines = skb_layout_get_lines(edit_layout);
			const int32_t lines_count = skb_layout_get_lines_count(edit_layout);
			const uint32_t* text = skb_layout_get_text(edit_layout);
			const skb_text_property_t* text_props = skb_layout_get_text_properties(edit_layout);

			for (int line_idx = 0; line_idx < lines_count; line_idx++) {
				const skb_layout_line_t* line = &lines[line_idx];
				const bool is_last_layout_line = line_idx == lines_count - 1;

				ox = 0.f;
				for (int32_t cp_idx = line->text_range.start; cp_idx < line->text_range.end; cp_idx++) {
					const uint32_t cp = text[cp_idx];

					// Selection
					if ((edit_text_offset + cp_idx) >= caret_selection_range.start && (cp_idx + edit_text_offset) < caret_selection_range.end)
						debug_render_filled_rect(ctx->rc, ox-1.f,oy-1.f,sz+2.f,sz+2.f,sel_color);

					// Glyph box
					debug_render_stroked_rect(ctx->rc,  ox+0.5f, oy+0.5f, sz, sz, log_color, -1.f);

					// Caret insert position
					if ((edit_text_offset + cp_idx) == caret_insert_idx) {
						debug_render_filled_rect(ctx->rc, ox+1.5f, oy+1.5f, sz-2, sz-2, caret_color_trans);
					}
					// Caret position
					if ((edit_text_offset + cp_idx) == edit_selection.end.offset) {
						float cx = ox + 6.f;
						float dir = 1.f;
						if (edit_selection.end.affinity == SKB_AFFINITY_EOL || edit_selection.end.affinity == SKB_AFFINITY_LEADING) {
							cx += sz - 12.f;
							dir = -1.f;
						}

						debug_render_line(ctx->rc, cx,oy+6.f,cx,oy+sz-5.f,caret_color, 4.f);

						// Direction triangle
						skb_caret_info_t caret_info = skb_editor_get_caret_info_at(ctx->editor, edit_selection.end);
						bool caret_is_rtl = skb_is_rtl(caret_info.direction);
						float as = sz / 8.f;
						float dx = (caret_is_rtl ? -as : as);
						debug_render_tri(ctx->rc, cx, oy+4,
							cx + dx, oy+4,
							cx, oy+3+as,
							caret_color);

						debug_render_text(ctx->rc, cx + dir*5.f, oy+sz-7+0.5f, 11, dir > 0.f ? RENDER_ALIGN_START : RENDER_ALIGN_END, caret_color, affinity_str[edit_selection.end.affinity]);
					}

					const uint8_t script = text_props[cp_idx].script;
					const bool is_emoji = (text_props[cp_idx].flags & SKB_TEXT_PROP_EMOJI);
					if (!font_handle || script != prev_script || is_emoji != prev_is_emoji) {
						if (skb_font_collection_match_fonts(ctx->font_collection, "", script,
							is_emoji ? SKB_FONT_FAMILY_EMOJI : font_family, font_weight, font_style, font_stretch, &font_handle, 1) == 0)
							font_handle = 0;
						prev_script = script;
						prev_is_emoji = is_emoji;
					}

					// Logical index
					debug_render_text(ctx->rc, ox+0.5f, oy-8+0.5f, 11, RENDER_ALIGN_START, log_color, "L%d", edit_text_offset + cp_idx);

					// Codepoint
					debug_render_text(ctx->rc, ox+4+0.5f, oy+14+0.5f,11,0, ink_color, "0x%X", cp);

					if (font_handle) {
						uint32_t gid = 0;
						hb_font_get_nominal_glyph(skb_font_get_hb_font(ctx->font_collection, font_handle), cp, &gid);

						// Draw glyph centered on the rect.
						skb_rect2_t bounds = skb_font_get_glyph_bounds(ctx->font_collection, font_handle, gid, font_size * font_scale);

						float base_line = oy + sz * 0.75f;
						debug_render_line(ctx->rc, ox+4+0.5f, base_line+0.5f, ox + sz - 4+0.5f, base_line+0.5f, log_color, -1.f);

						float gx = ox + sz * 0.5f - bounds.width * 0.5f+0.5f;
						float gy = base_line+0.5f;

						render_draw_glyph(ctx->rc, gx, gy,
							ctx->font_collection, font_handle, gid, font_size * font_scale,
							ink_color, SKB_RASTERIZE_ALPHA_MASK);

					} else {
						debug_render_text(ctx->rc, ox+10+0.5f, oy+sz*0.5f+0.5f, 13, RENDER_ALIGN_START, ink_color_trans, "<Empty>");
					}

					// Draw properties
					float lx = ox+4;
					float ly = oy + sz + 15;
					float rx = ox + sz-4;
					float ry = oy + sz + 15;

					if (text_props[cp_idx].flags & SKB_TEXT_PROP_GRAPHEME_BREAK) {
						debug_render_text(ctx->rc, rx-1.5f, ry+0.5f, 11, RENDER_ALIGN_END, caret_color, "GB");
						ry += 13.f;
					}

					if (text_props[cp_idx].flags & SKB_TEXT_PROP_WORD_BREAK) {
						debug_render_text(ctx->rc, rx-1.5f, ry+0.5f, 11,RENDER_ALIGN_END, ink_color_trans, "WB");
						ry += 13.f;
					}
					if (text_props[cp_idx].flags & SKB_TEXT_PROP_MUST_LINE_BREAK) {
						debug_render_text(ctx->rc, rx-1.5f, ry+0.5f, 11,RENDER_ALIGN_END, log_color, "LB!");
						ry += 13.f;
					}
					if (text_props[cp_idx].flags & SKB_TEXT_PROP_ALLOW_LINE_BREAK) {
						debug_render_text(ctx->rc, rx-1.5f, ry+0.5f, 11, RENDER_ALIGN_END, log_color, "LB?");
						ry += 13.f;
					}

					// Script
					debug_render_text(ctx->rc, lx+1.5f, ly+0.5f, 11, RENDER_ALIGN_START, log_color, "%c%c%c%c %s", SKB_UNTAG(skb_script_to_iso15924_tag(script)), (text_props[cp_idx].flags & SKB_TEXT_PROP_EMOJI) ? ":)" : "");
					ly += 13.f;
					// Direction
					debug_render_text(ctx->rc, lx+1.5f, ly+0.5f, 11, RENDER_ALIGN_START, log_color, skb_is_rtl(skb_layout_get_text_direction_at(edit_layout, (skb_text_position_t){.offset = cp_idx})) ? "<R" : "L>");
					ly += 13.f;

					// Next block
					ox += sz + 4.f;
				}

				if (is_last_edit_line && is_last_layout_line) {
					// Caret at EOS
					if ((edit_text_offset + line->last_grapheme_offset) == edit_text_count) {
						debug_render_filled_rect(ctx->rc, ox+1.5f,oy+1.5f,sz-2,sz-2,caret_color_trans);
					}
				}

				// next line
				oy += sz * 2.f;
			}
		}
	}

	render_pop_transform(ctx->rc);

	// Draw atlas
	render_update_atlas(ctx->rc);
	debug_render_atlas_overlay(ctx->rc, 20.f, 50.f, ctx->atlas_scale, 1);

	// Draw info
	debug_render_text(ctx->rc, (float)view_width - 20.f, (float)view_height - 15.f, 13, RENDER_ALIGN_END, skb_rgba(0,0,0,255),
		"F6: Run details %s   F7: Baseline details %s   F8: Caret details %s   F9: Glyph details %s   F10: Atlas %.1f%%",
		ctx->show_run_details ? "ON" : "OFF,",
		ctx->show_baseline_details ? "ON" : "OFF",
		ctx->show_caret_details ? "ON" : "OFF",
		ctx->show_glyph_details ? "ON" : "OFF",
		ctx->atlas_scale * 100.f);
}
