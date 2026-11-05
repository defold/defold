// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_editor.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hb.h"

#include "skb_layout.h"
#include "skb_common.h"
#include "skb_text.h"
#include "skb_text_internal.h"
#include "skb_rich_text.h"
#include "skb_rich_text_internal.h"
#include "skb_rich_layout.h"
#include "skb_rich_layout_internal.h"

// From skb_layout.c
skb_text_position_t skb__caret_prune_control_eol(const skb_layout_t* layout, const skb_layout_line_t* line, skb_text_position_t caret);

//
// Text edit
//


typedef enum {
	SKB_UNDO_TEXT,
	SKB_UNDO_TEXT_ATTRIBUTES,
} skb__undo_state_type_t;

// Stores enough data to be able to undo and redo a single text edit.
typedef struct skb__editor_undo_state_t {
	skb__undo_state_type_t type;

	skb_text_range_t removed_range;		// Removed text range before replace.
	skb_rich_text_t removed_text;		// Removed attributed text

	skb_text_range_t inserted_range;		// Inserted text range after replace.
	skb_rich_text_t inserted_text;		// Inserted attributed text

	bool allow_amend_undo;
} skb__editor_undo_state_t;

typedef struct skb__editor_undo_transaction_t {
	skb_range_t states_range;		// Removed text range before replace.
	skb_text_range_t selection_before;	// Selection before the change.
	skb_text_range_t selection_after;	// Selection after at the change (at the point of undo).
} skb__editor_undo_transaction_t;

typedef struct skb_editor_t {
	skb_editor_params_t params;

	skb_attribute_t* attributes;
	int32_t attributes_count;
	int32_t attributes_cap;

	skb_editor_on_text_change_func_t* on_text_change_callback;
	void* on_text_change_context;

	skb_editor_on_selection_change_func_t* on_selection_change_callback;
	void* on_selection_change_context;

	skb_editor_input_filter_func_t* input_filter_callback;
	void* input_filter_context;

	skb_attribute_t* active_attributes;
	int32_t active_attributes_count;
	int32_t active_attributes_cap;
	int32_t active_attribute_paragraph_idx;
	float preview_caret_slope;		// Preview font slope for active selection
	float preview_caret_ascender;	// Preview font ascender for active selection
	float preview_caret_descender;	// Preview font descender for active selection
	bool is_preview_caret_valid;

	skb_rich_text_t rich_text;		// The edited text
	skb_rich_layout_t rich_layout;	// Layout of the edited text, including composition text.

	skb_rich_text_t scratch_rich_text;	// Scratch rich text used for input handling.

	skb_text_range_t selection;

	double last_click_time;
	float drag_start_x;
	float drag_start_y;
	float preferred_x;
	int32_t click_count;
	skb_text_range_t drag_initial_selection;
	uint8_t drag_moved;
	uint8_t drag_mode;

	skb_vec2_t view_offset;

	// IME
	skb_text_t composition_text;
	int32_t composition_text_offset;				// Global text offset where the composition is displayed.
	skb_text_position_t composition_selection_base;	// Base position for setting the composition selection.
	skb_text_range_t composition_selection;			// Selection during composition.
	bool composition_cleared_selection;				// True if initial setting the composition text cleared the current selection.

	// Undo
	skb__editor_undo_transaction_t* undo_stack;	// The undo stack.
	int32_t undo_stack_count;				// Size if the undo stack
	int32_t undo_stack_cap;					// Allocated space for the undo stack.
	int32_t undo_stack_head;				// Current state inside the undo stack. Initially -1, increases on each change. Undo moves down, redo up (up to stack count).

	skb__editor_undo_state_t* undo_states;	// The undo stack.
	int32_t undo_states_count;				// Size if the undo stack
	int32_t undo_states_cap;					// Allocated space for the undo stack.

	int32_t in_undo_transaction;
} skb_editor_t;

// fwd decl
static void skb__reset_undo(skb_editor_t* editor);
static int32_t skb__capture_undo_text_begin(skb_editor_t* editor, skb_text_range_t text_range, const skb_rich_text_t* rich_text, bool allow_amend_undo);
static void skb__capture_undo_text_end(skb_editor_t* editor, int32_t transaction_id);

static skb_text_position_t skb__resolve_text_position(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	if (skb_text_position_is_current_selection_end(text_pos)) {
		// Call get current selection, so that we get correct handling of composition cursor too.
		const skb_text_range_t current_selection = skb_editor_get_current_selection(editor);
		text_pos = current_selection.end;
	}
	return text_pos;
}

static skb_text_range_t skb__resolve_text_range(const skb_editor_t* editor, skb_text_range_t text_range)
{
	if (skb_text_range_is_current_selection(text_range)) {
		// Call get current selection, so that we get correct handling of composition cursor too.
		text_range = skb_editor_get_current_selection(editor);
	}
	return text_range;
}

static void skb__update_selection_from_change(skb_editor_t* editor, skb_rich_text_change_t change)
{
	assert(editor);

	// Update selection
	if (change.edit_end_position.affinity != SKB_AFFINITY_NONE) {
		editor->selection.start = change.edit_end_position;
		editor->selection.end = change.edit_end_position;
		editor->preferred_x = -1.f; // reset preferred.
	}
}

static void skb__editor_clamp_view_offset(skb_editor_t* editor)
{
	skb_rect2_t view_bounds = skb_editor_get_view_bounds(editor);
	const skb_rect2_t content_bounds = skb_rich_layout_get_bounds(&editor->rich_layout);
	const float view_offset_max_x = skb_maxf(0.f, content_bounds.width - view_bounds.width);
	const float view_offset_max_y = skb_maxf(0.f, content_bounds.height - view_bounds.height);
	editor->view_offset.x = skb_clampf(editor->view_offset.x, -view_offset_max_x, 0.f);
	editor->view_offset.y = skb_clampf(editor->view_offset.y, -view_offset_max_y, 0.f);
}

static void skb__ensure_caret_visible(skb_editor_t* editor)
{
	skb_rect2_t view_bounds = skb_editor_get_view_bounds(editor);
	const skb_caret_info_t caret_info = skb_editor_get_caret_info_at(editor, SKB_CURRENT_SELECTION_END);
	const skb_rect2_t content_bounds = skb_rich_layout_get_bounds(&editor->rich_layout);
	const float view_offset_max_x = skb_maxf(0.f, content_bounds.width - view_bounds.width);
	const float view_offset_max_y = skb_maxf(0.f, content_bounds.height - view_bounds.height);

	const skb_attribute_caret_padding_t caret_padding = skb_attributes_get_caret_padding(editor->params.layout_attributes, editor->params.attribute_collection);
	const skb_text_overflow_t text_overflow = skb_attributes_get_text_overflow(editor->params.layout_attributes, editor->params.attribute_collection);

	if (text_overflow == SKB_OVERFLOW_SCROLL) {
		const float caret_min_x = caret_info.x - caret_padding.horizontal;
		const float caret_max_x = caret_info.x + caret_padding.horizontal;
		const float caret_min_y = caret_info.y + caret_info.ascender - caret_padding.vertical;
		const float caret_max_y = caret_info.y + caret_info.descender + caret_padding.vertical;

		const float view_min_x = -editor->view_offset.x;
		const float view_max_x = -editor->view_offset.x + view_bounds.width;
		const float view_min_y = -editor->view_offset.y;
		const float view_max_y = -editor->view_offset.y + view_bounds.height;

		if (caret_min_x < view_min_x)
			editor->view_offset.x = -caret_min_x;
		else if (caret_max_x > view_max_x)
			editor->view_offset.x = -(caret_max_x - view_bounds.width);
		editor->view_offset.x = skb_clampf(editor->view_offset.x, -view_offset_max_x, 0.f);

		if (caret_min_y < view_min_y)
			editor->view_offset.y = -caret_min_y;
		else if (caret_max_y > view_max_y)
			editor->view_offset.y = -(caret_max_y - view_bounds.height);
		editor->view_offset.y = skb_clampf(editor->view_offset.y, -view_offset_max_y, 0.f);
	} else {
		editor->view_offset.x = 0.f;
		editor->view_offset.y = 0.f;
	}
}

static void skb__update_layout(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_rich_text_change_t change)
{
	assert(editor);

	skb_rich_layout_apply_change(&editor->rich_layout, change);

	skb_layout_params_t layout_params = {0}; //editor->params.layout_params;
	layout_params.attribute_collection = editor->params.attribute_collection;
	layout_params.font_collection = editor->params.font_collection;
	layout_params.icon_collection = editor->params.icon_collection;
	layout_params.layout_width = editor->params.editor_width;
	layout_params.layout_height = editor->params.editor_height;
	layout_params.layout_attributes = editor->params.layout_attributes;
	layout_params.flags |= SKB_LAYOUT_PARAMS_IGNORE_MUST_LINE_BREAKS | SKB_LAYOUT_PARAMS_IGNORE_OVERFLOW;

	skb_rich_layout_set_from_rich_text(&editor->rich_layout, temp_alloc, &layout_params, &editor->rich_text, editor->composition_text_offset, &editor->composition_text);

	// Make sure the selection conforms the new layout.
	skb_paragraph_position_t selection_start_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, editor->selection.start, SKB_AFFINITY_IGNORE);
	skb_paragraph_position_t selection_end_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, editor->selection.end, SKB_AFFINITY_IGNORE);
	editor->selection.start.offset = selection_start_pos.global_text_offset;
	editor->selection.end.offset = selection_end_pos.global_text_offset;

	// Make sure the view offset is in bounds.
	skb__editor_clamp_view_offset(editor);
}

static const skb_layout_t* skb__get_layout(const skb_editor_t* editor, int32_t paragraph_idx)
{
	return skb_rich_layout_get_layout(&editor->rich_layout, paragraph_idx);
}

static skb_text_direction_t skb__get_layout_resolved_direction(const skb_editor_t* editor, int32_t paragraph_idx)
{
	return skb_rich_layout_get_direction(&editor->rich_layout, paragraph_idx);
}

static const skb_text_t* skb__get_text(const skb_editor_t* editor, int32_t paragraph_idx)
{
	return skb_rich_text_get_paragraph_text(&editor->rich_text, paragraph_idx);
}

static int32_t skb__get_text_count(const skb_editor_t* editor, int32_t paragraph_idx)
{
	return skb_rich_text_get_paragraph_text_utf32_count(&editor->rich_text, paragraph_idx);
}

static int32_t skb__get_global_text_offset(const skb_editor_t* editor, int32_t paragraph_idx)
{
	return skb_rich_text_get_paragraph_text_offset(&editor->rich_text, paragraph_idx);
}

static skb_attribute_set_t skb__get_paragraph_attributes(const skb_editor_t* editor, int32_t paragraph_idx)
{
	return skb_rich_text_get_paragraph_attributes(&editor->rich_text, paragraph_idx);
}

static bool skb__are_paragraphs_in_sync(const skb_editor_t* editor)
{
	return skb_rich_text_get_paragraphs_count(&editor->rich_text) == skb_rich_layout_get_paragraphs_count(&editor->rich_layout);
}

static int32_t skb__get_paragraph_count(const skb_editor_t* editor)
{
	assert(skb__are_paragraphs_in_sync(editor));
	return skb_rich_text_get_paragraphs_count(&editor->rich_text);
}

static void skb__update_preview_caret(skb_editor_t* editor)
{
	const skb_text_position_t caret_pos = skb__resolve_text_position(editor, editor->selection.end);
	const skb_paragraph_position_t caret_paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, caret_pos, SKB_AFFINITY_USE);

	const skb_layout_t* layout = skb__get_layout(editor, caret_paragraph_pos.paragraph_idx);
	const int32_t pick_offset = skb_layout_get_prev_grapheme_offset(layout, caret_paragraph_pos.text_offset);

	editor->is_preview_caret_valid = false;
	editor->preview_caret_slope = 0.f;
	editor->preview_caret_ascender = 0.f;
	editor->preview_caret_descender = 0.f;

	// Try to figure out the font slope based on the current active attributes.

	// Get script from the current selection
	const skb_text_property_t* text_properties = skb_layout_get_text_properties(layout);
	if (text_properties && pick_offset >= 0 && pick_offset < skb_layout_get_text_count(layout)) {
		const uint8_t script = text_properties[pick_offset].script;

		// Create attribute set chain (active > paragraph > layout)
		skb_attribute_set_t paragraph_attributes = skb__get_paragraph_attributes(editor, caret_paragraph_pos.paragraph_idx);
		paragraph_attributes.parent_set = &editor->params.layout_attributes;

		skb_attribute_set_t active_attributes = {
			.attributes = editor->active_attributes,
			.attributes_count = editor->active_attributes_count,
			.parent_set = &paragraph_attributes,
		};

		// Get relevant attributes to select a font.
		const float font_size = skb_attributes_get_font_size(active_attributes, editor->params.attribute_collection);
		const skb_attribute_font_size_scaling_t font_size_scaling = skb_attributes_get_font_size_scaling(active_attributes, editor->params.attribute_collection);
		const uint8_t font_family = skb_attributes_get_font_family(active_attributes, editor->params.attribute_collection);
		const skb_weight_t font_weight = skb_attributes_get_font_weight(active_attributes, editor->params.attribute_collection);
		const skb_style_t font_style = skb_attributes_get_font_style(active_attributes, editor->params.attribute_collection);
		const skb_stretch_t font_stretch = skb_attributes_get_font_stretch(active_attributes, editor->params.attribute_collection);
		const char* lang = skb_attributes_get_lang(active_attributes, editor->params.attribute_collection);
		const hb_language_t run_lang = hb_language_from_string(lang, -1);

		skb_font_handle_t font_handles[32];
		int32_t fonts_count = skb_font_collection_match_fonts(
			editor->params.font_collection, hb_language_to_string(run_lang), script, font_family,
			font_weight, font_style, font_stretch,
			font_handles, SKB_COUNTOF(font_handles));

		if (fonts_count > 0) {
			const skb_font_metrics_t font_metrics = skb_font_get_metrics(layout->params.font_collection, font_handles[0]);
			const skb_caret_metrics_t caret_metrics = skb_font_get_caret_metrics(editor->params.font_collection, font_handles[0]);

			float scaled_font_size = font_size;
			if (font_size_scaling.type == SKB_FONT_SIZE_SCALING_NORMAL) {
				scaled_font_size *= skb_absf(font_size_scaling.scale);
			} else if (font_size_scaling.type == SKB_FONT_SIZE_SCALING_SUBSCRIPT) {
				scaled_font_size *= font_metrics.subscript_scale;
			} else if (font_size_scaling.type == SKB_FONT_SIZE_SCALING_SUPERSCRIPT) {
				scaled_font_size *= font_metrics.superscript_scale;
			}

			editor->preview_caret_slope = caret_metrics.slope;
			editor->preview_caret_ascender = font_metrics.ascender * scaled_font_size;
			editor->preview_caret_descender = font_metrics.descender * scaled_font_size;
			editor->is_preview_caret_valid = true;
		}
	}
}

