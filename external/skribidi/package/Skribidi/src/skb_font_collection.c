// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_font_collection.h"
#include "skb_font_collection_internal.h"

#include <assert.h>
#include <float.h>
#include <string.h>

#include "SheenBidi/SBScript.h"
#include "hb.h"
#include "hb-ot.h"
#include "skb_layout.h"

//
// Fonts
//

typedef struct skb__sb_tag_array_t {
	uint8_t* tags;
	int32_t tags_count;
	int32_t tags_cap;
} skb__sb_tag_array_t;


static void skb__add_unique(skb__sb_tag_array_t* script_tags, uint8_t sb_script)
{
	for (int32_t i = 0; i < script_tags->tags_count; i++) {
		if (script_tags->tags[i] == sb_script)
			continue;
	}
	SKB_ARRAY_RESERVE(script_tags->tags, script_tags->tags_count+1);
	script_tags->tags[script_tags->tags_count++] = sb_script;
}

static void skb__add_unique_script_from_ot_tag(skb__sb_tag_array_t* script_tags, uint32_t ot_script_tag)
{
	// TODO: we could make a lookup table and binary search based on ot_script_tag, instead of going through the hoops each time.

	// Brute force over all SBScripts
	static const uint8_t sb_last_script_index = 0xab; // This is highest SBScript value.
	for (uint8_t sb_script = 0; sb_script < sb_last_script_index; sb_script++) {
		// SBScript -> ISO-15924
		uint32_t unicode_tag = SBScriptGetUnicodeTag(sb_script);
		// ISO-15924 -> hb_script_t
		hb_script_t hb_script = hb_script_from_iso15924_tag(unicode_tag);
		// hb_script_t -> all possible Opentype scripts
		hb_tag_t ot_script_tags[2];
		unsigned int ot_script_tags_count = 2;
		hb_ot_tags_from_script_and_language(hb_script, NULL, &ot_script_tags_count, ot_script_tags, NULL, NULL);
		for (unsigned int i = 0; i < ot_script_tags_count; i++) {
			if (ot_script_tags[i] == ot_script_tag) {
				// Found match, store the matching SBScript.
				skb__add_unique(script_tags, sb_script);
				break;
			}
		}
	}
}

static void skb__append_tags_from_table(hb_face_t* face, hb_tag_t table_tag, skb__sb_tag_array_t* scripts)
{
	hb_tag_t tags[32];
	uint32_t offset = 0;
	uint32_t tags_count = 32;
	while (tags_count == 32) {
		tags_count = 32;
		tags_count = hb_ot_layout_table_get_script_tags(face, table_tag, offset, &tags_count, tags);

		for (uint32_t i = 0; i < tags_count; i++)
			skb__add_unique_script_from_ot_tag(scripts, tags[i]);

		offset += tags_count;
	}
}

static void skb__append_tags_from_unicodes(hb_face_t* face, skb__sb_tag_array_t* scripts)
{
	hb_set_t* unicodes = hb_set_create();
	hb_face_collect_unicodes(face, unicodes);

	hb_unicode_funcs_t* unicode_funcs = hb_unicode_funcs_get_default();

	// To save us testing the script of each individual glyph, we just sample the first and last glyph in the range.
	hb_codepoint_t first = HB_SET_VALUE_INVALID;
	hb_codepoint_t last = HB_SET_VALUE_INVALID;
	while (hb_set_next_range (unicodes, &first, &last)) {

		int32_t unicode_count = 0;
		hb_script_t unicode_scripts[2];
		unicode_scripts[unicode_count++] = hb_unicode_script(unicode_funcs, first);
		if (first != last) {
			unicode_scripts[unicode_count++] = hb_unicode_script(unicode_funcs, last);
			if ((uint32_t)unicode_scripts[unicode_count-1] == (uint32_t)unicode_scripts[unicode_count-2])
				unicode_count--;
		}

		for (int32_t j = 0; j < unicode_count; j++) {
			hb_tag_t ot_scripts[4];
			uint32_t ot_scripts_count = 4;
			hb_ot_tags_from_script_and_language (unicode_scripts[j], HB_LANGUAGE_INVALID, &ot_scripts_count, ot_scripts, NULL, NULL);

			for (uint32_t i = 0; i < ot_scripts_count; i++)
				skb__add_unique_script_from_ot_tag(scripts, ot_scripts[i]);
		}
	}

	hb_set_destroy(unicodes);
}

