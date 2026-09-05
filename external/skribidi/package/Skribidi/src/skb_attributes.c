// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_attributes.h"
#include "skb_attribute_collection.h"
#include "skb_common.h"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hb.h"

static const char* skb__make_hb_lang(const char* lang)
{
	// Use Harfbuzz to sanitize and allocate a string, and return pointer to it.
	hb_language_t hb_lang = hb_language_from_string(lang, -1);
	return hb_language_to_string(hb_lang);
}


skb_attribute_set_t skb_attribute_set_make_reference(skb_attribute_set_handle_t handle)
{
	return (skb_attribute_set_t) {
		.set_handle = handle,
	};
}

skb_attribute_set_t skb_attribute_set_make_reference_by_name(const skb_attribute_collection_t* attribute_collection, const char* name)
{
	return (skb_attribute_set_t) {
		.set_handle = skb_attribute_collection_find_set_by_name(attribute_collection, name),
	};
}


skb_attribute_t skb_attribute_make_text_base_direction(skb_text_direction_t direction)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.text_base_direction = (skb_attribute_text_base_direction_t) {
		.kind = SKB_ATTRIBUTE_TEXT_BASE_DIRECTION,
		.direction = (uint8_t)direction,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_lang(const char* lang)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.lang = (skb_attribute_lang_t) {
		.kind = SKB_ATTRIBUTE_LANG,
		.lang = skb__make_hb_lang(lang),
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_family(uint8_t family)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.font_family = (skb_attribute_font_family_t) {
		.kind = SKB_ATTRIBUTE_FONT_FAMILY,
		.family = family,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_size(float size)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.font_size = (skb_attribute_font_size_t) {
		.kind = SKB_ATTRIBUTE_FONT_SIZE,
		.size = size,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_size_scaling(skb_font_size_scaling_t type, float scale)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.font_size_scaling = (skb_attribute_font_size_scaling_t) {
		.kind = SKB_ATTRIBUTE_FONT_SIZE_SCALING,
		.type = type,
		.scale = scale,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_weight(skb_weight_t weight)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.font_weight = (skb_attribute_font_weight_t) {
		.kind = SKB_ATTRIBUTE_FONT_WEIGHT,
		.weight = weight,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_style(skb_style_t style)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.font_style = (skb_attribute_font_style_t) {
		.kind = SKB_ATTRIBUTE_FONT_STYLE,
		.style = style,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_stretch(skb_stretch_t stretch)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.font_stretch = (skb_attribute_font_stretch_t) {
		.kind = SKB_ATTRIBUTE_FONT_STRETCH,
		.stretch = stretch,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_feature(uint32_t tag, uint32_t value)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.font_feature = (skb_attribute_font_feature_t) {
		.kind = SKB_ATTRIBUTE_FONT_FEATURE,
		.tag = tag,
		.value = value,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_letter_spacing(float letter_spacing)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.letter_spacing = (skb_attribute_letter_spacing_t) {
		.kind = SKB_ATTRIBUTE_LETTER_SPACING,
		.spacing = letter_spacing,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_word_spacing(float word_spacing)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.word_spacing = (skb_attribute_word_spacing_t) {
		.kind = SKB_ATTRIBUTE_WORD_SPACING,
		.spacing = word_spacing,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_line_height(skb_line_height_t type, float height)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.line_height = (skb_attribute_line_height_t) {
		.kind = SKB_ATTRIBUTE_LINE_HEIGHT,
		.type = (uint8_t)type,
		.height = height,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_inline_padding(float start, float end, float top, float bottom)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.inline_padding = (skb_attribute_inline_padding_t) {
		.kind = SKB_ATTRIBUTE_INLINE_PADDING,
		.start = start,
		.end = end,
		.top = top,
		.bottom = bottom,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_inline_padding_hv(float horizontal, float vertical)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.inline_padding = (skb_attribute_inline_padding_t) {
		.kind = SKB_ATTRIBUTE_INLINE_PADDING,
		.start = horizontal,
		.end = horizontal,
		.top = vertical,
		.bottom = vertical,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_tab_stop_increment(float increment)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.tab_stop_increment = (skb_attribute_tab_stop_increment_t) {
		.kind = SKB_ATTRIBUTE_TAB_STOP_INCREMENT,
		.increment = increment,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_paragraph_padding(float start, float end, float top, float bottom)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.paragraph_padding = (skb_attribute_paragraph_padding_t) {
		.kind = SKB_ATTRIBUTE_PARAGRAPH_PADDING,
		.start = start,
		.end = end,
		.top  = top,
		.bottom = bottom,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_paragraph_padding_with_spacing(float start, float end, float top, float bottom, float group_spacing)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.paragraph_padding = (skb_attribute_paragraph_padding_t) {
		.kind = SKB_ATTRIBUTE_PARAGRAPH_PADDING,
		.start = start,
		.end = end,
		.top  = top,
		.bottom = bottom,
		.group_spacing = group_spacing,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_indent_level(int32_t level)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.indent_level = (skb_attribute_indent_level_t) {
		.kind = SKB_ATTRIBUTE_INDENT_LEVEL,
		.level = level,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_indent_increment(float level_increment, float first_line_increment)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.indent_increment = (skb_attribute_indent_increment_t) {
		.kind = SKB_ATTRIBUTE_INDENT_INCREMENT,
		.level_increment = level_increment,
		.first_line_increment = first_line_increment,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_list_marker(skb_list_marker_style_t style, float indent, float spacing, uint32_t codepoint)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.list_marker = (skb_attribute_list_marker_t) {
		.kind = SKB_ATTRIBUTE_LIST_MARKER,
		.style = (uint8_t)style,
		.indent = indent,
		.spacing = spacing,
		.codepoint = codepoint,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_text_wrap(skb_text_wrap_t text_wrap)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.text_wrap = (skb_attribute_text_wrap_t) {
		.kind = SKB_ATTRIBUTE_TEXT_WRAP,
		.text_wrap = text_wrap,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_text_overflow(skb_text_overflow_t text_overflow)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.text_overflow = (skb_attribute_text_overflow_t) {
		.kind = SKB_ATTRIBUTE_TEXT_OVERFLOW,
		.text_overflow = text_overflow,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_vertical_trim(skb_vertical_trim_t vertical_trim)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.vertical_trim = (skb_attribute_vertical_trim_t) {
		.kind = SKB_ATTRIBUTE_VERTICAL_TRIM,
		.vertical_trim = vertical_trim,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_horizontal_align(skb_align_t horizontal_align)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.horizontal_align = (skb_attribute_align_t) {
		.kind = SKB_ATTRIBUTE_HORIZONTAL_ALIGN,
		.align = horizontal_align,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_vertical_align(skb_align_t vertical_align)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.vertical_align = (skb_attribute_align_t) {
		.kind = SKB_ATTRIBUTE_VERTICAL_ALIGN,
		.align = vertical_align,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_baseline_align(skb_baseline_t baseline_align)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.baseline_align = (skb_attribute_baseline_align_t) {
		.kind = SKB_ATTRIBUTE_BASELINE_ALIGN,
		.baseline = baseline_align,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_baseline_shift(skb_baseline_shift_t type, float shift)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.baseline_shift = (skb_attribute_baseline_shift_t) {
		.kind = SKB_ATTRIBUTE_BASELINE_SHIFT,
		.type = type,
		.offset = shift,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_paint_color(uint32_t paint_tag, uint32_t state, skb_color_t color)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.paint = (skb_attribute_paint_t) {
		.kind = SKB_ATTRIBUTE_PAINT,
		.paint_tag = paint_tag,
		.state = state,
		.color = color,
		.paint_id = 0,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_paint_id(uint32_t paint_tag, uint32_t state, intptr_t paint_id)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.paint = (skb_attribute_paint_t) {
		.kind = SKB_ATTRIBUTE_PAINT,
		.paint_tag = paint_tag,
		.state = state,
		.color = (skb_color_t){0},
		.paint_id = paint_id,
	};
	return attribute;
}


skb_attribute_t skb_attribute_make_decoration(skb_decoration_position_t position, skb_decoration_style_t style, float thickness, float offset, uint32_t paint_tag)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.decoration = (skb_attribute_decoration_t) {
		.kind = SKB_ATTRIBUTE_DECORATION,
		.position = (uint8_t)position,
		.style = (uint8_t)style,
		.thickness = thickness,
		.offset = offset,
		.paint_tag = paint_tag,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_indent_decoration(int32_t min_level, int32_t max_level, float offset_x, float width)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.indent_decoration = (skb_attribute_indent_decoration_t) {
		.kind = SKB_ATTRIBUTE_INDENT_DECORATION,
		.min_level = min_level,
		.max_level = max_level,
		.offset_x = offset_x,
		.width = width,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_object_align(float baseline_ratio, skb_object_align_reference_t align_ref, skb_baseline_t align_baseline)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.object_align = (skb_attribute_object_align_t) {
		.kind = SKB_ATTRIBUTE_OBJECT_ALIGN,
		.align_ref = (uint8_t)align_ref,
		.align_baseline = (uint8_t)align_baseline,
		.baseline_ratio = baseline_ratio,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_group_tag(uint32_t group_tag)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.group_tag = (skb_attribute_group_tag_t) {
		.kind = SKB_ATTRIBUTE_GROUP_TAG,
		.group_tag = group_tag,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_reference(skb_attribute_set_handle_t set_handle)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.reference = (skb_attribute_reference_t) {
		.kind = SKB_ATTRIBUTE_REFERENCE,
		.handle = set_handle,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_reference_by_name(const skb_attribute_collection_t* attribute_collection, const char* name)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.reference = (skb_attribute_reference_t) {
		.kind = SKB_ATTRIBUTE_REFERENCE,
		.handle = skb_attribute_collection_find_set_by_name(attribute_collection, name),
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_caret_padding(float horizontal, float vertical)
{
	skb_attribute_t attribute;
	SKB_ZERO_STRUCT(&attribute); // Makes sure that the padding gets zeroed too.
	attribute.caret_padding = (skb_attribute_caret_padding_t) {
		.kind = SKB_ATTRIBUTE_CARET_PADDING,
		.horizontal = horizontal,
		.vertical = vertical,
	};
	return attribute;
}

int32_t skb_attributes_get_by_kind(uint32_t kind, skb_attribute_set_t attributes, const skb_attribute_collection_t* collection, const skb_attribute_t** results, int32_t results_cap)
{
	int32_t count = 0;
	for (int32_t i = attributes.attributes_count-1; i >= 0; i--) {
		if (attributes.attributes[i].kind == kind) {
			if (count < results_cap)
				results[count++] = &attributes.attributes[i];
		}
		if (attributes.attributes[i].kind == SKB_ATTRIBUTE_REFERENCE) {
			skb_attribute_set_t ref_attributes = skb_attribute_collection_get_set(collection, attributes.attributes[i].reference.handle);
			count += skb_attributes_get_by_kind(kind, ref_attributes, collection, results + count, results_cap - count);
		}
	}

	if (attributes.set_handle) {
		skb_attribute_set_t ref_attributes = skb_attribute_collection_get_set(collection, attributes.set_handle);
		count += skb_attributes_get_by_kind(kind, ref_attributes, collection, results + count, results_cap - count);
	}

	if (attributes.parent_set)
		count += skb_attributes_get_by_kind(kind, *attributes.parent_set, collection, results + count, results_cap - count);

	return count;
}

bool skb_attributes_match(const skb_attribute_t* a, const skb_attribute_t* b)
{
	if (a->kind != b->kind)
		return false;
	if (a->kind == SKB_ATTRIBUTE_PAINT)
		return a->paint.paint_tag == b->paint.paint_tag;
	if (a->kind == SKB_ATTRIBUTE_REFERENCE)
		return skb_attribute_set_handle_get_group(a->reference.handle) == skb_attribute_set_handle_get_group(b->reference.handle);
	return true;

}

static const skb_attribute_t* skb__get_attribute_by_kind(uint32_t kind, const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	for (int32_t i = attributes.attributes_count-1; i >= 0; i--) {
		if (attributes.attributes[i].kind == kind)
			return &attributes.attributes[i];
		if (attributes.attributes[i].kind == SKB_ATTRIBUTE_REFERENCE) {
			skb_attribute_set_t ref_attributes = skb_attribute_collection_get_set(collection, attributes.attributes[i].reference.handle);
			const skb_attribute_t* attr = skb__get_attribute_by_kind(kind, ref_attributes, collection);
			if (attr)
				return attr;
		}
	}

	if (attributes.parent_set) {
		const skb_attribute_t* attr = skb__get_attribute_by_kind(kind, *attributes.parent_set, collection);
		if (attr)
			return attr;
	}
	if (attributes.set_handle) {
		skb_attribute_set_t ref_attributes = skb_attribute_collection_get_set(collection, attributes.set_handle);
		return skb__get_attribute_by_kind(kind, ref_attributes, collection);
	}

	return NULL;
}

skb_text_direction_t skb_attributes_get_text_base_direction(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_TEXT_BASE_DIRECTION, attributes, collection);
	return attr ? attr->text_base_direction.direction : SKB_DIRECTION_AUTO;
}

const char* skb_attributes_get_lang(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_LANG, attributes, collection);
	return attr ? attr->lang.lang : NULL;
}

uint8_t skb_attributes_get_font_family(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_FONT_FAMILY, attributes, collection);
	return attr ? attr->font_family.family : SKB_FONT_FAMILY_DEFAULT;
}

float skb_attributes_get_font_size(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_FONT_SIZE, attributes, collection);
	return attr ? attr->font_size.size : 16.f;
}

skb_attribute_font_size_scaling_t skb_attributes_get_font_size_scaling(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_font_size_scaling_t default_font_size_scaling = {
		.type = SKB_FONT_SIZE_SCALING_NONE,
	};
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_FONT_SIZE_SCALING, attributes, collection);
	return attr ? attr->font_size_scaling : default_font_size_scaling;
}

skb_weight_t skb_attributes_get_font_weight(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_FONT_WEIGHT, attributes, collection);
	return attr ? attr->font_weight.weight : SKB_WEIGHT_NORMAL;
}

skb_style_t skb_attributes_get_font_style(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_FONT_STYLE, attributes, collection);
	return attr ? attr->font_style.style : SKB_STYLE_NORMAL;
}

skb_stretch_t skb_attributes_get_font_stretch(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_FONT_STRETCH, attributes, collection);
	return attr ? attr->font_stretch.stretch : SKB_STRETCH_NORMAL;
}

float skb_attributes_get_letter_spacing(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_LETTER_SPACING, attributes, collection);
	return attr ? attr->letter_spacing.spacing : 0.f;
}

float skb_attributes_get_word_spacing(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_WORD_SPACING, attributes, collection);
	return attr ? attr->word_spacing.spacing : 0.f;
}

skb_attribute_line_height_t skb_attributes_get_line_height(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_line_height_t default_line_height = {
		.type = SKB_LINE_HEIGHT_NORMAL,
	};
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_LINE_HEIGHT, attributes, collection);
	return attr ? attr->line_height : default_line_height;
}

skb_attribute_inline_padding_t skb_attributes_get_inline_padding(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_inline_padding_t default_inline_padding = { 0 };
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_INLINE_PADDING, attributes, collection);
	return attr ? attr->inline_padding : default_inline_padding;
}

float skb_attributes_get_tab_stop_increment(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_TAB_STOP_INCREMENT, attributes, collection);
	return attr ? attr->tab_stop_increment.increment : 0.f;
}

skb_attribute_paragraph_padding_t skb_attributes_get_paragraph_padding(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_paragraph_padding_t default_vertical_padding = { 0 };
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_PARAGRAPH_PADDING, attributes, collection);
	return attr ? attr->paragraph_padding : default_vertical_padding;
}

int32_t skb_attributes_get_indent_level(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_INDENT_LEVEL, attributes, collection);
	return attr ? attr->indent_level.level : 0;
}

skb_attribute_indent_increment_t skb_attributes_get_indent_increment(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_indent_increment_t default_indent_increment = {
		.first_line_increment = 0.f,
		.level_increment = 0.f,
	};
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_INDENT_INCREMENT, attributes, collection);
	return attr ? attr->indent_increment : default_indent_increment;
}

skb_attribute_list_marker_t skb_attributes_get_list_marker(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_list_marker_t default_list_marker = { 0 };
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_LIST_MARKER, attributes, collection);
	return attr ? attr->list_marker : default_list_marker;
}

skb_attribute_paint_t skb_attributes_get_paint(uint32_t paint_tag, uint32_t state, const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_paint_t default_paint = {
		.color = {0, 0, 0, 255 },
	};

	const skb_attribute_paint_t* paints[16];
	int32_t paints_count = skb_attributes_get_paints_by_tag(paint_tag, attributes, collection, paints, SKB_COUNTOF(paints));

	skb_attribute_paint_t fallback = default_paint;

	for (int32_t i = 0; i < paints_count; i++) {
		const skb_attribute_paint_t* paint = paints[i];
		if ((paint->state & state) || (!state && !paint->state))
			return *paint;
		if (paint->state == SKB_PAINT_STATE_DEFAULT)
			fallback = *paint;
	}

	return fallback;
}

int32_t skb_attributes_get_paints_by_tag(uint32_t paint_tag, skb_attribute_set_t attributes, const skb_attribute_collection_t* collection, const skb_attribute_paint_t** results, int32_t results_cap)
{
	int32_t count = 0;
	for (int32_t i = attributes.attributes_count-1; i >= 0; i--) {
		if (attributes.attributes[i].kind == SKB_ATTRIBUTE_PAINT && attributes.attributes[i].paint.paint_tag == paint_tag) {
			if (count < results_cap)
				results[count++] = &attributes.attributes[i].paint;
		}
		if (attributes.attributes[i].kind == SKB_ATTRIBUTE_REFERENCE) {
			skb_attribute_set_t ref_attributes = skb_attribute_collection_get_set(collection, attributes.attributes[i].reference.handle);
			count += skb_attributes_get_paints_by_tag(paint_tag, ref_attributes, collection, results + count, results_cap - count);
		}
	}

	if (attributes.set_handle) {
		skb_attribute_set_t ref_attributes = skb_attribute_collection_get_set(collection, attributes.set_handle);
		count += skb_attributes_get_paints_by_tag(paint_tag, ref_attributes, collection, results + count, results_cap - count);
	}

	if (attributes.parent_set)
		count += skb_attributes_get_paints_by_tag(paint_tag, *attributes.parent_set, collection, results + count, results_cap - count);

	return count;
}

skb_attribute_indent_decoration_t skb_attributes_get_indent_decoration(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_indent_decoration_t default_indent_decoration = { 0 };

	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_INDENT_DECORATION, attributes, collection);
	return attr ? attr->indent_decoration : default_indent_decoration;
}

skb_attribute_object_align_t skb_attributes_get_object_align(const skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_object_align_t default_object_align = {
		.align_ref = SKB_OBJECT_ALIGN_TEXT_AFTER_OR_BEFORE,
		.align_baseline = SKB_BASELINE_CENTRAL,
		.baseline_ratio = 0.5f,
	};

	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_OBJECT_ALIGN, attributes, collection);
	return attr ? attr->object_align : default_object_align;
}

skb_text_wrap_t skb_attributes_get_text_wrap(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_TEXT_WRAP, attributes, collection);
	return attr ? attr->text_wrap.text_wrap : SKB_WRAP_NONE;
}

skb_text_overflow_t skb_attributes_get_text_overflow(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_TEXT_OVERFLOW, attributes, collection);
	return attr ? attr->text_overflow.text_overflow : SKB_OVERFLOW_NONE;
}

skb_vertical_trim_t skb_attributes_get_vertical_trim(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_VERTICAL_TRIM, attributes, collection);
	return attr ? attr->vertical_trim.vertical_trim : SKB_VERTICAL_TRIM_DEFAULT;
}

skb_align_t skb_attributes_get_horizontal_align(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_HORIZONTAL_ALIGN, attributes, collection);
	return attr ? attr->horizontal_align.align : SKB_ALIGN_START;
}

skb_align_t skb_attributes_get_vertical_align(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_VERTICAL_ALIGN, attributes, collection);
	return attr ? attr->vertical_align.align : SKB_ALIGN_START;
}

skb_baseline_t skb_attributes_get_baseline_align(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_BASELINE_ALIGN, attributes, collection);
	return attr ? attr->baseline_align.baseline : SKB_BASELINE_ALPHABETIC;
}

skb_attribute_baseline_shift_t skb_attributes_get_baseline_shift(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_baseline_shift_t default_baseline_shift = { 0 };

	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_BASELINE_SHIFT, attributes, collection);
	return attr ? attr->baseline_shift : default_baseline_shift;
}

skb_attribute_caret_padding_t skb_attributes_get_caret_padding(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	static const skb_attribute_caret_padding_t default_caret_padding = { 0 };

	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_CARET_PADDING, attributes, collection);
	return attr ? attr->caret_padding : default_caret_padding;

}

uint32_t skb_attributes_get_group(skb_attribute_set_t attributes, const skb_attribute_collection_t* collection)
{
	const skb_attribute_t* attr = skb__get_attribute_by_kind(SKB_ATTRIBUTE_GROUP_TAG, attributes, collection);
	return attr ? attr->group_tag.group_tag : 0;
}

int32_t skb_attributes_get_copy_flat_count(const skb_attribute_set_t attributes)
{
	int32_t count = attributes.attributes_count;
	if (attributes.set_handle)
		count++;
	if (attributes.parent_set)
		count += skb_attributes_get_copy_flat_count(*attributes.parent_set);
	return count;
}

int32_t skb_attributes_copy_flat(const skb_attribute_set_t attributes, skb_attribute_t* dest, int32_t dest_cap)
{
	int32_t copied = 0;

	if (attributes.parent_set)
		copied += skb_attributes_copy_flat(*attributes.parent_set, dest + copied, dest_cap - copied);

	if (attributes.set_handle && (copied + 1) <= dest_cap) {
		dest[copied] = skb_attribute_make_reference(attributes.set_handle);
		copied++;
	}

	int32_t count = skb_mini(attributes.attributes_count, dest_cap - copied);
	if (count > 0) {
		memcpy(dest + copied, attributes.attributes, sizeof(skb_attribute_t) * count);
		copied += count;
	}

	return copied;
}

uint64_t skb_attributes_hash_append(uint64_t hash, skb_attribute_set_t attributes)
{
	if (attributes.parent_set)
		hash = skb_attributes_hash_append(hash, *attributes.parent_set);

	if (attributes.set_handle)
		hash = skb_hash64_append(hash, &attributes.set_handle, sizeof(skb_attribute_set_handle_t));

	// Note: The attributes are zero initialized (including padding)
	for (int32_t i = 0; i < attributes.attributes_count; i++)
		hash = skb_hash64_append(hash, &attributes.attributes[i], sizeof(skb_attribute_t));

	return hash;
}