static bool skb__attribute_span_contains(const skb_attribute_span_t* attribute_span, int32_t offset)
{
	if (attribute_span->flags & SKB_ATTRIBUTE_SPAN_END_EXCLUSIVE) {
		return skb_range_contains((skb_range_t){ .start = attribute_span->text_range.start, .end = attribute_span->text_range.end-1 }, offset);
	}
	return skb_range_contains(attribute_span->text_range, offset);
}

static void skb__pick_active_attributes(skb_editor_t* editor)
{
	// Pick the active attributes from the text before the cursor.
	skb_text_position_t caret_pos = skb__resolve_text_position(editor, editor->selection.end);
	const skb_paragraph_position_t caret_paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, caret_pos, SKB_AFFINITY_USE);

	// Pick style from the character before.
	const skb_layout_t* layout = skb__get_layout(editor, caret_paragraph_pos.paragraph_idx);
	const int32_t pick_offset = skb_layout_get_prev_grapheme_offset(layout, caret_paragraph_pos.text_offset);

	const skb_text_t* paragraph_text = skb__get_text(editor, caret_paragraph_pos.paragraph_idx);
	int32_t attribute_spans_count = skb_text_get_attribute_spans_count(paragraph_text);
	const skb_attribute_span_t* attribute_spans = skb_text_get_attribute_spans(paragraph_text);

	editor->active_attributes_count = 0;
	for (int32_t i = 0; i < attribute_spans_count; i++) {
		if (skb__attribute_span_contains(&attribute_spans[i], pick_offset)) {
			SKB_ARRAY_RESERVE(editor->active_attributes, editor->active_attributes_count + 1);
			skb_attribute_t* attribute = &editor->active_attributes[editor->active_attributes_count++];
			*attribute = attribute_spans[i].attribute;
		}
	}

	editor->active_attribute_paragraph_idx = caret_paragraph_pos.paragraph_idx;

	skb__update_preview_caret(editor);
}

static void skb__emit_on_text_change(skb_editor_t* editor, skb_editor_text_change_reason_t reason)
{
	if (editor->on_text_change_callback)
		editor->on_text_change_callback(editor, reason, editor->on_text_change_context);
}

static void skb__emit_on_selection_change(skb_editor_t* editor, skb_editor_selection_change_reason_t reason)
{
	if (editor->on_selection_change_callback)
		editor->on_selection_change_callback(editor, reason, editor->on_text_change_context);
}

static void skb__set_params(skb_editor_t* editor, const skb_editor_params_t* params)
{
	// Copy params
	editor->params = *params;

	// Init defaults.
	if (editor->params.max_undo_levels == 0)
		editor->params.max_undo_levels = 50;

	// Copy attributes
	editor->params.layout_attributes = (skb_attribute_set_t){0};
	editor->params.paragraph_attributes = (skb_attribute_set_t){0};
	editor->params.composition_attributes = (skb_attribute_set_t){0};

	const int32_t layout_attributes_count = skb_attributes_get_copy_flat_count(params->layout_attributes);
	const int32_t text_attributes_count = skb_attributes_get_copy_flat_count(params->paragraph_attributes);
	const int32_t composition_attributes_count = skb_attributes_get_copy_flat_count(params->composition_attributes);

	SKB_ARRAY_RESERVE(editor->attributes, layout_attributes_count + text_attributes_count + composition_attributes_count);

	if (layout_attributes_count > 0) {
		skb_attribute_t* attributes = &editor->attributes[editor->attributes_count];
		editor->attributes_count += layout_attributes_count;
		assert(editor->attributes_count <= editor->attributes_cap);
		editor->params.layout_attributes.attributes = attributes;
		editor->params.layout_attributes.attributes_count = skb_attributes_copy_flat(params->layout_attributes, attributes, layout_attributes_count);
	}
	if (text_attributes_count > 0) {
		skb_attribute_t* attributes = &editor->attributes[editor->attributes_count];
		editor->attributes_count += text_attributes_count;
		assert(editor->attributes_count <= editor->attributes_cap);
		editor->params.paragraph_attributes.attributes = attributes;
		editor->params.paragraph_attributes.attributes_count = skb_attributes_copy_flat(params->paragraph_attributes, attributes, text_attributes_count);
	}
	if (composition_attributes_count > 0) {
		skb_attribute_t* attributes = &editor->attributes[editor->attributes_count];
		editor->attributes_count += composition_attributes_count;
		assert(editor->attributes_count <= editor->attributes_cap);
		editor->params.composition_attributes.attributes = attributes;
		editor->params.composition_attributes.attributes_count = skb_attributes_copy_flat(params->composition_attributes, attributes, composition_attributes_count);
	}
}


static int32_t skb__get_line_index(const skb_editor_t* editor, skb_paragraph_position_t edit_pos)
{
	const int32_t lines_count = skb_layout_get_lines_count(skb__get_layout(editor, edit_pos.paragraph_idx));
	int32_t line_idx = 0;

	if (edit_pos.text_offset < 0) {
		// We should hit this only when the pos.offset is before the first line.
		line_idx = 0;
	} else if (edit_pos.text_offset >= skb__get_text_count(editor, edit_pos.paragraph_idx)) {
		// We should hit this only when the pos.offset is past the last line.
		line_idx = lines_count - 1;
	} else {
		const skb_layout_line_t* lines = skb_layout_get_lines(skb__get_layout(editor, edit_pos.paragraph_idx));
		for (int32_t i = 0; i < lines_count; i++) {
			const skb_layout_line_t* line = &lines[i];
			if (edit_pos.text_offset < line->text_range.end) {
				line_idx = i;
				break;
			}
		}
	}

	return line_idx;
}

static skb_paragraph_position_t skb__get_next_grapheme_pos(const skb_editor_t* editor, skb_paragraph_position_t edit_pos)
{
	edit_pos.text_offset = skb_text_get_next_grapheme_offset(skb__get_text(editor, edit_pos.paragraph_idx), edit_pos.text_offset);

	// Affinity adjustment may push the offset to next edit line
	if (edit_pos.text_offset >= skb__get_text_count(editor, edit_pos.paragraph_idx)) {
		if ((edit_pos.paragraph_idx + 1) < skb__get_paragraph_count(editor)) {
			edit_pos.text_offset = 0;
			edit_pos.paragraph_idx++;
		}
	}

	edit_pos.global_text_offset = skb__get_global_text_offset(editor, edit_pos.paragraph_idx) + edit_pos.text_offset;

	return edit_pos;
}

static skb_paragraph_position_t skb__get_prev_grapheme_pos(const skb_editor_t* editor, skb_paragraph_position_t edit_pos)
{
	if (edit_pos.text_offset == 0) {
		if ((edit_pos.paragraph_idx - 1) >= 0) {
			edit_pos.paragraph_idx--;
			edit_pos.text_offset = skb__get_text_count(editor, edit_pos.paragraph_idx);
		}
	}

	edit_pos.text_offset = skb_text_get_prev_grapheme_offset(skb__get_text(editor, edit_pos.paragraph_idx), edit_pos.text_offset);

	edit_pos.global_text_offset = skb__get_global_text_offset(editor, edit_pos.paragraph_idx) + edit_pos.text_offset;

	return edit_pos;
}



skb_editor_t* skb_editor_create(const skb_editor_params_t* params)
{
	assert(params);

	skb_editor_t* editor = skb_malloc(sizeof(skb_editor_t));
	SKB_ZERO_STRUCT(editor);

	editor->scratch_rich_text = skb_rich_text_make_empty();
	editor->rich_text = skb_rich_text_make_empty();
	editor->rich_layout = skb_rich_layout_make_empty();
	editor->composition_text = skb_text_make_empty();

	skb__set_params(editor, params);

	const skb_text_position_t start_pos = { .offset = 0, .affinity = SKB_AFFINITY_SOL };
	editor->selection = (skb_text_range_t) { .start = start_pos, .end = start_pos };

	editor->preferred_x = -1.f;
	editor->undo_stack_head = -1;

	return editor;
}

void skb_editor_destroy(skb_editor_t* editor)
{
	if (!editor) return;

	skb_rich_text_destroy(&editor->scratch_rich_text);
	skb_rich_text_destroy(&editor->rich_text);
	skb_rich_layout_destroy(&editor->rich_layout);
	skb_text_destroy(&editor->composition_text);

	skb_free(editor->attributes);
	skb_free(editor->active_attributes);

	skb__reset_undo(editor);
	skb_free(editor->undo_stack);
	skb_free(editor->undo_states);

	SKB_ZERO_STRUCT(editor);

	skb_free(editor);
}

void skb_editor_set_on_text_change_callback(skb_editor_t* editor, skb_editor_on_text_change_func_t* on_change_func, void* context)
{
	assert(editor);
	editor->on_text_change_callback = on_change_func;
	editor->on_text_change_context = context;
}

void skb_editor_set_on_selection_change_callback(skb_editor_t* editor, skb_editor_on_selection_change_func_t* on_change_func, void* context)
{
	assert(editor);
	editor->on_selection_change_callback = on_change_func;
	editor->on_selection_change_context = context;
}

void skb_editor_set_input_filter_callback(skb_editor_t* editor, skb_editor_input_filter_func_t* filter_func, void* context)
{
	assert(editor);
	editor->input_filter_callback = filter_func;
	editor->input_filter_context = context;
}

const skb_editor_params_t* skb_editor_get_params(const skb_editor_t* editor)
{
	assert(editor);
	return &editor->params;
}

void skb_editor_reset(skb_editor_t* editor, const skb_editor_params_t* params)
{
	assert(editor);

	skb_rich_text_reset(&editor->rich_text);
	skb_rich_layout_reset(&editor->rich_layout);

	editor->view_offset = (skb_vec2_t){ 0 };

	editor->active_attributes_count = 0;
	editor->preferred_x = -1.f;
	editor->undo_stack_head = -1;

	const skb_text_position_t start_pos = { .offset = 0, .affinity = SKB_AFFINITY_SOL };
	editor->selection = (skb_text_range_t) { .start = start_pos, .end = start_pos };

	if (params)
		skb__set_params(editor, params);

	skb__reset_undo(editor);

	skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_RESET);
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_RESET);
}

void skb_editor_set_text_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_len)
{
	assert(editor);

	skb_editor_reset(editor, NULL);

	skb_rich_text_append_paragraph(&editor->rich_text, editor->params.paragraph_attributes);
	skb_rich_text_append_utf8(&editor->rich_text, temp_alloc, utf8, utf8_len, (skb_attribute_set_t){0});

	skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});

	skb__pick_active_attributes(editor);
	skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_RESET);
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_RESET);
}

void skb_editor_set_text_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len)
{
	assert(editor);

	skb_editor_reset(editor, NULL);

	skb_rich_text_append_paragraph(&editor->rich_text, editor->params.paragraph_attributes);
	skb_rich_text_append_utf32(&editor->rich_text, temp_alloc, utf32, utf32_len, (skb_attribute_set_t){0});

	skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});

	skb__pick_active_attributes(editor);
	skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_RESET);
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_RESET);
}

void skb_editor_set_rich_text(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_rich_text_t* rich_text)
{
	assert(editor);
	assert(rich_text);

	skb_editor_reset(editor, NULL);

	skb_rich_text_append(&editor->rich_text, rich_text);

	skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});

	skb__pick_active_attributes(editor);
	skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_RESET);
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_RESET);
}

//
// Text getters
//

int32_t skb_editor_get_text_utf8_count(const skb_editor_t* editor)
{
	assert(editor);

	int32_t count = 0;
	for (int32_t i = 0; i < skb__get_paragraph_count(editor); i++) {
		const skb_text_t* paragraph_text = skb__get_text(editor, i);
		count += skb_utf32_to_utf8_count(skb_text_get_utf32(paragraph_text), skb_text_get_utf32_count(paragraph_text));
	}
	return count;
}

int32_t skb_editor_get_text_utf8(const skb_editor_t* editor, char* utf8, int32_t utf8_cap)
{
	assert(editor);
	assert(utf8);

	int32_t count = 0;
	for (int32_t i = 0; i < skb__get_paragraph_count(editor); i++) {
		const skb_text_t* paragraph_text = skb__get_text(editor, i);
		const int32_t cur_buf_cap = skb_maxi(0, utf8_cap - count);
		if (cur_buf_cap == 0)
			break;
		char* cur_buf = utf8 + count;
		count += skb_utf32_to_utf8(skb_text_get_utf32(paragraph_text), skb_text_get_utf32_count(paragraph_text), cur_buf, cur_buf_cap);
	}
	return skb_mini(count, utf8_cap);
}

int32_t skb_editor_get_text_utf32_count(const skb_editor_t* editor)
{
	assert(editor);

	int32_t count = 0;
	for (int32_t i = 0; i < skb__get_paragraph_count(editor); i++) {
		const skb_text_t* paragraph_text = skb__get_text(editor, i);
		count += skb_text_get_utf32_count(paragraph_text);
	}

	return count;
}

int32_t skb_editor_get_text_utf32(const skb_editor_t* editor, uint32_t* utf32, int32_t utf32_cap)
{
	assert(editor);

	int32_t count = 0;
	for (int32_t i = 0; i < skb__get_paragraph_count(editor); i++) {
		const skb_text_t* paragraph_text = skb__get_text(editor, i);
		const int32_t cur_buf_cap = skb_maxi(0, utf32_cap - count);
		const int32_t copy_count = skb_mini(cur_buf_cap, skb_text_get_utf32_count(paragraph_text));
		if (utf32 && copy_count > 0)
			memcpy(utf32 + count, skb_text_get_utf32(paragraph_text), copy_count * sizeof(uint32_t));
		count += skb_text_get_utf32_count(paragraph_text);
	}

	return count;
}

const skb_rich_text_t* skb_editor_get_rich_text(const skb_editor_t* editor)
{
	assert(editor);
	return &editor->rich_text;
}


int32_t skb_editor_get_text_utf8_count_in_range(const skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(editor);
	text_range = skb__resolve_text_range(editor, text_range);
	return skb_rich_text_get_utf8_count_in_range(&editor->rich_text, text_range);
}

int32_t skb_editor_get_text_utf8_in_range(const skb_editor_t* editor, skb_text_range_t text_range, char* utf8, int32_t utf8_cap)
{
	assert(editor);
	text_range = skb__resolve_text_range(editor, text_range);
	return skb_rich_text_get_utf8_in_range(&editor->rich_text, text_range, utf8, utf8_cap);
}

int32_t skb_editor_get_text_utf32_count_in_range(const skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(editor);
	text_range = skb__resolve_text_range(editor, text_range);
	return skb_rich_text_get_utf32_count_in_range(&editor->rich_text, text_range);
}

int32_t skb_editor_get_text_utf32_in_range(const skb_editor_t* editor, skb_text_range_t text_range, uint32_t* utf32, int32_t utf32_cap)
{
	assert(editor);
	assert(utf32);
	text_range = skb__resolve_text_range(editor, text_range);
	return skb_rich_text_get_utf32_in_range(&editor->rich_text, text_range, utf32, utf32_cap);
}

