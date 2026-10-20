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


static void skb__add_unique_tag(skb__sb_tag_array_t* script_tags, uint8_t sb_script)
{
	for (int32_t i = 0; i < script_tags->tags_count; i++) {
		if (script_tags->tags[i] == sb_script)
			return;
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
				skb__add_unique_tag(script_tags, sb_script);
				break;
			}
		}
	}
}

static void skb__append_tags_from_unicodes(hb_set_t* unicodes, skb__sb_tag_array_t* scripts)
{
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

static void skb__font_destroy(skb_font_collection_t* font_collection, skb_font_t* font)
{
	if (!font) return;

	const int32_t font_idx = (int32_t)(font - font_collection->fonts);
	assert(font_idx >= 0 && font_idx < font_collection->fonts_count);
	const uint32_t generation = font->generation;

	skb_free(font->name);
	skb_free(font->scripts);
	skb_free(font->baseline_sets);
	skb_free(font->glyph_bounds);
	hb_set_destroy(font->unicodes);
	hb_font_destroy(font->hb_font);
	memset(font, 0, sizeof(skb_font_t));

	// Increate generation so that we can catch stale use.
	font->generation = generation + 1;

	// Update freelist
	font->next_free = font_collection->fonts_free_list;
	font_collection->fonts_free_list = font_idx;
}


static skb_font_t* skb__font_create(skb_font_collection_t* font_collection, hb_font_t* hb_font, const char* name, uint8_t font_family)
{
	assert(hb_font);

	int32_t font_idx = SKB_INVALID_INDEX;
	uint32_t generation = 1;
	if (font_collection->fonts_free_list != SKB_INVALID_INDEX) {
		// Pop from freelist if available.
		font_idx = font_collection->fonts_free_list;
		font_collection->fonts_free_list = font_collection->fonts[font_idx].next_free;
		generation = font_collection->fonts[font_idx].generation;
	} else {
		SKB_ARRAY_RESERVE(font_collection->fonts, font_collection->fonts_count+1);
		font_idx = font_collection->fonts_count++;
		memset(&font_collection->fonts[font_idx], 0, sizeof(skb_font_t));
	}
	assert(font_idx != SKB_INVALID_INDEX);

	skb_font_t* font = &font_collection->fonts[font_idx];

	font->generation = generation;
	font->next_free = SKB_INVALID_INDEX;
	font->handle = skb__make_font_handle(font_idx, generation);

	// Keep reference to the HB font
	font->hb_font = hb_font;
	hb_font_reference(font->hb_font);

	hb_face_t* face = hb_font_get_face(hb_font);
	assert(face);

	// Get how many points per EM, used to scale font size.
	unsigned int upem = hb_face_get_upem(face);
	font->upem = (int)upem;
	font->upem_scale = 1.f / (float)upem;

	// Get supported unicode range.
	font->unicodes = hb_set_create();
	hb_set_reference(font->unicodes);

	// Get supported scripts from supported characters.
	skb__sb_tag_array_t scripts = {0};
	hb_face_collect_unicodes(face, font->unicodes);
	skb__append_tags_from_unicodes(font->unicodes, &scripts);

	// Check synthetic properties.
	float synthetic_weight = 0.f;
	float synthetic_embolden_x = 0.f;
	float synthetic_embolden_y = 0.f;
	hb_font_get_synthetic_bold(hb_font, &synthetic_embolden_x, &synthetic_embolden_y, NULL);
	if (skb_absf(synthetic_embolden_x) > 1e-6f || skb_absf(synthetic_embolden_y) > 1e-6f)
		synthetic_weight = 700.f;
	const float synthetic_slant = hb_font_get_synthetic_slant(hb_font);

	// Font weight
	const float weight = skb_maxf(synthetic_weight, hb_style_get_value(hb_font, HB_STYLE_TAG_WEIGHT));
	font->weight = (uint16_t)weight;

	// Font italic and slant.
	const float italic = hb_style_get_value(hb_font, HB_STYLE_TAG_ITALIC);
	const float slant = hb_style_get_value(hb_font, HB_STYLE_TAG_SLANT_RATIO);
	if (italic > 0.1f)
		font->style = SKB_STYLE_ITALIC;
	else if (slant > 0.01f)
		font->style = SKB_STYLE_OBLIQUE;
	else
		font->style = SKB_STYLE_NORMAL;

	// Font stretch
	const float width = hb_style_get_value(hb_font, HB_STYLE_TAG_WIDTH);
	font->stretch = width / 100.f;

	font->font_family = font_family;

	// Store name
	size_t name_len = strlen(name);
	font->name = skb_malloc(name_len+4+1);
	memcpy(font->name, name, name_len);

	// Append synthetic markers.
	if (skb_absf(synthetic_embolden_x) > 1e-6f || skb_absf(synthetic_embolden_y) > 1e-6f) {
		font->name[name_len++] = '_';
		font->name[name_len++] = 'B';
	}
	if (skb_absf(synthetic_slant) > 1e-6f) {
		font->name[name_len++] = '_';
		font->name[name_len++] = 'I';
	}
	font->name[name_len] = '\0';

	// Hash name for ID
	font->hash = skb_hash64_append_str(skb_hash64_empty(), font->name);

	// Store supported scripts
	font->scripts = scripts.tags;
	font->scripts_count = scripts.tags_count;
	scripts.tags = NULL;
	scripts.tags_count = 0;

	// Leaving this debug log here, as it has often been needed.
//	for (uint32_t i = 0; i < font->scripts_count; i++)
//		skb_debug_log(" - script: %c%c%c%c\n", HB_UNTAG(SBScriptGetOpenTypeTag(font->scripts[i])));

	// Store metrics
	hb_font_extents_t font_extents;
	if (hb_font_get_h_extents(font->hb_font, &font_extents)) {
		// Undo embolden affecting ascender
		if (synthetic_embolden_y > 0.f) {
			const int32_t y_strength = (int32_t)skb_absf (roundf ((float)font->upem * synthetic_embolden_y));
			font->metrics.ascender = -(float)(font_extents.ascender - y_strength) * font->upem_scale;
		} else {
			font->metrics.ascender = -(float)font_extents.ascender * font->upem_scale;
		}		font->metrics.descender = -(float)font_extents.descender * font->upem_scale;
		font->metrics.line_gap = (float)font_extents.line_gap * font->upem_scale;
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

	hb_position_t superscript_offset;
	hb_position_t superscript_scale;
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_SUPERSCRIPT_EM_Y_OFFSET, &superscript_offset);
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_SUPERSCRIPT_EM_Y_SIZE, &superscript_scale);
	font->metrics.superscript_offset = -(float)superscript_offset * font->upem_scale;
	font->metrics.superscript_scale = (float)superscript_scale * font->upem_scale;

	hb_position_t subscript_offset;
	hb_position_t subscript_scale;
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_SUBSCRIPT_EM_Y_OFFSET, &subscript_offset);
	hb_ot_metrics_get_position_with_fallback (font->hb_font, HB_OT_METRICS_TAG_SUBSCRIPT_EM_Y_SIZE, &subscript_scale);
	font->metrics.subscript_offset = (float)subscript_offset * font->upem_scale;
	font->metrics.subscript_scale = (float)subscript_scale * font->upem_scale;

	// Caret metrics
	hb_position_t caret_offset;
	hb_position_t caret_rise;
	hb_position_t caret_run;
	hb_ot_metrics_get_position_with_fallback(font->hb_font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_OFFSET, &caret_offset);
	hb_ot_metrics_get_position_with_fallback(font->hb_font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_RISE, &caret_rise);
	hb_ot_metrics_get_position_with_fallback(font->hb_font, HB_OT_METRICS_TAG_HORIZONTAL_CARET_RUN, &caret_run);
	font->caret_metrics.offset = (float)caret_offset * font->upem_scale;
	font->caret_metrics.slope = -(float)caret_run / (float)caret_rise;

	// Cache glyph bounds
	font->glyph_bounds_count = (int32_t)hb_face_get_glyph_count(face);
	if (font->glyph_bounds_count > 0) {
		font->glyph_bounds = skb_malloc(sizeof(skb_rect2_t) * font->glyph_bounds_count);
		memset(font->glyph_bounds, 0, sizeof(skb_rect2_t) * font->glyph_bounds_count);

		for (int32_t gid = 0; gid < font->glyph_bounds_count; gid++) {
			hb_glyph_extents_t extents;
			if (hb_font_get_glyph_extents(hb_font, gid, &extents)) {
				font->glyph_bounds[gid] = (skb_rect2_t) {
					.x = (float)extents.x_bearing * font->upem_scale,
					.y = -(float)extents.y_bearing * font->upem_scale,
					.width = (float)extents.width * font->upem_scale,
					.height = -(float)extents.height * font->upem_scale,
				};
			}
		}
	}

	return font;
}