static void skb__reset_font(skb_font_t* font)
{
	// We must keep the generation even when the font is cleared, it is used to detect stale handles.
	uint32_t generation = font->generation;
	memset(font, 0, sizeof(skb_font_t));
	font->generation = generation;
}

static skb_font_handle_t skb__make_font_handle(int32_t index, uint32_t generation)
{
	assert(index >= 0 && index <= 0xffff);
	assert(generation >= 0 && generation <= 0xffff);
	return index | (generation << 16);
}

static skb_font_t* skb__get_font_by_handle(const skb_font_collection_t* font_collection, skb_font_handle_t font)
{
	uint32_t index = font & 0xffff;
	uint32_t generation = (font >> 16) & 0xffff;
	if ((int32_t)index < font_collection->fonts_count && font_collection->fonts[index].generation == generation)
		return &font_collection->fonts[index];
	return NULL;
}

static inline skb_font_t* skb__get_font_unchecked(const skb_font_collection_t* font_collection, skb_font_handle_t font)
{
	uint32_t index = font & 0xffff;
	return &font_collection->fonts[index];
}

static bool skb__font_create_from_hb_font(skb_font_t* font, hb_font_t* hb_font, const char* name, uint8_t font_family)
{
	skb__sb_tag_array_t scripts = {0};

	if (!font) goto error;

	hb_face_t* face = hb_font_get_face(hb_font);

	// Get how many points per EM, used to scale font size.
	unsigned int upem = hb_face_get_upem(face);

	// Try to get script tags from tables.
	skb__append_tags_from_table(face, HB_OT_TAG_GSUB, &scripts);
	skb__append_tags_from_table(face, HB_OT_TAG_GPOS, &scripts);

	// If the tables did not define the scripts, fallback to checking the supported glyph ranges.
	if (scripts.tags_count == 0)
		skb__append_tags_from_unicodes(face, &scripts);

	const float italic = hb_style_get_value(hb_font, HB_STYLE_TAG_ITALIC);
	const float slant = hb_style_get_value(hb_font, HB_STYLE_TAG_SLANT_RATIO);
	const float weight = hb_style_get_value(hb_font, HB_STYLE_TAG_WEIGHT);
	const float width = hb_style_get_value(hb_font, HB_STYLE_TAG_WIDTH);

	if (!hb_font) goto error;

	// Initialize font.
	font->upem = (int)upem;
	font->upem_scale = 1.f / (float)upem;

	if (italic > 0.1f)
		font->style = SKB_STYLE_ITALIC;
	else if (slant > 0.01f)
		font->style = SKB_STYLE_OBLIQUE;
	else
		font->style = SKB_STYLE_NORMAL;

	font->weight = (uint16_t)weight;

	font->stretch = width / 100.f;

	// Save HB font
	font->hb_font = hb_font;

	// Store name
	size_t name_len = strlen(name);
	font->name = skb_malloc(name_len+1);
	memcpy(font->name, name, name_len+1); // copy null term.
	font->hash = skb_hash64_append_str(skb_hash64_empty(), font->name);

	// Store supported scripts
	font->scripts = scripts.tags;
	font->scripts_count = scripts.tags_count;

	font->font_family = font_family;

	// Leaving this debug log here, as it has often been needed.
//	for (uint32_t i = 0; i < font->scripts_count; i++)
//		skb_debug_log(" - script: %c%c%c%c\n", HB_UNTAG(SBScriptGetOpenTypeTag(font->scripts[i])));

	// Store metrics
	hb_font_extents_t extents;
	if (hb_font_get_h_extents(font->hb_font, &extents)) {
		font->metrics.ascender = -(float)extents.ascender * font->upem_scale;
		font->metrics.descender = -(float)extents.descender * font->upem_scale;
		font->metrics.line_gap = (float)extents.line_gap * font->upem_scale;
	}

	hb_position_t x_height;
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_X_HEIGHT, &x_height);
	font->metrics.x_height = -(float)x_height * font->upem_scale;

	hb_position_t cap_height;
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_CAP_HEIGHT, &cap_height);
	font->metrics.cap_height = -(float)cap_height * font->upem_scale;

	hb_position_t underline_offset;
	hb_position_t underline_size;
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_UNDERLINE_OFFSET, &underline_offset);
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_UNDERLINE_SIZE, &underline_size);
	font->metrics.underline_offset = -(float)underline_offset * font->upem_scale;
	font->metrics.underline_size = (float)underline_size * font->upem_scale;

	hb_position_t strikeout_offset;
	hb_position_t strikeout_size;
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_STRIKEOUT_OFFSET, &strikeout_offset);
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_STRIKEOUT_SIZE, &strikeout_size);
	font->metrics.strikeout_offset = -(float)strikeout_offset * font->upem_scale;
	font->metrics.strikeout_size = (float)strikeout_size * font->upem_scale;

	// Caret metrics
	hb_position_t caret_offset;
	hb_position_t caret_rise;
	hb_position_t caret_run;
	hb_ot_metrics_get_position_with_fallback(font->hb_font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_OFFSET, &caret_offset);
	hb_ot_metrics_get_position_with_fallback(font->hb_font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_RISE, &caret_rise);
	hb_ot_metrics_get_position_with_fallback(font->hb_font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_RUN, &caret_run);
	font->caret_metrics.offset = (float)caret_offset * font->upem_scale;
	font->caret_metrics.slope = (float)caret_run / (float)caret_rise;

	return true;