void skb_editor_get_rich_text_in_range(const skb_editor_t* editor, skb_text_range_t text_range, skb_rich_text_t* rich_text)
{
	assert(editor);
	assert(rich_text);
	text_range = skb__resolve_text_range(editor, text_range);
	skb_rich_text_reset(rich_text);
	skb_rich_text_append_range(rich_text, &editor->rich_text, text_range);
}

//
// Layout getters
//

skb_vec2_t skb_editor_get_view_offset(const skb_editor_t* editor)
{
	assert(editor);
	return editor->view_offset;
}

void skb_editor_set_view_offset(skb_editor_t* editor, skb_vec2_t view_offset)
{
	assert(editor);

	editor->view_offset = view_offset;
	skb__editor_clamp_view_offset(editor);
}

skb_rect2_t skb_editor_get_layout_bounds(const skb_editor_t* editor)
{
	assert(editor);
	return skb_rich_layout_get_bounds(&editor->rich_layout);
}

skb_rect2_t skb_editor_get_view_bounds(const skb_editor_t* editor)
{
	const bool has_width_constraint = editor->params.editor_width >= 0.f;
	const bool has_height_constraint = editor->params.editor_height >= 0.f;
	const skb_rect2_t content_bounds = skb_rich_layout_get_bounds(&editor->rich_layout); // Use edit bounds, which contains line end white space too.
	return (skb_rect2_t) {
		.x = 0.f,
		.y = 0.f,
		.width = has_width_constraint ? editor->params.editor_width : content_bounds.width,
		.height = has_height_constraint ? editor->params.editor_height : content_bounds.height,
	};
}

const skb_rich_layout_t* skb_editor_get_rich_layout(const skb_editor_t* editor)
{
	assert(editor);
	return &editor->rich_layout;
}

int32_t skb_editor_get_paragraph_count(const skb_editor_t* editor)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb__get_paragraph_count(editor);
}

const skb_layout_t* skb_editor_get_paragraph_layout(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb__get_layout(editor, paragraph_idx);
}

skb_vec2_t skb_editor_get_paragraph_offset(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb_rich_layout_get_layout_offset(&editor->rich_layout, paragraph_idx);
}

float skb_editor_get_paragraph_advance_y(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb_rich_layout_get_layout_advance_y(&editor->rich_layout, paragraph_idx);
}

const skb_text_t* skb_editor_get_paragraph_text(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb__get_text(editor, paragraph_idx);
}

int32_t skb_editor_get_paragraph_text_count(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb__get_text_count(editor, paragraph_idx);
}

int32_t skb_editor_get_paragraph_text_content_count(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	const skb_text_t* text = skb__get_text(editor, paragraph_idx);
	if (text) {
		const uint32_t* utf32 = skb_text_get_utf32(text);
		const int32_t utf32_count = skb_text_get_utf32_count(text);
		if (utf32_count > 0 && skb_is_paragraph_separator(utf32[utf32_count-1]))
			return utf32_count - 1;
		return utf32_count;
	}
	return 0;
}

skb_attribute_set_t skb_editor_get_paragraph_attributes(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb_rich_text_get_paragraph_attributes(&editor->rich_text, paragraph_idx);
}

int32_t skb_editor_get_paragraph_global_text_offset(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return skb__get_global_text_offset(editor, paragraph_idx);
}

skb_text_range_t skb_editor_get_paragraph_text_range(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	const int32_t global_offset = skb__get_global_text_offset(editor, paragraph_idx);
	const int32_t text_count = skb__get_text_count(editor, paragraph_idx);
	return (skb_text_range_t) {
		.start = (skb_text_position_t) {
			.offset = global_offset,
			.affinity = SKB_AFFINITY_SOL,
		},
		.end = (skb_text_position_t) {
			.offset = global_offset + skb_maxi(0, text_count - 1),
			.affinity = SKB_AFFINITY_EOL,
		},
	};
}

skb_text_range_t skb_editor_get_paragraph_content_range(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));
	return (skb_text_range_t) {
		.start = skb_editor_get_paragraph_content_start_pos(editor, paragraph_idx),
		.end = skb_editor_get_paragraph_content_end_pos(editor, paragraph_idx),
	};
}
skb_text_position_t skb_editor_get_paragraph_content_start_pos(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);
	return (skb_text_position_t) {
		.offset = skb__get_global_text_offset(editor, paragraph_idx),
		.affinity = SKB_AFFINITY_TRAILING
	};
}

skb_text_position_t skb_editor_get_paragraph_content_end_pos(const skb_editor_t* editor, int32_t paragraph_idx)
{
	assert(editor);

	const int32_t paragraph_global_text_offset = skb__get_global_text_offset(editor, paragraph_idx);
	const skb_text_t* text = skb_editor_get_paragraph_text(editor, paragraph_idx);
	if (text) {
		const uint32_t* utf32 = skb_text_get_utf32(text);
		int32_t utf32_count = skb_text_get_utf32_count(text);
		if (utf32_count > 0 && skb_is_paragraph_separator(utf32[utf32_count - 1])) {
			return (skb_text_position_t) {
				.offset = paragraph_global_text_offset + utf32_count - 1,
				.affinity = SKB_AFFINITY_TRAILING
			};
		}
		return (skb_text_position_t) {
			.offset = paragraph_global_text_offset + skb_maxi(0, utf32_count - 1),
			.affinity = SKB_AFFINITY_EOL
		};
	}

	return (skb_text_position_t) {
		.offset = paragraph_global_text_offset,
		.affinity = SKB_AFFINITY_TRAILING
	};
}


//
// Text position
//

int32_t skb_editor_get_text_offset_from_text_position(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(editor);

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_USE);

	return paragraph_pos.global_text_offset;
}

skb_paragraph_position_t skb_editor_get_paragraph_position_from_text_position(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(editor);

	text_pos = skb__resolve_text_position(editor, text_pos);
	return skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_USE);
}

skb_range_t skb_editor_get_paragraphs_range_from_text_range(const skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(editor);
	if (skb__get_paragraph_count(editor) == 0)
		return (skb_range_t){0};
	text_range = skb__resolve_text_range(editor, text_range);
	const skb_paragraph_range_t range = skb_rich_text_get_paragraph_range_from_text_range(&editor->rich_text, text_range, SKB_AFFINITY_USE);
	return (skb_range_t) {
		.start = range.start.paragraph_idx,
		.end = range.end.paragraph_idx + 1,
	};
}

skb_range_t skb_editor_get_offset_range_from_text_range(const skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(editor);
	if (skb__get_paragraph_count(editor) == 0)
		return (skb_range_t){0};
	text_range = skb__resolve_text_range(editor, text_range);
	const skb_paragraph_range_t range = skb_rich_text_get_paragraph_range_from_text_range(&editor->rich_text, text_range, SKB_AFFINITY_USE);
	return (skb_range_t) {
		.start = range.start.global_text_offset,
		.end = range.end.global_text_offset,
	};
}

int32_t skb_editor_get_text_range_count(const skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(editor);
	text_range = skb__resolve_text_range(editor, text_range);
	const skb_paragraph_range_t range = skb_rich_text_get_paragraph_range_from_text_range(&editor->rich_text, text_range, SKB_AFFINITY_USE);
	return range.end.global_text_offset - range.start.global_text_offset;
}

skb_text_position_t skb_editor_get_line_start_at(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_USE);

	int32_t edit_line_idx = skb__get_line_index(editor, paragraph_pos);
	const skb_layout_line_t* lines = skb_layout_get_lines(skb__get_layout(editor, paragraph_pos.paragraph_idx));
	const skb_layout_line_t* line = &lines[edit_line_idx];

	skb_text_position_t result = {
		.offset = skb__get_global_text_offset(editor, paragraph_pos.paragraph_idx) + line->text_range.start,
		.affinity = SKB_AFFINITY_SOL,
	};
	return result;
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_line_end_at(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_USE);

	int32_t edit_line_idx = skb__get_line_index(editor, paragraph_pos);
	const skb_layout_line_t* lines = skb_layout_get_lines(skb__get_layout(editor, paragraph_pos.paragraph_idx));
	const skb_layout_line_t* line = &lines[edit_line_idx];

	skb_text_position_t result = {
		.offset = skb__get_global_text_offset(editor, paragraph_pos.paragraph_idx) + line->last_grapheme_offset,
		.affinity = SKB_AFFINITY_EOL,
	};
	return skb__caret_prune_control_eol(skb__get_layout(editor, paragraph_pos.paragraph_idx), line, result);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_word_start_at(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	// Ignoring affinity, since we want to start from the "character" the user has hit.
	skb_paragraph_position_t paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);

	int32_t offset = paragraph_pos.text_offset;

	const skb_text_property_t* text_props = skb_layout_get_text_properties(skb__get_layout(editor, paragraph_pos.paragraph_idx));

	if (text_props) {
		while (offset >= 0) {
			if (text_props[offset-1].flags & SKB_TEXT_PROP_WORD_BREAK) {
				offset = skb_layout_align_grapheme_offset(skb__get_layout(editor, paragraph_pos.paragraph_idx), offset);
				break;
			}
			offset--;
		}
	}

	if (offset < 0)
		offset = 0;

	return (skb_text_position_t) {
		.offset = skb__get_global_text_offset(editor, paragraph_pos.paragraph_idx) + offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_word_end_at(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	// Ignoring affinity, since we want to start from the "character" the user has hit.
	skb_paragraph_position_t paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);

	int32_t offset = paragraph_pos.text_offset;

	const int32_t text_count = skb_layout_get_text_count(skb__get_layout(editor, paragraph_pos.paragraph_idx));
	const skb_text_property_t* text_props = skb_layout_get_text_properties(skb__get_layout(editor, paragraph_pos.paragraph_idx));

	if (text_props) {
		while (offset < text_count) {
			if (text_props[offset].flags & SKB_TEXT_PROP_WORD_BREAK) {
				offset = skb_layout_align_grapheme_offset(skb__get_layout(editor, paragraph_pos.paragraph_idx), offset);
				break;
			}
			offset++;
		}
	}

	if (offset >= text_count)
		offset = skb_layout_align_grapheme_offset(skb__get_layout(editor, paragraph_pos.paragraph_idx), text_count-1);

	return (skb_text_position_t) {
		.offset = skb__get_global_text_offset(editor, paragraph_pos.paragraph_idx) + offset,
		.affinity = SKB_AFFINITY_LEADING,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_selection_ordered_start(const skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_range = skb__resolve_text_range(editor, text_range);
	// Get positions separate (not range) so that we can compare the direction.
	skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_range.start, SKB_AFFINITY_USE);
	skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_range.end, SKB_AFFINITY_USE);

	if (skb_is_rtl(skb__get_layout_resolved_direction(editor, end_pos.paragraph_idx)))
		return skb_paragraph_position_less_or_equal(start_pos, end_pos) ? text_range.end : text_range.start;

	return skb_paragraph_position_less_or_equal(start_pos, end_pos) ? text_range.start : text_range.end;
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_selection_ordered_end(const skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_range = skb__resolve_text_range(editor, text_range);
	// Get positions separate (not range) so that we can compare the direction.
	skb_paragraph_position_t start_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_range.start, SKB_AFFINITY_USE);
	skb_paragraph_position_t end_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_range.end, SKB_AFFINITY_USE);

	if (skb_is_rtl(skb__get_layout_resolved_direction(editor, end_pos.paragraph_idx)))
		return skb_paragraph_position_less_or_equal(start_pos, end_pos) ? text_range.start : text_range.end;

	return skb_paragraph_position_less_or_equal(start_pos, end_pos) ? text_range.end : text_range.start;
}

static bool skb__is_at_first_line(const skb_editor_t* editor, skb_paragraph_position_t paragraph_pos)
{
	const int32_t edit_line_idx = skb__get_line_index(editor, paragraph_pos);
	return paragraph_pos.paragraph_idx == 0 && edit_line_idx == 0;
}

static bool skb__is_at_last_line(const skb_editor_t* editor, skb_paragraph_position_t paragraph_pos, int32_t line_idx)
{
	assert(skb__are_paragraphs_in_sync(editor));

	const int32_t lines_count = skb_layout_get_lines_count(skb__get_layout(editor, paragraph_pos.paragraph_idx));
	return (paragraph_pos.paragraph_idx == skb__get_paragraph_count(editor) - 1) && (line_idx == lines_count - 1);
}

static bool skb__is_past_end_of_line(const skb_editor_t* editor, skb_paragraph_position_t paragraph_pos, int32_t line_idx)
{
	assert(skb__are_paragraphs_in_sync(editor));

	const skb_layout_line_t* lines = skb_layout_get_lines(skb__get_layout(editor, paragraph_pos.paragraph_idx));
	const skb_layout_line_t* line = &lines[line_idx];
	return paragraph_pos.text_offset > line->last_grapheme_offset;
}

static bool skb__is_rtl(const skb_editor_t* editor, skb_paragraph_position_t paragraph_pos, skb_caret_affinity_t affinity)
{
	assert(skb__are_paragraphs_in_sync(editor));

	const bool layout_is_rtl = skb_is_rtl(skb__get_layout_resolved_direction(editor, paragraph_pos.paragraph_idx));

	if (affinity == SKB_AFFINITY_EOL || affinity == SKB_AFFINITY_SOL)
		return layout_is_rtl;

	const int32_t edit_line_idx = skb__get_line_index(editor, paragraph_pos);
	const skb_layout_line_t* lines = skb_layout_get_lines(skb__get_layout(editor, paragraph_pos.paragraph_idx));
	const skb_layout_line_t* line = &lines[edit_line_idx];

	if (paragraph_pos.text_offset > line->last_grapheme_offset)
		return layout_is_rtl;

	return skb_is_rtl(skb_layout_get_text_direction_at(skb__get_layout(editor, paragraph_pos.paragraph_idx), (skb_text_position_t){.offset = paragraph_pos.text_offset}));
}

static bool skb__are_on_same_line(const skb_editor_t* editor, skb_paragraph_position_t a, skb_paragraph_position_t b)
{
	if (a.paragraph_idx == b.paragraph_idx) {
		const int32_t a_line_idx = skb__get_line_index(editor, a);
		const int32_t b_line_idx = skb__get_line_index(editor, b);
		return a_line_idx == b_line_idx;
	}
	return false;
}

typedef enum {
	SKB_MOVE_DEFAULT,
	SKB_MOVE_WITH_SELECTION,
} skb_move_mode_t;

