// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdbool.h>

#include "skb_common.h"
#include "skb_common_internal.h"
#include "skb_layout.h"
#include "skb_font_collection_internal.h"

#include "hb.h"
#include "SheenBidi/SheenBidi.h"
#include "graphemebreak.h"
#include "linebreak.h"
#include "wordbreak.h"
#include "budoux.h"

#include <assert.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#define SB_SCRIPT_COMMON SBScriptZYYY
#define SB_SCRIPT_INHERITED SBScriptZINH

typedef struct skb_layout_t {
	skb_layout_params_t params;

	uint32_t* text;
	skb_text_property_t* text_props;
	int32_t text_count;
	int32_t text_cap;

	skb_glyph_t* glyphs;
	int32_t glyphs_count;
	int32_t glyphs_cap;

	skb_glyph_run_t* glyph_runs;
	int32_t glyph_runs_count;
	int32_t glyph_runs_cap;

	skb_layout_line_t* lines;
	int32_t lines_count;
	int32_t lines_cap;

	skb_text_attributes_span_t* attribute_spans;
	int32_t attribute_spans_count;
	int32_t attribute_spans_cap;

	skb_attribute_t* attributes;
	int32_t attributes_count;
	int32_t attributes_cap;

	skb_decoration_t* decorations;
	int32_t decorations_count;
	int32_t decorations_cap;

	skb_rect2_t bounds;

	uint8_t resolved_direction;
} skb_layout_t;

uint64_t skb_layout_params_hash_append(uint64_t hash, const skb_layout_params_t* params)
{
	hash = skb_hash64_append_uint32(hash, params->font_collection ? skb_font_collection_get_id(params->font_collection) : 0);
	hash = skb_hash64_append_str(hash, params->lang);
	hash = skb_hash64_append_float(hash, params->origin.x);
	hash = skb_hash64_append_float(hash, params->origin.y);
	hash = skb_hash64_append_float(hash, params->layout_width);
	hash = skb_hash64_append_float(hash, params->layout_height);
	hash = skb_hash64_append_uint8(hash, params->base_direction);
	hash = skb_hash64_append_uint8(hash, params->text_wrap);
	hash = skb_hash64_append_uint8(hash, params->text_overflow);
	hash = skb_hash64_append_uint8(hash, params->vertical_trim);
	hash = skb_hash64_append_uint8(hash, params->horizontal_align);
	hash = skb_hash64_append_uint8(hash, params->vertical_align);
	hash = skb_hash64_append_uint8(hash, params->baseline_align);
	hash = skb_hash64_append_uint8(hash, params->flags);

	return hash;
}

uint64_t skb_attributes_hash_append(uint64_t hash, const skb_attribute_t* attributes, int32_t attributes_count)
{
	// Note: The attributes are zero initialized (including padding)
	for (int32_t i = 0; i < attributes_count; i++)
		hash = skb_hash64_append(hash, &attributes[i], sizeof(skb_attribute_t));
	return hash;
}

static hb_script_t skb__sb_script_to_hb(SBScript script)
{
	const SBUInt32 script_tag = SBScriptGetUnicodeTag(script);
	return hb_script_from_iso15924_tag(script_tag);
}

uint32_t skb_script_to_iso15924_tag(uint8_t script)
{
	return SBScriptGetUnicodeTag(script);
}

static const char* skb__make_hb_lang(const char* lang)
{
	// Use Harfbuzz to sanitize and allocate a string, and return pointer to it.
	hb_language_t hb_lang = hb_language_from_string(lang, -1);
	return hb_language_to_string(hb_lang);
}

skb_attribute_t skb_attribute_make_writing(const char* lang, skb_text_direction_t direction)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.writing = (skb_attribute_writing_t) {
		.kind = SKB_ATTRIBUTE_WRITING,
		.direction = (uint8_t)direction,
		.lang = skb__make_hb_lang(lang),
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font(skb_font_family_t family, float size, skb_weight_t weight, skb_style_t style, skb_stretch_t stretch)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.font = (skb_attribute_font_t) {
		.kind = SKB_ATTRIBUTE_FONT,
		.size = size,
		.family = (uint8_t)family,
		.weight = (uint8_t)weight,
		.style = (uint8_t)style,
		.stretch = (uint8_t)stretch,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_font_feature(uint32_t tag, uint32_t value)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.font_feature = (skb_attribute_font_feature_t) {
		.kind = SKB_ATTRIBUTE_FONT_FEATURE,
		.tag = tag,
		.value = value,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_spacing(float letter, float word)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.spacing = (skb_attribute_spacing_t) {
		.kind = SKB_ATTRIBUTE_SPACING,
		.letter = letter,
		.word = word,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_line_height(skb_line_height_t type, float height)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.line_height = (skb_attribute_line_height_t) {
		.kind = SKB_ATTRIBUTE_LINE_HEIGHT,
		.type = (uint8_t)type,
		.height = height,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_fill(skb_color_t color)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.fill = (skb_attribute_fill_t) {
		.kind = SKB_ATTRIBUTE_FILL,
		.color = color,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_decoration(skb_decoration_position_t position, skb_decoration_style_t style, float thickness, float offset, skb_color_t color)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.decoration = (skb_attribute_decoration_t) {
		.kind = SKB_ATTRIBUTE_DECORATION,
		.position = (uint8_t)position,
		.style = (uint8_t)style,
		.thickness = thickness,
		.offset = offset,
		.color = color,
	};
	return attribute;
}

skb_attribute_t skb_attribute_make_object(float width, float height, float baseline, intptr_t id)
{
	return skb_attribute_make_object_with_align(width, height, baseline, SKB_OBJECT_ALIGN_SELF, SKB_BASELINE_ALPHABETIC, id);
}

skb_attribute_t skb_attribute_make_object_with_align(float width, float height, float baseline, skb_object_align_reference_t align_ref, skb_baseline_t align_baseline, intptr_t id)
{
	skb_attribute_t attribute;
	memset(&attribute, 0, sizeof(attribute)); // Using memset() so that the padding gets zeroed too.
	attribute.object = (skb_attribute_object_t) {
		.kind = SKB_ATTRIBUTE_OBJECT,
		.width = width,
		.height = height,
		.baseline_offset = baseline,
		.align_ref = (uint8_t)align_ref,
		.align_baseline = (uint8_t)align_baseline,
		.id = id,
	};
	return attribute;
}

skb_attribute_writing_t skb_attributes_get_writing(const skb_attribute_t* attributes, int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_WRITING)
			return attributes[i].writing;
	}
	return (skb_attribute_writing_t) {0};
}

skb_attribute_font_t skb_attributes_get_font(const skb_attribute_t* attributes, int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_FONT)
			return attributes[i].font;
	}
	return (skb_attribute_font_t) {
		.size = 16.f,
		.family = SKB_FONT_FAMILY_DEFAULT,
		.weight = SKB_WEIGHT_NORMAL,
		.style = SKB_STYLE_NORMAL,
		.stretch = SKB_STRETCH_NORMAL,
	};
}

skb_attribute_spacing_t skb_attributes_get_spacing(const skb_attribute_t* attributes, int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_SPACING)
			return attributes[i].spacing;
	}
	return (skb_attribute_spacing_t) {0};
}

skb_attribute_line_height_t skb_attributes_get_line_height(const skb_attribute_t* attributes, int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_LINE_HEIGHT)
			return attributes[i].line_height;
	}
	return (skb_attribute_line_height_t) {
		.type = SKB_LINE_HEIGHT_NORMAL,
	};
}

skb_attribute_fill_t skb_attributes_get_fill(const skb_attribute_t* attributes, int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_FILL)
			return attributes[i].fill;
	}
	return (skb_attribute_fill_t) {
		.color = skb_rgba(0, 0, 0, 255),
	};
}

skb_attribute_object_t skb_attributes_get_object(const skb_attribute_t* attributes, int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_OBJECT)
			return attributes[i].object;
	}
	return (skb_attribute_object_t) {
		.width = 16.f,
		.height = 16.f,
		.id = 0
	};
}

/*
static bool skb__has_object_attribute(const skb_attribute_t* attributes, const int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_OBJECT)
			return true;
	}
	return false;
}
*/

/*
skb_attribute_object_baselines_t skb_attributes_get_object_baselines(const skb_attribute_t* attributes, int32_t attributes_count)
{
	for (int32_t i = 0; i < attributes_count; i++) {
		if (attributes[i].kind == SKB_ATTRIBUTE_OBJECT_BASELINES)
			return attributes[i].object_baselines;
	}
	return (skb_attribute_object_baselines_t) {
		.alphabetic = 0.775f,
		.ideographic = 1.f,
		.central = 0.45f,
		.hanging = 0.25f,
		.mathematical = 0.5f,
		.middle = 0.55f,
	};
}

float skb_object_calc_baseline(skb_baseline_t baseline_align, float height, skb_attribute_object_baselines_t object_baselines)
{
	switch (baseline_align) {
	case SKB_BASELINE_ALPHABETIC:
		return height * object_baselines.alphabetic;
	case SKB_BASELINE_IDEOGRAPHIC:
		return height * object_baselines.ideographic;
	case SKB_BASELINE_CENTRAL:
		return height * object_baselines.central;
	case SKB_BASELINE_HANGING:
		return height * object_baselines.hanging;
	case SKB_BASELINE_MATHEMATICAL:
		return height * object_baselines.mathematical;
	case SKB_BASELINE_MIDDLE:
		return height * object_baselines.middle;
	case SKB_BASELINE_TEXT_BOTTOM:
		return height;
	case SKB_BASELINE_TEXT_TOP:
		return 0.f;
	}
	return height * object_baselines.alphabetic;
}
*/

typedef struct skb__script_tag_t {
	uint32_t tag;
	uint8_t script;
} skb__script_tag_t;

enum {
	SKB_MAX_SCRIPTS = 0xab+1, // This is highest SBScript value + 1
};

static int32_t skb__compare_script_tags(const void* a, const void* b)
{
	const skb__script_tag_t* sa = a;
	const skb__script_tag_t* sb = b;
	return (int32_t)sa->tag - (int32_t)sb->tag;
}

uint8_t skb_script_from_iso15924_tag(uint32_t script_tag)
{
	// Sheenbidi does not provide script reverse lookup, make one.
	// Create SBScript -> ISO-15924 table, which can be binary searched.
	static skb__script_tag_t script_tags[SKB_MAX_SCRIPTS] = {0};
	static bool initialized = false;
	if (!initialized) {
		for (int32_t sb_script = 0; sb_script < SKB_MAX_SCRIPTS; sb_script++) {
			// SBScript -> ISO-15924
			script_tags[sb_script].tag = SBScriptGetUnicodeTag((uint8_t)sb_script);
			script_tags[sb_script].script = (uint8_t)sb_script;
		}
		// Sort in tag order
		qsort(script_tags, SKB_MAX_SCRIPTS, sizeof(skb__script_tag_t), skb__compare_script_tags);
		initialized = true;
	}

	// Binary search tag.
	int32_t low = 0;
	int32_t high = SKB_MAX_SCRIPTS - 1;
	while (low != high) {
		const int32_t mid = low + (high - low + 1) / 2; // ceil
		if (script_tags[mid].tag > script_tag)
			high = mid - 1;
		else
			low = mid;
	}
	if (script_tags[low].tag == script_tag)
		return script_tags[low].script;

	return SBScriptNil;
}

static bool skb__is_japanese_script(uint8_t script)
{
	return script == SBScriptHANI || script == SBScriptHIRA || script == SBScriptKANA;
}

//
// Layout
//

// Run ready for shaping.
typedef struct skb__shaping_run_t {
	int32_t offset;
	int32_t length;
	int32_t shaping_span_idx;
	uint8_t script;
	uint8_t direction;
	bool is_emoji;
} skb__shaping_run_t;

typedef struct skb__layout_build_context_t {
	skb__shaping_run_t* shaping_runs;
	int32_t shaping_runs_count;
	int32_t shaping_runs_cap;

	hb_language_t lang;

	uint8_t* emoji_types_buffer;

	skb_temp_alloc_t* temp_alloc;
} skb__layout_build_context_t;


#define SKB_MAX_FEATURES 32

static void skb__add_font_feature(hb_feature_t* features, int32_t* features_count, hb_tag_t tag, uint32_t value)
{
	if (*features_count >= SKB_MAX_FEATURES)
		return;

	features[*features_count] = (hb_feature_t) {
		.tag = tag,
		.value = value,
		.start = HB_FEATURE_GLOBAL_START,
		.end = HB_FEATURE_GLOBAL_END,
	};
	*features_count = *features_count + 1;
}