skb_font_collection_t* skb_font_collection_create(void)
{
	static uint32_t id = 0;

	skb_font_collection_t* result = skb_malloc(sizeof(skb_font_collection_t));
	memset(result, 0, sizeof(skb_font_collection_t));

	result->id = ++id;
	result->fonts_free_list = SKB_INVALID_INDEX;

	return result;
}

void skb_font_collection_destroy(skb_font_collection_t* font_collection)
{
	if (!font_collection) return;
	for (int32_t i = 0; i < font_collection->fonts_count; i++)
		skb__font_destroy(font_collection, &font_collection->fonts[i]);
	skb_free(font_collection->fonts);
	skb_free(font_collection);
}

void skb_font_collection_set_on_font_fallback(skb_font_collection_t* font_collection, skb_font_fallback_func_t* fallback_func, void* context)
{
	assert(font_collection);
	font_collection->fallback_func = fallback_func;
	font_collection->fallback_context = context;
}

static bool skb__equals_synthetic_params(hb_font_t* hb_font, const skb_font_create_params_t* params)
{
	if (!params)
		return true;

	float embolden_x = 0.f;
	float embolden_y = 0.f;
	hb_bool_t in_place = true;
	hb_font_get_synthetic_bold(hb_font, &embolden_x, &embolden_y, &in_place);
	float slant = hb_font_get_synthetic_slant(hb_font);

	return skb_equalsf(params->embolden_x, embolden_x, 1e-6f)
		&& skb_equalsf(params->embolden_y, embolden_y, 1e-6f)
		&& skb_equalsf(params->slant, slant, 1e-6f)
		&& in_place;
}