error:
	skb_free(scripts.tags);
	skb__reset_font(font);

	return false;
}

#if !defined(SKB_NO_OPEN)
static bool skb__font_create(skb_font_t* font, const char* path, uint8_t font_family)
{
	hb_blob_t* blob = NULL;
	hb_face_t* face = NULL;
	hb_font_t* hb_font = NULL;
	bool ok = false;

	// skb_debug_log("Loading font: %s\n", path);

	// Use Harfbuzz to load the font data, it uses mmap when possible.
	blob = hb_blob_create_from_file_or_fail(path);
	if (!blob) goto cleanup;

	face = hb_face_create_or_fail(blob, 0);
	if (!face) goto cleanup;

	hb_font = hb_font_create(face);
	if (!hb_font) goto cleanup;

	ok = skb__font_create_from_hb_font(font, hb_font, path, font_family);

cleanup:
	hb_blob_destroy(blob);
	hb_face_destroy(face);
	return ok;
}
#endif // !defined(SKB_NO_OPEN)

static bool skb__font_create_from_data(
	skb_font_t* font,
	const char* name,
	uint8_t font_family,
	const void* font_data,
	size_t font_data_length,
	void* context,
	skb_destroy_func_t* destroy_func)
{
	hb_blob_t* blob = NULL;
	hb_face_t* face = NULL;
	hb_font_t* hb_font = NULL;
	bool ok = false;

	// skb_debug_log("Loading font from data: %s\n", name);

	// Use Harfbuzz to create blob from memory data with read-only mode
	// Pass the context and destroy function to HarfBuzz so it can manage the lifetime
	blob = hb_blob_create_or_fail((const char*)font_data, (unsigned int)font_data_length, HB_MEMORY_MODE_READONLY, context, (hb_destroy_func_t)destroy_func);
	if (!blob) goto cleanup;

	face = hb_face_create_or_fail(blob, 0);
	if (!face) goto cleanup;

	hb_font = hb_font_create(face);
	if (!hb_font) goto cleanup;

	ok = skb__font_create_from_hb_font(font, hb_font, name, font_family);

cleanup:
	hb_blob_destroy(blob);
	hb_face_destroy(face);
	return ok;
}

static void skb__font_destroy(skb_font_t* font)
{
	if (!font) return;
	skb_free(font->name);
	skb_free(font->scripts);
	skb_free(font->baseline_sets);
	hb_font_destroy(font->hb_font);
}

