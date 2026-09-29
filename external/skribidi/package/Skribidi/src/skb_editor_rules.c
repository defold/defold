// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_editor_rules.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "skb_attribute_collection.h"
#include "skb_editor.h"


typedef struct skb_editor_rule_set_t {
	skb_editor_rule_t* rules;
	int32_t rules_count;
	int32_t rules_cap;
} skb_editor_rule_set_t;

static bool skb__match_prefix(const skb_editor_t* editor, const skb_paragraph_position_t paragraph_pos, const char* value_utf8, skb_text_range_t* prefix_selection)
{
	const skb_text_t* paragraph_text = skb_editor_get_paragraph_text(editor, paragraph_pos.paragraph_idx);
	if (!paragraph_text)
		return false;

	const int32_t paragraph_utf32_count = skb_text_get_utf32_count(paragraph_text);
	const uint32_t* paragraph_utf32 = skb_text_get_utf32(paragraph_text);

	const int32_t value_utf8_count = (int32_t)strlen(value_utf8);
	uint32_t value_utf32[8];
	int32_t value_utf32_count = skb_utf8_to_utf32(value_utf8, value_utf8_count, value_utf32, SKB_COUNTOF(value_utf32));

	// If the text is shorter than the prefix, no match.
	if (value_utf32_count > paragraph_utf32_count)
		return false;

	// The prefix is longer than the left of text position.
	if (value_utf32_count > paragraph_pos.text_offset)
		return false;

	// Check that the prefix matches
	int32_t paragraph_offset = paragraph_pos.text_offset - 1;
	int32_t value_offset = value_utf32_count - 1;
	while (paragraph_offset >= 0 && value_offset >= 0) {
		if (value_utf32[value_offset] != paragraph_utf32[paragraph_offset])
			break;
		paragraph_offset--;
		value_offset--;
	}
	// If we did not match the whole prefix, fail.
	if (value_offset != -1)
		return false;

	// Found match
	const int32_t prefix_global_start_offset = skb_editor_get_paragraph_global_text_offset(editor, paragraph_pos.paragraph_idx);
	prefix_selection->start = (skb_text_position_t) { .offset = prefix_global_start_offset + paragraph_offset + 1 };
	prefix_selection->end = (skb_text_position_t) { .offset = prefix_global_start_offset + paragraph_offset + 1 + value_utf32_count };

	return true;
}


skb_editor_rule_set_t* skb_editor_rule_set_create(void)
{
	skb_editor_rule_set_t* rule_set = SKB_MALLOC_STRUCT(skb_editor_rule_set_t);
	return rule_set;
}

void skb_editor_rule_set_destroy(skb_editor_rule_set_t* rule_set)
{
	if (!rule_set) return;
	skb_free(rule_set->rules);
	SKB_ZERO_STRUCT(rule_set);
	skb_free(rule_set);
}

void skb_editor_rule_set_append(skb_editor_rule_set_t* rule_set, const skb_editor_rule_t* rules, int32_t rules_count)
{
	assert(rule_set);
	if (rules_count <= 0) return;

	SKB_ARRAY_RESERVE(rule_set->rules, rule_set->rules_count + rules_count);
	memcpy(rule_set->rules + rule_set->rules_count, rules, sizeof(skb_editor_rule_t) * rules_count);
	rule_set->rules_count += rules_count;
}

