// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_editor.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "graphemebreak.h"
#include "hb.h"

#include "skb_layout.h"
#include "skb_common.h"

// From skb_layout.c
skb_text_position_t skb__caret_prune_control_eol(const skb_layout_t* layout, const skb_layout_line_t* line, skb_text_position_t caret);

//
// Text edit
//

typedef struct skb__editor_text_position_t {
	int32_t paragraph_idx;
	int32_t line_idx;
	int32_t paragraph_offset; // Offset inside the paragraph text.
	int32_t text_offset; // Offset of the whole text edited.
} skb__editor_text_position_t;

typedef struct skb__editor_text_range_t {
	skb__editor_text_position_t start;
	skb__editor_text_position_t end;
} skb__editor_text_range_t;

typedef enum {
	SKB_SANITIZE_ADJUST_AFFINITY,
	SKB_SANITIZE_IGNORE_AFFINITY = 1,
} skb_sanitize_affinity_t;

typedef struct skb__editor_paragraph_t {
	skb_layout_t* layout;
	uint32_t* text;
	int32_t text_count;
	int32_t text_cap;
	int32_t text_start_offset;
	bool has_ime_layout;
	float y;
} skb__editor_paragraph_t;

// Stores enough data to be able to undo and redo skb__replace_range().
typedef struct skb__editor_undo_state_t {
	skb_range_t removed_range;		// Removed text range before replace.
	uint32_t* removed_text;			// Temoved text
	int32_t removed_text_count;		// Length of the removed text.

	skb_range_t inserted_range;		// Inserted text range after replace.
	uint32_t* inserted_text;		// Inserted text
	int32_t inserted_text_count;	// Lenfth of the inserted text.

	skb_text_selection_t selection;	// Selection at the time of the replace.

} skb__editor_undo_state_t;


typedef struct skb_editor_t {
	skb_editor_params_t params;
	skb_editor_on_change_func_t* on_change_callback;
	void* on_change_context;

	skb__editor_paragraph_t* paragraphs;
	int32_t paragraphs_count;
	int32_t paragraphs_cap;

	skb_text_selection_t selection;

	double last_click_time;
	float drag_start_x;
	float drag_start_y;
	float preferred_x;
	int32_t click_count;
	skb_text_selection_t drag_initial_selection;
	uint8_t drag_moved;
	uint8_t drag_mode;

	// IME
	uint32_t* composition_text;				// Composition text set during IME editing, displayed temporariry at composition_position.
	int32_t composition_text_cap;			// Space allocated for composition text.
	int32_t composition_text_count;			// Composition text length.
	skb__editor_text_position_t composition_position;	// Paragraph and text offset where the composition is displayed.
	skb_text_position_t composition_selection_base;		// Base position for setting the composition selection.
	skb_text_selection_t composition_selection;			// Selection during composition.
	bool composition_cleared_selection;		// True if initial setting the composition text cleared the current selection.

	// Undo
	skb__editor_undo_state_t* undo_stack;	// The undo stack.
	int32_t undo_stack_count;				// Size if the undo stack
	int32_t undo_stack_cap;					// Allocated space for the undo stack.
	int32_t undo_stack_head;				// Current state inside the undo stack. Initially -1, increases on each change. Undo moves down, redo up (up to stack count).
	bool allow_append_undo;					// True if the next change can be appended to the current undo. E.g. typing characters in a row all goes into same undo. Moving caret will break the sequence.
} skb_editor_t;

// fwd decl
static void skb__reset_undo(skb_editor_t* editor);
static void skb__capture_undo(skb_editor_t* editor, skb__editor_text_position_t start, skb__editor_text_position_t end, const uint32_t* utf32, int32_t utf32_len);
static skb__editor_text_position_t skb__get_sanitized_position(const skb_editor_t* editor, skb_text_position_t pos, skb_sanitize_affinity_t sanitize_affinity);

static int32_t skb__copy_utf32(const uint32_t* src, int32_t count, uint32_t* dst, int32_t max_dst)
{
	const int32_t copy_count = skb_mini(max_dst, count);
	if (copy_count > 0)
		memcpy(dst, src, copy_count * sizeof(uint32_t));
	return count;
}


static void skb__update_layout(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);

	skb_layout_params_t layout_params = editor->params.layout_params;
	layout_params.origin = (skb_vec2_t){ 0 };
	layout_params.flags |= SKB_LAYOUT_PARAMS_IGNORE_MUST_LINE_BREAKS;
	// TODO: we will need to improve the logic pick up the direction automatically.
	// If left to AUTO, each split paragraph will adjust separately and it's confusing.
	if (layout_params.base_direction == SKB_DIRECTION_AUTO)
		layout_params.base_direction = SKB_DIRECTION_LTR;

	float y = 0.f;

	skb__editor_text_position_t ime_position = { .paragraph_idx = SKB_INVALID_INDEX };
	if (editor->composition_text_count > 0)
		ime_position = editor->composition_position;

	for (int32_t i = 0; i < editor->paragraphs_count; i++) {
		skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];

		if (ime_position.paragraph_idx == i) {
			if (paragraph->layout) {
				skb_layout_destroy(paragraph->layout);
				paragraph->layout = NULL;
			}

			// Compose the paragraph from 3 pieces.
			skb_text_run_utf32_t runs[] = {
				{ // Before
					.attributes = editor->params.text_attributes,
					.attributes_count = editor->params.text_attributes_count,
					.text = paragraph->text,
					.text_count = ime_position.paragraph_offset,
				},
				{ // Composition
					.attributes = editor->params.composition_attributes,
					.attributes_count = editor->params.composition_attributes_count,
					.text = editor->composition_text,
					.text_count = editor->composition_text_count,
				},
				{ // After
					.attributes = editor->params.text_attributes,
					.attributes_count = editor->params.text_attributes_count,
					.text = paragraph->text + ime_position.paragraph_offset,
					.text_count = paragraph->text_count - ime_position.paragraph_offset,
				},
			};

			paragraph->layout = skb_layout_create_from_runs_utf32(temp_alloc, &layout_params, runs, SKB_COUNTOF(runs));

			// Mark as IME layout, so that we can clean things up later.
			paragraph->has_ime_layout = true;

		} else {
			// If the paragraph previous had IME layout, relayout.
			if (paragraph->has_ime_layout) {
				skb_layout_destroy(paragraph->layout);
				paragraph->layout = NULL;
				paragraph->has_ime_layout = false;
			}
			if (!paragraph->layout) {
				paragraph->layout = skb_layout_create_utf32(
					temp_alloc, &layout_params, paragraph->text, paragraph->text_count,
					editor->params.text_attributes, editor->params.text_attributes_count);
			}
		}
		assert(paragraph->layout);

		paragraph->y = y;

		skb_rect2_t layout_bounds = skb_layout_get_bounds(paragraph->layout);
		y += layout_bounds.height;
	}
}

static skb_range_t* skb__split_text_into_paragraphs(skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len, int32_t* out_paragraphs_count)
{
	// Split text to paragraphs.
	int32_t start_offset = 0;
	int32_t offset = 0;

	int32_t paragraphs_count = 0;
	int32_t paragraphs_cap = 8;
	skb_range_t* paragraphs = SKB_TEMP_ALLOC(temp_alloc, skb_range_t, paragraphs_cap);

	while (offset < utf32_len) {
		if (skb_is_paragraph_separator(utf32[offset])) {
			// Handle CRLF
			if (offset + 1 < utf32_len && utf32[offset] == SKB_CHAR_CARRIAGE_RETURN && utf32[offset+1] == SKB_CHAR_LINE_FEED)
				offset++; // Skip over the separator
			offset++; // Skip over the separator

			// Create new paragraph
			if (paragraphs_count + 1 > paragraphs_cap) {
				paragraphs_cap += paragraphs_cap/2;
				paragraphs = SKB_TEMP_REALLOC(temp_alloc, paragraphs, skb_range_t, paragraphs_cap);
			}
			skb_range_t* new_paragraph = &paragraphs[paragraphs_count++];
			memset(new_paragraph, 0, sizeof(skb_range_t));
			new_paragraph->start = start_offset;
			new_paragraph->end = offset;
			start_offset = offset;
		} else {
			offset++;
		}
	}

	// The rest
	if (paragraphs_count + 1 > paragraphs_cap) {
		paragraphs_cap += paragraphs_cap/2;
		paragraphs = SKB_TEMP_REALLOC(temp_alloc, paragraphs, skb_range_t, paragraphs_cap);
	}
	skb_range_t* new_paragraph = &paragraphs[paragraphs_count++];
	memset(new_paragraph, 0, sizeof(skb_range_t));
	new_paragraph->start = start_offset;
	new_paragraph->end = offset;

	*out_paragraphs_count = paragraphs_count;
	return paragraphs;
}

static void skb__emit_on_change(skb_editor_t* editor)
{
	if (editor->on_change_callback)
		editor->on_change_callback(editor, editor->on_change_context);
}

static void skb__copy_params(skb_editor_params_t* target, const skb_editor_params_t* params)
{
	assert(target);
	assert(params);

	if (target->text_attributes_count) {
		skb_free((skb_attribute_t*)target->text_attributes);
		target->text_attributes = NULL;
	}
	if (target->composition_attributes) {
		skb_free((skb_attribute_t*)target->composition_attributes);
		target->composition_attributes = NULL;
	}

	*target = *params;

	if (params->text_attributes_count > 0) {
		target->text_attributes = skb_malloc(sizeof(skb_attribute_t) * params->text_attributes_count);
		memcpy((skb_attribute_t*)target->text_attributes, params->text_attributes, sizeof(skb_attribute_t) * params->text_attributes_count);
	} else {
		target->text_attributes = NULL;
	}

	if (params->composition_attributes > 0) {
		target->composition_attributes = skb_malloc(sizeof(skb_attribute_t) * params->composition_attributes_count);
		memcpy((skb_attribute_t*)target->composition_attributes, params->composition_attributes, sizeof(skb_attribute_t) * params->composition_attributes_count);
	} else {
		target->composition_attributes = NULL;
	}
}

skb_editor_t* skb_editor_create(const skb_editor_params_t* params)
{
	assert(params);

	skb_editor_t* editor = skb_malloc(sizeof(skb_editor_t));
	memset(editor, 0, sizeof(skb_editor_t));

	skb__copy_params(&editor->params, params);

	if (editor->params.max_undo_levels == 0)
		editor->params.max_undo_levels = 50;

	editor->preferred_x = -1.f;
	editor->undo_stack_head = -1;

	return editor;
}

void skb_editor_set_on_change_callback(skb_editor_t* editor, skb_editor_on_change_func_t* callback, void* context)
{
	assert(editor);
	editor->on_change_callback = callback;
	editor->on_change_context = context;
}


void skb_editor_destroy(skb_editor_t* editor)
{
	if (!editor) return;

	for (int32_t i = 0; i < editor->paragraphs_count; i++) {
		skb_layout_destroy(editor->paragraphs[i].layout);
		editor->paragraphs[i].layout = NULL;
		skb_free(editor->paragraphs[i].text);
	}
	skb_free(editor->paragraphs);
	skb_free(editor->composition_text);
	skb_free((skb_attribute_t*)editor->params.text_attributes);
	skb_free((skb_attribute_t*)editor->params.composition_attributes);

	memset(editor, 0, sizeof(skb_editor_t));

	skb_free(editor);
}