static void skb__apply_synthetic_params(hb_font_t* hb_font, const skb_font_create_params_t* params)
{
	if (params) {
		if (skb_absf(params->embolden_x) > 1e-6f || skb_absf(params->embolden_y) > 1e-6f)
			hb_font_set_synthetic_bold(hb_font, params->embolden_x, params->embolden_y, true);
		if (skb_absf(params->slant) > 1e-6f)
			hb_font_set_synthetic_slant(hb_font, params->slant);
	}
}

#if !defined(SKB_NO_OPEN)
skb_font_handle_t skb_font_collection_add_font(skb_font_collection_t* font_collection, const char* file_name, uint8_t font_family, const skb_font_create_params_t* params)
{
	hb_blob_t* blob = NULL;
	hb_face_t* face = NULL;
	hb_font_t* hb_font = NULL;
	skb_font_handle_t result = 0;

	// skb_debug_log("Loading font: %s\n", path);

	// Use Harfbuzz to load the font data, it uses mmap when possible.
	blob = hb_blob_create_from_file_or_fail(file_name);
	if (!blob) goto cleanup;

	face = hb_face_create_or_fail(blob, 0);
	if (!face) goto cleanup;

	hb_font = hb_font_create(face);
	if (!hb_font) goto cleanup;

	skb__apply_synthetic_params(hb_font, params);

	skb_font_t* font = skb__font_create(font_collection, hb_font, file_name, font_family);
	assert(font);
	result = font->handle;

cleanup:
	hb_blob_destroy(blob);
	hb_face_destroy(face);
	hb_font_destroy(hb_font);

	return result;
}
#endif // !defined SKB_NO_OPEN