static skb_text_position_t skb__advance_forward(const skb_editor_t* editor, skb_paragraph_position_t cur_pos, skb_caret_affinity_t cur_affinity, skb_move_mode_t move_mode)
{
	assert(skb__are_paragraphs_in_sync(editor));

	skb_paragraph_position_t next_edit_pos = skb__get_next_grapheme_pos(editor, cur_pos);

	const int32_t next_edit_line_idx = skb__get_line_index(editor, cur_pos);
	bool is_next_last_line = skb__is_at_last_line(editor, next_edit_pos, next_edit_line_idx);

	bool cur_is_rtl = skb__is_rtl(editor, cur_pos, cur_affinity);
	bool next_is_rtl = skb__is_rtl(editor, next_edit_pos, SKB_AFFINITY_TRAILING);

	// Do not add extra stop at the end of the line on intermediate lines.
	const bool stop_at_dir_change = move_mode == SKB_MOVE_DEFAULT && editor->params.caret_mode == SKB_CARET_MODE_SKRIBIDI &&  (is_next_last_line || skb__are_on_same_line(editor, cur_pos, next_edit_pos));

	uint8_t affinity = SKB_AFFINITY_TRAILING;
	bool check_eol = true;

	if (stop_at_dir_change && cur_is_rtl != next_is_rtl) {
		// Text direction change.
		if (cur_affinity == SKB_AFFINITY_LEADING || cur_affinity == SKB_AFFINITY_EOL) {
			// Switch over to the next character.
			affinity = SKB_AFFINITY_TRAILING;
			cur_pos = next_edit_pos;
		} else if (cur_affinity == SKB_AFFINITY_SOL) {
			// Move up to the trailing edge before proceeding.
			affinity = SKB_AFFINITY_TRAILING;
		} else {
			// On a trailing edge, and the direction will change in next character.
			// Move up to the leading edge before proceeding.
			affinity = SKB_AFFINITY_LEADING;
			check_eol = false;
		}
	} else {
		if (cur_affinity == SKB_AFFINITY_LEADING || cur_affinity == SKB_AFFINITY_EOL) {
			// If on leading edge, normalize the index to next trailing location.
			cur_is_rtl = next_is_rtl;
			cur_pos = next_edit_pos;

			// Update next
			next_edit_pos = skb__get_next_grapheme_pos(editor, cur_pos);
			next_is_rtl = skb__is_rtl(editor, next_edit_pos, SKB_AFFINITY_TRAILING);
		}

		if (stop_at_dir_change && cur_is_rtl != next_is_rtl) {
			// On a trailing edge, and the direction will change in next character.
			// Move up to the leading edge before proceeding.
			affinity = SKB_AFFINITY_LEADING;
			check_eol = false;
		} else {
			// Direction will stay the same, advance.
			affinity = SKB_AFFINITY_TRAILING;
			cur_pos = next_edit_pos;
		}
	}

	if (check_eol) {
		const int32_t cur_edit_line_idx = skb__get_line_index(editor, cur_pos);
		if (skb__is_at_last_line(editor, cur_pos, cur_edit_line_idx) && skb__is_past_end_of_line(editor, cur_pos, cur_edit_line_idx)) {
			const skb_layout_line_t* lines = skb_layout_get_lines(skb__get_layout(editor, cur_pos.paragraph_idx));
			const skb_layout_line_t* line = &lines[cur_edit_line_idx];
			affinity = SKB_AFFINITY_EOL;
			cur_pos.text_offset = line->last_grapheme_offset;
		}
	}

	return (skb_text_position_t) {
		.offset = skb__get_global_text_offset(editor, cur_pos.paragraph_idx) + cur_pos.text_offset,
		.affinity = affinity,
	};
}

static skb_text_position_t skb__advance_backward(const skb_editor_t* editor, skb_paragraph_position_t cur_pos, skb_caret_affinity_t cur_affinity, skb_move_mode_t move_mode)
{
	assert(skb__are_paragraphs_in_sync(editor));

	skb_paragraph_position_t prev_edit_pos = skb__get_prev_grapheme_pos(editor, cur_pos);

	const bool layout_is_rtl = skb_is_rtl(skb__get_layout_resolved_direction(editor, cur_pos.paragraph_idx));
	const bool is_at_start = cur_pos.global_text_offset == 0;
	const bool prev_at_start = prev_edit_pos.global_text_offset == 0;

	bool cur_is_rtl = skb__is_rtl(editor, cur_pos, cur_affinity);
	bool prev_is_rtl = is_at_start ? layout_is_rtl : skb__is_rtl(editor, prev_edit_pos, SKB_AFFINITY_TRAILING);


	// Do not add extra stop at the end of the line on intermediate lines.
	const bool stop_at_dir_change = move_mode == SKB_MOVE_DEFAULT && editor->params.caret_mode == SKB_CARET_MODE_SKRIBIDI && skb__are_on_same_line(editor, cur_pos, prev_edit_pos);

	skb_caret_affinity_t affinity = SKB_AFFINITY_TRAILING;

	if (stop_at_dir_change && prev_is_rtl != cur_is_rtl) {
		if (cur_affinity == SKB_AFFINITY_EOL) {
			// At the end of line, but the direction is changing. Move to leading edge first.
			affinity = SKB_AFFINITY_LEADING;
		} else if (cur_affinity == SKB_AFFINITY_LEADING) {
			// On a leading edge, and the direction will change in next character. Move to trailing edge first.
			affinity = SKB_AFFINITY_TRAILING;
		} else {
			if (is_at_start) {
				affinity = SKB_AFFINITY_SOL;
			} else {
				// On a trailing edge, and the direction will change in next character.
				// Switch over to the leading edge of the previous character.
				affinity = SKB_AFFINITY_LEADING;
				cur_pos = prev_edit_pos;
			}
		}
	} else {
		if (cur_affinity == SKB_AFFINITY_LEADING || cur_affinity == SKB_AFFINITY_EOL) {
			// On leading edge, normalize the index to next trailing location.
			affinity = SKB_AFFINITY_TRAILING;
		} else {
			if (is_at_start || (prev_at_start && prev_is_rtl == layout_is_rtl)) {
				affinity = SKB_AFFINITY_SOL;
				cur_pos = prev_edit_pos;
			} else {
				// On a trailing edge, advance to the previous character.
				affinity = SKB_AFFINITY_TRAILING;
				cur_pos = prev_edit_pos;
			}
		}
	}

	return (skb_text_position_t) {
		.offset = skb__get_global_text_offset(editor, cur_pos.paragraph_idx) + cur_pos.text_offset,
		.affinity = affinity,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_next_char(const skb_editor_t* editor, skb_text_position_t text_pos, skb_move_mode_t move_mode)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t cur_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);

	if (skb_is_rtl(skb__get_layout_resolved_direction(editor, cur_pos.paragraph_idx)))
		return skb__advance_backward(editor, cur_pos, text_pos.affinity, move_mode);
	return skb__advance_forward(editor, cur_pos, text_pos.affinity, move_mode);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_prev_char(const skb_editor_t* editor, skb_text_position_t text_pos, skb_move_mode_t move_mode)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t cur_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);

	if (skb_is_rtl(skb__get_layout_resolved_direction(editor, cur_pos.paragraph_idx)))
		return skb__advance_forward(editor, cur_pos, text_pos.affinity, move_mode);
	return skb__advance_backward(editor, cur_pos, text_pos.affinity, move_mode);
}

static skb_text_position_t skb__advance_word_forward(const skb_editor_t* editor, skb_paragraph_position_t cur_edit_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	int32_t offset = cur_edit_pos.text_offset;
	const int32_t text_count = skb_layout_get_text_count(skb__get_layout(editor, cur_edit_pos.paragraph_idx));
	const skb_text_property_t* text_props = skb_layout_get_text_properties(skb__get_layout(editor, cur_edit_pos.paragraph_idx));

	if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
		// skip whitespace and punctuation at start.
		while (offset < text_count && text_props[offset].flags & (SKB_TEXT_PROP_WHITESPACE | SKB_TEXT_PROP_PUNCTUATION))
			offset++;

		// Stop at the end of the word.
		while (offset < text_count) {
			if (text_props[offset].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_get_next_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), offset);
				offset = next_offset;
				break;
			}
			offset++;
		}
	} else {
		// Stop after the white space at the end of the word.
		while (offset < text_count) {
			if (text_props[offset].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_get_next_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), offset);
				if (!(text_props[next_offset].flags & SKB_TEXT_PROP_WHITESPACE)) {
					offset = next_offset;
					break;
				}
			}
			offset++;
		}
	}

	if (offset == text_count) {
		if (cur_edit_pos.paragraph_idx + 1 < skb__get_paragraph_count(editor)) {
			cur_edit_pos.paragraph_idx++;
			offset = 0; // Beginning of layout
		} else {
			offset = skb_layout_align_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), text_count-1);
			return (skb_text_position_t) {
				.offset = skb__get_global_text_offset(editor, cur_edit_pos.paragraph_idx) + offset,
				.affinity = SKB_AFFINITY_EOL,
			};
		}
	}

	return (skb_text_position_t) {
		.offset = skb__get_global_text_offset(editor, cur_edit_pos.paragraph_idx) + offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

static skb_text_position_t skb__advance_word_backward(const skb_editor_t* editor, skb_paragraph_position_t cur_edit_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	int32_t offset = cur_edit_pos.text_offset;

	if (offset == 0) {
		if (cur_edit_pos.paragraph_idx - 1 >= 0) {
			// Goto previous paragraph
			cur_edit_pos.paragraph_idx--;

			const int32_t text_count = skb_layout_get_text_count(skb__get_layout(editor, cur_edit_pos.paragraph_idx));
			offset = skb_layout_align_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), text_count-1); // Last grapheme of the paragraph.
			return (skb_text_position_t) {
				.offset = skb__get_global_text_offset(editor, cur_edit_pos.paragraph_idx) + offset,
				.affinity = SKB_AFFINITY_TRAILING,
			};
		}
		offset = 0;
		return (skb_text_position_t) {
			.offset = skb__get_global_text_offset(editor, cur_edit_pos.paragraph_idx) + offset,
			.affinity = SKB_AFFINITY_SOL,
		};
	}

	const skb_text_property_t* text_props = skb_layout_get_text_properties(skb__get_layout(editor, cur_edit_pos.paragraph_idx));

	if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
		// skip whitespace and punctuation at start.
		while (offset > 0 && text_props[offset-1].flags & (SKB_TEXT_PROP_WHITESPACE | SKB_TEXT_PROP_PUNCTUATION))
			offset--;
		// Stop at the beginning of the word.
		offset = skb_layout_get_prev_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), offset);
		while (offset > 0) {
			if (text_props[offset-1].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_get_next_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), offset-1);
				offset = next_offset;
				break;
			}
			offset--;
		}
	} else {
		// Stop at the beginning of a word (the exact same logic as moving forward).
		offset = skb_layout_get_prev_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), offset);
		while (offset > 0) {
			if (text_props[offset-1].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_get_next_grapheme_offset(skb__get_layout(editor, cur_edit_pos.paragraph_idx), offset-1);
				if (!(text_props[next_offset].flags & SKB_TEXT_PROP_WHITESPACE)) {
					offset = next_offset;
					break;
				}
			}
			offset--;
		}
	}

	return (skb_text_position_t) {
		.offset = skb__get_global_text_offset(editor, cur_edit_pos.paragraph_idx) + offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_next_word(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t cur_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);

	if (skb_is_rtl(skb__get_layout_resolved_direction(editor, cur_pos.paragraph_idx)))
		return skb__advance_word_backward(editor, cur_pos);
	return skb__advance_word_forward(editor, cur_pos);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_prev_word(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t cur_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);

	if (skb_is_rtl(skb__get_layout_resolved_direction(editor, cur_pos.paragraph_idx)))
		return skb__advance_word_forward(editor, cur_pos);
	return skb__advance_word_backward(editor, cur_pos);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_next_line(const skb_editor_t* editor, skb_text_position_t text_pos, float preferred_x)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t cur_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);

	int32_t cur_line_idx = skb__get_line_index(editor, cur_pos);

	if (skb__is_at_last_line(editor, cur_pos, cur_line_idx)) {
		// Goto end of the text
		return skb_editor_get_line_end_at(editor, text_pos);
	}

	const int32_t lines_count = skb_layout_get_lines_count(skb__get_layout(editor, cur_pos.paragraph_idx));

	// Goto next line
	if (cur_line_idx + 1 >= lines_count) {
		// End of current paragraph, goto first line of next paragraph.
		assert(cur_pos.paragraph_idx + 1 < skb__get_paragraph_count(editor)); // should have been handled by skb__is_at_last_line() above.
		cur_pos.paragraph_idx++;
		cur_line_idx = 0;
	} else {
		cur_line_idx++;
	}

	skb_text_position_t hit_pos = skb_layout_hit_test_at_line(skb__get_layout(editor, cur_pos.paragraph_idx), SKB_MOVEMENT_CARET, cur_line_idx, preferred_x);
	hit_pos.offset += skb__get_global_text_offset(editor, cur_pos.paragraph_idx);

	return hit_pos;
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_prev_line(const skb_editor_t* editor, skb_text_position_t text_pos, float preferred_x)
{
	assert(skb__are_paragraphs_in_sync(editor));

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t cur_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_IGNORE);
	int32_t cur_line_idx = skb__get_line_index(editor, cur_pos);

	if (skb__is_at_first_line(editor, cur_pos)) {
		// Goto beginning of the text
		return skb_editor_get_line_start_at(editor, text_pos);
	}

	int32_t lines_count = skb_layout_get_lines_count(skb__get_layout(editor, cur_pos.paragraph_idx));

	// Goto prev line
	if (cur_line_idx - 1 < 0) {
		// Beginning of current paragraph, goto last line of prev paragraph.

		assert(cur_pos.paragraph_idx - 1 >= 0); // should have been handled by skb__is_at_first_line() above.
		cur_pos.paragraph_idx--;
		lines_count = skb_layout_get_lines_count(skb__get_layout(editor, cur_pos.paragraph_idx));
		cur_line_idx = lines_count - 1;
	} else {
		cur_line_idx--;
	}

	skb_text_position_t hit_pos = skb_layout_hit_test_at_line(skb__get_layout(editor, cur_pos.paragraph_idx), SKB_MOVEMENT_CARET, cur_line_idx, preferred_x);
	hit_pos.offset += skb__get_global_text_offset(editor, cur_pos.paragraph_idx);

	return hit_pos;
}

// Helper function to get document start position
static skb_text_position_t skb__editor_get_document_start(const skb_editor_t* editor)
{
	skb_text_position_t result = {
		.offset = 0,
		.affinity = SKB_AFFINITY_SOL
	};
	return result;
}

// Helper function to get document end position
static skb_text_position_t skb__editor_get_document_end(const skb_editor_t* editor)
{
	assert(skb__are_paragraphs_in_sync(editor));

	if (skb__get_paragraph_count(editor) == 0) {
		return skb__editor_get_document_start(editor);
	}

	const int32_t last_paragraph_idx = skb__get_paragraph_count(editor) - 1;
	skb_text_position_t result = {
		.offset = skb__get_global_text_offset(editor, last_paragraph_idx) + skb__get_text_count(editor, last_paragraph_idx),
		.affinity = SKB_AFFINITY_EOL
	};
	return result;
}


//
// Selection
//

skb_text_range_t skb_editor_get_current_selection(const skb_editor_t* editor)
{
	assert(editor);

	if (skb_text_get_utf32_count(&editor->composition_text) > 0) {
		return editor->composition_selection;
	}

	return editor->selection;
}