void skb_editor_reset(skb_editor_t* editor, const skb_editor_params_t* params)
{
	assert(editor);

	if (params)
		skb__copy_params(&editor->params, params);

	for (int32_t i = 0; i < editor->paragraphs_count; i++) {
		skb_layout_destroy(editor->paragraphs[i].layout);
		editor->paragraphs[i].layout = NULL;
		skb_free(editor->paragraphs[i].text);
	}
	editor->paragraphs_count = 0;

	skb__reset_undo(editor);

	skb__emit_on_change(editor);
}


void skb_editor_set_text_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_len)
{
	assert(editor);

	skb_editor_reset(editor, NULL);

	if (utf8_len < 0) utf8_len = (int32_t)strlen(utf8);

	const int32_t utf32_len = skb_utf8_to_utf32(utf8, utf8_len, NULL, 0);
	uint32_t* utf32 = SKB_TEMP_ALLOC(temp_alloc, uint32_t, utf32_len);
	skb_utf8_to_utf32(utf8, utf8_len, utf32, utf32_len);

	int32_t new_paragraph_count = 0;
	skb_range_t* new_paragraph_ranges = skb__split_text_into_paragraphs(temp_alloc, utf32, utf32_len, &new_paragraph_count);
	assert(new_paragraph_count > 0); // We assume that even for empty input text there's one paragraph created.

	for (int32_t i = 0; i < new_paragraph_count; i++) {
		// Create new paragraph
		skb_range_t paragraph_range = new_paragraph_ranges[i];
		SKB_ARRAY_RESERVE(editor->paragraphs, editor->paragraphs_count+1);
		skb__editor_paragraph_t* new_paragraph = &editor->paragraphs[editor->paragraphs_count++];
		memset(new_paragraph, 0, sizeof(skb__editor_paragraph_t));
		new_paragraph->text_start_offset = paragraph_range.start;
		new_paragraph->text_count = paragraph_range.end - paragraph_range.start;
		if (new_paragraph->text_count > 0) {
			SKB_ARRAY_RESERVE(new_paragraph->text, new_paragraph->text_count);
			memcpy(new_paragraph->text, utf32 + paragraph_range.start, new_paragraph->text_count * sizeof(uint32_t));
		}
	}

	SKB_TEMP_FREE(temp_alloc, new_paragraph_ranges);
	SKB_TEMP_FREE(temp_alloc, utf32);

	skb__update_layout(editor, temp_alloc);
	skb__emit_on_change(editor);
}

void skb_editor_set_text_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len)
{
	assert(editor);

	skb_editor_reset(editor, NULL);

	if (utf32_len < 0) utf32_len = skb_utf32_strlen(utf32);

	int32_t new_paragraph_count = 0;
	skb_range_t* new_paragraph_ranges = skb__split_text_into_paragraphs(temp_alloc, utf32, utf32_len, &new_paragraph_count);
	assert(new_paragraph_count > 0); // We assume that even for empty input text there's one paragraph created.

	for (int32_t i = 0; i < new_paragraph_count; i++) {
		// Create new paragraph
		skb_range_t paragraph_range = new_paragraph_ranges[i];
		SKB_ARRAY_RESERVE(editor->paragraphs, editor->paragraphs_count+1);
		skb__editor_paragraph_t* new_paragraph = &editor->paragraphs[editor->paragraphs_count++];
		memset(new_paragraph, 0, sizeof(skb__editor_paragraph_t));
		new_paragraph->text_start_offset = paragraph_range.start;
		new_paragraph->text_count = paragraph_range.end - paragraph_range.start;
		if (new_paragraph->text_count > 0) {
			SKB_ARRAY_RESERVE(new_paragraph->text, new_paragraph->text_count);
			memcpy(new_paragraph->text, utf32 + paragraph_range.start, new_paragraph->text_count * sizeof(uint32_t));
		}
	}

	SKB_TEMP_FREE(temp_alloc, new_paragraph_ranges);

	skb__update_layout(editor, temp_alloc);
	skb__emit_on_change(editor);
}

int32_t skb_editor_get_paragraph_count(skb_editor_t* editor)
{
	assert(editor);
	return editor->paragraphs_count;
}

const skb_layout_t* skb_editor_get_paragraph_layout(skb_editor_t* editor, int32_t index)
{
	assert(editor);
	return editor->paragraphs[index].layout;
}

float skb_editor_get_paragraph_offset_y(skb_editor_t* editor, int32_t index)
{
	assert(editor);
	return editor->paragraphs[index].y;
}

int32_t skb_editor_get_paragraph_text_offset(skb_editor_t* editor, int32_t index)
{
	assert(editor);
	return editor->paragraphs[index].text_start_offset;
}

const skb_editor_params_t* skb_editor_get_params(skb_editor_t* editor)
{
	assert(editor);
	return &editor->params;
}

int32_t skb_editor_get_text_utf8_count(const skb_editor_t* editor)
{
	assert(editor);

	int32_t count = 0;
	for (int32_t i = 0; i < editor->paragraphs_count; i++) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
		count += skb_utf32_to_utf8_count(paragraph->text, paragraph->text_count);
	}
	return count;
}

int32_t skb_editor_get_text_utf8(const skb_editor_t* editor, char* buf, int32_t buf_cap)
{
	assert(editor);
	assert(buf);

	int32_t count = 0;
	for (int32_t i = 0; i < editor->paragraphs_count; i++) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
		const int32_t cur_buf_cap = skb_maxi(0, buf_cap - count);
		if (cur_buf_cap == 0)
			break;
		char* cur_buf = buf + count;
		count += skb_utf32_to_utf8(paragraph->text, paragraph->text_count, cur_buf, cur_buf_cap);
	}
	return skb_mini(count, buf_cap);
}

int32_t skb_editor_get_text_utf32_count(const skb_editor_t* editor)
{
	assert(editor);

	int32_t count = 0;
	for (int32_t i = 0; i < editor->paragraphs_count; i++) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
		count += paragraph->text_count;
	}

	return count;
}

int32_t skb_editor_get_text_utf32(const skb_editor_t* editor, uint32_t* buf, int32_t buf_cap)
{
	assert(editor);

	int32_t count = 0;
	for (int32_t i = 0; i < editor->paragraphs_count; i++) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
		const int32_t cur_buf_cap = skb_maxi(0, buf_cap - count);
		const int32_t copy_count = skb_mini(cur_buf_cap, paragraph->text_count);
		if (buf && copy_count > 0)
			memcpy(buf + count, paragraph->text, copy_count * sizeof(uint32_t));
		count += paragraph->text_count;
	}

	return count;
}

static skb__editor_text_position_t skb__get_sanitized_position(const skb_editor_t* editor, skb_text_position_t pos, skb_sanitize_affinity_t sanitize_affinity)
{
	assert(editor);
	assert(editor->paragraphs_count > 0);

	skb__editor_text_position_t edit_pos = {0};

	// Find edit line.
	const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[editor->paragraphs_count - 1];
	const int32_t total_text_count = last_paragraph->text_start_offset + last_paragraph->text_count;
	if (pos.offset < 0) {
		edit_pos.paragraph_idx = 0;
	} else if (pos.offset >= total_text_count) {
		edit_pos.paragraph_idx = editor->paragraphs_count - 1;
	} else {
		for (int32_t i = 0; i < editor->paragraphs_count; i++) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
			if (pos.offset < (paragraph->text_start_offset + paragraph->text_count)) {
				edit_pos.paragraph_idx = i;
				break;
			}
		}
	}

	// Find line
	const skb__editor_paragraph_t* cur_paragraph = &editor->paragraphs[edit_pos.paragraph_idx];

	edit_pos.paragraph_offset = pos.offset - cur_paragraph->text_start_offset;

	const skb_layout_line_t* lines = skb_layout_get_lines(cur_paragraph->layout);
	const int32_t lines_count = skb_layout_get_lines_count(cur_paragraph->layout);

	if (edit_pos.paragraph_offset < 0) {
		// We should hit this only when the pos.offset is before the first line.
		edit_pos.line_idx = 0;
		edit_pos.paragraph_offset = 0;
	} else if (edit_pos.paragraph_offset > cur_paragraph->text_count) {
		// We should hit this only when the pos.offset is past the last line.
		edit_pos.line_idx = lines_count - 1;
		edit_pos.paragraph_offset = cur_paragraph->text_count;
	} else {
		for (int32_t i = 0; i < lines_count; i++) {
			const skb_layout_line_t* line = &lines[i];
			if (edit_pos.paragraph_offset < line->text_range.end) {
				edit_pos.line_idx = i;
				break;
			}
		}
	}

	// Align to nearest grapheme.
	edit_pos.paragraph_offset = skb_layout_align_grapheme_offset(cur_paragraph->layout, edit_pos.paragraph_offset);

	// Adjust position based on affinity
	if (sanitize_affinity == SKB_SANITIZE_ADJUST_AFFINITY) {
		if (pos.affinity == SKB_AFFINITY_LEADING || pos.affinity == SKB_AFFINITY_EOL) {
			edit_pos.paragraph_offset = skb_layout_next_grapheme_offset(cur_paragraph->layout, edit_pos.paragraph_offset);
			// Affinity adjustment may push the offset to next edit line
			if (edit_pos.paragraph_offset >= cur_paragraph->text_count) {
				if ((edit_pos.paragraph_idx + 1) < editor->paragraphs_count) {
					edit_pos.paragraph_offset = 0;
					edit_pos.paragraph_idx++;
					cur_paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
				}
			}
		}
	}

	edit_pos.text_offset = cur_paragraph->text_start_offset + edit_pos.paragraph_offset;

	return edit_pos;
}

static skb__editor_text_range_t skb__get_sanitized_range(const skb_editor_t* editor, skb_text_selection_t selection)
{
	skb__editor_text_position_t start = skb__get_sanitized_position(editor, selection.start_pos, SKB_SANITIZE_ADJUST_AFFINITY);
	skb__editor_text_position_t end = skb__get_sanitized_position(editor, selection.end_pos, SKB_SANITIZE_ADJUST_AFFINITY);

	skb__editor_text_range_t result = {0};
	if (start.text_offset <= end.text_offset) {
		result.start = start;
		result.end = end;
	} else {
		result.start = end;
		result.end = start;
	}
	return result;
}

static skb__editor_text_position_t skb__get_next_grapheme_pos(const skb_editor_t* editor, skb__editor_text_position_t edit_pos)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];

	edit_pos.paragraph_offset = skb_layout_next_grapheme_offset(paragraph->layout, edit_pos.paragraph_offset);

	// Affinity adjustment may push the offset to next edit line
	if (edit_pos.paragraph_offset >= paragraph->text_count) {
		if ((edit_pos.paragraph_idx + 1) < editor->paragraphs_count) {
			edit_pos.paragraph_offset = 0;
			edit_pos.paragraph_idx++;
			paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
		}
	}

	// Update layout line index
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const int32_t lines_count = skb_layout_get_lines_count(paragraph->layout);

	edit_pos.line_idx = lines_count - 1;
	for (int32_t i = 0; i < lines_count; i++) {
		const skb_layout_line_t* line = &lines[i];
		if (edit_pos.paragraph_offset < line->text_range.end) {
			edit_pos.line_idx = i;
			break;
		}
	}

	edit_pos.text_offset = paragraph->text_start_offset + edit_pos.paragraph_offset;

	return edit_pos;
}