skb_font_collection_t* skb_font_collection_create(void)
{
	static uint32_t id = 0;

	skb_font_collection_t* result = skb_malloc(sizeof(skb_font_collection_t));
	memset(result, 0, sizeof(skb_font_collection_t));

	result->id = ++id;

	return result;
}

void skb_font_collection_destroy(skb_font_collection_t* font_collection)
{
	if (!font_collection) return;
	for (int32_t i = 0; i < font_collection->fonts_count; i++)
		skb__font_destroy(&font_collection->fonts[i]);
	skb_free(font_collection->fonts);
	skb_free(font_collection);
}

void skb_font_collection_set_on_font_fallback(skb_font_collection_t* font_collection, skb_font_fallback_func_t* fallback_func, void* context)
{
	assert(font_collection);
	font_collection->fallback_func = fallback_func;
	font_collection->fallback_context = context;
}

static skb_font_t* skb__alloc_font(skb_font_collection_t* font_collection)
{
	int32_t font_idx = SKB_INVALID_INDEX;
	uint32_t generation = 1;
	if (font_collection->empty_fonts_count > 0 ) {
		// Using linear search as we dont expect to have that many fonts loaded.
		for (int32_t i = 0; i < font_collection->fonts_count; i++) {
			if (font_collection->fonts[i].hash == 0) {
				font_idx = i;
				font_collection->empty_fonts_count--;
				generation = font_collection->fonts[i].generation;
				break;
			}
		}
	} else {
		SKB_ARRAY_RESERVE(font_collection->fonts, font_collection->fonts_count+1);
		font_idx = font_collection->fonts_count++;
	}
	assert(font_idx != SKB_INVALID_INDEX);

	skb_font_t* font = &font_collection->fonts[font_idx];
	memset(font, 0, sizeof(skb_font_t));

	font->generation = generation;
	font->handle = skb__make_font_handle(font_idx, generation);
	return font;
}

#if !defined(SKB_NO_OPEN)
skb_font_handle_t skb_font_collection_add_font(skb_font_collection_t* font_collection, const char* file_name, uint8_t font_family)
{
	skb_font_t* font = skb__alloc_font(font_collection);

	if (!skb__font_create(font, file_name, font_family)) {
		// skb__font_create() has emptied the font struct, indicate that we have one empty to use.
		font_collection->empty_fonts_count++;
		return false;
	}

	return font->handle;
}
#endif // !defined SKB_NO_OPEN

skb_font_handle_t skb_font_collection_add_font_from_data(
	skb_font_collection_t* font_collection,
	const char* name,
	uint8_t font_family,
	const void* font_data,
	size_t font_data_length,
	void* context,
	skb_destroy_func_t* destroy_func)
{
	skb_font_t* font = skb__alloc_font(font_collection);

	if (!skb__font_create_from_data(font, name, font_family, font_data, font_data_length, context, destroy_func)) {
		// skb__font_create_from_data() has emptied the font struct, indicate that we have one empty to use.
		font_collection->empty_fonts_count++;
		return false;
	}

	return font->handle;
}

skb_font_handle_t skb_font_collection_add_hb_font(
	skb_font_collection_t* font_collection,
	hb_font_t* hb_font,
	const char* name,
	uint8_t font_family)
{
	skb_font_t* font = skb__alloc_font(font_collection);

	// Increase the reference count
	hb_font = hb_font_reference(hb_font);

	if (!skb__font_create_from_hb_font(font, hb_font, name, font_family)) {
		// skb__font_create_from_hb_font() has emptied the font struct, indicate that we have one empty to use.
		font_collection->empty_fonts_count++;
		return false;
	}

	return font->handle;
}

bool skb_font_collection_remove_font(skb_font_collection_t* font_collection, skb_font_handle_t font_handle)
{
	skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	if (!font)
		return false;

	font->generation++;
	skb__font_destroy(font);
	skb__reset_font(font);

	return true;
}