bool skb_editor_rule_set_process(const skb_editor_rule_set_t* rule_set, skb_editor_t* editor, skb_temp_alloc_t* temp_alloc, int32_t key, uint32_t key_mods, void* context)
{
	assert(rule_set);
	assert(editor);
	assert(temp_alloc);

	const skb_editor_params_t* params = skb_editor_get_params(editor);

	skb_editor_rule_context_t rule_context = {
		.editor = editor,
		.temp_alloc = temp_alloc,
		.attribute_collection = params->attribute_collection,
		.selection_count = skb_editor_get_text_range_count(editor, SKB_CURRENT_SELECTION),
		.caret_paragraph_pos = skb_editor_get_paragraph_position_from_text_position(editor, SKB_CURRENT_SELECTION_END),
	};

	const bool empty_selection = rule_context.selection_count == 0;

	for (int32_t i = 0; i < rule_set->rules_count; i++) {
		const skb_editor_rule_t* rule = &rule_set->rules[i];
		if (rule->key != key)
			continue;
		if (!rule->any_mods && rule->key_mods != key_mods)
			continue;
		if (rule->empty_selection && !empty_selection)
			continue;
		if (rule->has_selection && empty_selection)
			continue;
		if (rule->on_paragraph_attribute_name != 0) {
			const skb_attribute_t style_attribute = skb_attribute_make_reference_by_name(rule_context.attribute_collection, rule->on_paragraph_attribute_name);
			if (!skb_editor_has_paragraph_attribute(editor, SKB_CURRENT_SELECTION, style_attribute))
				continue;
		}
		rule_context.key_mods = key_mods;
		rule_context.prefix_text_range = (skb_text_range_t){0};
		if (rule->prefix_utf8) {
			if (!skb__match_prefix(rule_context.editor, rule_context.caret_paragraph_pos, rule->prefix_utf8, &rule_context.prefix_text_range))
				continue;
			if (rule->prefix_at_paragraph_start) {
				const int32_t global_start_offset = skb_editor_get_paragraph_global_text_offset(editor, rule_context.caret_paragraph_pos.paragraph_idx);
				if (rule_context.prefix_text_range.start.offset != global_start_offset)
					continue;
			}
		}

		assert(rule->apply);
		if (rule->apply(rule, &rule_context, context))
			return true;
	}

	return false;
}

//
// Rules
//

static bool skb__editor_rule_apply_insert_codepoint(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	skb_editor_insert_codepoint(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, rule->applied_value);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_insert_codepoint(int32_t key, uint32_t key_mods, uint32_t codepoint)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.empty_selection = false,
		.apply = skb__editor_rule_apply_insert_codepoint,
		.applied_value = (int32_t)codepoint,
	};
}


static bool skb__editor_rule_apply_process_key(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	skb_editor_process_key_pressed(rule_context->editor, rule_context->temp_alloc, rule->applied_value, rule_context->key_mods);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_process_key(int32_t key, uint32_t key_mods, int32_t edit_key)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.apply = skb__editor_rule_apply_process_key,
		.applied_value = edit_key,
	};
}

skb_editor_rule_t skb_editor_rule_make_process_key_pass_mod(int32_t key, int32_t edit_key)
{
	return (skb_editor_rule_t) {
		.key = key,
		.any_mods = true,
		.apply = skb__editor_rule_apply_process_key,
		.applied_value = edit_key,
	};
}

static bool skb__editor_rule_apply_prefix_to_paragraph_style(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	const skb_attribute_t style = skb_attribute_make_reference_by_name(rule_context->attribute_collection, rule->applied_attribute_name);

	int32_t transaction_id = skb_editor_undo_transaction_begin(rule_context->editor);
	// Remove prefix
	skb_editor_insert_text_utf32(rule_context->editor, rule_context->temp_alloc, rule_context->prefix_text_range, NULL, 0);
	// Apply style to paragraph
	const skb_text_range_t paragraph_range = skb_editor_get_paragraph_text_range(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx);
	skb_editor_set_paragraph_attribute(rule_context->editor, rule_context->temp_alloc, paragraph_range, style);
	skb_editor_undo_transaction_end(rule_context->editor, transaction_id);

	return true;
}

skb_editor_rule_t skb_editor_rule_make_convert_start_prefix_to_paragraph_style(int32_t key, uint32_t key_mods, const char* prefix_utf8, const char* on_attribute_name, const char* applied_attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.empty_selection = true,
		.prefix_utf8 = prefix_utf8,
		.prefix_at_paragraph_start = true,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_prefix_to_paragraph_style,
		.applied_attribute_name = applied_attribute_name,
	};
}