void skb_editor_select(skb_editor_t* editor, skb_text_range_t text_range)
{
	assert(editor);
	editor->selection = text_range;
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EXTERNAL);
}

void skb_editor_select_all(skb_editor_t* editor)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));

	if (skb__get_paragraph_count(editor) > 0) {
		editor->selection.start = (skb_text_position_t) { .offset = 0, .affinity = SKB_AFFINITY_SOL };
		const int32_t last_paragraph_idx = skb__get_paragraph_count(editor) - 1;
		const int32_t last_text_count = skb__get_text_count(editor, last_paragraph_idx);
		const int32_t last_grapheme_offset = skb_layout_align_grapheme_offset(skb__get_layout(editor, last_paragraph_idx), last_text_count-1);
		editor->selection.end = (skb_text_position_t) { .offset = skb__get_global_text_offset(editor, last_paragraph_idx) + last_grapheme_offset, .affinity = SKB_AFFINITY_EOL };
	} else {
		editor->selection.start = (skb_text_position_t) { 0 };
		editor->selection.end = (skb_text_position_t) { 0 };
	}
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EXTERNAL);
}

void skb_editor_select_none(skb_editor_t* editor)
{
	assert(editor);
	// Clear selection, but retain current caret position.
	editor->selection.start = editor->selection.end;
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EXTERNAL);
}

skb_text_position_t skb_editor_hit_test(const skb_editor_t* editor, skb_movement_type_t type, float hit_x, float hit_y)
{
	assert(editor);
	return skb_rich_layout_hit_test(&editor->rich_layout, type, hit_x, hit_y);
}

void skb_editor_iterate_text_range_bounds(const skb_editor_t* editor, skb_text_range_t text_range, skb_text_range_bounds_func_t* callback, void* context)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));

	if (skb_text_range_is_current_selection(text_range))
		text_range = editor->selection;

	skb_rich_layout_get_text_range_bounds(&editor->rich_layout, text_range, callback, context);
}

enum {
	SKB_DRAG_NONE,
	SKB_DRAG_CHAR,
	SKB_DRAG_WORD,
	SKB_DRAG_LINE,
};

void skb_editor_process_mouse_click(skb_editor_t* editor, float x, float y, uint32_t mods, double time)
{
	assert(editor);
	assert(skb__are_paragraphs_in_sync(editor));

	static const double double_click_duration = 0.4;
	if (skb__get_paragraph_count(editor) <= 0)
		return;

	const double dt = time - editor->last_click_time;

	if (dt < double_click_duration)
		editor->click_count++;
	else
		editor->click_count = 1;

	if (editor->click_count > 3)
		editor->click_count = 1;

	editor->last_click_time = time;

	skb_text_position_t hit_caret_pos = skb_editor_hit_test(editor, SKB_MOVEMENT_CARET, x, y);

	if (mods & SKB_MOD_SHIFT) {
		// Shift click makes selection from current start pos to the new hit pos.
		editor->selection.end = hit_caret_pos;
		editor->drag_mode = SKB_DRAG_CHAR;
		skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
	} else {
		if (editor->click_count == 1) {
			editor->selection.end = hit_caret_pos;
			editor->selection.start = editor->selection.end;
			editor->drag_mode = SKB_DRAG_CHAR;
		} else if (editor->click_count == 2) {
			editor->selection.start = skb_editor_get_word_start_at(editor, hit_caret_pos);
			editor->selection.end = skb_editor_get_word_end_at(editor, hit_caret_pos);
			editor->drag_mode = SKB_DRAG_WORD;
		} else if (editor->click_count == 3) {
			editor->selection.start = skb_editor_get_line_start_at(editor, hit_caret_pos);
			editor->selection.end = skb_editor_get_line_end_at(editor, hit_caret_pos);
			editor->drag_mode = SKB_DRAG_LINE;
		}
		skb__pick_active_attributes(editor);
		if (editor->drag_mode == SKB_DRAG_CHAR)
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
		else
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
	}

	editor->drag_initial_selection = editor->selection;

	editor->drag_start_x = x;
	editor->drag_start_y = y;
	editor->drag_moved = false;
}

void skb_editor_process_mouse_drag(skb_editor_t* editor, float x, float y)
{
	assert(editor);

	static const float move_threshold = 5.f;

	if (!editor->drag_moved) {
		float dx = editor->drag_start_x - x;
		float dy = editor->drag_start_y - y;
		float len_sqr = dx*dx + dy*dy;
		if (len_sqr > move_threshold*move_threshold)
			editor->drag_moved = true;
	}

	if (editor->drag_moved) {

		skb_text_position_t hit_pos = skb_editor_hit_test(editor, SKB_MOVEMENT_SELECTION, x, y);

		skb_text_position_t sel_start_pos = hit_pos;
		skb_text_position_t sel_end_pos = hit_pos;

		if (editor->drag_mode == SKB_DRAG_CHAR) {
			sel_start_pos = hit_pos;
			sel_end_pos = hit_pos;
		} else if (editor->drag_mode == SKB_DRAG_WORD) {
			sel_start_pos = skb_editor_get_word_start_at(editor, hit_pos);
			sel_end_pos = skb_editor_get_word_end_at(editor, hit_pos);
		} else if (editor->drag_mode == SKB_DRAG_LINE) {
			sel_start_pos = skb_editor_get_line_start_at(editor, hit_pos);
			sel_end_pos = skb_editor_get_line_end_at(editor, hit_pos);
		}

		// Note: here the start/end positions are in order (not generally true).
		const skb_paragraph_position_t sel_start = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, sel_start_pos, SKB_AFFINITY_USE);
		const skb_paragraph_position_t sel_end = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, sel_end_pos, SKB_AFFINITY_USE);

		const skb_paragraph_position_t initial_start = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, editor->drag_initial_selection.start, SKB_AFFINITY_USE);
		const skb_paragraph_position_t initial_end = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, editor->drag_initial_selection.end, SKB_AFFINITY_USE);

		if (!skb_paragraph_position_less_or_equal(initial_start, sel_start)) {
			// The selection got expanded before the initial selection range start.
			editor->selection.start = sel_start_pos;
			editor->selection.end = editor->drag_initial_selection.end;
		} else if (!skb_paragraph_position_less_or_equal(sel_end, initial_end)) {
			// The selection got expanded past the initial selection range end.
			editor->selection.start = editor->drag_initial_selection.start;
			editor->selection.end = sel_end_pos;
		} else {
			// Restore
			editor->selection.start = editor->drag_initial_selection.start;
			editor->selection.end = editor->drag_initial_selection.end;
		}
		skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);

		editor->preferred_x = -1.f; // reset preferred.
	}
}

static skb_rich_text_change_t skb__replace_selection(skb_editor_t* editor, const skb_rich_text_t* rich_text, bool allow_amend_undo)
{
	// Insert pos gets clamped to the layout text size.
	int32_t transaction_id = skb__capture_undo_text_begin(editor, editor->selection, rich_text, allow_amend_undo);

	skb_rich_text_change_t change = skb_rich_text_insert(&editor->rich_text, editor->selection, rich_text);

	skb__update_selection_from_change(editor, change);
	skb__capture_undo_text_end(editor, transaction_id);

	return change;
}

static void skb__undo_state_init(skb__editor_undo_state_t* state, skb__undo_state_type_t type)
{
	SKB_ZERO_STRUCT(state);
	state->inserted_text = skb_rich_text_make_empty();
	state->removed_text = skb_rich_text_make_empty();
	state->type = type;
}

static void skb__undo_state_clear(skb__editor_undo_state_t* state)
{
	skb_rich_text_destroy(&state->inserted_text);
	skb_rich_text_destroy(&state->removed_text);
	SKB_ZERO_STRUCT(state);
}

static void skb__undo_clear_last_transaction(skb_editor_t* editor)
{
	if (!editor->undo_stack_count) return;

	skb__editor_undo_transaction_t* transaction = &editor->undo_stack[editor->undo_stack_count - 1];

	for (int32_t i = transaction->states_range.start; i < transaction->states_range.end; i++)
		skb__undo_state_clear(&editor->undo_states[i]);

	assert(editor->undo_states_count == transaction->states_range.end);
	editor->undo_states_count = transaction->states_range.start;

	SKB_ZERO_STRUCT(transaction);
	editor->undo_stack_count--;

	assert(editor->undo_stack_head < editor->undo_stack_count);
}

static void skb__undo_clear_first_transaction(skb_editor_t* editor)
{
	if (!editor->undo_stack_count) return;

	skb__editor_undo_transaction_t* transaction = &editor->undo_stack[0];

	// Clean up states
	for (int32_t i = transaction->states_range.start; i < transaction->states_range.end; i++)
		skb__undo_state_clear(&editor->undo_states[i]);

	// Remove states from front.
	assert(transaction->states_range.start == 0);
	int32_t removed_state_count = transaction->states_range.end - transaction->states_range.start;
	editor->undo_states_count -= removed_state_count;
	memmove(editor->undo_states, editor->undo_states + removed_state_count, sizeof(skb__editor_undo_state_t) * editor->undo_states_count);
	// Adjust transactions to match the removed states.
	for (int32_t i = 1; i < editor->undo_stack_count; i++) {
		editor->undo_stack[i].states_range.start -= removed_state_count;
		editor->undo_stack[i].states_range.end -= removed_state_count;
	}

	SKB_ZERO_STRUCT(transaction);
	editor->undo_stack_count--;
	memmove(editor->undo_stack, editor->undo_stack + 1, sizeof(skb__editor_undo_transaction_t) * editor->undo_stack_count);

	assert(editor->undo_stack_head < editor->undo_stack_count);
}

static void skb__reset_undo(skb_editor_t* editor)
{
	for (int32_t i = 0; i < editor->undo_states_count; i++)
		skb__undo_state_clear(&editor->undo_states[i]);

	editor->undo_states_count = 0;
	editor->undo_stack_count = 0;
	editor->undo_stack_head = -1;
}

static int32_t skb__capture_undo_text_begin(skb_editor_t* editor, skb_text_range_t text_range, const skb_rich_text_t* rich_text, bool allow_amend_undo)
{
	if (editor->params.max_undo_levels < 0)
		return SKB_INVALID_INDEX;

	const skb_paragraph_range_t range = skb_rich_text_get_paragraph_range_from_text_range(&editor->rich_text, text_range, SKB_AFFINITY_USE);

	// Check if we can amend the last undo state.
	if (allow_amend_undo && editor->undo_stack_head != -1) {
		skb__editor_undo_transaction_t* prev_undo_transaction = &editor->undo_stack[editor->undo_stack_head];
		skb__editor_undo_state_t* prev_undo_state = &editor->undo_states[prev_undo_transaction->states_range.end - 1];
		if (prev_undo_state->allow_amend_undo) {
			// Try to amend the previous
			const bool has_no_remove = range.start.global_text_offset == range.end.global_text_offset;
			const bool prev_has_insert = prev_undo_state->inserted_range.start.offset < prev_undo_state->inserted_range.end.offset;
			const bool prev_has_no_remove = prev_undo_state->removed_range.start.offset == prev_undo_state->removed_range.end.offset;
			const bool caret_at_end_of_prev = range.end.global_text_offset == prev_undo_state->inserted_range.end.offset;
			if (has_no_remove && prev_has_insert && prev_has_no_remove && caret_at_end_of_prev) {
				assert(prev_undo_state->inserted_range.end.affinity == SKB_AFFINITY_NONE);
				skb_rich_text_append(&prev_undo_state->inserted_text, rich_text);
				prev_undo_state->inserted_range.end.offset += skb_rich_text_get_utf32_count(rich_text);
				return SKB_INVALID_INDEX;
			}
		}
	}

	const int32_t transaction_id = skb_editor_undo_transaction_begin(editor);
	assert(editor->undo_stack_head >= 0);
	skb__editor_undo_transaction_t* transaction = &editor->undo_stack[editor->undo_stack_head];

	// Capture new undo state.
	SKB_ARRAY_RESERVE(editor->undo_states, editor->undo_states_count + 1);
	skb__editor_undo_state_t* undo_state = &editor->undo_states[editor->undo_states_count++];
	skb__undo_state_init(undo_state, SKB_UNDO_TEXT);
	transaction->states_range.end = editor->undo_states_count;

	undo_state->allow_amend_undo = allow_amend_undo;

	// Capture the text we're about to remove.
	undo_state->removed_range.start.offset = range.start.global_text_offset;
	undo_state->removed_range.end.offset = range.end.global_text_offset;
	skb_rich_text_append_range(&undo_state->removed_text, &editor->rich_text, undo_state->removed_range);

	// Capture the text we're about to insert.
	undo_state->inserted_range.start.offset = range.start.global_text_offset;
	undo_state->inserted_range.end.offset = range.start.global_text_offset + skb_rich_text_get_utf32_count(rich_text);
	skb_rich_text_append(&undo_state->inserted_text, rich_text);

	return transaction_id;
}

static void skb__capture_undo_text_end(skb_editor_t* editor, int32_t transaction_id)
{
	if (transaction_id == SKB_INVALID_INDEX)
		return;
	skb_editor_undo_transaction_end(editor, transaction_id);
}

static int32_t skb__capture_undo_attributes_begin(skb_editor_t* editor, skb_text_range_t text_range)
{
	if (editor->params.max_undo_levels < 0)
		return SKB_INVALID_INDEX;

	const skb_paragraph_range_t range = skb_rich_text_get_paragraph_range_from_text_range(&editor->rich_text, text_range, SKB_AFFINITY_USE);

	const int32_t transaction_id = skb_editor_undo_transaction_begin(editor);
	assert(editor->undo_stack_head >= 0);
	skb__editor_undo_transaction_t* transaction = &editor->undo_stack[editor->undo_stack_head];

	// Capture new undo state.
	SKB_ARRAY_RESERVE(editor->undo_states, editor->undo_states_count + 1);
	skb__editor_undo_state_t* undo_state = &editor->undo_states[editor->undo_states_count++];
	skb__undo_state_init(undo_state, SKB_UNDO_TEXT_ATTRIBUTES);
	transaction->states_range.end = editor->undo_states_count;

	// Capture the text we're about to change.
	undo_state->removed_range.start.offset = range.start.global_text_offset;
	undo_state->removed_range.end.offset = range.end.global_text_offset;
	skb_rich_text_copy_attributes_in_range(&undo_state->removed_text, &editor->rich_text, undo_state->removed_range);

	// Store the range after the change.
	undo_state->inserted_range = undo_state->removed_range;
	// We capture the attributed of inserted_text in skb__capture_undo_attributes_end().

	return transaction_id;
}

static void skb__capture_undo_attributes_end(skb_editor_t* editor, int32_t transaction_id)
{
	assert(editor->undo_stack_head != -1);
	const skb__editor_undo_transaction_t* transaction = &editor->undo_stack[editor->undo_stack_head];
	skb__editor_undo_state_t* prev_undo_state = &editor->undo_states[transaction->states_range.end - 1];

	skb_rich_text_copy_attributes_in_range(&prev_undo_state->inserted_text, &editor->rich_text, prev_undo_state->inserted_range);

	skb_editor_undo_transaction_end(editor, transaction_id);
}