static void skb__shape_run(
	skb__layout_build_context_t* build_context,
	skb_layout_t* layout,
	const skb__shaping_run_t* run,
	const skb_text_attributes_span_t* span,
	hb_buffer_t* buffer,
	const skb_font_handle_t* fonts,
	int32_t fonts_count,
	int32_t font_idx)
{
	assert(fonts_count > 0);

	const skb_attribute_writing_t attr_writing = skb_attributes_get_writing(span->attributes, span->attributes_count);
	const skb_attribute_spacing_t attr_spacing = skb_attributes_get_spacing(span->attributes, span->attributes_count);
	const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);

	const skb_font_t* font = skb_font_collection_get_font(layout->params.font_collection, fonts[font_idx]);

	hb_buffer_add_utf32(buffer, layout->text, layout->text_count, run->offset, run->length);

	const hb_language_t lang = attr_writing.lang ? hb_language_from_string(attr_writing.lang, -1) : build_context->lang;

	hb_buffer_set_direction(buffer, skb_is_rtl(run->direction) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
	hb_buffer_set_script(buffer, skb__sb_script_to_hb(run->script));
	hb_buffer_set_language(buffer, lang);

	hb_feature_t features[SKB_MAX_FEATURES];
	int32_t features_count = 0;

	if (skb_absf(attr_spacing.letter) > 0.01f) {
		// Disable ligratures when letter spacing is requested.
		skb__add_font_feature(features, &features_count, SKB_TAG_STR("clig"), 0); // Contextual ligatures
		skb__add_font_feature(features, &features_count, SKB_TAG_STR("dlig"), 0); // Discretionary ligatures
		skb__add_font_feature(features, &features_count, SKB_TAG_STR("rlig"), 0); // Required ligatures
		skb__add_font_feature(features, &features_count, SKB_TAG_STR("liga"), 0); // Standard ligatures
		skb__add_font_feature(features, &features_count, SKB_TAG_STR("hlig"), 0); // Historical ligatures
	}
	for (int32_t i = 0; i < span->attributes_count; i++) {
		if (span->attributes[i].kind == SKB_ATTRIBUTE_FONT_FEATURE)
			skb__add_font_feature(features, &features_count, span->attributes[i].font_feature.tag, span->attributes[i].font_feature.value);
	}

	hb_buffer_flags_t flags = HB_BUFFER_FLAG_DEFAULT;
	if (run->offset == 0)
		flags |= HB_BUFFER_FLAG_BOT;
	if ((run->offset + run->length) == layout->text_count)
		flags |= HB_BUFFER_FLAG_EOT;
	hb_buffer_set_flags(buffer, flags);

	hb_shape(font->hb_font, buffer, features, features_count);

	const int32_t glyph_count = (int32_t)hb_buffer_get_length(buffer);
	const hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buffer, NULL);
	const hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buffer, NULL);

	// Get info about regular space character, we'll use it for control characters.
	hb_codepoint_t space_gid = 0;
	hb_font_get_glyph (font->hb_font, 0x20 /*space*/, 0, &space_gid);
	hb_position_t space_x_advance = hb_font_get_glyph_h_advance(font->hb_font, space_gid);

	const float scale = attr_font.size * font->upem_scale;

	// Reserve space for the glyphs.
	SKB_ARRAY_RESERVE(layout->glyphs, layout->glyphs_count + glyph_count);

	const uint16_t span_idx = (uint16_t)(span - layout->attribute_spans);

	// Iterate clusters
	for (int32_t i = 0; i < glyph_count; ) {

		const bool is_whitespace = (layout->text_props[glyph_info[i].cluster].flags & SKB_TEXT_PROP_WHITESPACE);
		const bool is_control = (layout->text_props[glyph_info[i].cluster].flags & SKB_TEXT_PROP_CONTROL);

		// Encountered invalid glyph. If the grapheme is not a control character, reshape with a fallback font.
		if (glyph_info[i].codepoint == 0 && !is_whitespace && !is_control) {
			// Find the range of invalid glyphs.
			int32_t glyph_start = i;
			int32_t glyph_end = i;
			while ((glyph_end+1) < glyph_count && glyph_info[glyph_end+1].codepoint == 0)
				glyph_end++;

			// Figure out the section of codepoints that belongs to this grapheme cluster.
			int32_t missing_text_start, missing_text_end;
			if (skb_is_rtl(run->direction)) {
				missing_text_start = (int32_t)glyph_info[glyph_end].cluster;
				missing_text_end = (glyph_start > 0) ? (int32_t)glyph_info[glyph_start-1].cluster : (run->offset + run->length);
			} else {
				missing_text_start = (int32_t)glyph_info[glyph_start].cluster;
				missing_text_end = (glyph_end+1 < glyph_count) ? (int32_t)glyph_info[glyph_end+1].cluster : (run->offset + run->length);
			}

			// Try next matching font if available.
			if (font_idx+1 < fonts_count) {
				hb_buffer_t* fallback_buffer = hb_buffer_create();

				skb__shaping_run_t fallback_run = {
					.offset = missing_text_start,
					.length = missing_text_end - missing_text_start,
					.direction = run->direction,
					.is_emoji = run->is_emoji,
					.script = run->script,
				};
				skb__shape_run(build_context, layout, &fallback_run, span, fallback_buffer, fonts, fonts_count, font_idx+1);

				hb_buffer_destroy(fallback_buffer);
				i = glyph_end + 1;
				continue;
			}
			// If we could not find fallback font, we'll continue below and add the empty glyphs.
		}

		// Figure out cluster of glyphs matching cluster of codepoints. This can be e.g. a ligature matching multiple graphemes, or glyph combination that matches a grapheme cluster.
		int32_t glyph_start = i;
		int32_t glyph_end = i;
		int32_t text_start = 0;
		int32_t text_end = 0;

		// Merge \r\n into one glyph.
		if ((i+1) < glyph_count && layout->text[glyph_info[i].cluster] == SKB_CHAR_CARRIAGE_RETURN && layout->text[glyph_info[i+1].cluster] == SKB_CHAR_LINE_FEED) {
			glyph_start = i+1;
			glyph_end = i+1;
			text_start = (int32_t)glyph_info[i].cluster;
			text_end = (int32_t)glyph_info[i+1].cluster + 1;
			assert(is_control);
		} else {
			// Find current cluster boundary.
			const uint32_t cluster = glyph_info[i].cluster;
			while ((glyph_end+1) < glyph_count && glyph_info[glyph_end+1].cluster == cluster)
				glyph_end++;

			// Figure out the section of text that belongs to this grapheme cluster.
			if (skb_is_rtl(run->direction)) {
				text_start = (int32_t)glyph_info[glyph_end].cluster;
				text_end = (glyph_start > 0) ? (int32_t)glyph_info[glyph_start-1].cluster : (run->offset + run->length);
			} else {
				text_start = (int32_t)glyph_info[glyph_start].cluster;
				text_end = (glyph_end+1 < glyph_count) ? (int32_t)glyph_info[glyph_end+1].cluster : (run->offset + run->length);
			}
		}
		assert(text_end >= text_start);

		// Add glyphs
		for (int32_t j = glyph_start; j <= glyph_end; j++) {
			skb_glyph_t* glyph = &layout->glyphs[layout->glyphs_count++];

			if (is_control) {
				// Replace with space character to avoid showing invalid glyph.
				glyph->gid = (uint16_t)space_gid;
				glyph->offset_x = 0.f;
				glyph->offset_y = 0.f;
				glyph->advance_x = (float)space_x_advance * scale;
			} else {
				assert(glyph_info[j].codepoint <= 0xffff);
				glyph->gid = (uint16_t)glyph_info[j].codepoint;
				glyph->offset_x = (float)glyph_pos[j].x_offset * scale;
				glyph->offset_y = -(float)glyph_pos[j].y_offset * scale;
				glyph->advance_x = (float)glyph_pos[j].x_advance * scale;
			}

			glyph->font_handle = font->handle;
			glyph->text_range.start = text_start;
			glyph->text_range.end = text_end;
			glyph->visual_idx = layout->glyphs_count - 1;
			glyph->span_idx = span_idx;
		}

		i = glyph_end + 1;
	}
}

static int32_t skb__append_text_utf8(skb_layout_t* layout, const char* utf8, int32_t utf8_len)
{
	if (utf8_len < 0) utf8_len = utf8 ? (int32_t)strlen(utf8) : 0;
	const int32_t new_text_offset = layout->text_count;
	const int32_t new_text_count = skb_utf8_to_utf32(utf8, utf8_len, NULL, 0);
	if (!new_text_count)
		return new_text_count;

	layout->text_count += new_text_count;

	// Make sure we have enough space for the text and attribs.
	if (layout->text_count > layout->text_cap) {
		layout->text_cap = layout->text_count;
		layout->text = skb_realloc(layout->text, layout->text_count * sizeof(uint32_t));
		assert(layout->text);
		layout->text_props = skb_realloc(layout->text_props, layout->text_count * sizeof(skb_text_property_t));
		assert(layout->text_props);
	}

	// Convert utf-8 to utf-32 codepoints.
	skb_utf8_to_utf32(utf8, utf8_len, layout->text + new_text_offset, new_text_count);

	memset(layout->text_props + new_text_offset, 0, new_text_count * sizeof(skb_text_property_t));

	return new_text_count;
}

static int32_t skb__append_text_utf32(skb_layout_t* layout, const uint32_t* utf32, int32_t utf32_len)
{
	if (utf32_len < 0) utf32_len = utf32 ? skb_utf32_strlen(utf32) : 0;
	const int32_t new_text_offset = layout->text_count;
	const int32_t new_text_count = utf32_len;
	if (!new_text_count)
		return new_text_count;

	layout->text_count += new_text_count;

	if (layout->text_count > layout->text_cap) {
		layout->text_cap = layout->text_count;
		layout->text = skb_realloc(layout->text, layout->text_count * sizeof(uint32_t));
		assert(layout->text);
		layout->text_props = skb_realloc(layout->text_props, layout->text_count * sizeof(skb_text_property_t));
		assert(layout->text_props);
	}

	memcpy(layout->text + new_text_offset, utf32, new_text_count * sizeof(uint32_t));
	memset(layout->text_props + new_text_offset, 0, new_text_count * sizeof(skb_text_property_t));

	return utf32_len;
}

static void skb__init_text_props(skb_temp_alloc_t* temp_alloc, const char* lang, const uint32_t* text, skb_text_property_t* text_attribs, int32_t text_count)
{
	if (!text_count)
		return;

	char* breaks = SKB_TEMP_ALLOC(temp_alloc, char, text_count);

	set_graphemebreaks_utf32(text, text_count, lang, breaks);
	for (int i = 0; i < text_count; i++) {
		if (breaks[i] == GRAPHEMEBREAK_BREAK)
			text_attribs[i].flags |= SKB_TEXT_PROP_GRAPHEME_BREAK;
	}

	set_wordbreaks_utf32(text, text_count, lang, breaks);
	for (int i = 0; i < text_count; i++) {
		if (breaks[i] == WORDBREAK_BREAK)
			text_attribs[i].flags |= SKB_TEXT_PROP_WORD_BREAK;
	}

	set_linebreaks_utf32(text, text_count, lang, breaks);
	for (int i = 0; i < text_count; i++) {
		if (breaks[i] == LINEBREAK_MUSTBREAK)
			text_attribs[i].flags |= SKB_TEXT_PROP_MUST_LINE_BREAK;
		if (breaks[i] == LINEBREAK_ALLOWBREAK)
			text_attribs[i].flags |= SKB_TEXT_PROP_ALLOW_LINE_BREAK;
	}

	for (int i = 0; i < text_count; i++) {
		SBGeneralCategory category = SBCodepointGetGeneralCategory(text[i]);
		SKB_SET_FLAG(text_attribs[i].flags, SKB_TEXT_PROP_CONTROL, category == SBGeneralCategoryCC);
		SKB_SET_FLAG(text_attribs[i].flags, SKB_TEXT_PROP_WHITESPACE, SBGeneralCategoryIsSeparator(category));
		SKB_SET_FLAG(text_attribs[i].flags, SKB_TEXT_PROP_PUNCTUATION, SBGeneralCategoryIsPunctuation(category));
	}

	SKB_TEMP_FREE(temp_alloc, breaks);
}

static int skb__glyph_cmp_logical(const void *a, const void *b)
{
	const skb_glyph_t* ga = (const skb_glyph_t*)a;
	const skb_glyph_t *gb = (const skb_glyph_t*)b;
	return (int)ga->text_range.start - (int)gb->text_range.start;
}

static int skb__glyph_cmp_visual(const void *a, const void *b)
{
	const skb_glyph_t* ga = (const skb_glyph_t*)a;
	const skb_glyph_t *gb = (const skb_glyph_t*)b;
	return ga->visual_idx - gb->visual_idx;
}

static float skb_calculate_line_height(skb_attribute_line_height_t attr_line_height, const skb_font_t* font, float font_size)
{
	const float ascender = font->metrics.ascender * font_size;
	const float descender = font->metrics.descender * font_size;

	if (attr_line_height.type == SKB_LINE_HEIGHT_NORMAL)
		return -ascender + descender;
	if (attr_line_height.type == SKB_LINE_HEIGHT_METRICS_RELATIVE)
		return (-ascender + descender) * attr_line_height.height;
	if (attr_line_height.type == SKB_LINE_HEIGHT_FONT_SIZE_RELATIVE)
		return font_size * attr_line_height.height;
	// SKB_LINE_HEIGHT_ABSOLUTE) {
	return attr_line_height.height;
}


static float skb__calc_align(skb_align_t align, float size, float container_size)
{
	if (align == SKB_ALIGN_START)
		return 0.f;
	if (align == SKB_ALIGN_END)
		return container_size - size;
	if (align == SKB_ALIGN_CENTER)
		return (container_size - size) / 2.f;
	return 0.f;
}

static skb_align_t skb_get_directional_align(bool is_rtl, skb_align_t align)
{
	if (is_rtl) {
		if (align == SKB_ALIGN_START)
			return SKB_ALIGN_END;
		if (align == SKB_ALIGN_END)
			return SKB_ALIGN_START;
	}
	return align;
}

static void skb__prune_line_end_whitespace(const skb_layout_t* layout, skb_range_t* glyph_range, float* width)
{
	const bool layout_is_rtl = skb_is_rtl(layout->resolved_direction);

	float trimmed_width = width ? *width : 0.f;
	if (layout_is_rtl) {
		// Prune any trailing spaces
		while (glyph_range->end > glyph_range->start) {
			const int32_t glyph_idx = glyph_range->start;
			const int cp_offset = layout->glyphs[glyph_idx].text_range.end - 1;
			const bool codepoint_is_whitespace = (layout->text_props[cp_offset].flags & SKB_TEXT_PROP_WHITESPACE);
			if (!codepoint_is_whitespace)
				break;
			trimmed_width -= layout->glyphs[glyph_idx].advance_x;
			glyph_range->start++;
		}
	} else {
		// Prune any trailing spaces
		while (glyph_range->end > glyph_range->start) {
			const int32_t glyph_idx = glyph_range->end - 1;
			const int cp_offset = layout->glyphs[glyph_idx].text_range.end - 1;
			const bool codepoint_is_whitespace = (layout->text_props[cp_offset].flags & SKB_TEXT_PROP_WHITESPACE);
			if (!codepoint_is_whitespace)
				break;
			trimmed_width -= layout->glyphs[glyph_idx].advance_x;
			glyph_range->end--;
		}
	}
	if (width)
		*width = trimmed_width;
}

static int32_t skb__prune_line_width(const skb_layout_t* layout, skb_range_t* glyph_range, float* width, const float line_break_width)
{
	const bool layout_is_rtl = skb_is_rtl(layout->resolved_direction);
	int32_t last_pruned_glyph_idx = SKB_INVALID_INDEX;

	if (layout_is_rtl) {
		// Prune the glyphs that overflow the max line width.
		while (glyph_range->end > glyph_range->start && *width > line_break_width) {
			const int32_t glyph_idx = glyph_range->start;
			last_pruned_glyph_idx = glyph_idx;
			*width -= layout->glyphs[glyph_idx].advance_x;
			glyph_range->start++;
		}

	} else {
		// Prune the glyphs that overflow the max line width.
		while (glyph_range->end > glyph_range->start && *width > line_break_width) {
			const int32_t glyph_idx = glyph_range->end - 1;
			last_pruned_glyph_idx = glyph_idx;
			*width -= layout->glyphs[glyph_idx].advance_x;
			glyph_range->end--;
		}
	}

	return last_pruned_glyph_idx;
}

static int32_t skb__get_next_text_run(const skb_layout_t* layout, int32_t cur_glyph_idx, uint16_t cur_span_idx)
{
	for (int32_t gi = cur_glyph_idx + 1; gi < layout->glyphs_count; gi++) {
		const skb_glyph_t* glyph = &layout->glyphs[gi];
		if (glyph->span_idx != cur_span_idx) {
			const skb_text_attributes_span_t* span = &layout->attribute_spans[glyph->span_idx];
			if ((span->flags & SKB_TEXT_ATTRIBUTES_SPAN_HAS_OBJECT) == 0)
				return gi;
		}
	}
	return SKB_INVALID_INDEX;
}