static skb_text_range_t skb__make_paragraph_selection(const skb_editor_t* editor, int32_t paragraph_idx, skb_range_t text_range)
{
	const int32_t global_offset = skb_editor_get_paragraph_global_text_offset(editor, paragraph_idx);
	return (skb_text_range_t) {
		.start = (skb_text_position_t) { .offset = global_offset + text_range.start },
		.end = (skb_text_position_t) { .offset = global_offset + text_range.end },
	};
}

static int32_t notes__get_paragraph_start_tab_count(const skb_editor_t* editor, int32_t paragraph_idx)
{
	int32_t tab_count = 0;
	const skb_text_t* text = skb_editor_get_paragraph_text(editor, paragraph_idx);
	if (text) {
		const uint32_t* utf32 = skb_text_get_utf32(text);
		const int32_t utf32_count = skb_text_get_utf32_count(text);
		while (tab_count < utf32_count && utf32[tab_count] == '\t')
			tab_count++;
	}
	return tab_count;
}

static bool skb__editor_rule_apply_indent(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	skb_attribute_t indent_level_delta = skb_attribute_make_indent_level(rule->applied_value);
	skb_editor_set_paragraph_attribute_delta(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, indent_level_delta);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_change_indent(int32_t key, uint32_t key_mods, const char* on_attribute_name, int32_t delta)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_indent,
		.applied_value = delta,
	};
}


static bool skb__editor_rule_apply_indent_line_start(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	if (rule_context->caret_paragraph_pos.text_offset != 0)
		return false;
	skb_attribute_t indent_level_delta = skb_attribute_make_indent_level(rule->applied_value);
	skb_editor_set_paragraph_attribute_delta(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, indent_level_delta);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_change_indent_at_paragraph_start(int32_t key, uint32_t key_mods, const char* on_attribute_name, int32_t delta)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.empty_selection = true,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_indent_line_start,
		.applied_value = delta,
	};
}

static bool skb__editor_rule_apply_remove_indent_line_start(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	if (rule_context->caret_paragraph_pos.text_offset != 0)
		return false;

	skb_attribute_set_t paragraph_attributes = skb_editor_get_paragraph_attributes(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx);
	const int32_t indent_level = skb_attributes_get_indent_level(paragraph_attributes, rule_context->attribute_collection);
	if (indent_level == 0) {
		if (rule->applied_attribute_name) {
			// Convert to another style when no indent left.
			const skb_attribute_t attribute = skb_attribute_make_reference_by_name(rule_context->attribute_collection, rule->applied_attribute_name);
			skb_editor_set_paragraph_attribute(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, attribute);
			return true;
		}
	} else {
		// Outdent
		skb_attribute_t indent_level_delta = skb_attribute_make_indent_level(-1);
		skb_editor_set_paragraph_attribute_delta(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, indent_level_delta);
		return true;
	}

	return false;
}

skb_editor_rule_t skb_editor_rule_make_remove_indent_at_paragraph_start(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.empty_selection = true,
		.on_paragraph_attribute_name = on_attribute_name,
		.applied_attribute_name = applied_attribute_name,
		.apply = skb__editor_rule_apply_remove_indent_line_start,
	};
}


static bool skb__editor_rule_apply_reset_empty_paragraph(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	const int32_t paragraph_text_count_no_linebreak = skb_editor_get_paragraph_text_content_count(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx);
	// Require empty looking paragraph
	if (paragraph_text_count_no_linebreak != 0)
		return false;
	const skb_attribute_t attribute = skb_attribute_make_reference_by_name(rule_context->attribute_collection, rule->applied_attribute_name);
	skb_editor_set_paragraph_attribute(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, attribute);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_change_style_on_empty_paragraph(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.empty_selection = true,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_reset_empty_paragraph,
		.applied_attribute_name = applied_attribute_name,
	};
}