static skb__editor_text_position_t skb__get_prev_grapheme_pos(const skb_editor_t* editor, skb__editor_text_position_t edit_pos)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];

	if (edit_pos.paragraph_offset == 0) {
		if ((edit_pos.paragraph_idx - 1) >= 0) {
			edit_pos.paragraph_idx--;
			paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
			edit_pos.paragraph_offset = skb_layout_get_text_count(paragraph->layout);
		}
	}

	edit_pos.paragraph_offset = skb_layout_prev_grapheme_offset(paragraph->layout, edit_pos.paragraph_offset);

	// Update layout line index
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const int32_t lines_count = skb_layout_get_lines_count(paragraph->layout);

	edit_pos.line_idx = lines_count - 1;
	for (int32_t i = 0; i < lines_count; i++) {
		const skb_layout_line_t* line = &lines[i];
		if (edit_pos.paragraph_offset < line->text_range.end) {
			edit_pos.line_idx = i;
			break;
		}
	}

	edit_pos.text_offset = paragraph->text_start_offset + edit_pos.paragraph_offset;

	return edit_pos;
}


// TODO: should we expose this?
skb_text_position_t skb_editor_get_line_start_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_ADJUST_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const skb_layout_line_t* line = &lines[edit_pos.line_idx];

	skb_text_position_t result = {
		.offset = paragraph->text_start_offset + line->text_range.start,
		.affinity = SKB_AFFINITY_SOL,
	};
	return result;
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_line_end_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_ADJUST_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const skb_layout_line_t* line = &lines[edit_pos.line_idx];

	skb_text_position_t result = {
		.offset = paragraph->text_start_offset + line->last_grapheme_offset,
		.affinity = SKB_AFFINITY_EOL,
	};
	return skb__caret_prune_control_eol(paragraph->layout, line, result);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_word_start_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	// Ignoring affinity, since we want to start from the "character" the user has hit.
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];

	int32_t offset = edit_pos.paragraph_offset;

	const skb_text_property_t* text_props = skb_layout_get_text_properties(paragraph->layout);

	if (text_props) {
		while (offset >= 0) {
			if (text_props[offset-1].flags & SKB_TEXT_PROP_WORD_BREAK) {
				offset = skb_layout_align_grapheme_offset(paragraph->layout, offset);
				break;
			}
			offset--;
		}
	}

	if (offset < 0)
		offset = 0;

	return (skb_text_position_t) {
		.offset = paragraph->text_start_offset + offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_word_end_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	// Ignoring affinity, since we want to start from the "character" the user has hit.
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];

	int32_t offset = edit_pos.paragraph_offset;

	const int32_t text_count = skb_layout_get_text_count(paragraph->layout);
	const skb_text_property_t* text_props = skb_layout_get_text_properties(paragraph->layout);

	if (text_props) {
		while (offset < text_count) {
			if (text_props[offset].flags & SKB_TEXT_PROP_WORD_BREAK) {
				offset = skb_layout_align_grapheme_offset(paragraph->layout, offset);
				break;
			}
			offset++;
		}
	}

	if (offset >= text_count)
		offset = skb_layout_align_grapheme_offset(paragraph->layout, text_count-1);

	return (skb_text_position_t) {
		.offset = paragraph->text_start_offset + offset,
		.affinity = SKB_AFFINITY_LEADING,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_selection_ordered_start(const skb_editor_t* editor, skb_text_selection_t selection)
{
	skb__editor_text_position_t start = skb__get_sanitized_position(editor, selection.start_pos, SKB_SANITIZE_ADJUST_AFFINITY);
	skb__editor_text_position_t end = skb__get_sanitized_position(editor, selection.end_pos, SKB_SANITIZE_ADJUST_AFFINITY);

	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[end.paragraph_idx];

	if (skb_is_rtl(skb_layout_get_resolved_direction(paragraph->layout)))
		return start.text_offset > end.text_offset ? selection.start_pos : selection.end_pos;

	return start.text_offset <= end.text_offset ? selection.start_pos : selection.end_pos;
}

// TODO: should we expose this?
skb_text_position_t skb_editor_get_selection_ordered_end(const skb_editor_t* editor, skb_text_selection_t selection)
{
	skb__editor_text_position_t start = skb__get_sanitized_position(editor, selection.start_pos, SKB_SANITIZE_ADJUST_AFFINITY);
	skb__editor_text_position_t end = skb__get_sanitized_position(editor, selection.end_pos, SKB_SANITIZE_ADJUST_AFFINITY);

	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[end.paragraph_idx];

	if (skb_is_rtl(skb_layout_get_resolved_direction(paragraph->layout)))
		return start.text_offset <= end.text_offset ? selection.start_pos : selection.end_pos;

	return start.text_offset > end.text_offset ? selection.start_pos : selection.end_pos;
}

static bool skb__is_at_first_line(const skb_editor_t* editor, skb__editor_text_position_t edit_pos)
{
	return edit_pos.paragraph_idx == 0 && edit_pos.line_idx == 0;
}

static bool skb__is_at_last_line(const skb_editor_t* editor, skb__editor_text_position_t edit_pos)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	const int32_t lines_count = skb_layout_get_lines_count(paragraph->layout);
	return (edit_pos.paragraph_idx == editor->paragraphs_count - 1) && (edit_pos.line_idx == lines_count - 1);
}

static bool skb__is_at_start_of_line(const skb_editor_t* editor, skb__editor_text_position_t edit_pos)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const skb_layout_line_t* line = &lines[edit_pos.line_idx];
	return edit_pos.paragraph_offset == line->text_range.start;
}

static bool skb__is_past_end_of_line(const skb_editor_t* editor, skb__editor_text_position_t edit_pos)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const skb_layout_line_t* line = &lines[edit_pos.line_idx];
	return edit_pos.paragraph_offset > line->last_grapheme_offset;
}

static bool skb__is_rtl(const skb_editor_t* editor, skb__editor_text_position_t edit_pos, uint8_t affinity)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const skb_layout_line_t* line = &lines[edit_pos.line_idx];
	const bool layout_is_rtl = skb_is_rtl(skb_layout_get_resolved_direction(paragraph->layout));

	if (affinity == SKB_AFFINITY_EOL || affinity == SKB_AFFINITY_SOL)
		return layout_is_rtl;

	if (edit_pos.paragraph_offset > line->last_grapheme_offset)
		return layout_is_rtl;

	const int32_t text_count = skb_layout_get_text_count(paragraph->layout);

	if (text_count == 0)
		return layout_is_rtl;

	const skb_text_property_t* text_props = skb_layout_get_text_properties(paragraph->layout);

	return skb_is_rtl(text_props[edit_pos.paragraph_offset].direction);
}

static bool skb__are_on_same_line(skb__editor_text_position_t a, skb__editor_text_position_t b)
{
	return a.paragraph_idx == b.paragraph_idx && a.line_idx == b.line_idx;
}

static skb_text_position_t skb__advance_forward(const skb_editor_t* editor, skb__editor_text_position_t cur_edit_pos, uint8_t cur_affinity)
{
	skb__editor_text_position_t next_edit_pos = skb__get_next_grapheme_pos(editor, cur_edit_pos);

	bool is_next_last_line = skb__is_at_last_line(editor, next_edit_pos);

	bool cur_is_rtl = skb__is_rtl(editor, cur_edit_pos, cur_affinity);
	bool next_is_rtl = skb__is_rtl(editor, next_edit_pos, SKB_AFFINITY_TRAILING);

	// Do not add extra stop at the end of the line on intermediate lines.
	const bool stop_at_dir_change = editor->params.caret_mode == SKB_CARET_MODE_SKRIBIDI &&  (is_next_last_line || skb__are_on_same_line(cur_edit_pos, next_edit_pos));

	uint8_t affinity = SKB_AFFINITY_TRAILING;
	bool check_eol = true;

	if (stop_at_dir_change && cur_is_rtl != next_is_rtl) {
		// Text direction change.
		if (cur_affinity == SKB_AFFINITY_LEADING || cur_affinity == SKB_AFFINITY_EOL) {
			// Switch over to the next character.
			affinity = SKB_AFFINITY_TRAILING;
			cur_edit_pos = next_edit_pos;
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
			cur_edit_pos = next_edit_pos;

			// Update next
			next_edit_pos = skb__get_next_grapheme_pos(editor, cur_edit_pos);
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
			cur_edit_pos = next_edit_pos;
		}
	}

	if (check_eol) {
		if (skb__is_at_last_line(editor, cur_edit_pos) && skb__is_past_end_of_line(editor, cur_edit_pos)) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];
			const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
			const skb_layout_line_t* line = &lines[cur_edit_pos.line_idx];
			affinity = SKB_AFFINITY_EOL;
			cur_edit_pos.paragraph_offset = line->last_grapheme_offset;
		}
	}

	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];
	return (skb_text_position_t) {
		.offset = paragraph->text_start_offset + cur_edit_pos.paragraph_offset,
		.affinity = affinity,
	};
}