static float g_stretch_to_value[] = {
	1.0f, // SKB_STRETCH_NORMAL
	0.5f, // SKB_STRETCH_ULTRA_CONDENSED
	0.625f, // SKB_STRETCH_EXTRA_CONDENSED
	0.75f, // SKB_STRETCH_CONDENSED
	0.875f, // SKB_STRETCH_SEMI_CONDENSED
	1.125f, // SKB_STRETCH_SEMI_EXPANDED
	1.25f, // SKB_STRETCH_EXPANDED
	1.5f, // SKB_STRETCH_EXTRA_EXPANDED
	2.0f, // SKB_STRETCH_ULTRA_EXPANDED
};

static int32_t g_weight_to_value[] = {
	400, // SKB_WEIGHT_NORMAL
	100, // SKB_WEIGHT_THIN
	200, // SKB_WEIGHT_EXTRALIGHT
	200, // SKB_WEIGHT_ULTRALIGHT
	300, // SKB_WEIGHT_LIGHT
	400, // SKB_WEIGHT_REGULAR
	500, // SKB_WEIGHT_MEDIUM
	600, // SKB_WEIGHT_DEMIBOLD
	600, // SKB_WEIGHT_SEMIBOLD
	700, // SKB_WEIGHT_BOLD
	800, // SKB_WEIGHT_EXTRABOLD
	800, // SKB_WEIGHT_ULTRABOLD
	900, // SKB_WEIGHT_BLACK
	900, // SKB_WEIGHT_HEAVY
	950, // SKB_WEIGHT_EXTRABLACK
	950, // SKB_WEIGHT_ULTRABLACK
};

static bool skb__supports_script(const skb_font_t* font, uint8_t script)
{
	for (int32_t script_idx = 0; script_idx < font->scripts_count; script_idx++) {
		if (font->scripts[script_idx] == script)
			return true;
	}
	return false;
}