static int32_t skb__get_prev_text_run(const skb_layout_t* layout, int32_t cur_glyph_idx, uint16_t cur_span_idx)
{
	for (int32_t gi = cur_glyph_idx - 1; gi > 0; gi++) {
		const skb_glyph_t* glyph = &layout->glyphs[gi];
		if (glyph->span_idx != cur_span_idx) {
			const skb_text_attributes_span_t* span = &layout->attribute_spans[glyph->span_idx];
			if ((span->flags & SKB_TEXT_ATTRIBUTES_SPAN_HAS_OBJECT) == 0)
				return gi;
		}
	}
	return SKB_INVALID_INDEX;
}

static void skb__init_glyph_run(const skb_layout_t* layout, skb_glyph_run_t* glyph_run, int32_t first_glyph_idx)
{
	const skb_glyph_t* glyph = &layout->glyphs[first_glyph_idx];
	glyph_run->font_handle = glyph->font_handle;
	glyph_run->span_idx = glyph->span_idx;
	glyph_run->glyph_range.start = first_glyph_idx;
	glyph_run->glyph_range.end = first_glyph_idx;
	glyph_run->baseline = glyph->offset_y;

	const skb_text_attributes_span_t* span = &layout->attribute_spans[glyph->span_idx];
	SKB_SET_FLAG(glyph_run->flags, SKB_GLYPH_RUN_IS_OBJECT, span->flags & SKB_TEXT_ATTRIBUTES_SPAN_HAS_OBJECT);
}