static skb_text_position_t skb__advance_backward(const skb_editor_t* editor, skb__editor_text_position_t cur_edit_pos, uint8_t cur_affinity)
{
	skb__editor_text_position_t prev_edit_pos = skb__get_prev_grapheme_pos(editor, cur_edit_pos);

	bool cur_is_rtl = skb__is_rtl(editor, cur_edit_pos, cur_affinity);
	bool prev_is_rtl = skb__is_rtl(editor, prev_edit_pos, SKB_AFFINITY_TRAILING);

	// Do not add extra stop at the end of the line on intermediate lines.
	const bool stop_at_dir_change = editor->params.caret_mode == SKB_CARET_MODE_SKRIBIDI && skb__are_on_same_line(cur_edit_pos, prev_edit_pos);

	uint8_t affinity = SKB_AFFINITY_TRAILING;

	if (stop_at_dir_change && prev_is_rtl != cur_is_rtl) {
		if (cur_affinity == SKB_AFFINITY_EOL) {
			// At the end of line, but the direction is changing. Move to leading edge first.
			affinity = SKB_AFFINITY_LEADING;
		} else if (cur_affinity == SKB_AFFINITY_LEADING) {
			// On a leading edge, and the direction will change in next character. Move to trailing edge first.
			affinity = SKB_AFFINITY_TRAILING;
		} else {
			// On a trailing edge, and the direction will change in next character.
			// Switch over to the leading edge of the previous character.
			affinity = SKB_AFFINITY_LEADING;
			cur_edit_pos = prev_edit_pos;
		}
	} else {
		if (cur_affinity == SKB_AFFINITY_LEADING || (!skb__is_at_start_of_line(editor, cur_edit_pos) && cur_affinity == SKB_AFFINITY_EOL)) {
			// On leading edge, normalize the index to next trailing location.
			// Special handling for empty lines to avoid extra stop.
			affinity = SKB_AFFINITY_TRAILING;
		} else {
			// On a trailing edge, advance to the next character.
			affinity = SKB_AFFINITY_TRAILING;
			cur_edit_pos = prev_edit_pos;
		}
	}

	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];

	return (skb_text_position_t) {
		.offset = paragraph->text_start_offset + cur_edit_pos.paragraph_offset,
		.affinity = affinity,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_next_char(const skb_editor_t* editor, skb_text_position_t pos)
{
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	if (skb_is_rtl(skb_layout_get_resolved_direction(paragraph->layout)))
		return skb__advance_backward(editor, edit_pos, pos.affinity);
	return skb__advance_forward(editor, edit_pos, pos.affinity);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_prev_char(const skb_editor_t* editor, skb_text_position_t pos)
{
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	if (skb_is_rtl(skb_layout_get_resolved_direction(paragraph->layout)))
		return skb__advance_forward(editor, edit_pos, pos.affinity);
	return skb__advance_backward(editor, edit_pos, pos.affinity);
}

static skb_text_position_t skb__advance_word_forward(const skb_editor_t* editor, skb__editor_text_position_t cur_edit_pos)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];

	int32_t offset = cur_edit_pos.paragraph_offset;
	const int32_t text_count = skb_layout_get_text_count(paragraph->layout);
	const skb_text_property_t* text_props = skb_layout_get_text_properties(paragraph->layout);

	if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
		// skip whitespace and punctuation at start.
		while (offset < text_count && text_props[offset].flags & (SKB_TEXT_PROP_WHITESPACE | SKB_TEXT_PROP_PUNCTUATION))
			offset++;

		// Stop at the end of the word.
		while (offset < text_count) {
			if (text_props[offset].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_next_grapheme_offset(paragraph->layout, offset);
				offset = next_offset;
				break;
			}
			offset++;
		}
	} else {
		// Stop after the white space at the end of the word.
		while (offset < text_count) {
			if (text_props[offset].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_next_grapheme_offset(paragraph->layout, offset);
				if (!(text_props[next_offset].flags & SKB_TEXT_PROP_WHITESPACE)) {
					offset = next_offset;
					break;
				}
			}
			offset++;
		}
	}

	if (offset == text_count) {
		if (cur_edit_pos.paragraph_idx + 1 < editor->paragraphs_count) {
			cur_edit_pos.paragraph_idx++;
			paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];
			offset = 0; // Beginning of layout
		} else {
			offset = skb_layout_align_grapheme_offset(paragraph->layout, text_count-1);
			return (skb_text_position_t) {
				.offset = paragraph->text_start_offset + offset,
				.affinity = SKB_AFFINITY_EOL,
			};
		}
	}

	return (skb_text_position_t) {
		.offset = paragraph->text_start_offset + offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

static skb_text_position_t skb__advance_word_backward(const skb_editor_t* editor, skb__editor_text_position_t cur_edit_pos)
{
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];

	int32_t offset = cur_edit_pos.paragraph_offset;

	const int32_t text_count = skb_layout_get_text_count(paragraph->layout);
	const skb_text_property_t* text_props = skb_layout_get_text_properties(paragraph->layout);

	if (offset == 0) {
		if (cur_edit_pos.paragraph_idx - 1 >= 0) {
			// Goto previous paragraph
			cur_edit_pos.paragraph_idx--;
			paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];
			offset = skb_layout_align_grapheme_offset(paragraph->layout, text_count-1); // Last grapheme of the paragraph.
			return (skb_text_position_t) {
				.offset = paragraph->text_start_offset + offset,
				.affinity = SKB_AFFINITY_TRAILING,
			};
		}
		offset = 0;
		return (skb_text_position_t) {
			.offset = paragraph->text_start_offset + offset,
			.affinity = SKB_AFFINITY_SOL,
		};
	}

	if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
		// skip whitespace and punctuation at start.
		while (offset > 0 && text_props[offset-1].flags & (SKB_TEXT_PROP_WHITESPACE | SKB_TEXT_PROP_PUNCTUATION))
			offset--;
		// Stop at the beginning of the word.
		offset = skb_layout_prev_grapheme_offset(paragraph->layout, offset);
		while (offset > 0) {
			if (text_props[offset-1].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_next_grapheme_offset(paragraph->layout, offset-1);
				offset = next_offset;
				break;
			}
			offset--;
		}
	} else {
		// Stop at the beginning of a word (the exact same logic as moving forward).
		offset = skb_layout_prev_grapheme_offset(paragraph->layout, offset);
		while (offset > 0) {
			if (text_props[offset-1].flags & SKB_TEXT_PROP_WORD_BREAK) {
				int32_t next_offset = skb_layout_next_grapheme_offset(paragraph->layout, offset-1);
				if (!(text_props[next_offset].flags & SKB_TEXT_PROP_WHITESPACE)) {
					offset = next_offset;
					break;
				}
			}
			offset--;
		}
	}

	return (skb_text_position_t) {
		.offset = paragraph->text_start_offset + offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_next_word(const skb_editor_t* editor, skb_text_position_t pos)
{
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_ADJUST_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	if (skb_is_rtl(skb_layout_get_resolved_direction(paragraph->layout)))
		return skb__advance_word_backward(editor, edit_pos);
	return skb__advance_word_forward(editor, edit_pos);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_prev_word(const skb_editor_t* editor, skb_text_position_t pos)
{
	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_ADJUST_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	if (skb_is_rtl(skb_layout_get_resolved_direction(paragraph->layout)))
		return skb__advance_word_forward(editor, edit_pos);
	return skb__advance_word_backward(editor, edit_pos);
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_next_line(const skb_editor_t* editor, skb_text_position_t pos, float preferred_x)
{
	skb__editor_text_position_t cur_edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];

	if (skb__is_at_last_line(editor, cur_edit_pos)) {
		// Goto end of the text
		return skb_editor_get_line_end_at(editor, pos);
	}

	const int32_t lines_count = skb_layout_get_lines_count(paragraph->layout);

	// Goto next line
	if (cur_edit_pos.line_idx + 1 >= lines_count) {
		// End of current paragraph, goto first line of next paragraph.
		assert(cur_edit_pos.paragraph_idx + 1 < editor->paragraphs_count); // should have been handled by skb__is_at_last_line() above.
		cur_edit_pos.paragraph_idx++;
		paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];
		cur_edit_pos.line_idx = 0;
	} else {
		cur_edit_pos.line_idx++;
	}

	skb_text_position_t hit_pos = skb_layout_hit_test_at_line(paragraph->layout, SKB_MOVEMENT_CARET, cur_edit_pos.line_idx, preferred_x);
	hit_pos.offset += paragraph->text_start_offset;

	return hit_pos;
}

// TODO: should we expose this?
skb_text_position_t skb_editor_move_to_prev_line(const skb_editor_t* editor, skb_text_position_t pos, float preferred_x)
{
	skb__editor_text_position_t cur_edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];

	if (skb__is_at_first_line(editor, cur_edit_pos)) {
		// Goto beginning of the text
		return skb_editor_get_line_start_at(editor, pos);
	}

	int32_t lines_count = skb_layout_get_lines_count(paragraph->layout);

	// Goto prev line
	if (cur_edit_pos.line_idx - 1 < 0) {
		// Beginning of current paragraph, goto last line of prev paragraph.

		assert(cur_edit_pos.paragraph_idx - 1 >= 0); // should have been handled by skb__is_at_first_line() above.
		cur_edit_pos.paragraph_idx--;
		paragraph = &editor->paragraphs[cur_edit_pos.paragraph_idx];
		lines_count = skb_layout_get_lines_count(paragraph->layout);
		cur_edit_pos.line_idx = lines_count - 1;
	} else {
		cur_edit_pos.line_idx--;
	}

	skb_text_position_t hit_pos = skb_layout_hit_test_at_line(paragraph->layout, SKB_MOVEMENT_CARET, cur_edit_pos.line_idx, preferred_x);
	hit_pos.offset += paragraph->text_start_offset;

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
	if (editor->paragraphs_count == 0) {
		return skb__editor_get_document_start(editor);
	}
	
	const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[editor->paragraphs_count - 1];
	skb_text_position_t result = {
		.offset = last_paragraph->text_start_offset + last_paragraph->text_count,
		.affinity = SKB_AFFINITY_EOL
	};
	return result;
}

int32_t skb_editor_get_selection_text_utf8_count(const skb_editor_t* editor, skb_text_selection_t selection)
{
	assert(editor);

	skb__editor_text_range_t sel_range = skb__get_sanitized_range(editor, selection);

	if (sel_range.start.paragraph_idx == sel_range.end.paragraph_idx) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		int32_t count = sel_range.end.text_offset - sel_range.start.text_offset;
		return skb_utf32_to_utf8_count(paragraph->text + sel_range.start.paragraph_offset, count);
	} else {
		int32_t count = 0;
		// First line
		const skb__editor_paragraph_t* first_paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		const int32_t first_line_count = first_paragraph->text_count - first_paragraph->text_start_offset;
		count += skb_utf32_to_utf8_count(first_paragraph->text + sel_range.start.paragraph_offset, first_line_count);
		// Middle lines
		for (int32_t line_idx = sel_range.start.paragraph_idx + 1; line_idx < sel_range.end.paragraph_idx; line_idx++) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[line_idx];
			count += skb_utf32_to_utf8_count(paragraph->text, paragraph->text_count);
		}
		// Last line
		const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[sel_range.end.paragraph_idx];
		const int32_t last_line_count = skb_mini(last_paragraph->text_start_offset + 1, last_paragraph->text_count);
		count += skb_utf32_to_utf8_count(last_paragraph->text, last_line_count);

		return count;
	}
}

int32_t skb_editor_get_selection_text_utf8(const skb_editor_t* editor, skb_text_selection_t selection, char* buf, int32_t buf_cap)
{
	assert(editor);

	skb__editor_text_range_t sel_range = skb__get_sanitized_range(editor, selection);

	if (sel_range.start.paragraph_idx == sel_range.end.paragraph_idx) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		int32_t count = sel_range.end.text_offset - sel_range.start.text_offset;
		return skb_utf32_to_utf8(paragraph->text + sel_range.start.paragraph_offset, count, buf, buf_cap);
	} else {
		int32_t count = 0;
		// First line
		const skb__editor_paragraph_t* first_paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		const int32_t first_line_count = first_paragraph->text_count - first_paragraph->text_start_offset;
		count += skb_utf32_to_utf8(first_paragraph->text + sel_range.start.paragraph_offset, first_line_count, buf + count, buf_cap - count);
		// Middle lines
		for (int32_t line_idx = sel_range.start.paragraph_idx + 1; line_idx < sel_range.end.paragraph_idx; line_idx++) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[line_idx];
			count += skb_utf32_to_utf8(paragraph->text, paragraph->text_count, buf + count, buf_cap - count);
		}
		// Last line
		const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[sel_range.end.paragraph_idx];
		const int32_t last_line_count = skb_mini(last_paragraph->text_start_offset + 1, last_paragraph->text_count);
		count += skb_utf32_to_utf8(last_paragraph->text, last_line_count, buf + count, buf_cap - count);

		return count;
	}
}

int32_t skb_editor_get_selection_text_utf32_count(const skb_editor_t* editor, skb_text_selection_t selection)
{
	assert(editor);

	skb__editor_text_range_t sel_range = skb__get_sanitized_range(editor, selection);

	if (sel_range.start.paragraph_idx == sel_range.end.paragraph_idx) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		return sel_range.end.text_offset - sel_range.start.text_offset;
	} else {
		int32_t count = 0;
		// First line
		const skb__editor_paragraph_t* first_paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		const int32_t first_line_count = first_paragraph->text_count - first_paragraph->text_start_offset;
		count += first_line_count;
		// Middle lines
		for (int32_t line_idx = sel_range.start.paragraph_idx + 1; line_idx <= sel_range.end.paragraph_idx - 1; line_idx++) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[line_idx];
			count += count;
		}
		// Last line
		const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[sel_range.end.paragraph_idx];
		const int32_t last_line_count = skb_mini(last_paragraph->text_start_offset + 1, last_paragraph->text_count);
		count += last_line_count;

		return count;
	}
}