int32_t skb__match_fonts(
	const skb_font_collection_t* font_collection,
	const char* requested_lang, const uint8_t requested_script, uint8_t requested_font_family,
	skb_weight_t requested_weight, skb_style_t requested_style, skb_stretch_t requested_stretch,
	skb_font_handle_t* results, int32_t results_cap)
{
	// Based on https://drafts.csswg.org/css-fonts-3/#font-style-matching

	int32_t candidates_count = 0;
	int32_t current_candidates_count = 0;
	bool multiple_stretch = false;
	bool multiple_styles = false;
	bool multiple_weights = false;

	// Match script and font family.
	for (int32_t font_idx = 0; font_idx < font_collection->fonts_count; font_idx++) {
		const skb_font_t* font = &font_collection->fonts[font_idx];
		if (font->font_family == requested_font_family
			&& (requested_font_family == SKB_FONT_FAMILY_EMOJI || skb__supports_script(font, requested_script))) { // Ignore script for emoji fonts, as emojis are the same on each writing system.
			if (candidates_count < results_cap) {
				if (candidates_count > 0) {
					const skb_font_t* prev_font = skb__get_font_unchecked(font_collection, results[candidates_count - 1]);
					multiple_stretch |= !skb_equalsf(prev_font->stretch, font->stretch, 0.01f);
					multiple_styles |= prev_font->style != font->style;
					multiple_weights |= prev_font->weight != font->weight;
				}
				results[candidates_count++] = font->handle;
			}
		}
	}

	if (!candidates_count)
		return 0;

	// Match stretch.
	if (multiple_stretch) {
		float requested_stretch_value = g_stretch_to_value[skb_clampi((int32_t)requested_stretch, 0, SKB_COUNTOF(g_stretch_to_value)-1)];

		bool exact_stretch_match = false;
		float nearest_narrow_error = FLT_MAX;
		float nearest_narrow = requested_stretch_value;
		float nearest_wide_error = FLT_MAX;
		float nearest_wide = requested_stretch_value;

		for (int32_t i = 0; i < candidates_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (skb_equalsf(requested_stretch_value, font->stretch, 0.01f)) {
				exact_stretch_match = true;
				break;
			}
			const float error = skb_absf(font->stretch - requested_stretch_value);
			if (font->stretch <= 0.f) {
				if (error < nearest_narrow_error) {
					nearest_narrow_error = error;
					nearest_narrow = font->stretch;
				}
			} else {
				if (error < nearest_wide_error) {
					nearest_wide_error = error;
					nearest_wide = font->stretch;
				}
			}
		}

		float selected_stretch = -1.f;
		if (exact_stretch_match) {
			selected_stretch = requested_stretch_value;
		} else {
			if (requested_stretch_value <= 1.f) {
				if (nearest_narrow_error < FLT_MAX)
					selected_stretch = nearest_narrow;
				else if (nearest_wide_error < FLT_MAX)
					selected_stretch = nearest_wide;
			} else {
				if (nearest_wide_error < FLT_MAX)
					selected_stretch = nearest_wide;
				else if (nearest_narrow_error < FLT_MAX)
					selected_stretch = nearest_narrow;
			}
		}

		// Prune out everything but the selected stretch.
		current_candidates_count = candidates_count;
		candidates_count = 0;
		for (int32_t i = 0; i < current_candidates_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (!skb_equalsf(selected_stretch, font->stretch, 0.01f))
				continue;
			results[candidates_count++] = results[i];
		}

		if (candidates_count <= 1)
			return candidates_count;
	}

	// Style
	if (multiple_styles) {
		int32_t normal_count = 0;
		int32_t italic_count = 0;
		int32_t oblique_count = 0;
		for (int32_t i = 0; i < candidates_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			uint8_t style = font->style;
			if (style == SKB_STYLE_NORMAL)
				normal_count++;
			if (style == SKB_STYLE_ITALIC)
				italic_count++;
			if (style == SKB_STYLE_OBLIQUE)
				oblique_count++;
		}

		uint8_t selected_style = SKB_STYLE_NORMAL;
		if (requested_style == SKB_STYLE_ITALIC) {
			if (italic_count > 0)
				selected_style = SKB_STYLE_ITALIC;
			else if (oblique_count > 0)
				selected_style = SKB_STYLE_OBLIQUE;
			else if (normal_count > 0)
				selected_style = SKB_STYLE_NORMAL;
		} else if (requested_style == SKB_STYLE_OBLIQUE) {
			if (oblique_count > 0)
				selected_style = SKB_STYLE_OBLIQUE;
			else if (italic_count > 0)
				selected_style = SKB_STYLE_ITALIC;
			else if (normal_count > 0)
				selected_style = SKB_STYLE_NORMAL;
		} else {
			if (normal_count > 0)
				selected_style = SKB_STYLE_NORMAL;
			else if (oblique_count > 0)
				selected_style = SKB_STYLE_OBLIQUE;
			else if (italic_count > 0)
				selected_style = SKB_STYLE_ITALIC;
		}

		// Prune out everything but the selected style.
		current_candidates_count = candidates_count;
		candidates_count = 0;
		for (int32_t i = 0; i < current_candidates_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (font->style != selected_style)
				continue;
			results[candidates_count++] = results[i];
		}

		if (candidates_count <= 1)
			return candidates_count;
	}

	// Font weight
	if (multiple_weights) {
		const int32_t requested_weight_value = g_weight_to_value[skb_clampi((int32_t)requested_weight, 0, SKB_COUNTOF(g_weight_to_value)-1)];

		bool exact_weight_match = false;
		bool has_400 = false;
		bool has_500 = false;
		int32_t nearest_lighter_error = INT32_MAX;
		int32_t nearest_lighter = requested_weight_value;
		int32_t nearest_darker_error = INT32_MAX;
		int32_t nearest_darker = requested_weight_value;

		for (int32_t i = 0; i < candidates_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (requested_weight == font->weight) {
				exact_weight_match = true;
				break;
			}
			const int32_t error = skb_absi(font->weight - requested_weight_value);
			if (font->weight <= 450) {
				if (error < nearest_lighter_error) {
					nearest_lighter_error = error;
					nearest_lighter = font->weight;
				}
			} else {
				if (error < nearest_darker_error) {
					nearest_darker_error = error;
					nearest_darker = font->weight;
				}
			}
			has_400 |= font->weight == 400;
			has_500 |= font->weight == 500;
		}

		int32_t selected_weight = 0;
		if (exact_weight_match) {
			selected_weight = requested_weight_value;
		} else {
			if (requested_weight_value >= 400 && requested_weight_value < 450 && has_500) {
				selected_weight = 500;
			} else if (requested_weight_value >= 450 && requested_weight_value <= 450 && has_400) {
				selected_weight = 400;
			} else {
				// Nearest
				if (requested_weight_value <= 450) {
					if (nearest_lighter_error < INT32_MAX)
						selected_weight = nearest_lighter;
					else if (nearest_darker_error < INT32_MAX)
						selected_weight = nearest_darker;
				} else {
					if (nearest_darker_error < INT32_MAX)
						selected_weight = nearest_darker;
					else if (nearest_lighter_error < INT32_MAX)
						selected_weight = nearest_lighter;
				}
			}
		}

		// Prune out everything but the selected weight.
		current_candidates_count = candidates_count;
		candidates_count = 0;
		for (int32_t i = 0; i < current_candidates_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (font->weight != selected_weight)
				continue;
			results[candidates_count++] = results[i];
		}
	}

	return candidates_count;
}