int32_t skb_editor_undo_transaction_begin(skb_editor_t* editor)
{
	assert(editor);
	if (!editor->in_undo_transaction) {

		//TODO: we should do this on skb_editor_undo_transaction_end(), after we have checked that we have valid transaction.

		// Delete transactions that cannot be reached anymore
		while (editor->undo_stack_count > (editor->undo_stack_head + 1))
			skb__undo_clear_last_transaction(editor);

		assert(editor->undo_stack_count == editor->undo_stack_head + 1);

		// Keep the undo stack size under control.
		if ((editor->undo_stack_count + 1) > editor->params.max_undo_levels) {
			editor->undo_stack_head--;
			skb__undo_clear_first_transaction(editor);
		}

		// Add new transaction
		SKB_ARRAY_RESERVE(editor->undo_stack, editor->undo_stack_count + 1);
		editor->undo_stack_head = editor->undo_stack_count++;
		assert(editor->undo_stack_count == editor->undo_stack_head + 1);
		skb__editor_undo_transaction_t* transaction = &editor->undo_stack[editor->undo_stack_head];
		SKB_ZERO_STRUCT(transaction);
		// Capture initial state
		transaction->states_range.start = editor->undo_states_count;
		transaction->states_range.end = editor->undo_states_count;
		transaction->selection_before = editor->selection;
	}
	editor->in_undo_transaction++;
	return editor->in_undo_transaction;
}

void skb_editor_undo_transaction_end(skb_editor_t* editor, int32_t transaction_id)
{
	assert(editor);
	assert(editor->in_undo_transaction == transaction_id);
	editor->in_undo_transaction--;
	if (editor->in_undo_transaction == 0) {
		assert(editor->undo_stack_head >= 0);
		skb__editor_undo_transaction_t* transaction = &editor->undo_stack[editor->undo_stack_head];
		if (skb_range_is_empty(transaction->states_range)) {
			editor->undo_stack_head--;
			skb__undo_clear_last_transaction(editor);
		}
	}
}

bool skb_editor_can_undo(skb_editor_t* editor)
{
	assert(editor);
	return editor->undo_stack_head >= 0 && editor->in_undo_transaction == 0;
}

void skb_editor_undo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);
	if (editor->undo_stack_head >= 0) {
		skb__editor_undo_transaction_t* undo_transaction = &editor->undo_stack[editor->undo_stack_head];

		editor->undo_stack_head--; // This can become -1, which is the initial state.

		// Store the selection to come back to if we redo.
		undo_transaction->selection_after = editor->selection;

		// Undo states in reverse order
		for (int32_t i = undo_transaction->states_range.end - 1; i >= undo_transaction->states_range.start; i--) {
			skb__editor_undo_state_t* undo_state = &editor->undo_states[i];
			skb_rich_text_change_t change = {0};
			if (undo_state->type == SKB_UNDO_TEXT) {
				change = skb_rich_text_insert(&editor->rich_text, undo_state->inserted_range, &undo_state->removed_text);
			} else if (undo_state->type == SKB_UNDO_TEXT_ATTRIBUTES) {
				skb_rich_text_insert_attributes(&editor->rich_text, undo_state->inserted_range, &undo_state->removed_text);
			}
			skb_rich_layout_apply_change(&editor->rich_layout, change);
		}

		editor->selection = undo_transaction->selection_before;
		editor->preferred_x = -1.f; // reset preferred.

		skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});
		skb__pick_active_attributes(editor);
		skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_UNDO);
		skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_UNDO);
		skb__ensure_caret_visible(editor);
	}
}

bool skb_editor_can_redo(skb_editor_t* editor)
{
	assert(editor);
	return editor->undo_stack_head + 1 < editor->undo_stack_count && editor->in_undo_transaction == 0;
}

void skb_editor_redo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);
	if (editor->undo_stack_head + 1 < editor->undo_stack_count) {
		editor->undo_stack_head++;
		assert(editor->undo_stack_head < editor->undo_stack_count);
		const skb__editor_undo_transaction_t* undo_transaction = &editor->undo_stack[editor->undo_stack_head];

		for (int32_t i = undo_transaction->states_range.start; i < undo_transaction->states_range.end; i++) {
			const skb__editor_undo_state_t* undo_state = &editor->undo_states[i];
			skb_rich_text_change_t change = {0};
			if (undo_state->type == SKB_UNDO_TEXT) {
				change = skb_rich_text_insert(&editor->rich_text, undo_state->removed_range, &undo_state->inserted_text);
			} else if (undo_state->type == SKB_UNDO_TEXT_ATTRIBUTES) {
				skb_rich_text_insert_attributes(&editor->rich_text, undo_state->removed_range, &undo_state->inserted_text);
			}
			skb_rich_layout_apply_change(&editor->rich_layout, change);
		}

		editor->selection = undo_transaction->selection_after;
		editor->preferred_x = -1.f; // reset preferred.

		skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});
		skb__pick_active_attributes(editor);
		skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_UNDO);
		skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_UNDO);
		skb__ensure_caret_visible(editor);
	}
}


// Based on android.text.method.BaseKeyListener.getOffsetForBackspaceKey().
enum {
	BACKSPACE_STATE_START = 0,	// Initial state
	BACKSPACE_STATE_LF = 1,	// The offset is immediately before line feed.
	BACKSPACE_STATE_BEFORE_KEYCAP = 2, // The offset is immediately before a KEYCAP.
	BACKSPACE_STATE_BEFORE_VS_AND_KEYCAP = 3,	// The offset is immediately before a variation selector and a KEYCAP.
	BACKSPACE_STATE_BEFORE_EMOJI_MODIFIER = 4,	// The offset is immediately before an emoji modifier.
	BACKSPACE_STATE_BEFORE_VS_AND_EMOJI_MODIFIER = 5, // The offset is immediately before a variation selector and an emoji modifier.
	BACKSPACE_STATE_BEFORE_VS = 6, // The offset is immediately before a variation selector.
	BACKSPACE_STATE_BEFORE_EMOJI = 7, // The offset is immediately before an emoji.
	BACKSPACE_STATE_BEFORE_ZWJ = 8, // The offset is immediately before a ZWJ that were seen before a ZWJ emoji.
	BACKSPACE_STATE_BEFORE_VS_AND_ZWJ = 9, // The offset is immediately before a variation selector and a ZWJ that were seen before a ZWJ emoji.
	BACKSPACE_STATE_ODD_NUMBERED_RIS = 10, // The number of following RIS code points is odd.
	BACKSPACE_STATE_EVEN_NUMBERED_RIS = 11, // // The number of following RIS code points is even.
	BACKSPACE_STATE_IN_TAG_SEQUENCE = 12, // The offset is in emoji tag sequence.
	BACKSPACE_STATE_FINISHED = 13, // The state machine has been stopped.
};

static skb_text_position_t skb__get_backspace_start_offset(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(editor);

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_USE);

	// If at beginning of line, go to the end of the previous line.
	if (pos.text_offset <= 0) {
		if (pos.paragraph_idx > 0) {
			pos.paragraph_idx--;
			pos.text_offset = skb__get_text_count(editor, pos.paragraph_idx);
		}
	}

	if (pos.text_offset <= 0) {
		return (skb_text_position_t) {
			.offset = 0,
			.affinity = SKB_AFFINITY_SOL,
		};
	}

	int32_t offset = pos.text_offset;

	int32_t delete_char_count = 0;  // Char count to be deleted by backspace.
	int32_t last_seen_var_sel_char_count = 0;  // Char count of previous variation selector.
	int32_t state = BACKSPACE_STATE_START;
	int32_t cur_offset = offset;

	const uint32_t* paragraph_text = skb_text_get_utf32(skb__get_text(editor, pos.paragraph_idx));

	do {
		const uint32_t cp = paragraph_text[cur_offset - 1];
		cur_offset--;
		switch (state) {
		case BACKSPACE_STATE_START:
			delete_char_count = 1;
			if (cp == SKB_CHAR_LINE_FEED)
				state = BACKSPACE_STATE_LF;
			else if (skb_is_variation_selector(cp))
				state = BACKSPACE_STATE_BEFORE_VS;
			else if (skb_is_regional_indicator_symbol(cp))
				state = BACKSPACE_STATE_ODD_NUMBERED_RIS;
			else if (skb_is_emoji_modifier(cp))
				state = BACKSPACE_STATE_BEFORE_EMOJI_MODIFIER;
			else if (cp == SKB_CHAR_COMBINING_ENCLOSING_KEYCAP)
				state = BACKSPACE_STATE_BEFORE_KEYCAP;
			else if (skb_is_emoji(cp))
				state = BACKSPACE_STATE_BEFORE_EMOJI;
			else if (cp == SKB_CHAR_CANCEL_TAG)
				state = BACKSPACE_STATE_IN_TAG_SEQUENCE;
			else
				state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_LF:
			if (cp == SKB_CHAR_CARRIAGE_RETURN)
				delete_char_count++;
			state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_ODD_NUMBERED_RIS:
			if (skb_is_regional_indicator_symbol(cp)) {
				delete_char_count += 2; /* Char count of RIS */
				state = BACKSPACE_STATE_EVEN_NUMBERED_RIS;
			} else {
				state = BACKSPACE_STATE_FINISHED;
			}
			break;
		case BACKSPACE_STATE_EVEN_NUMBERED_RIS:
			if (skb_is_regional_indicator_symbol(cp)) {
				delete_char_count -= 2; /* Char count of RIS */
				state = BACKSPACE_STATE_ODD_NUMBERED_RIS;
			} else {
				state = BACKSPACE_STATE_FINISHED;
			}
			break;
		case BACKSPACE_STATE_BEFORE_KEYCAP:
			if (skb_is_variation_selector(cp)) {
				last_seen_var_sel_char_count = 1;
				state = BACKSPACE_STATE_BEFORE_VS_AND_KEYCAP;
				break;
			}
			if (skb_is_keycap_base(cp))
				delete_char_count++;
			state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_BEFORE_VS_AND_KEYCAP:
			if (skb_is_keycap_base(cp))
				delete_char_count += last_seen_var_sel_char_count + 1;
			state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_BEFORE_EMOJI_MODIFIER:
			if (skb_is_variation_selector(cp)) {
				last_seen_var_sel_char_count = 1;
				state = BACKSPACE_STATE_BEFORE_VS_AND_EMOJI_MODIFIER;
				break;
			}
			if (skb_is_emoji_modifier_base(cp)) {
				delete_char_count++;
				state = BACKSPACE_STATE_BEFORE_EMOJI;
				break;
			}
			state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_BEFORE_VS_AND_EMOJI_MODIFIER:
			if (skb_is_emoji_modifier_base(cp))
				delete_char_count += last_seen_var_sel_char_count + 1;
			state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_BEFORE_VS:
			if (skb_is_emoji(cp)) {
				delete_char_count++;
				state = BACKSPACE_STATE_BEFORE_EMOJI;
				break;
			}
			if (!skb_is_variation_selector(cp) && hb_unicode_combining_class(hb_unicode_funcs_get_default(), cp) == HB_UNICODE_COMBINING_CLASS_NOT_REORDERED)
				delete_char_count++;
			state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_BEFORE_EMOJI:
			if (cp == SKB_CHAR_ZERO_WIDTH_JOINER)
				state = BACKSPACE_STATE_BEFORE_ZWJ;
			else
				state = BACKSPACE_STATE_FINISHED;
			break;
		case BACKSPACE_STATE_BEFORE_ZWJ:
			if (skb_is_emoji(cp)) {
				delete_char_count += 1 + 1;  // +1 for ZWJ.
				state = skb_is_emoji_modifier(cp) ? BACKSPACE_STATE_BEFORE_EMOJI_MODIFIER : BACKSPACE_STATE_BEFORE_EMOJI;
			} else if (skb_is_variation_selector(cp)) {
				last_seen_var_sel_char_count = 1;
				state = BACKSPACE_STATE_BEFORE_VS_AND_ZWJ;
			} else {
				state = BACKSPACE_STATE_FINISHED;
			}
			break;
		case BACKSPACE_STATE_BEFORE_VS_AND_ZWJ:
			if (skb_is_emoji(cp)) {
				delete_char_count += last_seen_var_sel_char_count + 1 + 1; // +1 for ZWJ.
				last_seen_var_sel_char_count = 0;
				state = BACKSPACE_STATE_BEFORE_EMOJI;
			} else {
				state = BACKSPACE_STATE_FINISHED;
			}
			break;
		case BACKSPACE_STATE_IN_TAG_SEQUENCE:
			if (skb_is_tag_spec_char(cp)) {
				delete_char_count++;
				// Keep the same state.
			} else if (skb_is_emoji(cp)) {
				delete_char_count++;
				state = BACKSPACE_STATE_FINISHED;
			} else {
				// Couldn't find tag_base character. Delete the last tag_term character.
				delete_char_count = 2;  // for U+E007F
				state = BACKSPACE_STATE_FINISHED;
			}
			break;
		default:
			assert(0);
			state = BACKSPACE_STATE_FINISHED;
			break;
		}
	} while (cur_offset > 0 && state != BACKSPACE_STATE_FINISHED);

	pos.text_offset -= delete_char_count;
	pos.global_text_offset = skb__get_global_text_offset(editor, pos.paragraph_idx) + pos.text_offset;

	return (skb_text_position_t) {
		.offset = pos.global_text_offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

static skb_rich_text_t* skb__make_scratch_text_input_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_count)
{
	skb_rich_text_reset(&editor->scratch_rich_text);

	skb_attribute_set_t paragraph_attributes = {0};
	if (skb__get_paragraph_count(editor) > 0)
		paragraph_attributes = skb__get_paragraph_attributes(editor, editor->active_attribute_paragraph_idx);
	else
		paragraph_attributes = editor->params.paragraph_attributes;

	skb_rich_text_append_paragraph(&editor->scratch_rich_text, paragraph_attributes);

	skb_attribute_set_t attributes = {
		.attributes = editor->active_attributes,
		.attributes_count = editor->active_attributes_count,
	};

	skb_rich_text_append_utf32(&editor->scratch_rich_text, temp_alloc, utf32, utf32_count, attributes);

	return &editor->scratch_rich_text;
}