void skb__layout_lines(skb_layout_t* layout, skb_temp_alloc_t* temp_alloc)
{
	// Sort glyphs in logical order, word breaks must be done in logical order.
	qsort(layout->glyphs, layout->glyphs_count, sizeof(skb_glyph_t), skb__glyph_cmp_logical);

	skb_vec2_t origin = layout->params.origin;
	const bool ignore_must_breaks = layout->params.flags & SKB_LAYOUT_PARAMS_IGNORE_MUST_LINE_BREAKS;

	layout->bounds.x = origin.x;
	layout->bounds.y = origin.y;
	layout->bounds.width = 0.f;
	layout->bounds.height = 0.f;

	SKB_ARRAY_RESERVE(layout->lines, layout->lines_count+1);
	skb_layout_line_t* cur_line = &layout->lines[layout->lines_count++];
	memset(cur_line, 0, sizeof(skb_layout_line_t));
	cur_line->bounds.width = 0.f;
	cur_line->glyph_range.start = 0;
	cur_line->glyph_range.end = 0;

	const bool layout_is_rtl = skb_is_rtl(layout->resolved_direction);
	const float line_break_width = layout->params.layout_width;
	const skb_text_wrap_t text_wrap = layout->params.text_wrap;
	const skb_text_overflow_t text_overflow = layout->params.text_overflow;

	if (text_wrap == SKB_WRAP_NONE) {
		// No wrapping, all glyphs in one line.
		cur_line->glyph_range.start = 0;
		cur_line->glyph_range.end = layout->glyphs_count;

		// Update bounds
		for (int32_t i = 0; i < layout->glyphs_count; i++)
			cur_line->bounds.width += layout->glyphs[i].advance_x;

	} else {
		// Wrapping
		int32_t glyph_idx = 0;
		while (glyph_idx < layout->glyphs_count) {

			// Calc run up to the next line break.
			int32_t glyph_run_start = glyph_idx;
			int32_t glyph_run_end = glyph_idx;

			float run_end_whitespace_width = 0.f;
			float run_width = 0.f;

			bool must_break = false;
			while (glyph_run_end < layout->glyphs_count) {

				// Advance whole glyph cluster, cannot split in between.
				float cluster_width = layout->glyphs[glyph_run_end].advance_x;
				const int32_t cluster_start = layout->glyphs[glyph_run_end].text_range.start;
				while ((glyph_run_end+1) < layout->glyphs_count && layout->glyphs[glyph_run_end+1].text_range.start == cluster_start) {
					glyph_run_end++;
					cluster_width += layout->glyphs[glyph_run_end].advance_x;
				}

				const int cp_offset = layout->glyphs[glyph_run_end].text_range.end - 1;

				glyph_run_end++;

				// Keep track of the white space after the run end, it will not be taken into account for the line breaking.
				// When the direction does not match, the space will be inside the line (not end of it), so we ignore that.
				const bool codepoint_is_rtl = skb_is_rtl(layout->text_props[cp_offset].direction);
				const bool codepoint_is_whitespace = (layout->text_props[cp_offset].flags & SKB_TEXT_PROP_WHITESPACE);
				const bool codepoint_is_control = (layout->text_props[cp_offset].flags & SKB_TEXT_PROP_CONTROL);
				if (codepoint_is_rtl == layout_is_rtl && (codepoint_is_whitespace || codepoint_is_control)) {
					run_end_whitespace_width += cluster_width;
				} else {
					if (run_end_whitespace_width > 0.f) {
						run_width += run_end_whitespace_width;
						run_end_whitespace_width = 0.f;
					}
					run_width += cluster_width;
				}

				if (layout->text_props[cp_offset].flags & SKB_TEXT_PROP_MUST_LINE_BREAK) {
					must_break = true;
					break;
				}
				if (layout->text_props[cp_offset].flags & SKB_TEXT_PROP_ALLOW_LINE_BREAK)
					break;
			}

			if (text_wrap == SKB_WRAP_WORD_CHAR && run_width > line_break_width) {
				// If text wrap is set to word & char, allow to break at a character when the whole word does not fit.

				// Start a  new line
				if (cur_line->glyph_range.start != cur_line->glyph_range.end) {
					SKB_ARRAY_RESERVE(layout->lines, layout->lines_count+1);
					cur_line = &layout->lines[layout->lines_count++];
					memset(cur_line, 0, sizeof(skb_layout_line_t));
					cur_line->bounds.width = 0.f;
					cur_line->glyph_range.start = glyph_run_start;
					cur_line->glyph_range.end = glyph_run_start;
				}

				// Fit as many glyphs as we can on the line, and adjust run_end up to that point.
				// TODO: we should use the info from harfbuzz to see where we can cut the glyph sequence.
				run_width = 0.f;
				for (int i = glyph_run_start; i < glyph_run_end; i++) {
					if ((cur_line->bounds.width + run_width + layout->glyphs[i].advance_x) > line_break_width) {
						// This glyph would overflow, stop here. run_end is exclusive, so one past the last valid index.
						glyph_run_end = i;
						break;
					}
					run_width += layout->glyphs[i].advance_x;
				}
				// Consume at least one character so that we don't get stuck.
				if (glyph_run_start == glyph_run_end) {
					run_width = layout->glyphs[glyph_run_start].advance_x;
					glyph_run_end = glyph_run_start + 1;
				}

				// Update width so far.
				cur_line->bounds.width += run_width;
				cur_line->glyph_range.end = glyph_run_end;
			} else {
				// If the word does not fit, start a new line (unless it's an empty line).
				if (cur_line->glyph_range.start != cur_line->glyph_range.end && (cur_line->bounds.width + run_width) > line_break_width) {
					SKB_ARRAY_RESERVE(layout->lines, layout->lines_count+1);
					cur_line = &layout->lines[layout->lines_count++];
					memset(cur_line, 0, sizeof(skb_layout_line_t));
					cur_line->bounds.width = 0.f;
					cur_line->glyph_range.start = glyph_run_start;
					cur_line->glyph_range.end = glyph_run_start;
				}

				// Update width so far.
				cur_line->bounds.width += run_width + run_end_whitespace_width;
				cur_line->glyph_range.end = glyph_run_end;

				if (must_break && !ignore_must_breaks) {
					// Line break character start a new line.
					SKB_ARRAY_RESERVE(layout->lines, layout->lines_count+1);
					cur_line = &layout->lines[layout->lines_count++];
					memset(cur_line, 0, sizeof(skb_layout_line_t));
					cur_line->bounds.width = 0.f;
					cur_line->glyph_range.start = glyph_run_end; // Using run end here, since we have already committed the start-end to the current line.
					cur_line->glyph_range.end = glyph_run_end;
				}
			}

			glyph_idx = glyph_run_end;
		}
	}

	// Update line heights
	float calculated_height = 0.f;
	float first_line_cap_height = 0.f;

	const float max_height = layout->params.layout_height;
	bool last_line_ellipsis = false;

	for (int32_t li = 0; li < layout->lines_count; li++) {
		skb_layout_line_t* line = &layout->lines[li];

		float line_height = 0.f;

		if (line->glyph_range.start != line->glyph_range.end) {
			// The line is still in logical order and not truncated for overflow, grab the text range.
			line->text_range.start = layout->glyphs[line->glyph_range.start].text_range.start;
			line->text_range.end = layout->glyphs[line->glyph_range.end - 1].text_range.end;
			// Find beginning of the last grapheme on the line (needed or caret positioning, etc).
			line->last_grapheme_offset = skb_layout_align_grapheme_offset(layout, line->text_range.end - 1);

			skb_font_handle_t prev_font_handle = 0;
			int32_t prev_span_idx = -1;
			float baseline_align_offset = 0.f;

			// Update line dimensions
			for (int32_t gi = line->glyph_range.start; gi < line->glyph_range.end; gi++) {
				skb_glyph_t* glyph = &layout->glyphs[gi];

				if (prev_font_handle != glyph->font_handle || prev_span_idx != glyph->span_idx) {

					const skb_text_attributes_span_t* span = &layout->attribute_spans[glyph->span_idx];
					if (span->flags & SKB_TEXT_ATTRIBUTES_SPAN_HAS_OBJECT) {
						const skb_attribute_object_t attr_object = skb_attributes_get_object(span->attributes, span->attributes_count);

						// Find index of the reference glyph to align to.
						int32_t ref_glyph_idx = gi; // self
						if (attr_object.align_ref == SKB_OBJECT_ALIGN_TEXT_BEFORE)
							ref_glyph_idx = skb__get_prev_text_run(layout, gi, glyph->span_idx);
						else if (attr_object.align_ref == SKB_OBJECT_ALIGN_TEXT_AFTER)
							ref_glyph_idx = skb__get_next_text_run(layout, gi, glyph->span_idx);

						// Find baseline to align to.
						float ref_baseline = 0.f;
						if (ref_glyph_idx != SKB_INVALID_INDEX) {
							const skb_glyph_t* ref_glyph = &layout->glyphs[ref_glyph_idx];
							skb_text_property_t ref_text_prop = layout->text_props[ref_glyph->text_range.start];
							const skb_text_attributes_span_t* ref_span = &layout->attribute_spans[ref_glyph->span_idx];
							const skb_attribute_font_t attr_font = skb_attributes_get_font(ref_span->attributes, ref_span->attributes_count);
							const skb_baseline_set_t baseline_set = skb_font_get_baseline_set(layout->params.font_collection, ref_glyph->font_handle, ref_text_prop.direction, ref_text_prop.script, attr_font.size);
							ref_baseline = baseline_set.baselines[attr_object.align_baseline] - baseline_set.baselines[layout->params.baseline_align];
						}

						line_height = skb_maxf(line_height, attr_object.height);
						line->ascender = skb_minf(line->ascender, -attr_object.baseline_offset);
						line->descender = skb_maxf(line->descender, attr_object.height - attr_object.baseline_offset);

						baseline_align_offset = ref_baseline - attr_object.baseline_offset;

					} else {
						const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);
						const skb_attribute_line_height_t attr_line_height = skb_attributes_get_line_height(span->attributes, span->attributes_count);

						const skb_font_t* font = skb_font_collection_get_font(layout->params.font_collection, glyph->font_handle);

						skb_text_property_t text_prop = layout->text_props[glyph->text_range.start];
						const float baseline = skb_font_get_baseline(layout->params.font_collection, glyph->font_handle, layout->params.baseline_align, text_prop.direction, text_prop.script, attr_font.size);

						baseline_align_offset = -baseline;

						line_height = skb_maxf(line_height, skb_calculate_line_height(attr_line_height, font, attr_font.size));
						line->ascender = skb_minf(line->ascender, font->metrics.ascender * attr_font.size - baseline);
						line->descender = skb_maxf(line->descender, font->metrics.descender * attr_font.size - baseline);
						if (li == 0)
							first_line_cap_height = skb_minf(first_line_cap_height, font->metrics.cap_height * attr_font.size - baseline);
					}

					prev_font_handle = glyph->font_handle;
					prev_span_idx = glyph->span_idx;
				}

				glyph->offset_y += baseline_align_offset;
			}

		} else {
			// The last line can be empty, if the last character is new line separator.
			assert(li == layout->lines_count - 1);
			line->text_range.start = layout->text_count;
			line->text_range.end = layout->text_count;
			line->glyph_range.start = layout->glyphs_count;
			line->glyph_range.end = layout->glyphs_count;
			// This is intentionally past the last valid glyph so that we can uniquely address the line using text position.
			line->last_grapheme_offset = layout->text_count;

			// If we end up here, the last glyph should be a new line.
			const skb_text_attributes_span_t* last_span = &layout->attribute_spans[layout->attribute_spans_count - 1];
			const skb_attribute_font_t attr_font = skb_attributes_get_font(last_span->attributes, last_span->attributes_count);
			const skb_attribute_line_height_t attr_line_height = skb_attributes_get_line_height(last_span->attributes, last_span->attributes_count);

			const skb_font_handle_t default_font_handle = skb_font_collection_get_default_font(layout->params.font_collection, attr_font.family);
			const skb_font_t* font = skb_font_collection_get_font(layout->params.font_collection, default_font_handle);
			if (font) {
				line_height = skb_maxf(line_height, skb_calculate_line_height(attr_line_height, font, attr_font.size));
				line->ascender = skb_minf(line->ascender, font->metrics.ascender * attr_font.size);
				line->descender = skb_maxf(line->descender, font->metrics.descender * attr_font.size);
				if (li == 0)
					first_line_cap_height = font->metrics.cap_height * attr_font.size;
			}
		}

		if (text_overflow != SKB_OVERFLOW_NONE) {
			if (li > 0 && (calculated_height + line_height) > max_height) {
				// The line will overflow the max height for the layout, trim this and any following lines.
				layout->lines_count = li;
				// Force to ellipsize the last visible line, if overflow is ellipsis.
				last_line_ellipsis = text_overflow == SKB_OVERFLOW_ELLIPSIS;
				break;
			}
		}

		line->bounds.height = line_height;
		calculated_height += line_height;
	}

	// Sort the glyphs back to visual order
	for (int32_t li = 0; li < layout->lines_count; li++) {
		skb_layout_line_t* line = &layout->lines[li];
		if (line->glyph_range.start != line->glyph_range.end) {
			// Sort back to visual order.
			const int32_t glyphs_count = line->glyph_range.end - line->glyph_range.start;
			qsort(&layout->glyphs[line->glyph_range.start], glyphs_count, sizeof(skb_glyph_t), skb__glyph_cmp_visual);
		}
	}


	// Calculate glyph runs, line width, and handle overflow.
	// Similar to CSS, the overflow is calculated in visual runs of glyphs, truncated based on dominant reading direction.
	layout->glyph_runs_count = 0;
	float calculated_width = 0.f;
	for (int32_t li = 0; li < layout->lines_count; li++) {
		skb_layout_line_t* line = &layout->lines[li];

		const bool is_last_line = li == (layout->lines_count-1);
		const bool force_ellipsis = is_last_line && last_line_ellipsis;

		skb_glyph_run_t ellipsis_run = {0};

		skb_range_t glyph_range = line->glyph_range;

		if (text_overflow != SKB_OVERFLOW_NONE || force_ellipsis) {
			if (line->bounds.width > line_break_width || force_ellipsis) {

				SKB_SET_FLAG(line->flags, SKB_LAYOUT_LINE_IS_TRUNCATED, true);

				// Prune characters to fit the line (this is common with clip and ellipsis).
				int32_t last_pruned_glyph_idx = skb__prune_line_width(layout, &glyph_range, &line->bounds.width, line_break_width);
				if (force_ellipsis)
					last_pruned_glyph_idx = layout_is_rtl ? glyph_range.start : (glyph_range.end - 1);

				if (text_overflow == SKB_OVERFLOW_ELLIPSIS || force_ellipsis) {

					skb__prune_line_end_whitespace(layout, &glyph_range, &line->bounds.width);

					if (last_pruned_glyph_idx != SKB_INVALID_INDEX) {
						// Get out text attributes for the ellipsis from the last pruned glyph.
						const uint16_t span_idx = layout->glyphs[last_pruned_glyph_idx].span_idx;
						const skb_font_handle_t font_handle = layout->glyphs[last_pruned_glyph_idx].font_handle;
						const float offset_y = layout->glyphs[last_pruned_glyph_idx].offset_y;

						// We expect these to exist, since they were calculated earlier.
						const skb_font_t* font = skb_font_collection_get_font(layout->params.font_collection, font_handle);
						const skb_text_attributes_span_t* span = &layout->attribute_spans[span_idx];
						assert(font);
						assert(span);

						// Figure out ellipsis size.
						const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);

						// Try to use the actual ellipsis character, but fall back to 3 periods.
						int32_t ellipsis_glyph_count = 0;
						hb_codepoint_t ellipsis_gid = 0;
						if (hb_font_get_glyph(font->hb_font, 0x2026 /*ellipsis*/, 0, &ellipsis_gid)) {
							ellipsis_glyph_count = 1;
						} else if (hb_font_get_glyph(font->hb_font, 0x2e /*period*/, 0, &ellipsis_gid)) {
							ellipsis_glyph_count = 3;
						}

						if (ellipsis_glyph_count > 0) {
							const float scale = attr_font.size * font->upem_scale;
							const float ellipsis_x_advance = (float)hb_font_get_glyph_h_advance(font->hb_font, ellipsis_gid) * scale;
							const float ellipsis_width = ellipsis_x_advance * (float)ellipsis_glyph_count;

							// Prune further until the ellipsis fits.
							const float max_line_width = line_break_width - ellipsis_width;
							skb__prune_line_width(layout, &glyph_range, &line->bounds.width, max_line_width);

							// Place the ellipsis characters.
							ellipsis_run.font_handle = font_handle;
							ellipsis_run.span_idx = span_idx;
							ellipsis_run.glyph_range.start = layout->glyphs_count;
							ellipsis_run.glyph_range.end = layout->glyphs_count + ellipsis_glyph_count;

							SKB_ARRAY_RESERVE(layout->glyphs, layout->glyphs_count + ellipsis_glyph_count);

							for (int32_t ei = 0; ei < ellipsis_glyph_count; ei++) {
								skb_glyph_t* glyph = &layout->glyphs[layout->glyphs_count++];
								glyph->offset_x = 0.f;
								glyph->offset_y = offset_y;
								glyph->advance_x = ellipsis_x_advance;
								glyph->text_range = (skb_range_t){0};
								glyph->gid = (uint16_t)ellipsis_gid;
								glyph->span_idx = span_idx;
								glyph->font_handle = font_handle;

								line->bounds.width += ellipsis_x_advance;
							}
						}
					}
				}
			}
		}

		line->glyph_run_range.start = layout->glyph_runs_count;

		// Create glyph runs.
		// Each glyph run has same attributes and font (same attributes may lead to different font due to script).

		if (layout_is_rtl) {
			SKB_ARRAY_RESERVE(layout->glyph_runs, layout->glyph_runs_count+1);
			layout->glyph_runs[layout->glyph_runs_count++] = ellipsis_run;
		}

		if (glyph_range.start != glyph_range.end) {

			SKB_ARRAY_RESERVE(layout->glyph_runs, layout->glyph_runs_count+1);
			skb_glyph_run_t* glyph_run = &layout->glyph_runs[layout->glyph_runs_count++];
			skb__init_glyph_run(layout, glyph_run, glyph_range.start);

			for (int32_t gi = glyph_range.start + 1; gi < glyph_range.end; gi++) {
				const skb_glyph_t* glyph = &layout->glyphs[gi];
				if (glyph->font_handle != glyph_run->font_handle || glyph->span_idx != glyph_run->span_idx) {
					// Close current run
					glyph_run->glyph_range.end = gi;
					// Start new run
					SKB_ARRAY_RESERVE(layout->glyph_runs, layout->glyph_runs_count+1);
					glyph_run = &layout->glyph_runs[layout->glyph_runs_count++];
					skb__init_glyph_run(layout, glyph_run, gi);
				}
			}
			glyph_run->glyph_range.end = glyph_range.end;
		}

		if (!layout_is_rtl) {
			SKB_ARRAY_RESERVE(layout->glyph_runs, layout->glyph_runs_count+1);
			layout->glyph_runs[layout->glyph_runs_count++] = ellipsis_run;
		}

		line->glyph_run_range.end = layout->glyph_runs_count;

		calculated_width = skb_maxf(calculated_width, line->bounds.width);
	}

	if (layout->params.vertical_trim == SKB_VERTICAL_TRIM_CAP_TO_BASELINE) {
		// Adjust the calculated_height so that first line only accounts for cap height (not all the way to ascender), and last line does not count descender.
		const float first_line_ascender = layout->lines[0].ascender;
		const float height_diff = first_line_cap_height - first_line_ascender; // Note: cap height and ascender are negative.
		calculated_height -= height_diff;
		const float last_line_descender = layout->lines[layout->lines_count-1].descender;
		calculated_height -= last_line_descender;
	}

	// Align layout
	layout->bounds.x = origin.x + skb__calc_align(skb_get_directional_align(layout_is_rtl, layout->params.horizontal_align), calculated_width, layout->params.layout_width);
	layout->bounds.y = origin.y + skb__calc_align(layout->params.vertical_align, calculated_height, layout->params.layout_height);
	layout->bounds.width = calculated_width;
	layout->bounds.height = calculated_height;

	float start_y = layout->bounds.y;

	if (layout->params.vertical_trim == SKB_VERTICAL_TRIM_CAP_TO_BASELINE) {
		// Adjust start position so that the top is aligned to cap height.
		const float first_line_ascender = layout->lines[0].ascender;
		const float height_diff = first_line_cap_height - first_line_ascender; // Note: cap height and ascender are negative.
		start_y -= height_diff;
	}

	// Align lines
	for (int32_t li = 0; li < layout->lines_count; li++) {
		skb_layout_line_t* line = &layout->lines[li];

		float start_x = 0.f;
		if (layout_is_rtl) {
			// Prune space used by whitespace or control characters.
			float whitespace_width = 0.f;
			if (line->glyph_run_range.start != line->glyph_run_range.end) {
				const skb_glyph_run_t* glyph_run = &layout->glyph_runs[line->glyph_run_range.start];
				for (int32_t gi = glyph_run->glyph_range.start; gi < glyph_run->glyph_range.end; gi++) {
					skb_glyph_t* glyph = &layout->glyphs[gi];
					if ((layout->text_props[glyph->text_range.start].flags & SKB_TEXT_PROP_WHITESPACE) || (layout->text_props[glyph->text_range.start].flags & SKB_TEXT_PROP_CONTROL))
						whitespace_width += glyph->advance_x;
					else
						break;
				}
			}
			line->bounds.width -= whitespace_width;
			line->bounds.x = layout->bounds.x + skb__calc_align(skb_get_directional_align(layout_is_rtl, layout->params.horizontal_align), line->bounds.width, calculated_width);
			start_x = line->bounds.x - whitespace_width;

		} else {
			// Prune space used by whitespace or control characters.
			float whitespace_width = 0.f;
			if (line->glyph_run_range.start != line->glyph_run_range.end) {
				const skb_glyph_run_t* glyph_run = &layout->glyph_runs[line->glyph_run_range.end - 1];
				for (int32_t gi = glyph_run->glyph_range.end-1; gi >= glyph_run->glyph_range.start; gi--) {
					skb_glyph_t* glyph = &layout->glyphs[gi];
					if ((layout->text_props[glyph->text_range.start].flags & SKB_TEXT_PROP_WHITESPACE) || (layout->text_props[glyph->text_range.start].flags & SKB_TEXT_PROP_CONTROL))
						whitespace_width += glyph->advance_x;
					else
						break;
				}
			}
			line->bounds.width -= whitespace_width;
			line->bounds.x = layout->bounds.x + skb__calc_align(skb_get_directional_align(layout_is_rtl, layout->params.horizontal_align), line->bounds.width, calculated_width);
			start_x = line->bounds.x;
		}

		line->bounds.y = start_y;

		// Update glyph offsets
		const float leading = line->bounds.height - (-line->ascender + line->descender);
		const float leading_above = leading * 0.5f;
		line->baseline = line->bounds.y + leading_above - line->ascender;

		float cur_x = start_x;
		for (int32_t ri = line->glyph_run_range.start; ri < line->glyph_run_range.end; ri++) {
			skb_glyph_run_t* glyph_run = &layout->glyph_runs[ri];
			glyph_run->baseline += line->baseline;
			for (int32_t j = glyph_run->glyph_range.start; j < glyph_run->glyph_range.end; j++) {
				skb_glyph_t* glyph = &layout->glyphs[j];
				glyph->offset_x += cur_x;
				glyph->offset_y += line->baseline;
				cur_x += glyph->advance_x;
			}
		}

		start_y += line->bounds.height;
	}

	// Build decorations.
	layout->decorations_count = 0;

	for (int32_t li = 0; li < layout->lines_count; li++) {
		skb_layout_line_t* line = &layout->lines[li];

		line->decorations_range.start = layout->decorations_count;

		// Iterate over glyphs of same attribute span.
		for (int32_t ri = line->glyph_run_range.start; ri < line->glyph_run_range.end; ri++) {

			// Find range of glyphs runs that share same attribute span.
			int32_t first_run = ri;
			int32_t last_run = ri;
			const uint16_t span_idx = layout->glyph_runs[first_run].span_idx;
			while (last_run+1 < line->glyph_run_range.end && layout->glyph_runs[last_run+1].span_idx == span_idx)
				last_run++;
			ri = last_run;

			// For each decoration.
			const skb_text_attributes_span_t* span = &layout->attribute_spans[span_idx];
			const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);

			for (int32_t attr_idx = 0; attr_idx < span->attributes_count; attr_idx++) {
				const skb_attribute_t* attribute = &span->attributes[attr_idx];
				if (attribute->kind != SKB_ATTRIBUTE_DECORATION)
					continue;
				const skb_attribute_decoration_t attr_decoration = attribute->decoration;

				// Find line position.
				float line_position = 0.f; // At baseline
				float line_position_div = 0.0f;
				float thickness = 0.f;
				float thickness_div = 0.f;

				// Calculate the position of the line.
				const float ref_baseline = layout->glyph_runs[first_run].baseline;
				skb_font_handle_t prev_font_handle = 0;
				for (int32_t sri = first_run; sri <= last_run; sri++) {
					const skb_glyph_run_t* glyph_run = &layout->glyph_runs[sri];

					if (glyph_run->font_handle != prev_font_handle) {
						const skb_font_t* font = skb_font_collection_get_font(layout->params.font_collection, glyph_run->font_handle);
						if (font) {
							const float delta_y = glyph_run->baseline - ref_baseline;
							if (attr_decoration.position == SKB_DECORATION_UNDERLINE) {
								line_position = skb_maxf(line_position, delta_y + font->metrics.underline_offset * attr_font.size);
								thickness += font->metrics.underline_size * attr_font.size;
							} else if (attr_decoration.position == SKB_DECORATION_BOTTOMLINE) {
								line_position = skb_maxf(line_position, delta_y + font->metrics.descender * attr_font.size);
								thickness += font->metrics.underline_size * attr_font.size;
							} else if (attr_decoration.position == SKB_DECORATION_OVERLINE) {
								line_position = skb_minf(line_position, delta_y + font->metrics.ascender * attr_font.size);
								thickness += font->metrics.underline_size * attr_font.size;
							} else if (attr_decoration.position == SKB_DECORATION_THROUGHLINE) {
								line_position += delta_y + font->metrics.strikeout_offset * attr_font.size;
								line_position_div += 1.0f;
								thickness += font->metrics.strikeout_size * attr_font.size;
							}
							thickness_div += 1.f;
						}
						prev_font_handle = glyph_run->font_handle;
					}
				}

				// Average position if requested.
				if (line_position_div > 0.f)
					line_position /= line_position_div;

				// Use thickness from the attribute if specified, or calculate average of thickness based on font data.
				if (attr_decoration.thickness > 0.f) {
					thickness = attr_decoration.thickness;
				} else {
					if (thickness_div > 0.f)
						thickness /= thickness_div;
				}

				// Apply offset.
				if (attr_decoration.position == SKB_DECORATION_UNDERLINE || attr_decoration.position == SKB_DECORATION_BOTTOMLINE)
					line_position += attr_decoration.offset;
				else if (attr_decoration.position == SKB_DECORATION_THROUGHLINE || attr_decoration.position == SKB_DECORATION_OVERLINE)
					line_position -= attr_decoration.offset;

				// Find first and last glyph, omit white space at the end of the line.
				skb_range_t first_run_range = layout->glyph_runs[first_run].glyph_range;
				skb_range_t last_run_range = layout->glyph_runs[last_run].glyph_range;
				if (layout_is_rtl)
					skb__prune_line_end_whitespace(layout, &first_run_range, NULL);
				else
					skb__prune_line_end_whitespace(layout, &last_run_range, NULL);
				const skb_glyph_t* first_glyph = &layout->glyphs[first_run_range.start];
				const skb_glyph_t* last_glyph = &layout->glyphs[last_run_range.end-1];

				// Add decoration
				SKB_ARRAY_RESERVE(layout->decorations, layout->decorations_count + 1);
				skb_decoration_t* decoration = &layout->decorations[layout->decorations_count++];
				decoration->offset_x = first_glyph->offset_x;
				decoration->offset_y = ref_baseline + line_position;
				decoration->length = last_glyph->offset_x - first_glyph->offset_x + last_glyph->advance_x;
				decoration->pattern_offset = first_glyph->offset_x - origin.x;
				decoration->thickness = thickness;
				decoration->span_idx = span_idx;
				decoration->attribute_idx = (uint16_t)attr_idx;
			}
		}

		line->decorations_range.end = layout->decorations_count;
	}
}