int32_t skb_editor_get_selection_text_utf32(const skb_editor_t* editor, skb_text_selection_t selection, uint32_t* buf, int32_t buf_cap)
{
	assert(editor);

	skb__editor_text_range_t sel_range = skb__get_sanitized_range(editor, selection);

	if (sel_range.start.paragraph_idx == sel_range.end.paragraph_idx) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		int32_t count = sel_range.end.text_offset - sel_range.start.text_offset;
		return skb__copy_utf32(paragraph->text + sel_range.start.paragraph_offset, count, buf, buf_cap);
	} else {
		int32_t count = 0;
		// First line
		const skb__editor_paragraph_t* first_paragraph = &editor->paragraphs[sel_range.start.paragraph_idx];
		const int32_t first_line_count = first_paragraph->text_count - first_paragraph->text_start_offset;
		count += skb__copy_utf32(first_paragraph->text + sel_range.start.paragraph_offset, first_line_count, buf + count, buf_cap - count);
		// Middle lines
		for (int32_t line_idx = sel_range.start.paragraph_idx + 1; line_idx <= sel_range.end.paragraph_idx - 1; line_idx++) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[line_idx];
			count += skb__copy_utf32(paragraph->text, paragraph->text_count, buf + count, buf_cap - count);
		}
		// Last line
		const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[sel_range.end.paragraph_idx];
		const int32_t last_line_count = skb_mini(last_paragraph->text_start_offset + 1, last_paragraph->text_count);
		count += skb__copy_utf32(last_paragraph->text, last_line_count, buf + count, buf_cap - count);

		return count;
	}
}

skb_text_selection_t skb_editor_get_current_selection(skb_editor_t* editor)
{
	assert(editor);

	if (editor->composition_text_count > 0) {
		return editor->composition_selection;
	}

	return editor->selection;
}

void skb_editor_select_all(skb_editor_t* editor)
{
	assert(editor);

	if (editor->paragraphs_count > 0) {
		editor->selection.start_pos = (skb_text_position_t) { .offset = 0, .affinity = SKB_AFFINITY_SOL };
		const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[editor->paragraphs_count - 1];
		const int32_t last_text_count = skb_layout_get_text_count(last_paragraph->layout);
		const int32_t last_grapheme_offset = skb_layout_align_grapheme_offset(last_paragraph->layout, last_text_count-1);
		editor->selection.end_pos = (skb_text_position_t) { .offset = last_paragraph->text_start_offset + last_grapheme_offset, .affinity = SKB_AFFINITY_EOL };
	} else {
		editor->selection.start_pos = (skb_text_position_t) { 0 };
		editor->selection.end_pos = (skb_text_position_t) { 0 };
	}
}

void skb_editor_select_none(skb_editor_t* editor)
{
	// Clear selection, but retain current caret position.
	editor->selection.start_pos = editor->selection.end_pos;
}

void skb_editor_select(skb_editor_t* editor, skb_text_selection_t selection)
{
	assert(editor);

	editor->selection = selection;
}

skb_text_position_t skb_editor_hit_test(const skb_editor_t* editor, skb_movement_type_t type, float hit_x, float hit_y)
{
	assert(editor);
	assert(editor->paragraphs_count > 0);

	const skb__editor_paragraph_t* hit_paragraph = NULL;
	int32_t hit_line_idx = SKB_INVALID_INDEX;

	const skb__editor_paragraph_t* first_paragraph = &editor->paragraphs[0];
	const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[editor->paragraphs_count - 1];

	const skb_rect2_t first_paragraph_bounds = skb_layout_get_bounds(first_paragraph->layout);
	const skb_rect2_t last_paragraph_bounds = skb_layout_get_bounds(last_paragraph->layout);

	const float first_top_y = first_paragraph->y + first_paragraph_bounds.y;

	const float last_top_y = last_paragraph->y + last_paragraph_bounds.y;
	const float last_bot_y = last_paragraph->y + last_paragraph_bounds.y + last_paragraph_bounds.height;

	if (hit_y < first_top_y) {
		hit_paragraph = first_paragraph;
		hit_line_idx = 0;
	} else if (hit_y >= last_bot_y) {
		hit_paragraph = last_paragraph;
		hit_line_idx = skb_layout_get_lines_count(last_paragraph->layout) - 1;
	} else {
		for (int32_t i = 0; i < editor->paragraphs_count; i++) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
			const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
			const int32_t lines_count = skb_layout_get_lines_count(paragraph->layout);
			for (int32_t j = 0; j < lines_count; j++) {
				const skb_layout_line_t* line = &lines[j];
				const float bot_y = paragraph->y + line->bounds.y + -line->ascender + line->descender;
				if (hit_y < bot_y) {
					hit_line_idx = j;
					break;
				}
			}
			if (hit_line_idx != SKB_INVALID_INDEX) {
				hit_paragraph = paragraph;
				break;
			}
		}
		if (hit_line_idx == SKB_INVALID_INDEX) {
			hit_paragraph = last_paragraph;
			hit_line_idx = skb_layout_get_lines_count(last_paragraph->layout) - 1;
		}
	}

	skb_text_position_t pos = skb_layout_hit_test_at_line(hit_paragraph->layout, type, hit_line_idx, hit_x);
	pos.offset += hit_paragraph->text_start_offset;

	return pos;
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

	static const double double_click_duration = 0.4;
	if (editor->paragraphs_count <= 0)
		return;

	const double dt = time - editor->last_click_time;

	if (dt < double_click_duration)
		editor->click_count++;
	else
		editor->click_count = 1;

	if (editor->click_count > 3)
		editor->click_count = 1;

	editor->last_click_time = time;

	skb_text_position_t hit_caret = skb_editor_hit_test(editor, SKB_MOVEMENT_CARET, x, y);

	if (mods & SKB_MOD_SHIFT) {
		// Shift click makes selection from current start pos to the new hit pos.
		editor->selection.end_pos = hit_caret;
		editor->drag_mode = SKB_DRAG_CHAR;
	} else {
		if (editor->click_count == 1) {
			editor->selection.end_pos = hit_caret;
			editor->selection.start_pos = editor->selection.end_pos;
			editor->drag_mode = SKB_DRAG_CHAR;
		} else if (editor->click_count == 2) {
			editor->selection.start_pos = skb_editor_get_word_start_at(editor, hit_caret);
			editor->selection.end_pos = skb_editor_get_word_end_at(editor, hit_caret);
			editor->drag_mode = SKB_DRAG_WORD;
		} else if (editor->click_count == 3) {
			editor->selection.start_pos = skb_editor_get_line_start_at(editor, hit_caret);
			editor->selection.end_pos = skb_editor_get_line_end_at(editor, hit_caret);
			editor->drag_mode = SKB_DRAG_LINE;
		}
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
		const skb__editor_text_position_t sel_start = skb__get_sanitized_position(editor, sel_start_pos, SKB_SANITIZE_ADJUST_AFFINITY);
		const skb__editor_text_position_t sel_end = skb__get_sanitized_position(editor, sel_end_pos, SKB_SANITIZE_ADJUST_AFFINITY);

		const skb__editor_text_position_t initial_start = skb__get_sanitized_position(editor, editor->drag_initial_selection.start_pos, SKB_SANITIZE_ADJUST_AFFINITY);
		const skb__editor_text_position_t initial_end = skb__get_sanitized_position(editor, editor->drag_initial_selection.end_pos, SKB_SANITIZE_ADJUST_AFFINITY);

		if (sel_start.text_offset < initial_start.text_offset) {
			// The selection got expanded before the initial selection range start.
			editor->selection.start_pos = sel_start_pos;
			editor->selection.end_pos = editor->drag_initial_selection.end_pos;
		} else if (sel_end.text_offset > initial_end.text_offset) {
			// The selection got expanded past the initial selection range end.
			editor->selection.start_pos = editor->drag_initial_selection.start_pos;
			editor->selection.end_pos = sel_end_pos;
		} else {
			// Restore
			editor->selection.start_pos = editor->drag_initial_selection.start_pos;
			editor->selection.end_pos = editor->drag_initial_selection.end_pos;
		}

		editor->preferred_x = -1.f; // reset preferred.
	}
}


static void skb__set_line_combined_text(skb__editor_paragraph_t* paragraph, const uint32_t* a, int32_t a_count, const uint32_t* b, int32_t b_count, const uint32_t* c, int32_t c_count)
{
	paragraph->text_count = a_count + b_count + c_count;
	if (paragraph->text_count > 0) {
		SKB_ARRAY_RESERVE(paragraph->text, paragraph->text_count);
		memcpy(paragraph->text, a, a_count * sizeof(uint32_t));
		memcpy(paragraph->text + a_count, b, b_count * sizeof(uint32_t));
		memcpy(paragraph->text + a_count + b_count, c, c_count * sizeof(uint32_t));
	}
}