void skb_editor_process_key_pressed(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_editor_key_t key, uint32_t mods)
{
	assert(editor);

	if (key == SKB_KEY_RIGHT) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			if (mods & SKB_MOD_SHIFT) {
				// MacOS mode with shift
				if (mods & SKB_MOD_COMMAND)
					editor->selection.end = skb_editor_get_line_end_at(editor, editor->selection.end);
				else if (mods & SKB_MOD_OPTION)
					editor->selection.end = skb_editor_move_to_next_word(editor, editor->selection.end);
				else
					editor->selection.end = skb_editor_move_to_next_char(editor, editor->selection.end, SKB_MOVE_WITH_SELECTION);
				// Do not move g_selection_start_caret, to allow the selection to grow.
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
				skb__ensure_caret_visible(editor);
			} else {
				// MacOS mode without shift
				if (mods & SKB_MOD_COMMAND) {
					editor->selection.end = skb_editor_get_line_end_at(editor, editor->selection.end);
				} else if (mods & SKB_MOD_OPTION) {
					editor->selection.end = skb_editor_move_to_next_word(editor, editor->selection.end);
				} else {
					// Reset selection, choose left-most caret position.
					if (skb_editor_get_text_range_count(editor, editor->selection) > 0)
						editor->selection.end = skb_editor_get_selection_ordered_end(editor, editor->selection);
					else
						editor->selection.end = skb_editor_move_to_next_char(editor, editor->selection.end, SKB_MOVE_DEFAULT);
				}
				editor->selection.start = editor->selection.end;
				skb__pick_active_attributes(editor);
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
				skb__ensure_caret_visible(editor);
			}
		} else {
			if (mods & SKB_MOD_SHIFT) {
				// Default mode with shift
				if (mods & SKB_MOD_CONTROL)
					editor->selection.end = skb_editor_move_to_next_word(editor, editor->selection.end);
				else
					editor->selection.end = skb_editor_move_to_next_char(editor, editor->selection.end, SKB_MOVE_WITH_SELECTION);
				// Do not move g_selection_start_caret, to allow the selection to grow.
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
				skb__ensure_caret_visible(editor);
			} else {
				// Default mode without shift
				if (mods & SKB_MOD_CONTROL) {
					editor->selection.end = skb_editor_move_to_next_word(editor, editor->selection.end);
				} else {
					// Reset selection, choose left-most caret position.
					if (skb_editor_get_text_range_count(editor, editor->selection) > 0)
						editor->selection.end = skb_editor_get_selection_ordered_end(editor, editor->selection);
					else
						editor->selection.end = skb_editor_move_to_next_char(editor, editor->selection.end, SKB_MOVE_DEFAULT);
				}
				editor->selection.start = editor->selection.end;
				skb__pick_active_attributes(editor);
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
				skb__ensure_caret_visible(editor);
			}
		}
		editor->preferred_x = -1.f; // reset preferred.
	}

	if (key == SKB_KEY_LEFT) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			if (mods & SKB_MOD_SHIFT) {
				// MacOS mode with shift
				if (mods & SKB_MOD_COMMAND)
					editor->selection.end = skb_editor_get_line_start_at(editor, editor->selection.end);
				else if (mods & SKB_MOD_OPTION)
					editor->selection.end = skb_editor_move_to_prev_word(editor, editor->selection.end);
				else
					editor->selection.end = skb_editor_move_to_prev_char(editor, editor->selection.end, SKB_MOVE_WITH_SELECTION);
				// Do not move g_selection_start_caret, to allow the selection to grow.
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
				skb__ensure_caret_visible(editor);
			} else {
				// macOS mode without shift
				if (mods & SKB_MOD_COMMAND) {
					editor->selection.end = skb_editor_get_line_start_at(editor, editor->selection.end);
				} else if (mods & SKB_MOD_OPTION) {
					editor->selection.end = skb_editor_move_to_prev_word(editor, editor->selection.end);
				} else {
					// Reset selection, choose right-most caret position.
					if (skb_editor_get_text_range_count(editor, editor->selection) > 0)
						editor->selection.end = skb_editor_get_selection_ordered_start(editor, editor->selection);
					else
						editor->selection.end = skb_editor_move_to_prev_char(editor, editor->selection.end, SKB_MOVE_DEFAULT);
				}
				editor->selection.start = editor->selection.end;
				skb__pick_active_attributes(editor);
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
				skb__ensure_caret_visible(editor);
			}
		} else {
			if (mods & SKB_MOD_SHIFT) {
				// Default mode with shift
				if (mods & SKB_MOD_CONTROL)
					editor->selection.end = skb_editor_move_to_prev_word(editor, editor->selection.end);
				else
					editor->selection.end = skb_editor_move_to_prev_char(editor, editor->selection.end, SKB_MOVE_WITH_SELECTION);
				// Do not move g_selection_start_caret, to allow the selection to grow.
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
				skb__ensure_caret_visible(editor);
			} else {
				// Default mode without shift
				if (mods & SKB_MOD_CONTROL) {
					editor->selection.end = skb_editor_move_to_prev_word(editor, editor->selection.end);
				} else {
					if (skb_editor_get_text_range_count(editor, editor->selection) > 0)
						editor->selection.end = skb_editor_get_selection_ordered_start(editor, editor->selection);
					else
						editor->selection.end = skb_editor_move_to_prev_char(editor, editor->selection.end, SKB_MOVE_DEFAULT);
				}
				editor->selection.start = editor->selection.end;
				skb__pick_active_attributes(editor);
				skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
				skb__ensure_caret_visible(editor);
			}
		}
		editor->preferred_x = -1.f; // reset preferred.
	}

	if (key == SKB_KEY_HOME) {
		editor->selection.end = skb_editor_get_line_start_at(editor, editor->selection.end);
		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start = editor->selection.end;
			skb__pick_active_attributes(editor);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
		} else {
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
		}
		skb__ensure_caret_visible(editor);
		editor->preferred_x = -1.f; // reset preferred.
	}

	if (key == SKB_KEY_END) {
		editor->selection.end = skb_editor_get_line_end_at(editor, editor->selection.end);
		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start = editor->selection.end;
			skb__pick_active_attributes(editor);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
		} else {
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
		}
		skb__ensure_caret_visible(editor);
		editor->preferred_x = -1.f; // reset preferred.
	}

	if (key == SKB_KEY_UP) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			// macOS mode
			if (mods & SKB_MOD_COMMAND) {
				// Command + Up: Move to beginning of document
				editor->selection.end = skb__editor_get_document_start(editor);
				editor->preferred_x = -1.f; // reset preferred
			} else {
				// Regular up movement
				if (editor->preferred_x < 0.f) {
					skb_caret_info_t vis = skb_editor_get_caret_info_at(editor, editor->selection.end);
					editor->preferred_x = vis.x;
				}
				editor->selection.end = skb_editor_move_to_prev_line(editor, editor->selection.end, editor->preferred_x);
			}
		} else {
			// Default mode
			if (editor->preferred_x < 0.f) {
				skb_caret_info_t vis = skb_editor_get_caret_info_at(editor, editor->selection.end);
				editor->preferred_x = vis.x;
			}
			editor->selection.end = skb_editor_move_to_prev_line(editor, editor->selection.end, editor->preferred_x);
		}

		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start = editor->selection.end;
			skb__pick_active_attributes(editor);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
		} else {
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
		}
		skb__ensure_caret_visible(editor);
	}
	if (key == SKB_KEY_DOWN) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			// macOS mode
			if (mods & SKB_MOD_COMMAND) {
				// Command + Down: Move to end of document
				editor->selection.end = skb__editor_get_document_end(editor);
				editor->preferred_x = -1.f; // reset preferred
			} else {
				// Regular down movement
				if (editor->preferred_x < 0.f) {
					skb_caret_info_t vis = skb_editor_get_caret_info_at(editor, editor->selection.end);
					editor->preferred_x = vis.x;
				}
				editor->selection.end = skb_editor_move_to_next_line(editor, editor->selection.end, editor->preferred_x);
			}
		} else {
			// Default mode
			if (editor->preferred_x < 0.f) {
				skb_caret_info_t vis = skb_editor_get_caret_info_at(editor, editor->selection.end);
				editor->preferred_x = vis.x;
			}
			editor->selection.end = skb_editor_move_to_next_line(editor, editor->selection.end, editor->preferred_x);
		}

		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start = editor->selection.end;
			skb__pick_active_attributes(editor);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_MOVE);
		} else {
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_GROW);
		}
		skb__ensure_caret_visible(editor);
	}

	if (key == SKB_KEY_BACKSPACE) {
		if (skb_editor_get_text_range_count(editor, editor->selection) > 0) {
			skb_rich_text_change_t change = skb__replace_selection(editor, NULL, false);
			skb__update_layout(editor, temp_alloc, change);
			skb__pick_active_attributes(editor);
			skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_EDIT);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EDIT);
			skb__ensure_caret_visible(editor);
		} else {
			skb_text_range_t remove_range = {
				.start = skb__get_backspace_start_offset(editor, editor->selection.end),
				.end = editor->selection.end,
			};
			int32_t transaction_id = skb__capture_undo_text_begin(editor, remove_range, NULL, false);
			skb_rich_text_change_t change = skb_rich_text_remove(&editor->rich_text, remove_range);
			skb__update_selection_from_change(editor, change);
			skb__capture_undo_text_end(editor, transaction_id);

			skb__update_layout(editor, temp_alloc, change);
			skb__pick_active_attributes(editor);
			skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_EDIT);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EDIT);
			skb__ensure_caret_visible(editor);
		}
	}

	if (key == SKB_KEY_DELETE) {
		if (skb_editor_get_text_range_count(editor, editor->selection) > 0) {
			skb_rich_text_change_t change = skb__replace_selection(editor, NULL, false);
			skb__update_layout(editor, temp_alloc, change);
			skb__pick_active_attributes(editor);
			skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_EDIT);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EDIT);
			skb__ensure_caret_visible(editor);
		} else {
			skb_text_range_t remove_range = {
				.start = editor->selection.end,
				.end = skb_rich_text_get_next_grapheme_pos(&editor->rich_text, editor->selection.end),
			};
			int32_t transaction_id = skb__capture_undo_text_begin(editor, remove_range, NULL, false);
			skb_rich_text_change_t change = skb_rich_text_remove(&editor->rich_text, remove_range);
			skb__update_selection_from_change(editor, change);
			skb__capture_undo_text_end(editor, transaction_id);

			skb__update_layout(editor, temp_alloc, change);
			skb__pick_active_attributes(editor);
			skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_EDIT);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EDIT);
			skb__ensure_caret_visible(editor);
		}
	}

	if (key == SKB_KEY_ENTER) {
		uint32_t cp = SKB_CHAR_LINE_FEED;
		skb_rich_text_t* input_text = skb__make_scratch_text_input_utf32(editor, temp_alloc, &cp, 1);

		if (editor->input_filter_callback)
			editor->input_filter_callback(editor, input_text, editor->selection, editor->input_filter_context);

		if (skb_rich_text_get_utf32_count(input_text) > 0) {
			skb_rich_text_change_t change = skb__replace_selection(editor, input_text, false);
			skb__update_layout(editor, temp_alloc, change);

			// The call to skb__replace_selection changes selection to after the inserted text.
			// The caret is placed on the leading edge, which is usually good, but for new line we want trailing.
			skb_paragraph_position_t caret_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, editor->selection.end, SKB_AFFINITY_USE);
			editor->selection.end = (skb_text_position_t) {
				.offset = caret_pos.global_text_offset,
				.affinity = SKB_AFFINITY_TRAILING,
			};
			editor->selection.start = editor->selection.end;
			skb__pick_active_attributes(editor);
			skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_EDIT);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EDIT);
			skb__ensure_caret_visible(editor);
		}
	}
}


static skb_text_position_t skb__adjust_text_position(skb_text_position_t pos, skb_range_t removed_range, int32_t inserted_count)
{
	const int32_t removed_count = removed_range.end - removed_range.start;

	if (pos.offset > removed_range.end) {
		// Out of range, move by changed amount.
		pos.offset += inserted_count - removed_count;
	} else if (pos.offset >= removed_range.start) {
		// In range, make sure we stay inside the range.
		const int32_t new_end_offset = removed_range.end - removed_count + inserted_count;
		pos.offset = skb_mini(pos.offset, new_end_offset);
		// Make sure the leading affinity stays inside the new range.
		if (pos.offset == new_end_offset && pos.affinity == SKB_AFFINITY_LEADING)
			pos.affinity = SKB_AFFINITY_TRAILING;
	}

	if (pos.offset < 0) {
		pos.offset = 0;
		pos.affinity = SKB_AFFINITY_SOL;
	}

	return pos;
}

static skb_text_range_t skb__adjust_text_selection(const skb_editor_t* editor, skb_text_range_t selection, skb_text_range_t removed_text_range, int32_t inserted_count)
{
	skb_range_t removed_range = skb_rich_text_get_offset_range_from_text_range(&editor->rich_text, removed_text_range);
	return (skb_text_range_t) {
		.start = skb__adjust_text_position(selection.start, removed_range, inserted_count),
		.end = skb__adjust_text_position(selection.end, removed_range, inserted_count),
	};
}

static void skb__insert_rich_text(
	skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range,
	const skb_rich_text_t* rich_text, bool allow_amend_undo, bool external)
{
	const bool is_current_selection = skb_text_range_is_current_selection(text_range);
	text_range = skb__resolve_text_range(editor, text_range);

	if (editor->input_filter_callback && rich_text) {
		skb_rich_text_t* rich_text_copy = &editor->scratch_rich_text;
		if (rich_text != rich_text_copy) {
			skb_rich_text_reset(rich_text_copy);
			skb_rich_text_append(rich_text_copy, rich_text);
		}

		editor->input_filter_callback(editor, rich_text_copy, editor->selection, editor->input_filter_context);
		rich_text = rich_text_copy;
	}

	int32_t transaction_id = skb__capture_undo_text_begin(editor, text_range, rich_text, allow_amend_undo);
	skb_rich_text_change_t change = skb_rich_text_insert(&editor->rich_text, text_range, rich_text);

	if (is_current_selection) {
		skb__update_selection_from_change(editor, change);
	} else {
		// Adjust selection
		const int32_t inserted_text_length = skb_rich_text_get_utf32_count(rich_text);
		editor->selection = skb__adjust_text_selection(editor, editor->selection, text_range, inserted_text_length);
	}

	skb__capture_undo_text_end(editor, transaction_id);

	skb__update_layout(editor, temp_alloc, change);
	skb__pick_active_attributes(editor);
	skb__emit_on_text_change(editor, external ? SKB_EDITOR_TEXT_EXTERNAL : SKB_EDITOR_TEXT_EDIT);
	skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EDIT);
	skb__ensure_caret_visible(editor);
}


void skb_editor_insert_paragraph(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t paragraph_attribute)
{
	uint32_t cp = SKB_CHAR_LINE_FEED;
	skb_rich_text_t* input_text = skb__make_scratch_text_input_utf32(editor, temp_alloc, &cp, 1);

	if (paragraph_attribute.kind != 0) {
		skb_rich_text_set_paragraph_attribute(input_text, (skb_text_range_t){.start.offset = 0, .end.offset = 2 }, paragraph_attribute);
	}

	skb__insert_rich_text(editor, temp_alloc, text_range, input_text, false, true);
}

void skb_editor_insert_codepoint(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, uint32_t codepoint)
{
	assert(editor);

	skb_rich_text_t* input_text = skb__make_scratch_text_input_utf32(editor, temp_alloc, &codepoint, 1);
	skb__insert_rich_text(editor, temp_alloc, text_range, input_text, true, false);
}