int32_t skb_font_collection_match_fonts(
	skb_font_collection_t* font_collection,
	const char* requested_lang, const uint8_t requested_script, uint8_t requested_font_family,
	skb_weight_t requested_weight, skb_style_t requested_style, skb_stretch_t requested_stretch,
	skb_font_handle_t* results, int32_t results_cap)
{
	int32_t results_count =  skb__match_fonts(
		font_collection, requested_lang, requested_script, requested_font_family,
		requested_weight, requested_style, requested_stretch, results, results_cap);

	if (results_count != 0)
		return results_count;

	// No fonts found, signal callback and try again.
	if (font_collection->fallback_func) {
		if (font_collection->fallback_func(font_collection, requested_lang, requested_script, requested_font_family, font_collection->fallback_context)) {
			results_count =  skb__match_fonts(
				font_collection, requested_lang, requested_script, requested_font_family,
				requested_weight, requested_style, requested_stretch, results, results_cap);
		}
	}

	return results_count;
}

skb_font_handle_t skb_font_collection_get_default_font(skb_font_collection_t* font_collection, uint8_t font_family)
{
	skb_font_handle_t results[32];
	int32_t results_count = skb_font_collection_match_fonts(
		font_collection, "", SBScriptLATN, font_family,
		SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL,
		results, SKB_COUNTOF( results ) );
	return results_count > 0 ? results[0] : (skb_font_handle_t)0;
}

skb_font_t* skb_font_collection_get_font(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle)
{
	return skb__get_font_by_handle(font_collection, font_handle);
}

uint32_t skb_font_collection_get_id(const skb_font_collection_t* font_collection)
{
	assert(font_collection);
	return font_collection->id;
}

skb_rect2_t skb_font_get_glyph_bounds(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle, uint32_t glyph_id, float font_size)
{
	const skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	if (!font || glyph_id == 0) return (skb_rect2_t) { 0 };

	hb_glyph_extents_t extents;
	if (hb_font_get_glyph_extents(font->hb_font, glyph_id, &extents)) {
		const float scale = font_size * font->upem_scale;
		const float x = (float)extents.x_bearing * scale;
		const float y = -(float)extents.y_bearing * scale;
		const float width = (float)extents.width * scale;
		const float height = -(float)extents.height * scale;

		return (skb_rect2_t) {
			.x = x,
			.y = y,
			.width = width,
			.height = height,
		};
	}
	return (skb_rect2_t) { 0 };
}


skb_font_metrics_t skb_font_get_metrics(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle)
{
	const skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	if (!font) return (skb_font_metrics_t) {0};
	return font->metrics;
}

skb_caret_metrics_t skb_font_get_caret_metrics(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle)
{
	const skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	if (!font) return (skb_caret_metrics_t) {0};
	return font->caret_metrics;
}

hb_font_t* skb_font_get_hb_font(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle)
{
	const skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	if (!font) return NULL;
	return font->hb_font;
}

static float skb__get_baseline_normalized(const skb_font_t* font, hb_ot_layout_baseline_tag_t baseline_tag, skb_text_direction_t direction, hb_script_t script)
{
	hb_position_t coord;
	hb_ot_layout_get_baseline_with_fallback2(font->hb_font, baseline_tag, skb_is_rtl(direction) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR, script, NULL, &coord);
	return -(float)coord * font->upem_scale;
}