skb_font_handle_t skb_font_collection_add_font_from_data(
	skb_font_collection_t* font_collection, const char* name,
	const void* font_data, size_t font_data_length, void* context, skb_destroy_func_t* destroy_func,
	uint8_t font_family, const skb_font_create_params_t* params)
{
	hb_blob_t* blob = NULL;
	hb_face_t* face = NULL;
	hb_font_t* hb_font = NULL;
	skb_font_handle_t result = 0;

	// skb_debug_log("Loading font from data: %s\n", name);

	// Use Harfbuzz to create blob from memory data with read-only mode
	// Pass the context and destroy function to HarfBuzz so it can manage the lifetime
	blob = hb_blob_create_or_fail((const char*)font_data, (unsigned int)font_data_length, HB_MEMORY_MODE_READONLY, context, (hb_destroy_func_t)destroy_func);
	if (!blob) goto cleanup;

	face = hb_face_create_or_fail(blob, 0);
	if (!face) goto cleanup;

	hb_font = hb_font_create(face);
	if (!hb_font) goto cleanup;

	skb__apply_synthetic_params(hb_font, params);

	skb_font_t* font = skb__font_create(font_collection, hb_font, name, font_family);
	assert(font);
	result = font->handle;

cleanup:
	hb_blob_destroy(blob);
	hb_face_destroy(face);
	hb_font_destroy(hb_font);

	return result;
}

skb_font_handle_t skb_font_collection_add_hb_font(
	skb_font_collection_t* font_collection, const char* name,
	hb_font_t* hb_font, uint8_t font_family, const skb_font_create_params_t* params)
{
	hb_font_t* new_hb_font = NULL;

	// If parameters do not match, create new font.
	if (!skb__equals_synthetic_params(hb_font, params)) {
		hb_face_t* hb_face = hb_font_get_face(hb_font);
		assert(hb_face);
		new_hb_font = hb_font_create(hb_face);
		skb__apply_synthetic_params(new_hb_font, params);
		hb_font = new_hb_font;
	}

	skb_font_t* font = skb__font_create(font_collection, hb_font, name, font_family);
	assert(font);

	// Just unref, skb__font_create will take a reference to the passed font.
	hb_font_destroy(new_hb_font);

	return font->handle;
}