void skb_editor_insert_text_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, const char* utf8, int32_t utf8_len)
{
	assert(editor);
	if (utf8 && utf8_len < 0) utf8_len = (int32_t)strlen(utf8);

	const int32_t utf32_count = skb_utf8_to_utf32(utf8, utf8_len, NULL, 0);
	uint32_t* utf32 = SKB_TEMP_ALLOC(temp_alloc, uint32_t, utf32_count);
	skb_utf8_to_utf32(utf8, utf8_len, utf32, utf32_count);

	skb_rich_text_t* input_text = skb__make_scratch_text_input_utf32(editor, temp_alloc, utf32, utf32_count);

	SKB_TEMP_FREE(temp_alloc, utf32);

	skb__insert_rich_text(editor, temp_alloc, text_range, input_text, false, true);
}

void skb_editor_insert_text_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, const uint32_t* utf32, int32_t utf32_len)
{
	assert(editor);
	skb_rich_text_t* input_text = skb__make_scratch_text_input_utf32(editor, temp_alloc, utf32, utf32_len);
	skb__insert_rich_text(editor, temp_alloc, text_range, input_text, false, true);
}

void skb_editor_insert_rich_text(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, const skb_rich_text_t* rich_text)
{
	assert(editor);
	skb__insert_rich_text(editor, temp_alloc, text_range, rich_text, false, true);
}

void skb_editor_remove(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range)
{
	assert(editor);
	skb__insert_rich_text(editor, temp_alloc, text_range, NULL, false, true);
}

void skb_editor_toggle_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute)
{
	skb_editor_toggle_attribute_with_payload(editor, temp_alloc, text_range, attribute, 0, NULL);
}

void skb_editor_toggle_attribute_with_payload(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);
	const int32_t text_count = skb_editor_get_text_range_count(editor, text_range);

	if (text_count == 0) {
		// Apply to current style
		int32_t attribute_idx = SKB_INVALID_INDEX;
		for (int32_t i = 0; i < editor->active_attributes_count; i++) {
			if (editor->active_attributes[i].kind == attribute.kind) {
				attribute_idx = i;
				break;
			}
		}
		if (attribute_idx == SKB_INVALID_INDEX) {
			// Add
			SKB_ARRAY_RESERVE(editor->active_attributes, editor->active_attributes_count + 1);
			editor->active_attributes[editor->active_attributes_count++] = attribute;
		} else {
			// Remove
			editor->active_attributes[attribute_idx] = editor->active_attributes[editor->active_attributes_count - 1];
			editor->active_attributes_count--;
		}
		skb__update_preview_caret(editor);
	} else {
		// Apply to selection
		if (skb_rich_text_has_attribute(&editor->rich_text, text_range, attribute)) {
			skb_editor_clear_attribute(editor, temp_alloc, text_range, attribute);
		} else {
			skb_editor_set_attribute_with_payload(editor, temp_alloc, text_range, attribute, span_flags, payload);
		}
	}
}

void skb_editor_set_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute)
{
	skb_editor_set_attribute_with_payload(editor, temp_alloc, text_range, attribute, 0, NULL);
}

void skb_editor_set_attribute_with_payload(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute, uint8_t span_flags, const skb_data_blob_t* payload)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);

	const int32_t text_count = skb_editor_get_text_range_count(editor, text_range);

	if (text_count == 0) {
		// Apply to current style
		int32_t attribute_idx = SKB_INVALID_INDEX;
		for (int32_t i = 0; i < editor->active_attributes_count; i++) {
			if (editor->active_attributes[i].kind == attribute.kind) {
				attribute_idx = i;
				break;
			}
		}
		if (attribute_idx == SKB_INVALID_INDEX) {
			SKB_ARRAY_RESERVE(editor->active_attributes, editor->active_attributes_count + 1);
			attribute_idx = editor->active_attributes_count++;
		}
		editor->active_attributes[attribute_idx] = attribute;
		skb__update_preview_caret(editor);
	} else {
		// Apply to selection
		int32_t transaction_id = skb__capture_undo_attributes_begin(editor, text_range);

		skb_rich_text_set_attribute_with_payload(&editor->rich_text, text_range, attribute, span_flags, payload);

		skb__capture_undo_attributes_end(editor, transaction_id);

		skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});
		skb__pick_active_attributes(editor);

		skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_ATTRIBUTE);
	}
}

void skb_editor_clear_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);

	const int32_t text_count = skb_editor_get_text_range_count(editor, text_range);

	if (text_count == 0) {
		// Apply to current style
		int32_t attribute_idx = SKB_INVALID_INDEX;
		for (int32_t i = 0; i < editor->active_attributes_count; i++) {
			if (editor->active_attributes[i].kind == attribute.kind) {
				attribute_idx = i;
				break;
			}
		}
		if (attribute_idx != SKB_INVALID_INDEX) {
			editor->active_attributes[attribute_idx] = editor->active_attributes[editor->active_attributes_count - 1];
			editor->active_attributes_count--;
		}
		skb__update_preview_caret(editor);
	} else {
		int32_t transaction_id = skb__capture_undo_attributes_begin(editor, text_range);

		skb_rich_text_clear_attribute(&editor->rich_text, text_range, attribute);

		skb__capture_undo_attributes_end(editor, transaction_id);

		skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});
		skb__pick_active_attributes(editor);

		skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_ATTRIBUTE);
	}
}

void skb_editor_clear_all_attributes(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);

	int32_t transaction_id = skb__capture_undo_attributes_begin(editor, text_range);

	skb_rich_text_clear_all_attributes(&editor->rich_text, text_range);

	skb__capture_undo_attributes_end(editor, transaction_id);

	skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});
	skb__pick_active_attributes(editor);

	skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_ATTRIBUTE);
}

void skb_editor_set_paragraph_attribute(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);

	int32_t transaction_id = skb__capture_undo_attributes_begin(editor, text_range);

	skb_rich_text_set_paragraph_attribute(&editor->rich_text, text_range, attribute);

	skb__capture_undo_attributes_end(editor, transaction_id);

	skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});
	skb__pick_active_attributes(editor);

	skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_ATTRIBUTE);
}

void skb_editor_set_paragraph_attribute_delta(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);

	int32_t transaction_id = skb__capture_undo_attributes_begin(editor, text_range);

	skb_rich_text_set_paragraph_attribute_delta(&editor->rich_text, text_range, attribute);

	skb__capture_undo_attributes_end(editor, transaction_id);

	skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){0});
	skb__pick_active_attributes(editor);

	skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_ATTRIBUTE);
}

bool skb_editor_has_paragraph_attribute(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);

	skb_range_t paragraph_range = skb_editor_get_paragraphs_range_from_text_range(editor, text_range);
	int32_t match_count = 0;
	for (int32_t i = paragraph_range.start; i < paragraph_range.end; i++) {
		skb_attribute_set_t paragraph_attributes = skb_rich_text_get_paragraph_attributes(&editor->rich_text, i);
		for (int32_t j = 0; j < paragraph_attributes.attributes_count; j++) {
			if (paragraph_attributes.attributes[j].kind == attribute.kind && memcmp(&paragraph_attributes.attributes[j], &attribute, sizeof(skb_attribute_t)) == 0) {
				match_count++;
				break;
			}
		}
	}
	int32_t paragraph_count = paragraph_range.end - paragraph_range.start;
	return paragraph_count == match_count;
}

bool skb_editor_has_attribute(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);
	const bool is_current_selection = skb_text_range_is_current_selection(text_range);
	text_range = skb__resolve_text_range(editor, text_range);

	const int32_t selection_count = skb_editor_get_text_range_count(editor, text_range);

	if (selection_count > 0)
		return skb_rich_text_has_attribute(&editor->rich_text, text_range, attribute);

	if (is_current_selection) {
		for (int32_t i = 0; i < editor->active_attributes_count; i++) {
			if (editor->active_attributes[i].kind == attribute.kind && memcmp(&editor->active_attributes[i], &attribute, sizeof(skb_attribute_t)) == 0)
				return true;
		}
	}

	return false;
}

bool skb_editor_has_text_attribute(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);
	text_range = skb__resolve_text_range(editor, text_range);
	return skb_rich_text_has_attribute(&editor->rich_text, text_range, attribute);
}


int32_t skb_editor_get_attributes(const skb_editor_t* editor, skb_text_range_t text_range, uint32_t attribute_kind, skb_attribute_t* attributes, int32_t attributes_cap)
{
	assert(editor);
	const bool is_current_selection = skb_text_range_is_current_selection(text_range);
	text_range = skb__resolve_text_range(editor, text_range);

	const int32_t selection_count = skb_editor_get_text_range_count(editor, text_range);

	if (selection_count > 0)
		return skb_rich_text_get_attributes(&editor->rich_text, text_range, attribute_kind, attributes, attributes_cap);

	if (is_current_selection) {
		int32_t attributes_count = 0;
		for (int32_t i = 0; i < editor->active_attributes_count; i++) {
			if (editor->active_attributes[i].kind == attribute_kind) {
				if (attributes_count < attributes_cap)
					attributes[attributes_count++] = editor->active_attributes[i];
			}
		}
		return attributes_count;
	}

	return 0;
}

skb_data_blob_t* skb_editor_get_attribute_payload(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);
	return skb_rich_text_get_attribute_payload(&editor->rich_text, text_range, attribute);
}

skb_text_range_t skb_editor_get_attribute_text_range(const skb_editor_t* editor, skb_text_range_t text_range, skb_attribute_t attribute)
{
	assert(editor);

	text_range = skb__resolve_text_range(editor, text_range);
	return skb_rich_text_get_attribute_text_range(&editor->rich_text, text_range, attribute);
}

int32_t skb_editor_get_active_attributes_count(const skb_editor_t* editor)
{
	assert(editor);
	return editor->active_attributes_count;
}

const skb_attribute_t* skb_editor_get_active_attributes(const skb_editor_t* editor)
{
	assert(editor);
	return editor->active_attributes;
}

void skb_editor_set_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len, int32_t caret_position)
{
	assert(editor);

	if (utf32_len == -1)
		utf32_len = skb_utf32_strlen(utf32);

	const bool had_ime_text = skb_text_get_utf32_count(&editor->composition_text) > 0;

	skb_text_reset(&editor->composition_text);
	skb_text_append_utf32(&editor->composition_text, utf32, utf32_len, editor->params.composition_attributes);

	if (!had_ime_text) {
		// Capture the text position the first time we set the text.
		editor->composition_selection_base = skb_editor_get_selection_ordered_start(editor, editor->selection);
		// Text offset where we're displaying the composition text.
		skb_paragraph_position_t base_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, editor->composition_selection_base, SKB_AFFINITY_USE);
		editor->composition_text_offset = base_pos.global_text_offset;

		// Clear the selection. Since the IME can be cancelled ideally we would clear on commit,
		// but it gets really complicated when multiple lines area involved.
		editor->composition_cleared_selection = false;
		if (skb_editor_get_text_range_count(editor, editor->selection) > 0) {
			skb__replace_selection(editor, NULL, true);
			editor->composition_cleared_selection = true;
		}
	}

	// The ime cursor is within the IME string, offset from the selection base.
	editor->composition_selection.start = editor->composition_selection_base;
	editor->composition_selection.end = editor->composition_selection_base;
	editor->composition_selection.start.offset += caret_position;
	editor->composition_selection.end.offset += caret_position;

	skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){ .edit_end_position.offset = SKB_INVALID_INDEX });
}

void skb_editor_commit_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len)
{
	assert(editor);

	if (utf32 == NULL) {
		utf32 = skb_text_get_utf32(&editor->composition_text);
		utf32_len = skb_text_get_utf32_count(&editor->composition_text);
		// This makes bold assumption that the text memory buffer is not freed.
		skb_text_reset(&editor->composition_text);
		assert(utf32 == skb_text_get_utf32(&editor->composition_text));
	} else {
		skb_text_reset(&editor->composition_text);
	}

	editor->composition_selection_base = (skb_text_position_t){0};
	editor->composition_selection = (skb_text_range_t){0};
	editor->composition_text_offset = 0;

	skb_rich_text_t* input_text = skb__make_scratch_text_input_utf32(editor, temp_alloc, utf32, utf32_len);
	skb__insert_rich_text(editor, temp_alloc, SKB_CURRENT_SELECTION, input_text, false, false);
}

void skb_editor_clear_composition(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);

	if (skb_text_get_utf32_count(&editor->composition_text)) {

		skb_text_reset(&editor->composition_text);

		editor->composition_selection_base = (skb_text_position_t){0};
		editor->composition_selection = (skb_text_range_t){0};
		editor->composition_text_offset = 0;

		skb__update_layout(editor, temp_alloc, (skb_rich_text_change_t){ .edit_end_position.offset = SKB_INVALID_INDEX });

		if (editor->composition_cleared_selection) {
			skb__emit_on_text_change(editor, SKB_EDITOR_TEXT_EDIT);
			skb__emit_on_selection_change(editor, SKB_EDITOR_SELECTION_EDIT);
		}
	}
}

int32_t skb_editor_get_line_index_at(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(editor);

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_USE);

	int32_t total_line_count = 0;

	// Lines up to the text position.
	for (int32_t i = 0; i < paragraph_pos.paragraph_idx; i++)
		total_line_count += skb_layout_get_lines_count(skb__get_layout(editor, i));

	total_line_count += skb__get_line_index(editor, paragraph_pos);

	return total_line_count;
}

int32_t skb_editor_get_column_index_at(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(editor);

	text_pos = skb__resolve_text_position(editor, text_pos);
	skb_paragraph_position_t paragraph_pos = skb_rich_text_get_paragraph_position_from_text_position(&editor->rich_text, text_pos, SKB_AFFINITY_USE);

	const skb_layout_t* layout = skb__get_layout(editor, paragraph_pos.paragraph_idx);
	if (layout) {
		const int32_t edit_line_idx = skb__get_line_index(editor, paragraph_pos);
		const skb_layout_line_t* lines = skb_layout_get_lines(layout);
		const skb_layout_line_t* line = &lines[edit_line_idx];
		return paragraph_pos.text_offset - line->text_range.start;
	}

	return 0;
}

skb_caret_info_t skb_editor_get_caret_info_at(const skb_editor_t* editor, skb_text_position_t text_pos)
{
	assert(editor);

	const bool is_current_selection_end = skb_text_position_is_current_selection_end(text_pos);

	text_pos = skb__resolve_text_position(editor, text_pos);

	skb_caret_info_t caret = skb_rich_layout_get_caret_info_at(&editor->rich_layout, text_pos);

	if (is_current_selection_end && editor->is_preview_caret_valid) {
		// Override slot from active attributes.
		caret.slope = editor->preview_caret_slope;
		caret.ascender = editor->preview_caret_ascender;
		caret.descender = editor->preview_caret_descender;
	}

	return caret;
}