static void skb__init_baseline_set(const skb_font_t* font, skb_baseline_set_t* baseline_set, skb_text_direction_t direction, uint8_t script)
{
	const uint32_t unicode_tag = SBScriptGetUnicodeTag(script);
	const hb_script_t hb_script = hb_script_from_iso15924_tag(unicode_tag);
	baseline_set->direction = (uint8_t)direction;
	baseline_set->script = script;
	baseline_set->alphabetic = skb__get_baseline_normalized(font, HB_OT_LAYOUT_BASELINE_TAG_ROMAN, direction, hb_script);

	{
		// Harfbuzz uses descender as synthesized value for ideographic, which seems often too low.
		// Using the CSS algorithm here, which is descender scaled to (ascender + descender) normalized to em-square.
		// If value exists in the tables, use it.
		hb_position_t coord;
		if (hb_ot_layout_get_baseline(font->hb_font, HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT, skb_is_rtl(direction) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR, script, HB_OT_TAG_DEFAULT_LANGUAGE, &coord)) {
			baseline_set->ideographic = -(float)coord * font->upem_scale;
		} else {
			// Synthesize as per https://www.w3.org/TR/css-inline-3/#baseline-synthesis-em
			const float sum = -font->metrics.ascender + font->metrics.descender;
			baseline_set->ideographic = font->metrics.descender / sum;
		}
	}

	baseline_set->central = font->metrics.cap_height * 0.5f; // This is deviating from the CSS, but results nicer center alignment.
	baseline_set->hanging = skb__get_baseline_normalized(font, HB_OT_LAYOUT_BASELINE_TAG_HANGING, direction, hb_script);
	baseline_set->mathematical = skb__get_baseline_normalized(font, HB_OT_LAYOUT_BASELINE_TAG_MATH, direction, hb_script);
	baseline_set->middle = font->metrics.x_height * 0.5f;
	baseline_set->text_bottom = font->metrics.descender;
	baseline_set->text_top = font->metrics.ascender;
}

const skb_baseline_set_t* skb__font_get_normalzed_baseline_set(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle, skb_text_direction_t direction, uint8_t script)
{
	skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	if (!font) return NULL;

	skb_baseline_set_t* baseline_set = NULL;
	for (int32_t i = 0; i < font->baseline_sets_count; i++) {
		if (font->baseline_sets[i].direction == (uint8_t)direction && font->baseline_sets[i].script == script) {
			baseline_set = &font->baseline_sets[i];
			break;
		}
	}
	if (!baseline_set) {
		SKB_ARRAY_RESERVE(font->baseline_sets, font->baseline_sets_count + 1);
		baseline_set = &font->baseline_sets[font->baseline_sets_count++];
		skb__init_baseline_set(font, baseline_set, direction, script);
	}

	return baseline_set;
}

skb_baseline_set_t skb_font_get_baseline_set(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle, skb_text_direction_t direction, uint8_t script, float font_size)
{
	const skb_baseline_set_t* baseline_set = skb__font_get_normalzed_baseline_set(font_collection, font_handle, direction, script);
	if (!baseline_set) return (skb_baseline_set_t){0};

	return (skb_baseline_set_t) {
		.alphabetic = baseline_set->alphabetic * font_size,
		.ideographic = baseline_set->ideographic * font_size,
		.central = baseline_set->central * font_size,
		.hanging = baseline_set->hanging * font_size,
		.mathematical = baseline_set->mathematical * font_size,
		.middle = baseline_set->middle * font_size,
		.text_bottom = baseline_set->text_bottom * font_size,
		.text_top = baseline_set->text_top * font_size,
	};
}

float skb_font_get_baseline(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle, skb_baseline_t baseline, skb_text_direction_t direction, uint8_t script, float font_size)
{
	const skb_baseline_set_t* baseline_set = skb__font_get_normalzed_baseline_set(font_collection, font_handle, direction, script);
	if (!baseline_set) return 0.f;
	return baseline_set->baselines[baseline] * font_size;
}