typedef struct skb__script_run_iter_t {
	const skb_text_property_t* text_props;
	int32_t pos;
	int32_t end;
} skb__script_run_iter_t;

static skb__script_run_iter_t skb__script_run_iter_make(skb_range_t range, const skb_text_property_t* text_attribs)
{
	skb__script_run_iter_t iter = {
		.text_props = text_attribs,
		.pos = range.start,
		.end = range.end,
	};
	return iter;
}

static bool skb__script_run_iter_next(skb__script_run_iter_t* iter, skb_range_t* run_range, uint8_t* run_script)
{
	if (iter->pos == iter->end)
		return false;

	run_range->start = iter->pos;

	// Find continuous script range.
	uint8_t prev_script = iter->text_props[iter->pos].script;
	while (iter->pos < iter->end) {
		iter->pos++;
		const uint8_t script = iter->pos < iter->end ? iter->text_props[iter->pos].script : 0;
		if (prev_script != script)
			break;
		prev_script = script;
	}

	*run_script = prev_script;
	run_range->end = iter->pos;

	return true;
}


typedef struct skb__text_style_run_iter_t {
	skb_range_t range;
	bool is_rtl;
	int32_t span_idx;
	int32_t span_end;
	const skb_text_attributes_span_t* spans;
	int32_t spans_count;
} skb__text_style_run_iter_t;

static skb__text_style_run_iter_t skb__text_style_run_iter_make(skb_range_t range, bool is_rtl, const skb_text_attributes_span_t* spans, int32_t spans_count)
{
	skb__text_style_run_iter_t iter = {
		.range = range,
		.is_rtl = is_rtl,
		.spans = spans,
		.spans_count = spans_count
	};

	if (is_rtl) {
		iter.span_idx = spans_count - 1;
		iter.span_end = -1;
	} else {
		iter.span_idx = 0;
		iter.span_end = spans_count;
	}

	return iter;
}

static bool skb__text_style_run_iter_next(skb__text_style_run_iter_t* iter, skb_range_t* range, int32_t* range_span_idx)
{
	if (iter->span_idx == iter->span_end)
		return false;

	if (iter->is_rtl) {
		// Reverse if RTL
		while (iter->span_idx > iter->span_end) {
			const skb_text_attributes_span_t* span = &iter->spans[iter->span_idx];
			if (span->text_range.end <= iter->range.start) {
				iter->span_idx = iter->span_end;
				return false;
			}
			const skb_range_t shaping_range = {
				.start = skb_maxi(iter->range.start, span->text_range.start),
				.end = skb_mini(iter->range.end, span->text_range.end),
			};
			iter->span_idx--;
			if (shaping_range.start < shaping_range.end) {
				*range = shaping_range;
				*range_span_idx = iter->span_idx + 1;
				return true;
			}
		}
	} else {
		// Forward if RTL
		while (iter->span_idx < iter->span_end) {
			const skb_text_attributes_span_t* span = &iter->spans[iter->span_idx];
			if (span->text_range.start > iter->range.end) {
				iter->span_idx = iter->span_end;
				return false;
			}
			const skb_range_t shaping_range = {
				.start = skb_maxi(iter->range.start, span->text_range.start),
				.end = skb_mini(iter->range.end, span->text_range.end),
			};
			iter->span_idx++;
			if (shaping_range.start < shaping_range.end) {
				*range = shaping_range;
				*range_span_idx = iter->span_idx - 1;
				return true;
			}
		}
	}

	return false;
}


static void skb__itemize(skb__layout_build_context_t* build_context, skb_layout_t* layout)
{
	if (!layout->text_count)
		return;

	SBLevel base_level = SBLevelDefaultLTR;
	if (layout->params.base_direction == SKB_DIRECTION_RTL)
		base_level = 1;
	else if (layout->params.base_direction == SKB_DIRECTION_LTR)
		base_level = 0;

	SBCodepointSequence codepoint_seq = { SBStringEncodingUTF32, layout->text, layout->text_count };

	// Resolve scripts for codepoints.
	SBScriptLocatorRef script_locator = SBScriptLocatorCreate();
	SBScriptLocatorLoadCodepoints(script_locator, &codepoint_seq);
	while (SBScriptLocatorMoveNext(script_locator)) {
		const SBScriptAgent* agent = SBScriptLocatorGetAgent(script_locator);
		const int32_t run_start = (int32_t)agent->offset;
		const int32_t run_end = (int32_t)(agent->offset + agent->length);
		for (int32_t i = run_start; i < run_end; i++)
			layout->text_props[i].script = agent->script;
	}
	SBScriptLocatorRelease(script_locator);

	// Special case, the text starts with common script, look forward to find the first non-implicit script.
	if (layout->text_count && layout->text_props[0].script == SB_SCRIPT_COMMON) {
		uint8_t prev_script = SBScriptLATN; // Fallback to latin as backup.
		int32_t run_end = 0;
		while (run_end < layout->text_count) {
			if (layout->text_props[run_end].script != SB_SCRIPT_INHERITED && layout->text_props[run_end].script != SB_SCRIPT_COMMON) {
				prev_script = layout->text_props[run_end].script;
				break;
			}
			run_end++;
		}
		for (int32_t i = 0; i < run_end; i++)
			layout->text_props[i].script = prev_script;
	}
	// Inherited and common scripts get resolved to the previous script type.
	for (int32_t i = 1; i < layout->text_count; i++) {
		if (layout->text_props[i].script == SB_SCRIPT_INHERITED || layout->text_props[i].script == SB_SCRIPT_COMMON) {
			layout->text_props[i].script = layout->text_props[i - 1].script;
		}
	}

	build_context->emoji_types_buffer = SKB_TEMP_ALLOC(build_context->temp_alloc, uint8_t, layout->text_count);

	// Iterate over the text until we have processed all paragraphs.
	SBAlgorithmRef bidi_algorithm = SBAlgorithmCreate(&codepoint_seq);
	int32_t paragraph_start = 0;
	while (paragraph_start < layout->text_count) {
		const SBParagraphRef bidi_paragraph = SBAlgorithmCreateParagraph(bidi_algorithm, paragraph_start, INT32_MAX, base_level);
		const int32_t paragraph_length = (int32_t)SBParagraphGetLength(bidi_paragraph);

		// The overal text direction is taken from the first paragraph.
		if (paragraph_start == 0)
			layout->resolved_direction = (SBParagraphGetBaseLevel(bidi_paragraph) & 1) ? SKB_DIRECTION_RTL : SKB_DIRECTION_LTR;

		// Iterate over all the bidi runs.
		const SBLineRef bidi_line = SBParagraphCreateLine(bidi_paragraph, paragraph_start, paragraph_length);
		const SBRun* bidi_line_runs = SBLineGetRunsPtr(bidi_line);
		const int32_t bidi_line_runs_count = (int32_t)SBLineGetRunCount(bidi_line);

		for (int32_t i = 0; i < bidi_line_runs_count; ++i) {
			const SBRun* bidi_run = &bidi_line_runs[i];
			const skb_range_t bidi_range = { .start = (int32_t)bidi_run->offset, .end = (int32_t)(bidi_run->offset + bidi_run->length) };
			const uint8_t bidi_direction = (bidi_run->level & 1) ? SKB_DIRECTION_RTL : SKB_DIRECTION_LTR;

			// Split bidi runs at shaping style span boundaries.
			skb__text_style_run_iter_t style_iter = skb__text_style_run_iter_make(bidi_range, skb_is_rtl(bidi_direction), layout->attribute_spans, layout->attribute_spans_count);

			skb_range_t style_range = {0};
			int32_t style_span_idx = 0;
			while (skb__text_style_run_iter_next(&style_iter, &style_range, &style_span_idx)) {
				const skb_text_attributes_span_t* span = &layout->attribute_spans[style_span_idx];

				const skb_attribute_writing_t attr_writing = skb_attributes_get_writing(span->attributes, span->attributes_count);
				const uint8_t style_direction = attr_writing.direction != SKB_DIRECTION_AUTO ? attr_writing.direction : bidi_direction;

				// Process the rest of the itemization forward, and reverse the sequence of runs once completed.
				int32_t first_run = build_context->shaping_runs_count;

				// Split the style run into runs of same script.
				skb__script_run_iter_t script_iter = skb__script_run_iter_make(style_range, layout->text_props);

				skb_range_t script_range = {0};
				uint8_t script = 0;

				while (skb__script_run_iter_next(&script_iter, &script_range, &script)) {
					// Split script range into sequences of emojis or text.
					skb_emoji_run_iterator_t emoji_iter = skb_emoji_run_iterator_make(script_range, layout->text, build_context->emoji_types_buffer);
					skb_range_t emoji_range = {0};
					bool has_emoji = false;
					while (skb_emoji_run_iterator_next(&emoji_iter, &emoji_range, &has_emoji)) {
						SKB_TEMP_RESERVE(build_context->temp_alloc, build_context->shaping_runs, build_context->shaping_runs_count + 1);
						skb__shaping_run_t* shaping_run = &build_context->shaping_runs[build_context->shaping_runs_count++];
						shaping_run->script = script;
						shaping_run->offset = emoji_range.start;
						shaping_run->length = emoji_range.end - emoji_range.start;
						shaping_run->direction = style_direction;
						shaping_run->is_emoji = has_emoji;
						shaping_run->shaping_span_idx = style_span_idx;
					}
				}

				// Reverse shaping runs if RTL. The runs are processed forward for simplicity.
				if (skb_is_rtl(style_direction)) {
					int32_t last_run = build_context->shaping_runs_count - 1;
					while (first_run < last_run) {
						skb__shaping_run_t tmp = build_context->shaping_runs[first_run];
						build_context->shaping_runs[first_run] = build_context->shaping_runs[last_run];
						build_context->shaping_runs[last_run] = tmp;
						first_run++;
						last_run--;
					}
				}
			}
 		}

		SBLineRelease(bidi_line);
		SBParagraphRelease(bidi_paragraph);

		paragraph_start += paragraph_length;
	}

	SBAlgorithmRelease(bidi_algorithm);

#if 0
	// The unicode bidi algorithm assigns the space after opposite direction run to the outer level.
	//
	// For example (arranged in visual order):
	//    latin CIBARA foo
	// will be split to bidi runs:
	//    "lating ", "CIBARA", " foo"
	//
	// If the user wants to add to the ARABIC text by placing the caret after C, and pressed space,
	// the space will be assigned to the last run (now "  foo"), and the caret will jump visually to the beginning of the line.
	//
	// The code below tries to borrow the extra space to the opposite direction run, allowing the user to type uninterrupted.
	// Note that, many line breaking implementations produce similar visual double spaces when opposite direction run is split across lines.
	//
	if (0) {
		for (int32_t ri = 1; ri < build_context->shaping_runs_count; ri++) {
			skb__shaping_run_t* cur_run = &build_context->shaping_runs[ri];

			// Find runs that have more than one white space, or one white space at the end of line.
			if (!(layout->text_props[cur_run->offset].flags & SKB_TEXT_PROP_WHITESPACE))
				continue;
			if (!((cur_run->offset + 1 >= layout->text_count) || (layout->text_props[cur_run->offset+1].flags & SKB_TEXT_PROP_WHITESPACE)))
				continue;

			// Find previous adjacent run.
			skb__shaping_run_t* prev_run = NULL;
			int32_t prev_run_idx = ri - 1;
			while (prev_run_idx > 0) {
				skb__shaping_run_t* run = &build_context->shaping_runs[prev_run_idx];
				if (run->shaping_span_idx != cur_run->shaping_span_idx)
					break;
				if ((run->length + run->offset) == cur_run->offset) {
					prev_run = run;
					break;
				}
				prev_run_idx--;
			}
			if (!prev_run)
				continue;

			if (prev_run->script != cur_run->script)
				continue;

			// Move the first white space to the previous run.
			prev_run->length++;

			cur_run->offset++;
			cur_run->length--;
		}
	}
#endif
}

static void skb__override_line_breaks(skb_layout_t* layout, int32_t start_offset, int32_t end_offset, boundary_iterator_t iter)
{
	// Override line breaks.
	for (int32_t j = start_offset; j < end_offset; j++)
		layout->text_props[j].flags &= ~SKB_TEXT_PROP_ALLOW_LINE_BREAK;

	int32_t range_start = 0, range_end = 0;
	while (boundary_iterator_next(&iter, &range_start, &range_end)) {
		// Include white space after the word to be consistent with unibreak.
		// The iterator is fed a substring starting at start_offset, we will need to adjust the iterator to start with start_offset.
		int32_t offset = start_offset + range_end-1;
		while ((offset+1) < layout->text_count && (layout->text_props[offset].flags & SKB_TEXT_PROP_WHITESPACE))
			offset++;
		layout->text_props[offset].flags |= SKB_TEXT_PROP_ALLOW_LINE_BREAK;
	}
}


static void skb__apply_lang_based_word_breaks(const skb__layout_build_context_t* build_context, skb_layout_t* layout)
{
	// Language based word breaks. These are applied only to specific sections of script.
	const hb_language_t lang_ja = hb_language_from_string("ja", 2);
	const hb_language_t lang_zh_hant = hb_language_from_string("zh-hant", 7);
	const hb_language_t lang_zh_hans = hb_language_from_string("zh-hans", 7);
	const hb_language_t lang_th = hb_language_from_string("th", 2);

	for (int32_t i = 0; i < build_context->shaping_runs_count; ++i) {
		const skb__shaping_run_t* run = &build_context->shaping_runs[i];
		const skb_text_attributes_span_t* span = &layout->attribute_spans[run->shaping_span_idx];

		const skb_attribute_writing_t attr_writing = skb_attributes_get_writing(span->attributes, span->attributes_count);
		const hb_language_t run_lang = attr_writing.lang ? hb_language_from_string(attr_writing.lang, -1) : build_context->lang;

		if (skb__is_japanese_script(run->script) && hb_language_matches(lang_ja, run_lang)) {
			// Merge supported runs into one longer one.
			const int32_t start = run->offset;
			while ((i+1) < build_context->shaping_runs_count && skb__is_japanese_script(build_context->shaping_runs[i+1].script))
				i++;
			const int32_t end = build_context->shaping_runs[i].offset + build_context->shaping_runs[i].length;
			boundary_iterator_t iter = boundary_iterator_init_ja_utf32(layout->text + start, end - start);
			skb__override_line_breaks(layout, start, end, iter);
		} else if (run->script == SBScriptHANI && (hb_language_matches(lang_zh_hant, run_lang) || hb_language_matches(lang_zh_hans, run_lang))) {
			const int32_t start = run->offset;
			const int32_t end = run->offset + run->length;
			boundary_iterator_t iter = {0};
			if (hb_language_matches(run_lang, lang_zh_hans))
				iter = boundary_iterator_init_zh_hans_utf32(layout->text + start, end - start);
			else
				iter = boundary_iterator_init_zh_hant_utf32(layout->text + start, end - start);
			skb__override_line_breaks(layout, start, end, iter);
		} else if (run->script == SBScriptTHAI && hb_language_matches(lang_th, run_lang)) {
			const int32_t start = run->offset;
			const int32_t end = run->offset + run->length;
			boundary_iterator_t iter = boundary_iterator_init_th_utf32(layout->text + start, end - start);
			skb__override_line_breaks(layout, start, end, iter);
		}
	}
}