static void skb__replace_range(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb__editor_text_position_t start, skb__editor_text_position_t end, const uint32_t* utf32, int32_t utf32_len)
{
	int32_t new_paragraph_count = 0;
	skb_range_t* new_paragraph_ranges = skb__split_text_into_paragraphs(temp_alloc, utf32, utf32_len, &new_paragraph_count);
	assert(new_paragraph_count > 0); // We assume that even for empty input text there's one paragraph created.

	// Save start and end edit lines from being freed (these may be the same).
	uint32_t* start_paragraph_text = editor->paragraphs[start.paragraph_idx].text;
	uint32_t* end_paragraph_text = editor->paragraphs[end.paragraph_idx].text;
	const int32_t end_paragraph_text_count = editor->paragraphs[end.paragraph_idx].text_count;

	editor->paragraphs[start.paragraph_idx].text = NULL;
	editor->paragraphs[start.paragraph_idx].text_count = 0;
	editor->paragraphs[end.paragraph_idx].text = NULL;
	editor->paragraphs[end.paragraph_idx].text_count = 0;

	// Free lines that we'll remove or rebuild.
	for (int32_t i = start.paragraph_idx; i <= end.paragraph_idx; i++) {
		skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
		skb_free(paragraph->text);
		skb_layout_destroy(paragraph->layout);
		memset(paragraph, 0, sizeof(skb__editor_paragraph_t));
	}

	// Allocate new lines or prune.
	const int32_t selection_paragraph_count = (end.paragraph_idx + 1) - start.paragraph_idx;
	const int32_t new_paragraphs_count = skb_maxi(0, editor->paragraphs_count - selection_paragraph_count + new_paragraph_count);
	const int32_t old_paragraphs_count = editor->paragraphs_count;
	SKB_ARRAY_RESERVE(editor->paragraphs, new_paragraphs_count);
	editor->paragraphs_count = new_paragraphs_count;

	// Move tail of the text to create space for the lines to be inserted, accounting for the removed lines.
	const int32_t old_tail_idx = end.paragraph_idx + 1; // range_end is the last one to remove.
	const int32_t tail_count = old_paragraphs_count - old_tail_idx;
	const int32_t new_tail_idx = start.paragraph_idx + new_paragraph_count;
	if (new_tail_idx != old_tail_idx && tail_count > 0)
		memmove(editor->paragraphs + new_tail_idx, editor->paragraphs + old_tail_idx, tail_count * sizeof(skb__editor_paragraph_t));

	// Create new lines.
	const int32_t first_new_paragraph_idx = start.paragraph_idx;
	int32_t paragraph_idx = first_new_paragraph_idx;

	const int32_t start_paragraph_copy_count = start.paragraph_offset;
	const int32_t end_paragraph_copy_offset = end.paragraph_offset;
	const int32_t end_paragraph_copy_count = skb_maxi(0, end_paragraph_text_count - end_paragraph_copy_offset);

	skb__editor_paragraph_t* last_paragraph = NULL;
	int32_t last_paragraph_offset = 0;

	if (new_paragraph_count == 1) {
		skb__editor_paragraph_t* new_paragraph = &editor->paragraphs[paragraph_idx++];
		memset(new_paragraph, 0, sizeof(skb__editor_paragraph_t));
		skb__set_line_combined_text(new_paragraph,
			start_paragraph_text, start_paragraph_copy_count,
			utf32, utf32_len,
			end_paragraph_text + end_paragraph_copy_offset, end_paragraph_copy_count);
		// Keep track of last paragraph and last codepoint inserted for caret positioning.
		last_paragraph = new_paragraph;
		last_paragraph_offset = start_paragraph_copy_count + utf32_len - 1;
	} else if (new_paragraph_count > 0) {
		// Start
		const skb_range_t start_paragraph_range = new_paragraph_ranges[0];
		skb__editor_paragraph_t* new_start_paragraph = &editor->paragraphs[paragraph_idx++];
		memset(new_start_paragraph, 0, sizeof(skb__editor_paragraph_t));
		skb__set_line_combined_text(new_start_paragraph,
			start_paragraph_text, start_paragraph_copy_count,
			utf32 + start_paragraph_range.start, start_paragraph_range.end - start_paragraph_range.start,
			NULL, 0);

		// Middle
		for (int32_t i = 1; i < new_paragraph_count - 1; i++) {
			const skb_range_t paragraph_range = new_paragraph_ranges[i];
			skb__editor_paragraph_t* new_paragraph = &editor->paragraphs[paragraph_idx++];
			memset(new_paragraph, 0, sizeof(skb__editor_paragraph_t));
			new_paragraph->text_count = paragraph_range.end - paragraph_range.start;
			if (new_paragraph->text_count > 0) {
				SKB_ARRAY_RESERVE(new_paragraph->text, new_paragraph->text_count);
				memcpy(new_paragraph->text, utf32 + paragraph_range.start, new_paragraph->text_count * sizeof(uint32_t));
			}
		}

		// End
		const skb_range_t end_paragraph_range = new_paragraph_ranges[new_paragraph_count - 1];
		skb__editor_paragraph_t* new_end_paragraph = &editor->paragraphs[paragraph_idx++];
		memset(new_end_paragraph, 0, sizeof(skb__editor_paragraph_t));
		skb__set_line_combined_text(new_end_paragraph,
			utf32 + end_paragraph_range.start, end_paragraph_range.end - end_paragraph_range.start,
			end_paragraph_text + end_paragraph_copy_offset, end_paragraph_copy_count,
			NULL, 0);

		// Keep track of last paragraph and last codepoint inserted for caret positioning.
		last_paragraph = new_end_paragraph;
		last_paragraph_offset = end_paragraph_range.end - end_paragraph_range.start - 1;
	}


	// Update start offsets.
	int32_t start_offset = (first_new_paragraph_idx > 0) ? (editor->paragraphs[first_new_paragraph_idx - 1].text_start_offset + editor->paragraphs[first_new_paragraph_idx - 1].text_count) : 0;
	for (int32_t i = first_new_paragraph_idx; i < editor->paragraphs_count; i++) {
		editor->paragraphs[i].text_start_offset = start_offset;
		start_offset += editor->paragraphs[i].text_count;
	}

	// Free old lines
	skb_free(start_paragraph_text);
	if (end_paragraph_text != start_paragraph_text)
		skb_free(end_paragraph_text);


	// Find offset of the last grapheme, this is needed to place the caret on the leading edge of the last grapheme.
	// We use leading edge of last grapheme so that the caret stays in context when typing at the direction change of a bidi text.
	if (last_paragraph->text_count > 0) {
		char* grapheme_breaks = SKB_TEMP_ALLOC(temp_alloc, char, last_paragraph->text_count);
		set_graphemebreaks_utf32(last_paragraph->text, last_paragraph->text_count, editor->params.layout_params.lang, grapheme_breaks);

		// Find beginning of the last grapheme.
		while ((last_paragraph_offset - 1) >= 0 && grapheme_breaks[last_paragraph_offset - 1] != GRAPHEMEBREAK_BREAK)
			last_paragraph_offset--;

		SKB_TEMP_FREE(temp_alloc, grapheme_breaks);
	}

	// Set selection to the end of the inserted text.
	if (last_paragraph_offset < 0) {
		// This can happen when we delete the first character.
		editor->selection.start_pos = (skb_text_position_t){
			.offset = last_paragraph->text_start_offset,
			.affinity = SKB_AFFINITY_TRAILING
		};
	} else {
		editor->selection.start_pos = (skb_text_position_t){
			.offset = last_paragraph->text_start_offset + last_paragraph_offset,
			.affinity = SKB_AFFINITY_LEADING
		};
	}

	editor->selection.end_pos = editor->selection.start_pos;
	editor->preferred_x = -1.f; // reset preferred.

	SKB_TEMP_FREE(temp_alloc, new_paragraph_ranges);
}

static void skb__replace_selection(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len)
{
	// Insert pos gets clamped to the layout text size.
	skb__editor_text_range_t sel_range = skb__get_sanitized_range(editor, editor->selection);

	skb__capture_undo(editor, sel_range.start, sel_range.end, utf32, utf32_len);

	skb__replace_range(editor, temp_alloc, sel_range.start, sel_range.end, utf32, utf32_len);
}


static int32_t skb__get_selection_text(const skb_editor_t* editor, skb__editor_text_position_t start, skb__editor_text_position_t end, uint32_t* buf, int32_t buf_cap)
{
	assert(editor);
	if (start.paragraph_idx == end.paragraph_idx) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[start.paragraph_idx];
		int32_t count = end.text_offset - start.text_offset;
		if (buf)
			return skb__copy_utf32(paragraph->text + start.paragraph_offset, count, buf, buf_cap);
		return count;
	}

	int32_t count = 0;
	// First line
	const skb__editor_paragraph_t* first_paragraph = &editor->paragraphs[start.paragraph_idx];
	const int32_t first_line_count = first_paragraph->text_count - first_paragraph->text_start_offset;
	if (buf)
		count += skb__copy_utf32(first_paragraph->text + start.paragraph_offset, first_line_count, buf + count, buf_cap - count);
	else
		count += first_line_count;
	// Middle lines
	for (int32_t line_idx = start.paragraph_idx + 1; line_idx <= end.paragraph_idx - 1; line_idx++) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[line_idx];
		if (buf)
			count += skb__copy_utf32(paragraph->text, paragraph->text_count, buf + count, buf_cap - count);
		else
			count += paragraph->text_count;
	}
	// Last line
	const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[end.paragraph_idx];
	const int32_t last_line_count = skb_mini(last_paragraph->text_start_offset + 1, last_paragraph->text_count);
	if (buf)
		count += skb__copy_utf32(last_paragraph->text, last_line_count, buf + count, buf_cap - count);
	else
		count += last_line_count;

	return count;
}

static void skb__reset_undo(skb_editor_t* editor)
{
	for (int32_t i = 0; i < editor->undo_stack_count; i++) {
		skb_free(editor->undo_stack[i].inserted_text);
		skb_free(editor->undo_stack[i].removed_text);
		memset(&editor->undo_stack[i], 0, sizeof(skb__editor_undo_state_t));
	}
	editor->undo_stack_count = 0;
	editor->undo_stack_head = -1;
}

static void skb__capture_undo(skb_editor_t* editor, skb__editor_text_position_t start, skb__editor_text_position_t end, const uint32_t* utf32, int32_t utf32_len)
{
	if (editor->params.max_undo_levels < 0)
		return;

	// Check if we can amend the last undo state.
	if (editor->undo_stack_head != -1 && editor->allow_append_undo) {
		skb__editor_undo_state_t* prev_undo_state = &editor->undo_stack[editor->undo_stack_head];
		// If there's no text to remove, and we're inserting at the end of the previous undo insert.
		if (start.text_offset == end.text_offset && end.text_offset == prev_undo_state->inserted_range.end) {
			prev_undo_state->inserted_text = skb_realloc(prev_undo_state->inserted_text, (prev_undo_state->inserted_text_count + utf32_len) * sizeof(uint32_t));
			assert(prev_undo_state->inserted_text);
			skb__copy_utf32(utf32, utf32_len, prev_undo_state->inserted_text + prev_undo_state->inserted_text_count, utf32_len);
			prev_undo_state->inserted_text_count += utf32_len;
			prev_undo_state->inserted_range.end += utf32_len;
			return;
		}
	}

	// Delete states that cannot be reached anymore
	for (int32_t i = editor->undo_stack_head + 1; i < editor->undo_stack_count; i++) {
		skb_free(editor->undo_stack[i].inserted_text);
		skb_free(editor->undo_stack[i].removed_text);
		memset(&editor->undo_stack[i], 0, sizeof(skb__editor_undo_state_t));
	}
	editor->undo_stack_count = editor->undo_stack_head + 1;

	// Keep the undo stack size under control.
	if ((editor->undo_stack_count + 1) > editor->params.max_undo_levels) {
		for (int32_t i = 0; i < editor->undo_stack_count - 1; i++)
			editor->undo_stack[i] = editor->undo_stack[i+1];
		editor->undo_stack_count--;
		if (editor->undo_stack_head > 0)
			editor->undo_stack_head--;
	}

	// Capture new undo state.
	SKB_ARRAY_RESERVE(editor->undo_stack, editor->undo_stack_count + 1);
	editor->undo_stack_head = editor->undo_stack_count++;
	skb__editor_undo_state_t* undo_state = &editor->undo_stack[editor->undo_stack_head];

	// Capture the text we'reabout to remove.
	undo_state->removed_range.start = start.text_offset;
	undo_state->removed_range.end = end.text_offset;
	undo_state->removed_text_count = skb__get_selection_text(editor, start, end, NULL, 0);
	if (undo_state->removed_text_count > 0) {
		undo_state->removed_text = skb_malloc(undo_state->removed_text_count * sizeof(uint32_t));
		assert(undo_state->removed_text);
		skb__get_selection_text(editor, start, end, undo_state->removed_text, undo_state->removed_text_count);
	}

	// Capture the text we're about to insert.
	undo_state->inserted_range.start = start.text_offset;
	undo_state->inserted_range.end = start.text_offset + utf32_len;
	if (utf32_len > 0) {
		undo_state->inserted_text_count = utf32_len;
		undo_state->inserted_text = skb_malloc(undo_state->inserted_text_count * sizeof(uint32_t));
		assert(undo_state->inserted_text);
		skb__copy_utf32(utf32, utf32_len, undo_state->inserted_text, undo_state->inserted_text_count);
	}

	undo_state->selection = editor->selection;
}

bool skb_editor_can_undo(skb_editor_t* editor)
{
	assert(editor);
	return editor->undo_stack_head >= 0;
}