static bool skb__editor_rule_apply_change_style_line_end(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	const int32_t paragraph_text_count = skb_editor_get_paragraph_text_count(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx);
	// Expect caret at line end.
	if (rule_context->caret_paragraph_pos.text_offset < (paragraph_text_count - 1))
		return false;

	const skb_attribute_t attribute = skb_attribute_make_reference_by_name(rule_context->attribute_collection, rule->applied_attribute_name);
	skb_editor_insert_paragraph(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, attribute);

	return true;
}

skb_editor_rule_t skb_editor_rule_make_change_style_at_paragraph_end(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.empty_selection = true,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_change_style_line_end,
		.applied_attribute_name = applied_attribute_name,
	};
}



static bool skb__editor_rule_apply_indent_code(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	const skb_range_t paragraph_range = skb_editor_get_paragraphs_range_from_text_range(rule_context->editor, SKB_CURRENT_SELECTION);
	int32_t transaction_id = skb_editor_undo_transaction_begin(rule_context->editor);

	if (rule->applied_value < 0) {
		for (int32_t pi = paragraph_range.start; pi < paragraph_range.end; pi++) {
			const int32_t tab_count = notes__get_paragraph_start_tab_count(rule_context->editor, pi);
			if (tab_count > 0) {
				skb_text_range_t remove_range = skb__make_paragraph_selection(rule_context->editor, pi, (skb_range_t){ .start = 0, .end = 1});
				skb_editor_insert_text_utf32(rule_context->editor, rule_context->temp_alloc, remove_range, NULL, 0);
			}
		}
	} else {
		for (int32_t pi = paragraph_range.start; pi < paragraph_range.end; pi++) {
			skb_text_range_t insert_pos = skb__make_paragraph_selection(rule_context->editor, pi, (skb_range_t){ .start = 0, .end = 0});
			const uint32_t tab[] = { '\t' };
			skb_editor_insert_text_utf32(rule_context->editor, rule_context->temp_alloc, insert_pos, tab, 1);
		}
	}
	skb_editor_undo_transaction_end(rule_context->editor, transaction_id);

	return true;
}

skb_editor_rule_t skb_editor_rule_make_code_change_indent(int32_t key, uint32_t key_mods, const char* on_attribute_name, int32_t delta)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.has_selection = true,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_indent_code,
		.applied_value = delta,
	};
}

static bool skb__editor_rule_apply_code_new_line(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	// Two empty (looking) lines in a row, will end code block
	const int32_t paragraph_text_count_no_linebreak = skb_editor_get_paragraph_text_content_count(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx);
	const int32_t tab_count = notes__get_paragraph_start_tab_count(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx);
	if (tab_count != paragraph_text_count_no_linebreak)
		return false;

	const int32_t prev_paragraph_text_count_no_linebreaks = skb_editor_get_paragraph_text_content_count(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx - 1);
	const int32_t prev_tab_count = notes__get_paragraph_start_tab_count(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx - 1);
	if (prev_tab_count != prev_paragraph_text_count_no_linebreaks)
		return false;

	int32_t transaction_id = skb_editor_undo_transaction_begin(rule_context->editor);
	// Remove first empty line, and the contents of the second (sans linebreak)
	skb_text_range_t paragraph_range = {
		.start = skb_editor_get_paragraph_content_start_pos(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx - 1),
		.end = skb_editor_get_paragraph_content_end_pos(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx),
	};
	skb_editor_insert_text_utf32(rule_context->editor, rule_context->temp_alloc, paragraph_range, NULL, 0);

	// Set the remaining empty line to requested style
	skb_text_position_t paragraph_start = skb_editor_get_paragraph_content_start_pos(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx - 1);

	const skb_attribute_t attribute = skb_attribute_make_reference_by_name(rule_context->attribute_collection, rule->applied_attribute_name);
	skb_editor_set_paragraph_attribute(rule_context->editor, rule_context->temp_alloc, (skb_text_range_t){ .start = paragraph_start, .end = paragraph_start }, attribute);

	skb_editor_undo_transaction_end(rule_context->editor, transaction_id);

	return true;
}