static bool skb__allow_letter_spacing(uint8_t script)
{
	// These scripts have cursive connection, and can't handle letter spacing.
	switch (script) {
		case SBScriptARAB:	// Arabic
		case SBScriptNKOO:	// Nko
		case SBScriptPHLP:	// Psalter_Pahlavi
		case SBScriptMAND:	// Mandaic
		case SBScriptMONG:	// Mongolian
		case SBScriptPHAG:	// Phags_Pa
		case SBScriptDEVA:	// Devanagari
		case SBScriptBENG:	// Bengali
		case SBScriptGURU:	// Gurmukhi
		case SBScriptMODI:	// Modi
		case SBScriptSHRD:	// Sharada
		case SBScriptSYLO:	// Syloti_Nagric
		case SBScriptTIRH:	// Tirhuta
		case SBScriptOGAM:	// Ogham
			return false;
	}
	return true;
}

static void skb__build_layout(skb_layout_t* layout, skb_temp_alloc_t* temp_alloc)
{
	skb__layout_build_context_t build_context = {0};
	build_context.lang = hb_language_from_string(layout->params.lang, -1);
	build_context.temp_alloc = temp_alloc;

	// Itemize text into runs of same direction and script. A run of emojis is treated the same as script.
	skb__itemize(&build_context, layout);

	// Apply run attribs to text properties
	for (int32_t i = 0; i < build_context.shaping_runs_count; ++i) {
		const skb__shaping_run_t* run = &build_context.shaping_runs[i];
		for (int32_t j = 0; j < run->length; j++) {
			SKB_SET_FLAG(layout->text_props[run->offset + j].flags, SKB_TEXT_PROP_EMOJI, run->is_emoji);
			layout->text_props[run->offset + j].direction = run->direction;
			layout->text_props[run->offset + j].script = run->script;
		}
	}

	// Handle word breaks for languages what do not have word break characters.
	skb__apply_lang_based_word_breaks(&build_context, layout);

	// Shape runs
	layout->glyphs_count = 0;

	hb_buffer_t* buffer = hb_buffer_create();
	for (int32_t i = 0; i < build_context.shaping_runs_count; ++i) {
		const skb__shaping_run_t* run = &build_context.shaping_runs[i];
		const skb_text_attributes_span_t* span = &layout->attribute_spans[run->shaping_span_idx];

		// Check if this run is a replacement object.
		if (span->flags & SKB_TEXT_ATTRIBUTES_SPAN_HAS_OBJECT) {
			// Add the replacement object as a glyph.
			SKB_ARRAY_RESERVE(layout->glyphs, layout->glyphs_count + 1);

			skb_attribute_object_t object = skb_attributes_get_object(span->attributes, span->attributes_count);

			skb_glyph_t* glyph = &layout->glyphs[layout->glyphs_count++];
			glyph->gid = 0;
			glyph->offset_x = 0.f;
			glyph->offset_y = 0.f;
			glyph->advance_x = object.width;
			glyph->font_handle = 0;
			glyph->text_range.start = run->offset;
			glyph->text_range.end = run->offset + run->length;
			glyph->visual_idx = layout->glyphs_count - 1;
			glyph->span_idx = (uint16_t)run->shaping_span_idx;

		} else {
			const skb_attribute_font_t attr_font = skb_attributes_get_font(span->attributes, span->attributes_count);
			const skb_attribute_writing_t attr_writing = skb_attributes_get_writing(span->attributes, span->attributes_count);

			const uint8_t font_family = run->is_emoji ? SKB_FONT_FAMILY_EMOJI : attr_font.family;
			const hb_language_t run_lang = attr_writing.lang ? hb_language_from_string(attr_writing.lang, -1) : build_context.lang;

			skb_font_handle_t fonts[32];
			int32_t fonts_count = skb_font_collection_match_fonts(
				layout->params.font_collection, hb_language_to_string(run_lang), run->script, font_family,
				attr_font.weight, attr_font.style, attr_font.stretch,
				fonts, SKB_COUNTOF(fonts));

			if (fonts_count == 0) {
				// If not fonts found, try the font family's default font.
				fonts[0] = skb_font_collection_get_default_font(layout->params.font_collection, font_family);
				// If still not found, there's nothing we can do, so continue to next run.
				if (!fonts[0])
					continue;
				fonts_count++;
			}

			hb_buffer_clear_contents(buffer);
			skb__shape_run(&build_context, layout, run, span, buffer, fonts, fonts_count, 0);
		}
	}
	hb_buffer_destroy(buffer);

	// There are freed in the order they are allocated so that the allocations get unwound.
	SKB_TEMP_FREE(build_context.temp_alloc, build_context.shaping_runs);
	SKB_TEMP_FREE(build_context.temp_alloc, build_context.emoji_types_buffer);

	// Apply letter and word spacing
	for (int32_t gi = 0; gi < layout->glyphs_count; gi++) {
		skb_glyph_t* glyph = &layout->glyphs[gi];
		const int32_t next_gi = gi + 1;

		// Apply spacing at the end of a glyph cluster.
		if (next_gi >= layout->glyphs_count || layout->glyphs[next_gi].text_range.start != glyph->text_range.start) {
			const skb_text_attributes_span_t* span = &layout->attribute_spans[glyph->span_idx];
			const skb_attribute_spacing_t attr_spacing = skb_attributes_get_spacing(span->attributes, span->attributes_count);

			// Apply letter spacing for each grapheme.
			skb_text_property_t text_props = layout->text_props[glyph->text_range.end-1];
			if (text_props.flags & SKB_TEXT_PROP_GRAPHEME_BREAK) {
				if ((text_props.flags & SKB_TEXT_PROP_WHITESPACE) || skb__allow_letter_spacing(text_props.script))
					glyph->advance_x += attr_spacing.letter;
			}

			// Apply word spacing for each white space.
			if (layout->text_props[glyph->text_range.end-1].flags & SKB_TEXT_PROP_WHITESPACE)
				glyph->advance_x += attr_spacing.word;
		}
	}

	// Break layout to lines.
	skb__layout_lines(layout, temp_alloc);
}



static bool skb__lang_equals(const char* lhs, const char* rhs)
{
	if (!lhs && !rhs)
		return true;
	if (!lhs || !rhs)
		return false;
	// Not using hb_language_matches(), as we want to check for exact maths.
	return hb_language_from_string(lhs, -1) == hb_language_from_string(rhs, -1);
}

static bool skb__attribs_equals(const skb_text_attributes_span_t* span, const skb_attribute_t* attributes, int32_t attributes_count)
{
	if (span->attributes_count != attributes_count)
		return false;

	// Expect exact match of attributes.
	for (int32_t i = 0; i < attributes_count; i++) {
		const skb_attribute_t* lhs = &span->attributes[i];
		const skb_attribute_t* rhs = &attributes[i];
		if (lhs->kind != rhs->kind)
			return false;
		if (lhs->kind == SKB_ATTRIBUTE_WRITING) {
			// Language needs special comparison.
			if (lhs->writing.direction != rhs->writing.direction)
				return false;
			if (!skb__lang_equals(lhs->writing.lang, rhs->writing.lang))
				return false;
		} else {
			// Generic compare (Note: the attributes are fully zero initialized, including padding)
			if (memcmp(lhs, rhs, sizeof(skb_attribute_t)) != 0)
				return false;
		}
	}
	return true;
}

static void skb__append_attributes(skb_layout_t* layout, const skb_attribute_t* attributes, int32_t attributes_count, int32_t text_start, int32_t text_end)
{
	// If the style is the same as previous span, combine.
	if (layout->attribute_spans_count > 0 && skb__attribs_equals(&layout->attribute_spans[layout->attribute_spans_count - 1], attributes, attributes_count)) {
		layout->attribute_spans[layout->attribute_spans_count - 1].text_range.end = layout->text_count;
	} else {
		SKB_ARRAY_RESERVE(layout->attribute_spans, layout->attribute_spans_count + 1);
		skb_text_attributes_span_t* span = &layout->attribute_spans[layout->attribute_spans_count++];
		span->text_range.start = text_start;
		span->text_range.end = text_end;

		SKB_ARRAY_RESERVE(layout->attributes, layout->attributes_count + attributes_count);
        span->attributes = &layout->attributes[layout->attributes_count];
        span->attributes_count = attributes_count;

        memcpy(span->attributes, attributes, attributes_count * sizeof(skb_attribute_t));

		span->flags = 0;
		for (int32_t i = 0; i < attributes_count; i++) {
			if (attributes[i].kind == SKB_ATTRIBUTE_OBJECT)
				span->flags |= SKB_TEXT_ATTRIBUTES_SPAN_HAS_OBJECT;
		}

        layout->attributes_count += attributes_count;
	}
}

static void skb__reserve_text(skb_layout_t* layout, int32_t text_count)
{
	if (text_count > layout->text_cap) {
		layout->text_cap = text_count;
		layout->text = skb_realloc(layout->text, layout->text_cap * sizeof(uint32_t));
		assert(layout->text);
		layout->text_props = skb_realloc(layout->text_props, layout->text_cap * sizeof(skb_text_property_t));
		assert(layout->text_props);
	}
}

skb_layout_t* skb_layout_create(const skb_layout_params_t* params)
{
	skb_layout_t* layout = skb_malloc(sizeof(skb_layout_t));
	memset(layout, 0, sizeof(skb_layout_t));

	layout->params = *params;

	return layout;
}

skb_layout_t* skb_layout_create_utf8(skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const char* text, int32_t text_count, const skb_attribute_t* attributes, int32_t attributes_count)
{
	skb_text_run_utf8_t run = {
		.text = text,
		.text_count = text_count,
		.attributes = attributes,
		.attributes_count = attributes_count,
	};
	return skb_layout_create_from_runs_utf8(temp_alloc, params, &run, 1);
}

skb_layout_t* skb_layout_create_utf32(skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const uint32_t* text, int32_t text_count, const skb_attribute_t* attributes, int32_t attributes_count)
{
	skb_text_run_utf32_t run = {
		.text = text,
		.text_count = text_count,
		.attributes = attributes,
		.attributes_count = attributes_count
	};
	return skb_layout_create_from_runs_utf32(temp_alloc, params, &run, 1);
}

skb_layout_t* skb_layout_create_from_runs_utf8(skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const skb_text_run_utf8_t* runs, int32_t runs_count)
{
	skb_layout_t* layout = skb_layout_create(params);
	skb_layout_set_from_runs_utf8(layout, temp_alloc, params, runs, runs_count);
	return layout;
}

skb_layout_t* skb_layout_create_from_runs_utf32(skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const skb_text_run_utf32_t* runs, int32_t runs_count)
{
	skb_layout_t* layout = skb_layout_create(params);
	skb_layout_set_from_runs_utf32(layout, temp_alloc, params, runs, runs_count);
	return layout;
}

void skb_layout_set_utf8(skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const char* text, int32_t text_count, const skb_attribute_t* attributes, int32_t attributes_count)
{
	skb_text_run_utf8_t run = {
		.text = text,
		.text_count = text_count,
		.attributes = attributes,
		.attributes_count = attributes_count,
	};
	skb_layout_set_from_runs_utf8(layout, temp_alloc, params, &run, 1);
}

void skb_layout_set_utf32(skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const uint32_t* text, int32_t text_count, const skb_attribute_t* attributes, int32_t attributes_count)
{
	skb_text_run_utf32_t run = {
		.text = text,
		.text_count = text_count,
		.attributes = attributes,
		.attributes_count = attributes_count,
	};
	skb_layout_set_from_runs_utf32(layout, temp_alloc, params, &run, 1);
}

void skb_layout_reset(skb_layout_t* layout)
{
	assert(layout);

	// Reset without freeing memory.
	layout->text_count = 0;
	layout->glyphs_count = 0;
	layout->lines_count = 0;
	layout->attribute_spans_count = 0;
	layout->attributes_count = 0;
	layout->bounds = skb_rect2_make_undefined();
	layout->resolved_direction = SKB_DIRECTION_AUTO;
}

static const char* skb__resolve_lang(const skb_layout_t* layout, const skb_text_attributes_span_t* span)
{
	const char* lang = NULL;
	for (int32_t i = 0; i < span->attributes_count; i++) {
		if (span->attributes[i].kind == SKB_ATTRIBUTE_WRITING) {
			lang = span->attributes[i].writing.lang;
			break;
		}
	}
	return lang ? lang : layout->params.lang;
}

static void skb__init_text_props_from_spans(skb_layout_t* layout, skb_temp_alloc_t* temp_alloc)
{
	// Init text props for contiguous runs of same language.
	// Some properties calculated in skb__init_text_props() depends on the text context, only language affects it.
	int32_t start_offset = 0;
	int32_t cur_offset = 0;
	const char* prev_lang = layout->params.lang;
	for (int32_t i = 0; i < layout->attribute_spans_count; i++) {
		const skb_text_attributes_span_t* span = &layout->attribute_spans[i];
		const char* lang = skb__resolve_lang(layout, span);

		if (lang != prev_lang) {
			if (cur_offset > start_offset)
				skb__init_text_props(temp_alloc, prev_lang, layout->text + start_offset, layout->text_props + start_offset, cur_offset - start_offset);
			prev_lang = lang;
			start_offset = cur_offset;
		}
		cur_offset = span->text_range.end;
	}
	if (cur_offset > start_offset)
		skb__init_text_props(temp_alloc, prev_lang, layout->text + start_offset, layout->text_props + start_offset, cur_offset - start_offset);
}


void skb_layout_set_from_runs_utf8(skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const skb_text_run_utf8_t* runs, int32_t runs_count)
{
	assert(layout);

	layout->params = *params;
	skb_layout_reset(layout);

	int32_t* text_counts = SKB_TEMP_ALLOC(temp_alloc, int32_t, runs_count);

	// Reserve memory for the text and attributes
	int32_t total_text_count = 0;
	int32_t total_attribs_count = 0;
	for (int32_t i = 0; i < runs_count; i++) {
		text_counts[i] = runs[i].text_count >= 0 ? runs[i].text_count : (int32_t)strlen(runs[i].text);
		total_text_count += text_counts[i];
		total_attribs_count += runs[i].attributes_count;
	}
	skb__reserve_text(layout, total_text_count);

	// Reserve space for spans and font features.
	SKB_ARRAY_RESERVE(layout->attribute_spans, runs_count);
	SKB_ARRAY_RESERVE(layout->attributes, total_attribs_count);

	for (int32_t i = 0; i < runs_count; i++) {
		int32_t offset = layout->text_count;
		int32_t count = skb__append_text_utf8(layout, runs[i].text, text_counts[i]);
		skb__append_attributes(layout, runs[i].attributes, runs[i].attributes_count, offset, offset + count);
	}

	skb__init_text_props_from_spans(layout, temp_alloc);

	SKB_TEMP_FREE(temp_alloc, text_counts);

	skb__build_layout(layout, temp_alloc);
}