void skb_editor_undo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);
	if (editor->undo_stack_head >= 0) {
		skb__editor_undo_state_t* undo_state = &editor->undo_stack[editor->undo_stack_head];
		skb__editor_text_position_t start = skb__get_sanitized_position(editor, (skb_text_position_t){ .offset = undo_state->inserted_range.start }, SKB_SANITIZE_ADJUST_AFFINITY);
		skb__editor_text_position_t end = skb__get_sanitized_position(editor, (skb_text_position_t){ .offset = undo_state->inserted_range.end }, SKB_SANITIZE_ADJUST_AFFINITY);
		skb__replace_range(editor, temp_alloc, start, end, undo_state->removed_text, undo_state->removed_text_count);
		editor->undo_stack_head--; // This can become -1, which is the initial state.
		skb__update_layout(editor, temp_alloc);
		editor->allow_append_undo = false;
	}
}

bool skb_editor_can_redo(skb_editor_t* editor)
{
	assert(editor);
	return editor->undo_stack_head + 1 < editor->undo_stack_count;
}
void skb_editor_redo(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);
	if (editor->undo_stack_head + 1 < editor->undo_stack_count) {
		editor->undo_stack_head++;
		skb__editor_undo_state_t* undo_state = &editor->undo_stack[editor->undo_stack_head];
		skb__editor_text_position_t start = skb__get_sanitized_position(editor, (skb_text_position_t){ .offset = undo_state->removed_range.start }, SKB_SANITIZE_ADJUST_AFFINITY);
		skb__editor_text_position_t end = skb__get_sanitized_position(editor, (skb_text_position_t){ .offset = undo_state->removed_range.end }, SKB_SANITIZE_ADJUST_AFFINITY);
		skb__replace_range(editor, temp_alloc, start, end, undo_state->inserted_text, undo_state->inserted_text_count);
		skb__update_layout(editor, temp_alloc);
		editor->allow_append_undo = false;
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

static skb__editor_text_position_t skb__get_backspace_start_offset(const skb_editor_t* editor, skb__editor_text_position_t pos)
{
	assert(editor);

	// If at beginning of line, go to the end of the previous line.
	if (pos.paragraph_offset == 0) {
		if (pos.paragraph_idx > 0) {
			pos.paragraph_idx--;
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[pos.paragraph_idx];
			pos.paragraph_offset = paragraph->text_count;
			pos.text_offset = paragraph->text_start_offset + pos.paragraph_offset;
		}
	}

	if (pos.paragraph_offset <= 0)
		return pos;

	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[pos.paragraph_idx];
	int32_t offset = pos.paragraph_offset;

	int32_t delete_char_count = 0;  // Char count to be deleted by backspace.
	int32_t last_seen_var_sel_char_count = 0;  // Char count of previous variation selector.
	int32_t state = BACKSPACE_STATE_START;
	int32_t cur_offset = offset;

	do {
		const uint32_t cp = paragraph->text[cur_offset - 1];
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

	pos.paragraph_offset -= delete_char_count;
	pos.text_offset = paragraph->text_start_offset + pos.paragraph_offset;

	return pos;
}

void skb_editor_process_key_pressed(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, skb_editor_key_t key, uint32_t mods)
{
	assert(editor);

	if (key == SKB_KEY_RIGHT) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			if (mods & SKB_MOD_SHIFT) {
				// MacOS mode with shift
				if (mods & SKB_MOD_COMMAND)
					editor->selection.end_pos = skb_editor_get_line_end_at(editor, editor->selection.end_pos);
				else if (mods & SKB_MOD_OPTION)
					editor->selection.end_pos = skb_editor_move_to_next_word(editor, editor->selection.end_pos);
				else
					editor->selection.end_pos = skb_editor_move_to_next_char(editor, editor->selection.end_pos);
				// Do not move g_selection_start_caret, to allow the selection to grow.
			} else {
				// MacOS mode without shift
				if (mods & SKB_MOD_COMMAND) {
					editor->selection.end_pos = skb_editor_get_line_end_at(editor, editor->selection.end_pos);
				} else if (mods & SKB_MOD_OPTION) {
					editor->selection.end_pos = skb_editor_move_to_next_word(editor, editor->selection.end_pos);
				} else {
					// Reset selection, choose left-most caret position.
					if (skb_editor_get_selection_count(editor, editor->selection) > 0)
						editor->selection.end_pos = skb_editor_get_selection_ordered_end(editor, editor->selection);
					else
						editor->selection.end_pos = skb_editor_move_to_next_char(editor, editor->selection.end_pos);
				}
				editor->selection.start_pos = editor->selection.end_pos;
			}
		} else {
			if (mods & SKB_MOD_SHIFT) {
				// Default mode with shift
				if (mods & SKB_MOD_CONTROL)
					editor->selection.end_pos = skb_editor_move_to_next_word(editor, editor->selection.end_pos);
				else
					editor->selection.end_pos = skb_editor_move_to_next_char(editor, editor->selection.end_pos);
				// Do not move g_selection_start_caret, to allow the selection to grow.
			} else {
				// Default mode without shift
				if (mods & SKB_MOD_CONTROL) {
					editor->selection.end_pos = skb_editor_move_to_next_word(editor, editor->selection.end_pos);
				} else {
					// Reset selection, choose left-most caret position.
					if (skb_editor_get_selection_count(editor, editor->selection) > 0)
						editor->selection.end_pos = skb_editor_get_selection_ordered_end(editor, editor->selection);
					else
						editor->selection.end_pos = skb_editor_move_to_next_char(editor, editor->selection.end_pos);
				}
				editor->selection.start_pos = editor->selection.end_pos;
			}
		}
		editor->preferred_x = -1.f; // reset preferred.
		editor->allow_append_undo = false;
	}

	if (key == SKB_KEY_LEFT) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			if (mods & SKB_MOD_SHIFT) {
				// MacOS mode with shift
				if (mods & SKB_MOD_COMMAND)
					editor->selection.end_pos = skb_editor_get_line_start_at(editor, editor->selection.end_pos);
				else if (mods & SKB_MOD_OPTION)
					editor->selection.end_pos = skb_editor_move_to_prev_word(editor, editor->selection.end_pos);
				else
					editor->selection.end_pos = skb_editor_move_to_prev_char(editor, editor->selection.end_pos);
				// Do not move g_selection_start_caret, to allow the selection to grow.
			} else {
				// macOS mode without shift
				if (mods & SKB_MOD_COMMAND) {
					editor->selection.end_pos = skb_editor_get_line_start_at(editor, editor->selection.end_pos);
				} else if (mods & SKB_MOD_OPTION) {
					editor->selection.end_pos = skb_editor_move_to_prev_word(editor, editor->selection.end_pos);
				} else {
					// Reset selection, choose right-most caret position.
					if (skb_editor_get_selection_count(editor, editor->selection) > 0)
						editor->selection.end_pos = skb_editor_get_selection_ordered_start(editor, editor->selection);
					else
						editor->selection.end_pos = skb_editor_move_to_prev_char(editor, editor->selection.end_pos);
				}
				editor->selection.start_pos = editor->selection.end_pos;
			}
		} else {
			if (mods & SKB_MOD_SHIFT) {
				// Default mode with shift
				if (mods & SKB_MOD_CONTROL)
					editor->selection.end_pos = skb_editor_move_to_prev_word(editor, editor->selection.end_pos);
				else
					editor->selection.end_pos = skb_editor_move_to_prev_char(editor, editor->selection.end_pos);
				// Do not move g_selection_start_caret, to allow the selection to grow.
			} else {
				// Default mode without shift
				if (mods & SKB_MOD_CONTROL) {
					editor->selection.end_pos = skb_editor_move_to_prev_word(editor, editor->selection.end_pos);
				} else {
					if (skb_editor_get_selection_count(editor, editor->selection) > 0)
						editor->selection.end_pos = skb_editor_get_selection_ordered_start(editor, editor->selection);
					else
						editor->selection.end_pos = skb_editor_move_to_prev_char(editor, editor->selection.end_pos);
				}
				editor->selection.start_pos = editor->selection.end_pos;
			}
		}
		editor->preferred_x = -1.f; // reset preferred.
		editor->allow_append_undo = false;
	}

	if (key == SKB_KEY_HOME) {
		editor->selection.end_pos = skb_editor_get_line_start_at(editor, editor->selection.end_pos);
		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start_pos = editor->selection.end_pos;
		}
		editor->preferred_x = -1.f; // reset preferred.
		editor->allow_append_undo = false;
	}

	if (key == SKB_KEY_END) {
		editor->selection.end_pos = skb_editor_get_line_end_at(editor, editor->selection.end_pos);
		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start_pos = editor->selection.end_pos;
		}
		editor->preferred_x = -1.f; // reset preferred.
		editor->allow_append_undo = false;
	}

	if (key == SKB_KEY_UP) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			// macOS mode
			if (mods & SKB_MOD_COMMAND) {
				// Command + Up: Move to beginning of document
				editor->selection.end_pos = skb__editor_get_document_start(editor);
				editor->preferred_x = -1.f; // reset preferred
			} else {
				// Regular up movement
				if (editor->preferred_x < 0.f) {
					skb_visual_caret_t vis = skb_editor_get_visual_caret(editor, editor->selection.end_pos);
					editor->preferred_x = vis.x;
				}
				editor->selection.end_pos = skb_editor_move_to_prev_line(editor, editor->selection.end_pos, editor->preferred_x);
			}
		} else {
			// Default mode
			if (editor->preferred_x < 0.f) {
				skb_visual_caret_t vis = skb_editor_get_visual_caret(editor, editor->selection.end_pos);
				editor->preferred_x = vis.x;
			}
			editor->selection.end_pos = skb_editor_move_to_prev_line(editor, editor->selection.end_pos, editor->preferred_x);
		}

		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start_pos = editor->selection.end_pos;
		}
		editor->allow_append_undo = false;
	}
	if (key == SKB_KEY_DOWN) {
		if (editor->params.editor_behavior == SKB_BEHAVIOR_MACOS) {
			// macOS mode
			if (mods & SKB_MOD_COMMAND) {
				// Command + Down: Move to end of document
				editor->selection.end_pos = skb__editor_get_document_end(editor);
				editor->preferred_x = -1.f; // reset preferred
			} else {
				// Regular down movement
				if (editor->preferred_x < 0.f) {
					skb_visual_caret_t vis = skb_editor_get_visual_caret(editor, editor->selection.end_pos);
					editor->preferred_x = vis.x;
				}
				editor->selection.end_pos = skb_editor_move_to_next_line(editor, editor->selection.end_pos, editor->preferred_x);
			}
		} else {
			// Default mode
			if (editor->preferred_x < 0.f) {
				skb_visual_caret_t vis = skb_editor_get_visual_caret(editor, editor->selection.end_pos);
				editor->preferred_x = vis.x;
			}
			editor->selection.end_pos = skb_editor_move_to_next_line(editor, editor->selection.end_pos, editor->preferred_x);
		}

		if ((mods & SKB_MOD_SHIFT) == 0) {
			editor->selection.start_pos = editor->selection.end_pos;
		}
		editor->allow_append_undo = false;
	}

	if (key == SKB_KEY_BACKSPACE) {
		if (skb_editor_get_selection_count(editor, editor->selection) > 0) {
			skb__replace_selection(editor, temp_alloc, NULL, 0);
			editor->allow_append_undo = false;
			skb__update_layout(editor, temp_alloc);
			skb__emit_on_change(editor);
		} else {
			skb__editor_text_position_t range_end = skb__get_sanitized_position(editor, editor->selection.end_pos, SKB_SANITIZE_ADJUST_AFFINITY);
			skb__editor_text_position_t range_start = skb__get_backspace_start_offset(editor, range_end);
			skb__capture_undo(editor, range_start, range_end, NULL, 0);
			skb__replace_range(editor, temp_alloc, range_start, range_end, NULL, 0);
			editor->allow_append_undo = false;
			skb__update_layout(editor, temp_alloc);
			skb__emit_on_change(editor);
		}
	}

	if (key == SKB_KEY_DELETE) {
		if (skb_editor_get_selection_count(editor, editor->selection) > 0) {
			skb__replace_selection(editor, temp_alloc, NULL, 0);
			editor->allow_append_undo = false;
			skb__update_layout(editor, temp_alloc);
			skb__emit_on_change(editor);
		} else {
			skb__editor_text_position_t range_start = skb__get_sanitized_position(editor, editor->selection.end_pos, SKB_SANITIZE_ADJUST_AFFINITY);
			skb__editor_text_position_t range_end = skb__get_next_grapheme_pos(editor, range_start);
			skb__capture_undo(editor, range_start, range_end, NULL, 0);
			skb__replace_range(editor, temp_alloc, range_start, range_end, NULL, 0);
			editor->allow_append_undo = false;
			skb__update_layout(editor, temp_alloc);
			skb__emit_on_change(editor);
		}
	}

	if (key == SKB_KEY_ENTER) {
		const uint32_t cp = SKB_CHAR_LINE_FEED;
		skb__replace_selection(editor, temp_alloc, &cp, 1);
		editor->allow_append_undo = false;
		skb__update_layout(editor, temp_alloc);
		// The call to skb_editor_replace_selection() changes selection to after the inserted text.
		// The caret is placed on the leading edge, which is usually good, but for new line we want trailing.
		skb__editor_text_position_t range_start = skb__get_sanitized_position(editor, editor->selection.end_pos, SKB_SANITIZE_ADJUST_AFFINITY);
		editor->selection.end_pos = (skb_text_position_t) {
			.offset = range_start.text_offset,
			.affinity = SKB_AFFINITY_TRAILING,
		};
		editor->selection.start_pos = editor->selection.end_pos;
		skb__emit_on_change(editor);
	}
}