bool skb_font_collection_remove_font(skb_font_collection_t* font_collection, skb_font_handle_t font_handle)
{
	skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	if (!font)
		return false;

	skb__font_destroy(font_collection, font);

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

	int32_t results_count = 0;
	int32_t cur_results_count = 0;
	bool multiple_stretch = false;
	bool multiple_styles = false;
	bool multiple_weights = false;

	// Match script and font family.
	for (int32_t font_idx = 0; font_idx < font_collection->fonts_count; font_idx++) {
		const skb_font_t* font = &font_collection->fonts[font_idx];
		if (font->font_family == requested_font_family
			&& (requested_font_family == SKB_FONT_FAMILY_EMOJI || skb__supports_script(font, requested_script))) { // Ignore script for emoji fonts, as emojis are the same on each writing system.
			if (results_count < results_cap) {
				if (results_count > 0) {
					const skb_font_t* prev_font = skb__get_font_unchecked(font_collection, results[results_count - 1]);
					multiple_stretch |= !skb_equalsf(prev_font->stretch, font->stretch, 0.01f);
					multiple_styles |= prev_font->style != font->style;
					multiple_weights |= prev_font->weight != font->weight;
				}
				results[results_count++] = font->handle;
			}
		}
	}

	if (!results_count)
		return 0;

	// Match stretch.
	if (multiple_stretch) {
		float requested_stretch_value = g_stretch_to_value[skb_clampi((int32_t)requested_stretch, 0, SKB_COUNTOF(g_stretch_to_value)-1)];

		bool exact_stretch_match = false;
		float nearest_narrow_error = FLT_MAX;
		float nearest_narrow = requested_stretch_value;
		float nearest_wide_error = FLT_MAX;
		float nearest_wide = requested_stretch_value;

		for (int32_t i = 0; i < results_count; i++) {
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
		cur_results_count = results_count;
		results_count = 0;
		for (int32_t i = 0; i < cur_results_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (!skb_equalsf(selected_stretch, font->stretch, 0.01f))
				continue;
			results[results_count++] = results[i];
		}

		if (results_count <= 1)
			return results_count;
	}

	// Style
	if (multiple_styles) {
		int32_t normal_count = 0;
		int32_t italic_count = 0;
		int32_t oblique_count = 0;
		for (int32_t i = 0; i < results_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			uint8_t style = font->style;
			if (style == SKB_STYLE_NORMAL)
				normal_count++;
			if (style == SKB_STYLE_ITALIC)
				italic_count++;
			if (style == SKB_STYLE_OBLIQUE)
				oblique_count++;
		}

		// Filter and sort results based on style.
		// Treat italic and oblique equal, but give preference to the one requested.
		// This allows makes it easier to mix faked fonts with proper ones, like synthetic slant bold with regular italic.
		skb_style_t styles[3] = { 0 };
		int32_t styles_count = 0;
		if (requested_style == SKB_STYLE_ITALIC) {
			if (italic_count > 0 || oblique_count > 0) {
				styles[styles_count++] = SKB_STYLE_ITALIC;
				styles[styles_count++] = SKB_STYLE_OBLIQUE;
			} else {
				styles[styles_count++] = SKB_STYLE_NORMAL;
			}
		} else if (requested_style == SKB_STYLE_OBLIQUE) {
			if (italic_count > 0 || oblique_count > 0) {
				styles[styles_count++] = SKB_STYLE_OBLIQUE;
				styles[styles_count++] = SKB_STYLE_ITALIC;
			} else {
				styles[styles_count++] = SKB_STYLE_NORMAL;
			}
		} else {
			if (normal_count > 0) {
				styles[styles_count++] = SKB_STYLE_NORMAL;
			} else {
				styles[styles_count++] = SKB_STYLE_ITALIC;
				styles[styles_count++] = SKB_STYLE_OBLIQUE;
			}
		}

		// Prune and sort based on preferred order.
		cur_results_count = results_count;
		results_count = 0;
		for (int32_t si = 0; si < styles_count; si++) {
			for (int32_t i = 0; i < cur_results_count; i++) {
				const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
				if (font->style != styles[si])
					continue;
				// Add this result to the end of the results to keep, while keeping the remainder of the results in initial order.
				skb_font_handle_t res = results[i];
				for (int32_t j = i; j > results_count; j--)
					results[j] = results[j-1];
				results[results_count++] = res;
			}
		}

		if (results_count <= 1)
			return results_count;
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

		for (int32_t i = 0; i < results_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (requested_weight_value == font->weight) {
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
		cur_results_count = results_count;
		results_count = 0;
		for (int32_t i = 0; i < cur_results_count; i++) {
			const skb_font_t* font = skb__get_font_unchecked(font_collection, results[i]);
			if (font->weight != selected_weight)
				continue;
			results[results_count++] = results[i];
		}
	}

	return results_count;
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

bool skb_font_collection_font_has_codepoint(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle, uint32_t codepoint)
{
	const skb_font_t* font = skb__get_font_by_handle(font_collection, font_handle);
	return font && hb_set_has(font->unicodes, codepoint);
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
	if (!font || glyph_id == 0 || glyph_id >= (uint32_t)font->glyph_bounds_count) return (skb_rect2_t) { 0 };

	const skb_rect2_t* bounds = &font->glyph_bounds[glyph_id];

	return (skb_rect2_t) {
		.x = bounds->x * font_size,
		.y = bounds->y * font_size,
		.width = bounds->width * font_size,
		.height = bounds->height * font_size,
	};
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