skb_editor_rule_t skb_editor_rule_make_code_change_style_on_empty_paragraph(int32_t key, uint32_t key_mods, const char* on_attribute_name, const char* applied_attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_code_new_line,
		.applied_attribute_name = applied_attribute_name,
	};
}


static bool skb__editor_rule_apply_code_match_tabs(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	const int32_t tab_count = notes__get_paragraph_start_tab_count(rule_context->editor, rule_context->caret_paragraph_pos.paragraph_idx);

	// Match paragraph tabs
	int32_t transaction_id = skb_editor_undo_transaction_begin(rule_context->editor);
	// Enter
	skb_editor_insert_paragraph(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, (skb_attribute_t){0});
	// Match tabs on new line.
	if (tab_count > 0) {
		const uint32_t tabs[] = { '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t' };
		skb_editor_insert_text_utf32(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, tabs, skb_mini(tab_count, SKB_COUNTOF(tabs)));
	}
	skb_editor_undo_transaction_end(rule_context->editor, transaction_id);

	return true;
}

skb_editor_rule_t skb_editor_rule_make_code_match_tabs(int32_t key, uint32_t key_mods, const char* on_attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.on_paragraph_attribute_name = on_attribute_name,
		.apply = skb__editor_rule_apply_code_match_tabs,
	};
}

static bool skb__editor_rule_apply_paragraph_attribute(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	skb_attribute_t attribute = skb_attribute_make_reference_by_name(rule_context->attribute_collection, rule->applied_attribute_name);
	skb_editor_set_paragraph_attribute(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, attribute);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_set_paragraph_attribute(int32_t key, uint32_t key_mods, const char* attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.apply = skb__editor_rule_apply_paragraph_attribute,
		.applied_attribute_name = attribute_name,
	};
}

static bool skb__editor_rule_apply_toggle_attribute(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	skb_attribute_t attribute = skb_attribute_make_reference_by_name(rule_context->attribute_collection, rule->applied_attribute_name);
	skb_editor_toggle_attribute(rule_context->editor, rule_context->temp_alloc, SKB_CURRENT_SELECTION, attribute);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_toggle_attribute(int32_t key, uint32_t key_mods, const char* attribute_name)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.apply = skb__editor_rule_apply_toggle_attribute,
		.applied_attribute_name = attribute_name,
	};
}

static bool skb__editor_rule_apply_undo_redo(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	if (rule->applied_value == SKB_EDITOR_RULE_UNDO)
		skb_editor_undo(rule_context->editor, rule_context->temp_alloc);
	else
		skb_editor_redo(rule_context->editor, rule_context->temp_alloc);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_undo_redo(int32_t key, uint32_t key_mods, skb_editor_rule_undo_redo_type_t type)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.apply = skb__editor_rule_apply_undo_redo,
		.applied_value = (int32_t)type,
	};
}


static bool skb__editor_rule_apply_select(const skb_editor_rule_t* rule, const skb_editor_rule_context_t* rule_context, void* context)
{
	if (rule->applied_value == SKB_EDITOR_RULE_SELECT_NONE)
		skb_editor_select_none(rule_context->editor);
	else
		skb_editor_select_all(rule_context->editor);
	return true;
}

skb_editor_rule_t skb_editor_rule_make_select(int32_t key, uint32_t key_mods, skb_editor_rule_select_type_t type)
{
	return (skb_editor_rule_t) {
		.key = key,
		.key_mods = key_mods,
		.apply = skb__editor_rule_apply_select,
		.applied_value = (int32_t)type,
	};
}