void skb_layout_set_from_runs_utf32(skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params, const skb_text_run_utf32_t* runs, int32_t runs_count)
{
	assert(layout);

	layout->params = *params;
	skb_layout_reset(layout);

	int32_t* text_counts = SKB_TEMP_ALLOC(temp_alloc, int32_t, runs_count);

	// Reserve memory for the text
	int32_t total_text_count = 0;
	int32_t total_attribs_count = 0;
	for (int32_t i = 0; i < runs_count; i++) {
		text_counts[i] = runs[i].text_count >= 0 ? runs[i].text_count : skb_utf32_strlen(runs[i].text);
		total_text_count += text_counts[i];
		total_attribs_count += runs[i].attributes_count;
	}
	skb__reserve_text(layout, total_text_count);

	// Reserve space for spans and font features.
	SKB_ARRAY_RESERVE(layout->attribute_spans, runs_count);
	SKB_ARRAY_RESERVE(layout->attributes, total_attribs_count);

	for (int32_t i = 0; i < runs_count; i++) {
		int32_t offset = layout->text_count;
		int32_t count = skb__append_text_utf32(layout, runs[i].text, text_counts[i]);
		skb__append_attributes(layout, runs[i].attributes, runs[i].attributes_count, offset, offset + count);
	}

	skb__init_text_props_from_spans(layout, temp_alloc);

	SKB_TEMP_FREE(temp_alloc, text_counts);

	skb__build_layout(layout, temp_alloc);
}

void skb_layout_destroy(skb_layout_t* layout)
{
	if (!layout) return;

	skb_free(layout->attributes);
	skb_free(layout->attribute_spans);
	skb_free(layout->glyphs);
	skb_free(layout->glyph_runs);
	skb_free(layout->decorations);
	skb_free(layout->text);
	skb_free(layout->text_props);
	skb_free(layout);
}

const skb_layout_params_t* skb_layout_get_params(const skb_layout_t* layout)
{
	assert(layout);
	return &layout->params;
}

int32_t skb_layout_get_text_count(const skb_layout_t* layout)
{
	assert(layout);
	return layout->text_count;
}

const uint32_t* skb_layout_get_text(const skb_layout_t* layout)
{
	assert(layout);
	return layout->text;
}

const skb_text_property_t* skb_layout_get_text_properties(const skb_layout_t* layout)
{
	assert(layout);
	return layout->text_props;
}

int32_t skb_layout_get_glyph_runs_count(const skb_layout_t* layout)
{
	assert(layout);
	return layout->glyph_runs_count;
}

const skb_glyph_run_t* skb_layout_get_glyph_runs(const skb_layout_t* layout)
{
	assert(layout);
	return layout->glyph_runs;
}

int32_t skb_layout_get_glyphs_count(const skb_layout_t* layout)
{
	assert(layout);
	return layout->glyphs_count;
}

const skb_glyph_t* skb_layout_get_glyphs(const skb_layout_t* layout)
{
	assert(layout);
	return layout->glyphs;
}

int32_t skb_layout_get_decorations_count(const skb_layout_t* layout)
{
	assert(layout);
	return layout->decorations_count;
}

const skb_decoration_t* skb_layout_get_decorations(const skb_layout_t* layout)
{
	assert(layout);
	return layout->decorations;
}

int32_t skb_layout_get_lines_count(const skb_layout_t* layout)
{
	assert(layout);
	return layout->lines_count;
}

const skb_layout_line_t* skb_layout_get_lines(const skb_layout_t* layout)
{
	assert(layout);
	return layout->lines;
}

int32_t skb_layout_get_attribute_spans_count(const skb_layout_t* layout)
{
	assert(layout);
	return layout->attribute_spans_count;
}

const skb_text_attributes_span_t* skb_layout_get_attribute_spans(const skb_layout_t* layout)
{
	assert(layout);
	return layout->attribute_spans;
}

skb_rect2_t skb_layout_get_bounds(const skb_layout_t* layout)
{
	assert(layout);
	return layout->bounds;
}

skb_text_direction_t skb_layout_get_resolved_direction(const skb_layout_t* layout)
{
	assert(layout);
	return layout->resolved_direction;
}

int32_t skb_layout_next_grapheme_offset(const skb_layout_t* layout, int32_t offset)
{
	offset = skb_clampi(offset, 0, layout->text_count); // We allow one past the last codepoint as valid insertion point.

	// Find end of the current grapheme.
	while (offset < layout->text_count && !(layout->text_props[offset].flags & SKB_TEXT_PROP_GRAPHEME_BREAK))
		offset++;

	if (offset >= layout->text_count)
		return layout->text_count;

	// Step over.
	offset++;

	return offset;
}

int32_t skb_layout_prev_grapheme_offset(const skb_layout_t* layout, int32_t offset)
{
	offset = skb_clampi(offset, 0, layout->text_count); // We allow one past the last codepoint as valid insertion point.

	if (!layout->text_count)
		return offset;

	// Find begining of the current grapheme.
	if (layout->text_count) {
		while ((offset - 1) >= 0 && !(layout->text_props[offset - 1].flags & SKB_TEXT_PROP_GRAPHEME_BREAK))
			offset--;
	}

	if (offset <= 0)
		return 0;

	// Step over.
	offset--;

	// Find beginning of the previous grapheme.
	while ((offset - 1) >= 0 && !(layout->text_props[offset - 1].flags & SKB_TEXT_PROP_GRAPHEME_BREAK))
		offset--;

	return offset;
}


int32_t skb_layout_align_grapheme_offset(const skb_layout_t* layout, int32_t offset)
{
	offset = skb_clampi(offset, 0, layout->text_count); // We allow one past the last codepoint as valid insertion point.

	if (!layout->text_count)
		return offset;

	// Find beginning of the current grapheme.
	while ((offset - 1) >= 0 && !(layout->text_props[offset - 1].flags & SKB_TEXT_PROP_GRAPHEME_BREAK))
		offset--;

	if (offset <= 0)
		return 0;

	return offset;
}


skb_text_position_t skb__caret_prune_control_eol(const skb_layout_t* layout, const skb_layout_line_t* line, skb_text_position_t caret)
{
	if (layout->text_count > 0) {
		// If the caret is at the leading edge of a control character and the end of line, move it to trailing.
		// This is used for selection, mouse drag can place the caret at the "forbidden" location, but mouse click should not.
		if ((caret.affinity == SKB_AFFINITY_LEADING || caret.affinity == SKB_AFFINITY_EOL) && caret.offset == line->last_grapheme_offset) {
			if (layout->text_props[line->last_grapheme_offset].flags & SKB_TEXT_PROP_CONTROL) {
				caret.affinity = SKB_AFFINITY_TRAILING;
			}
		}
	}

	return caret;
}

int32_t skb_layout_get_line_index(const skb_layout_t* layout, skb_text_position_t pos)
{
	int32_t line_idx = SKB_INVALID_INDEX;
	for (int32_t i = 0; i < layout->lines_count; i++) {
		skb_layout_line_t* line = &layout->lines[i];
		if (pos.offset >= line->text_range.start && pos.offset < line->text_range.end) {
			line_idx = i;
			break;
		}
	}
	if (line_idx == SKB_INVALID_INDEX) {
		if (pos.offset < layout->lines[0].text_range.start)
			line_idx = 0;
		else if (pos.offset >= layout->lines[layout->lines_count-1].text_range.end)
			line_idx = layout->lines_count-1;
	}

	return line_idx;
}

int32_t skb_layout_get_text_offset(const skb_layout_t* layout, skb_text_position_t pos)
{
	if (pos.affinity == SKB_AFFINITY_LEADING || pos.affinity == SKB_AFFINITY_EOL)
		return skb_layout_next_grapheme_offset(layout, pos.offset);
	return skb_clampi(pos.offset, 0, layout->text_count);
}

skb_text_direction_t skb_layout_get_text_direction_at(const skb_layout_t* layout, skb_text_position_t pos)
{
	assert(layout);
	if (pos.offset >= 0 && pos.offset < layout->text_count)
		return layout->text_props[pos.offset].direction;
	return layout->resolved_direction;
}

skb_text_position_t skb_layout_hit_test_at_line(const skb_layout_t* layout, skb_movement_type_t type, int32_t line_idx, float hit_x)
{
	const skb_layout_line_t* line = &layout->lines[line_idx];

	skb_text_position_t result = {0};

	if (hit_x < line->bounds.x) {
		if (skb_is_rtl(layout->resolved_direction)) {
			result = (skb_text_position_t) {
				.offset = line->last_grapheme_offset,
				.affinity = SKB_AFFINITY_EOL,
			};
		} else {
			result = (skb_text_position_t) {
				.offset = line->text_range.start,
				.affinity = SKB_AFFINITY_SOL,
			};
		}
	} else if (hit_x >= (line->bounds.x + line->bounds.width)) {
		if (skb_is_rtl(layout->resolved_direction)) {
			result = (skb_text_position_t) {
				.offset = line->text_range.start,
				.affinity = SKB_AFFINITY_SOL,
			};
		} else {
			result = (skb_text_position_t) {
				.offset = line->last_grapheme_offset,
				.affinity = SKB_AFFINITY_EOL,
			};
		}
	} else {

		skb_caret_iterator_t caret_iter = skb_caret_iterator_make(layout, line_idx);

		float x = 0.f;
		float advance = 0.f;
		skb_caret_iterator_result_t left = {0};
		skb_caret_iterator_result_t right = {0};

		while (skb_caret_iterator_next(&caret_iter, &x, &advance, &left, &right)) {
			if (hit_x < x) {
				result = left.text_position;
				break;
			}
			if (hit_x < x + advance * 0.5f) {
				result = right.text_position;
				break;
			}
		}
	}

	if (type ==	SKB_MOVEMENT_CARET) {
		// When placing caret, do not allow to place caret after the newline character.
		// Selection can get there, though, so that we can select a whole line.
		result = skb__caret_prune_control_eol(layout, line, result);
	}

	return result;
}

skb_text_position_t skb_layout_hit_test(const skb_layout_t* layout, skb_movement_type_t type, float hit_x, float hit_y)
{
	if (layout->lines_count == 0)
		return (skb_text_position_t){0};

	// Find the row the hit position is at.
	int32_t line_idx = layout->lines_count - 1;
	for (int32_t i = 0; i < layout->lines_count; i++) {
		skb_layout_line_t* line = &layout->lines[i];
		const float bot_y = line->bounds.y + -line->ascender + line->descender;
		if (hit_y < bot_y) {
			line_idx = i;
			break;
		}
	}

	return skb_layout_hit_test_at_line(layout, type, line_idx, hit_x);
}

skb_text_position_t skb__sanitize_offset(const skb_layout_t* layout, const skb_layout_line_t* line, const skb_text_position_t caret)
{
	bool start_of_line = false;
	bool end_of_line = false;
	int32_t offset = caret.offset;
	if (offset < line->text_range.start) {
		offset = line->text_range.start;
		start_of_line = true;
	}
	if (offset > line->last_grapheme_offset) {
		offset = line->last_grapheme_offset;
		end_of_line = true;
	}

	// Make sure the offset is at the beginning of a grapheme.
	offset = skb_layout_align_grapheme_offset(layout, offset);

	uint8_t affinity = caret.affinity;
	if (affinity == SKB_AFFINITY_NONE)
		affinity = SKB_AFFINITY_TRAILING;

	if (affinity == SKB_AFFINITY_EOL && offset != line->last_grapheme_offset)
		affinity = SKB_AFFINITY_LEADING;
	if (affinity == SKB_AFFINITY_SOL && offset != line->text_range.start)
		affinity = SKB_AFFINITY_TRAILING;

	// Set out of bounds indices to SOL/EOL. This can happen e.g. with insert position.
	if (start_of_line && offset == line->text_range.start)
		affinity = SKB_AFFINITY_SOL;
	if (end_of_line && offset == line->last_grapheme_offset)
		affinity = SKB_AFFINITY_EOL;

	return (skb_text_position_t) {
		.offset = offset,
		.affinity = affinity,
	};
}

skb_visual_caret_t skb_layout_get_visual_caret_at_line(const skb_layout_t* layout, int32_t line_idx, skb_text_position_t pos)
{
	assert(layout);
	assert(layout->lines_count > 0);

	const skb_layout_line_t* line = &layout->lines[line_idx];
	pos = skb__sanitize_offset(layout, line, pos);

	skb_visual_caret_t vis_caret = {
		.x = line->bounds.x,
		.y = line->bounds.y,
		.width = 0.f,
		.height = -line->ascender + line->descender,
		.direction = layout->resolved_direction,
	};

	skb_caret_iterator_t caret_iter = skb_caret_iterator_make(layout, line_idx);

	skb_font_handle_t font_handle = 0;
	skb_text_attributes_span_t* attribute_span = NULL;
	float x = 0.f;
	float advance = 0.f;
	skb_caret_iterator_result_t left = {0};
	skb_caret_iterator_result_t right = {0};

	while (skb_caret_iterator_next(&caret_iter, &x, &advance, &left, &right)) {
		if (pos.offset == left.text_position.offset && pos.affinity == left.text_position.affinity) {
			skb_glyph_t* glyph = &layout->glyphs[left.glyph_idx];
			attribute_span = &layout->attribute_spans[glyph->span_idx];
			font_handle = glyph->font_handle;
			vis_caret.x = caret_iter.x;
			vis_caret.y = glyph->offset_y;
			vis_caret.direction = left.direction;
			break;
		}
		if (pos.offset == right.text_position.offset && pos.affinity == right.text_position.affinity) {
			skb_glyph_t* glyph = &layout->glyphs[left.glyph_idx];
			attribute_span = &layout->attribute_spans[glyph->span_idx];
			font_handle = glyph->font_handle;
			vis_caret.x = caret_iter.x;
			vis_caret.y = glyph->offset_y;
			vis_caret.direction = right.direction;
			break;
		}
	}

	if (font_handle && attribute_span) {
		const skb_font_metrics_t font_metrics = skb_font_get_metrics(layout->params.font_collection, font_handle);
		const skb_caret_metrics_t caret_metrics = skb_font_get_caret_metrics(layout->params.font_collection, font_handle);
		const skb_attribute_font_t attr_font = skb_attributes_get_font(attribute_span->attributes, attribute_span->attributes_count);

		vis_caret.x -= caret_metrics.slope * font_metrics.descender * attr_font.size;
		vis_caret.y += font_metrics.ascender * attr_font.size;
		vis_caret.height = (-font_metrics.ascender + font_metrics.descender) * attr_font.size;
		vis_caret.width = vis_caret.height * caret_metrics.slope;

	}

	return vis_caret;
}

skb_visual_caret_t skb_layout_get_visual_caret_at(const skb_layout_t* layout, skb_text_position_t pos)
{
	if (!layout->lines)
		return (skb_visual_caret_t) { 0 };

	const int32_t line_idx = skb_layout_get_line_index(layout, pos);

	return skb_layout_get_visual_caret_at_line(layout, line_idx, pos);
}