void skb_editor_insert_codepoint(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, uint32_t codepoint)
{
	assert(editor);

	skb__replace_selection(editor, temp_alloc, &codepoint, 1);
	editor->allow_append_undo = true;
	skb__update_layout(editor, temp_alloc);
	skb__emit_on_change(editor);
}

void skb_editor_paste_utf8(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const char* utf8, int32_t utf8_len)
{
	assert(editor);

	if (utf8_len < 0) utf8_len = (int32_t)strlen(utf8);
	const int32_t utf32_len = skb_utf8_to_utf32(utf8, utf8_len, NULL, 0);

	uint32_t* utf32 = SKB_TEMP_ALLOC(temp_alloc, uint32_t, utf32_len);

	skb_utf8_to_utf32(utf8, utf8_len, utf32, utf32_len);

	skb__replace_selection(editor, temp_alloc, utf32, utf32_len);
	editor->allow_append_undo = false;

	SKB_TEMP_FREE(temp_alloc, utf32);

	skb__update_layout(editor, temp_alloc);
	skb__emit_on_change(editor);
}

void skb_editor_paste_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len)
{
	assert(editor);

	if (utf32_len < 0) utf32_len = skb_utf32_strlen(utf32);
	skb__replace_selection(editor, temp_alloc, utf32, utf32_len);
	editor->allow_append_undo = false;
	skb__update_layout(editor, temp_alloc);
	skb__emit_on_change(editor);
}

void skb_editor_cut(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);

	skb__replace_selection(editor, temp_alloc, NULL, 0);
	editor->allow_append_undo = false;
	skb__update_layout(editor, temp_alloc);
	skb__emit_on_change(editor);
}

void skb_editor_set_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len, int32_t caret_position)
{
	assert(editor);

	if (utf32_len == -1)
		utf32_len = skb_utf32_strlen(utf32);

	const bool had_ime_text = editor->composition_text_count > 0;

	SKB_ARRAY_RESERVE(editor->composition_text, utf32_len);
	memcpy(editor->composition_text, utf32, utf32_len * sizeof(uint32_t));
	editor->composition_text_count = utf32_len;

	if (!had_ime_text) {
		// Capture the text position the first time we set the text.
		editor->composition_selection_base = skb_editor_get_selection_ordered_start(editor, editor->selection);
		editor->composition_position = skb__get_sanitized_position(editor, editor->composition_selection_base, SKB_SANITIZE_ADJUST_AFFINITY);
		// Clear the selection. Since the IME can be cancelled ideally we would clear on commit,
		// but it gets really complicated when multiple lines area involved.
		editor->composition_cleared_selection = false;
		if (skb_editor_get_selection_count(editor, editor->selection) > 0) {
			skb__replace_selection(editor, temp_alloc, NULL, 0);
			editor->composition_cleared_selection = true;
			editor->allow_append_undo = true; // Allow the inserted character to be appended to the undo where the text is removed.
		}
	}

	// The ime cursor is within the IME string, offset from the selection base.
	editor->composition_selection.start_pos = editor->composition_selection_base;
	editor->composition_selection.end_pos = editor->composition_selection_base;
	editor->composition_selection.start_pos.offset += caret_position;
	editor->composition_selection.end_pos.offset += caret_position;

	skb__update_layout(editor, temp_alloc);
}

void skb_editor_commit_composition_utf32(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, const uint32_t* utf32, int32_t utf32_len)
{
	assert(editor);

	if (utf32 == NULL) {
		utf32 = editor->composition_text;
		utf32_len = editor->composition_text_count;
	}

	editor->composition_text_count = 0;
	editor->composition_selection_base = (skb_text_position_t){0};
	editor->composition_selection = (skb_text_selection_t){0};
	editor->composition_position = (skb__editor_text_position_t){0};

	skb_editor_paste_utf32(editor, temp_alloc, utf32, utf32_len);
}

void skb_editor_clear_composition(skb_editor_t* editor, skb_temp_alloc_t* temp_alloc)
{
	assert(editor);

	if (editor->composition_text_count) {
		editor->composition_text_count = 0;
		editor->composition_selection_base = (skb_text_position_t){0};
		editor->composition_selection = (skb_text_selection_t){0};
		editor->composition_position = (skb__editor_text_position_t){0};

		skb__update_layout(editor, temp_alloc);

		editor->allow_append_undo = false;

		if (editor->composition_cleared_selection)
			skb__emit_on_change(editor);
	}
}

int32_t skb_editor_get_line_index_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	assert(editor);

	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_ADJUST_AFFINITY);

	int32_t total_line_count = 0;
	// Lines up to the text position.
	for (int32_t i = 0; i < edit_pos.paragraph_idx; i++) {
		skb__editor_paragraph_t* paragraph = &editor->paragraphs[i];
		total_line_count += skb_layout_get_lines_count(paragraph->layout);
	}
	total_line_count += edit_pos.line_idx;

	return total_line_count;
}

int32_t skb_editor_get_column_index_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	assert(editor);

	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_ADJUST_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	const skb_layout_line_t* lines = skb_layout_get_lines(paragraph->layout);
	const skb_layout_line_t* line = &lines[edit_pos.line_idx];
	return edit_pos.paragraph_offset - line->text_range.start;
}

int32_t skb_editor_get_text_offset_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	assert(editor);

	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_ADJUST_AFFINITY);
	return edit_pos.text_offset;
}

skb_text_direction_t skb_editor_get_text_direction_at(const skb_editor_t* editor, skb_text_position_t pos)
{
	assert(editor);

	skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	skb_text_position_t layout_pos = {
		.offset = edit_pos.paragraph_offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
	return skb_layout_get_text_direction_at(paragraph->layout, layout_pos);
}

skb_visual_caret_t skb_editor_get_visual_caret(const skb_editor_t* editor, skb_text_position_t pos)
{
	assert(editor);

	const skb__editor_text_position_t edit_pos = skb__get_sanitized_position(editor, pos, SKB_SANITIZE_IGNORE_AFFINITY);
	const skb__editor_paragraph_t* paragraph = &editor->paragraphs[edit_pos.paragraph_idx];
	pos.offset -= paragraph->text_start_offset;
	skb_visual_caret_t caret = skb_layout_get_visual_caret_at_line(paragraph->layout, edit_pos.line_idx, pos);
	caret.y += paragraph->y;
	return caret;
}

skb_range_t skb_editor_get_selection_text_offset_range(const skb_editor_t* editor, skb_text_selection_t selection)
{
	assert(editor);

	const skb__editor_text_range_t range = skb__get_sanitized_range(editor, selection);
	return (skb_range_t) {
		.start = range.start.text_offset,
		.end = range.end.text_offset,
	};
}

int32_t skb_editor_get_selection_count(const skb_editor_t* editor, skb_text_selection_t selection)
{
	assert(editor);

	const skb__editor_text_range_t range = skb__get_sanitized_range(editor, selection);
	return range.end.text_offset - range.start.text_offset;
}

void skb_editor_get_selection_bounds(const skb_editor_t* editor, skb_text_selection_t selection, skb_selection_rect_func_t* callback, void* context)
{
	assert(editor);

	const skb__editor_text_range_t range = skb__get_sanitized_range(editor, selection);

	if (range.start.paragraph_idx == range.end.paragraph_idx) {
		const skb__editor_paragraph_t* paragraph = &editor->paragraphs[range.start.paragraph_idx];
		skb_text_selection_t line_sel = {
			.start_pos = { .offset = range.start.paragraph_offset },
			.end_pos = { .offset = range.end.paragraph_offset },
		};
		skb_layout_get_selection_bounds_with_offset(paragraph->layout, paragraph->y, line_sel, callback, context);
	} else {
		// First line
		const skb__editor_paragraph_t* first_paragraph = &editor->paragraphs[range.start.paragraph_idx];
		skb_text_selection_t first_line_sel = {
			.start_pos = { .offset = range.start.paragraph_offset },
			.end_pos = { .offset = first_paragraph->text_count },
		};
		skb_layout_get_selection_bounds_with_offset(first_paragraph->layout, first_paragraph->y, first_line_sel, callback, context);

		// Middle lines
		for (int32_t line_idx = range.start.paragraph_idx + 1; line_idx < range.end.paragraph_idx; line_idx++) {
			const skb__editor_paragraph_t* paragraph = &editor->paragraphs[line_idx];
			skb_text_selection_t line_sel = {
				.start_pos = { .offset = 0 },
				.end_pos = { .offset = first_paragraph->text_count },
			};
			skb_layout_get_selection_bounds_with_offset(paragraph->layout, paragraph->y, line_sel, callback, context);
		}

		// Last line
		const skb__editor_paragraph_t* last_paragraph = &editor->paragraphs[range.end.paragraph_idx];
		skb_text_selection_t last_line_sel = {
			.start_pos = { .offset = 0 },
			.end_pos = { .offset = range.end.paragraph_offset },
		};
		skb_layout_get_selection_bounds_with_offset(last_paragraph->layout, last_paragraph->y, last_line_sel, callback, context);
	}
}