skb_text_position_t skb_layout_get_line_start_at(const skb_layout_t* layout, skb_text_position_t pos)
{
	const int32_t line_idx = skb_layout_get_line_index(layout, pos);
	const skb_layout_line_t* line = &layout->lines[line_idx];
	skb_text_position_t result = {
		.offset = line->text_range.start,
		.affinity = SKB_AFFINITY_SOL,
	};
	return result;
}

skb_text_position_t skb_layout_get_line_end_at(const skb_layout_t* layout, skb_text_position_t pos)
{
	const int32_t line_idx = skb_layout_get_line_index(layout, pos);
	const skb_layout_line_t* line = &layout->lines[line_idx];
	skb_text_position_t result = {
		.offset = line->last_grapheme_offset,
		.affinity = SKB_AFFINITY_EOL,
	};
	return skb__caret_prune_control_eol(layout, line, result);
}

skb_text_position_t skb_layout_get_word_start_at(const skb_layout_t* layout, skb_text_position_t pos)
{
	const int32_t line_idx = skb_layout_get_line_index(layout, pos);
	const skb_layout_line_t* line = &layout->lines[line_idx];

	pos = skb__sanitize_offset(layout, line, pos);

	// Not using insert position here, since we want to start from the "character" the user has hit.
	int32_t offset = pos.offset;

	while (offset >= 0) {
		if (layout->text_props[offset-1].flags & SKB_TEXT_PROP_WORD_BREAK) {
			offset = skb_layout_align_grapheme_offset(layout, offset);
			break;
		}
		offset--;
	}

	if (offset < 0)
		offset = 0;

	return (skb_text_position_t) {
		.offset = offset,
		.affinity = SKB_AFFINITY_TRAILING,
	};
}

skb_text_position_t skb_layout_get_word_end_at(const skb_layout_t* layout, skb_text_position_t pos)
{
	const int32_t line_idx = skb_layout_get_line_index(layout, pos);
	const skb_layout_line_t* line = &layout->lines[line_idx];

	pos = skb__sanitize_offset(layout, line, pos);

	// Not using insert position here, since we want to start from the "character" the user has hit.
	int32_t offset = pos.offset;

	while (offset < layout->text_count) {
		if (layout->text_props[offset].flags & SKB_TEXT_PROP_WORD_BREAK) {
			offset = skb_layout_align_grapheme_offset(layout, offset);
			break;
		}
		offset++;
	}

	if (offset >= layout->text_count)
		offset = skb_layout_align_grapheme_offset(layout, layout->text_count-1);

	return (skb_text_position_t) {
		.offset = offset,
		.affinity = SKB_AFFINITY_LEADING,
	};
}

skb_text_position_t skb_layout_get_selection_ordered_start(const skb_layout_t* layout, skb_text_selection_t selection)
{
	const int32_t start_offset = skb_layout_get_text_offset(layout, selection.start_pos);
	const int32_t end_offset = skb_layout_get_text_offset(layout, selection.end_pos);

	if (skb_is_rtl(layout->resolved_direction))
		return start_offset > end_offset ? selection.start_pos : selection.end_pos;

	return start_offset <= end_offset ? selection.start_pos : selection.end_pos;
}

skb_text_position_t skb_layout_get_selection_ordered_end(const skb_layout_t* layout, skb_text_selection_t selection)
{
	const int32_t start_offset = skb_layout_get_text_offset(layout, selection.start_pos);
	const int32_t end_offset = skb_layout_get_text_offset(layout, selection.end_pos);

	if (skb_is_rtl(layout->resolved_direction))
		return start_offset <= end_offset ? selection.start_pos : selection.end_pos;

	return start_offset > end_offset ? selection.start_pos : selection.end_pos;
}

skb_range_t skb_layout_get_selection_text_offset_range(const skb_layout_t* layout, skb_text_selection_t selection)
{
	int32_t start_offset = skb_layout_get_text_offset(layout, selection.start_pos);
	int32_t end_offset = skb_layout_get_text_offset(layout, selection.end_pos);
	return (skb_range_t) {
		.start = skb_mini(start_offset, end_offset),
		.end = skb_maxi(start_offset, end_offset),
	};
}

int32_t skb_layout_get_selection_count(const skb_layout_t* layout, skb_text_selection_t selection)
{
	skb_range_t range = skb_layout_get_selection_text_offset_range(layout, selection);
	return range.end - range.start;
}

void skb_layout_get_selection_bounds(const skb_layout_t* layout, skb_text_selection_t selection, skb_selection_rect_func_t* callback, void* context)
{
	skb_layout_get_selection_bounds_with_offset(layout, 0.f, selection, callback, context);
}

void skb_layout_get_selection_bounds_with_offset(const skb_layout_t* layout, float offset_y, skb_text_selection_t selection, skb_selection_rect_func_t* callback, void* context)
{
	assert(layout);
	assert(callback);

	skb_range_t sel_range = skb_layout_get_selection_text_offset_range(layout, selection);

	for (int32_t line_idx = 0; line_idx < layout->lines_count; line_idx++) {
		const skb_layout_line_t* line = &layout->lines[line_idx];
		if (skb_range_overlap((skb_range_t){line->text_range.start, line->text_range.end}, sel_range)) {

			int32_t rect_start_offset = 0;
			int32_t rect_end_offset = 0;
			float rect_start_x = line->bounds.x;
			float rect_end_x = line->bounds.x;
			float x = line->bounds.x;
			bool prev_is_right_adjacent = false;

			for (int32_t glyph_idx = line->glyph_range.start; glyph_idx < line->glyph_range.end; glyph_idx++) {
				// Figure out glyph run
				const bool is_rtl = skb_is_rtl(layout->text_props[layout->glyphs[glyph_idx].text_range.start].direction);
				float glyph_run_width = layout->glyphs[glyph_idx].advance_x;
				int32_t glyph_run_start_offset = layout->glyphs[glyph_idx].text_range.start;
				int32_t glyph_run_end_offset = layout->glyphs[glyph_idx].text_range.end;
				while ((glyph_idx+1) < layout->glyphs_count && layout->glyphs[glyph_idx+1].text_range.start == glyph_run_start_offset) {
					glyph_idx++;
					glyph_run_width += layout->glyphs[glyph_idx].advance_x;
					glyph_run_end_offset = layout->glyphs[glyph_idx].text_range.end;
				}

				int32_t selected_run_start_offset = skb_maxi(glyph_run_start_offset, sel_range.start);
				int32_t selected_run_end_offset = skb_mini(glyph_run_end_offset, sel_range.end);

				if (selected_run_start_offset < selected_run_end_offset) {

					// Code codepoint_idx is inside this run.
					// Find number of graphemes and the grapheme index of the cp_offset
					int32_t grapheme_start_idx = 0;
					int32_t grapheme_end_idx = 0;
					int32_t grapheme_count = 0;

					for (int32_t cp_offset = glyph_run_start_offset; cp_offset < glyph_run_end_offset; cp_offset++) {
						if (cp_offset == selected_run_start_offset)
							grapheme_start_idx = grapheme_count;
						if (cp_offset == selected_run_end_offset)
							grapheme_end_idx = grapheme_count;
						if (layout->text_props[cp_offset].flags & SKB_TEXT_PROP_GRAPHEME_BREAK)
							grapheme_count++;
					}
					if (selected_run_end_offset == glyph_run_end_offset)
						grapheme_end_idx = grapheme_count;

					// Interpolate caret location.
					float start_u = (float)(grapheme_start_idx) / (float)grapheme_count;
					float end_u = (float)(grapheme_end_idx) / (float)grapheme_count;

					if (is_rtl) {
						float u = start_u;
						start_u = 1.f - end_u;
						end_u = 1.f - u;
					}

					bool is_left_adjacent = false;
					bool is_right_adjacent = false;
					if (is_rtl) {
						is_left_adjacent = selected_run_end_offset == glyph_run_end_offset;
						is_right_adjacent = selected_run_start_offset == glyph_run_start_offset;
					} else {
						is_left_adjacent = selected_run_start_offset == glyph_run_start_offset;
						is_right_adjacent = selected_run_end_offset == glyph_run_end_offset;
					}

					if (prev_is_right_adjacent && is_left_adjacent) {
						// Adjacent, merge with existing.
						rect_start_offset = skb_mini(rect_start_offset, selected_run_start_offset);
						rect_end_offset = skb_maxi(rect_end_offset, selected_run_end_offset);
						rect_end_x = x + glyph_run_width * end_u;
					} else {
						// Start new rect
						if (skb_absf(rect_end_x - rect_start_x) > 0.01f) {
							skb_rect2_t rect = {
								.x = rect_start_x,
								.y = offset_y + line->baseline + line->ascender,
								.width = rect_end_x - rect_start_x,
								.height = -line->ascender + line->descender,
							};
							callback(rect, context);
						}
						rect_start_offset = selected_run_start_offset;
						rect_end_offset = selected_run_end_offset;
						rect_start_x = x + glyph_run_width * start_u;
						rect_end_x = x + glyph_run_width * end_u;
					}

					prev_is_right_adjacent = is_right_adjacent;
				} else {
					prev_is_right_adjacent = 0;
				}

				x += glyph_run_width;
			}

			if (skb_absf(rect_end_x - rect_start_x) > 0.01f) {
				// Output rect.
				skb_rect2_t rect = {
					.x = rect_start_x,
					.y = offset_y + line->baseline + line->ascender,
					.width = rect_end_x - rect_start_x,
					.height = -line->ascender + line->descender,
				};
				callback(rect, context);
			}
		}
	}
}

skb_caret_iterator_t skb_caret_iterator_make(const skb_layout_t* layout, int32_t line_idx)
{
	assert(layout);
	assert(line_idx >= 0 && line_idx < layout->lines_count);

	const bool line_is_rtl = skb_is_rtl(layout->resolved_direction);
	skb_caret_iterator_t iter = {0};
	const skb_layout_line_t* line = &layout->lines[line_idx];

	iter.layout = layout;
	iter.line_first_grapheme_offset = line->text_range.start;
	iter.line_last_grapheme_offset = line->last_grapheme_offset;
	iter.end_of_line = false;

	iter.x = line->bounds.x;
	iter.advance = 0.f;

	// Previous caret is at the start of the line.
	if (line_is_rtl) {
		iter.pending_left.text_position.offset = iter.line_last_grapheme_offset;
		iter.pending_left.text_position.affinity = SKB_AFFINITY_EOL;
	} else {
		iter.pending_left.text_position.offset = iter.line_first_grapheme_offset;
		iter.pending_left.text_position.affinity = SKB_AFFINITY_SOL;
	}
	iter.pending_left.direction = layout->resolved_direction;
	iter.pending_left.glyph_idx = line->glyph_range.start;

	iter.glyph_pos = line->glyph_range.start;
	iter.glyph_end = line->glyph_range.end;

	iter.grapheme_pos = 0;
	iter.grapheme_end = 0;

	iter.end_of_glyph = true;

	return iter;
}

bool skb_caret_iterator_next(skb_caret_iterator_t* iter, float* x, float* advance, skb_caret_iterator_result_t* left, skb_caret_iterator_result_t* right)
{
	if (iter->end_of_line)
		return false;

	const skb_layout_t* layout = iter->layout;
	const bool line_is_rtl = skb_is_rtl(layout->resolved_direction);

	// Carry over from previous update.
	iter->x += iter->advance;

	*left = iter->pending_left;

	// Advance to next glyph
	if (iter->end_of_glyph) {
		if (iter->glyph_pos == iter->glyph_end) {
			iter->end_of_line = true;
			iter->advance = 0.f;
		} else {
			// Find new glyph run
			float glyph_run_width = 0.f;
			const skb_range_t text_range = layout->glyphs[iter->glyph_pos].text_range;
			iter->glyph_direction = layout->text_props[text_range.start].direction;

			while (iter->glyph_pos < iter->glyph_end && layout->glyphs[iter->glyph_pos].text_range.start == text_range.start) {
				glyph_run_width += layout->glyphs[iter->glyph_pos].advance_x;
				iter->glyph_pos++;
			}

			// Find graphemes in the run
			int32_t grapheme_count = 0;
			for (int32_t ti = text_range.start; ti < text_range.end; ti++) {
				if (layout->text_props[ti].flags & SKB_TEXT_PROP_GRAPHEME_BREAK)
					grapheme_count++;
			}

			iter->advance = (grapheme_count > 0) ? (glyph_run_width / (float)grapheme_count) : 0.f;

			if (skb_is_rtl(iter->glyph_direction)) {
				iter->grapheme_pos = text_range.end;
				iter->grapheme_end = text_range.start;
			} else {
				iter->grapheme_pos = text_range.start;
				iter->grapheme_end = text_range.end;
			}

		}
	}

	if (iter->end_of_line) {
		if (line_is_rtl) {
			right->text_position.offset = iter->line_first_grapheme_offset;
			right->text_position.affinity = SKB_AFFINITY_SOL;
		} else {
			right->text_position.offset = iter->line_last_grapheme_offset;
			right->text_position.affinity = SKB_AFFINITY_EOL;
		}
		right->direction = layout->resolved_direction;
		right->glyph_idx = skb_mini(iter->glyph_pos, iter->glyph_end - 1);
	} else {
		// Advance to next grapheme location
		int32_t offset = 0;
		if (skb_is_rtl(iter->glyph_direction)) {
			assert(iter->grapheme_pos > iter->grapheme_end);
			iter->grapheme_pos--;
			while (iter->grapheme_pos > iter->grapheme_end && !(layout->text_props[iter->grapheme_pos - 1].flags & SKB_TEXT_PROP_GRAPHEME_BREAK))
				iter->grapheme_pos--;
			offset = iter->grapheme_pos;
			iter->end_of_glyph = (iter->grapheme_pos <= iter->grapheme_end);
		} else {
			assert(iter->grapheme_pos < iter->grapheme_end);
			offset = iter->grapheme_pos;
			while (iter->grapheme_pos < (iter->grapheme_end - 1) && !(layout->text_props[iter->grapheme_pos].flags & SKB_TEXT_PROP_GRAPHEME_BREAK))
				iter->grapheme_pos++;
			iter->grapheme_pos++;
			iter->end_of_glyph = (iter->grapheme_pos >= iter->grapheme_end);
		}

		right->text_position.offset = offset;
		right->text_position.affinity = skb_is_rtl(iter->glyph_direction) ? SKB_AFFINITY_LEADING : SKB_AFFINITY_TRAILING; // LTR = trailing;
		right->direction = iter->glyph_direction;
		right->glyph_idx = skb_mini(iter->glyph_pos, iter->glyph_end - 1);

		iter->pending_left.text_position.offset = offset;
		iter->pending_left.text_position.affinity = skb_is_rtl(iter->glyph_direction) ? SKB_AFFINITY_TRAILING : SKB_AFFINITY_LEADING; // LTR = leading;
		iter->pending_left.direction = iter->glyph_direction;
		iter->pending_left.glyph_idx = skb_mini(iter->glyph_pos, iter->glyph_end - 1);
	}

	*x = iter->x;
	*advance = iter->advance;

	return true;
}
