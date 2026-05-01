// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_icon_collection.h"
#include "skb_icon_collection_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if !defined(SKB_NO_OPEN)
#include <stdio.h>
#endif // !defined(SKB_NO_OPEN)

typedef struct skb__picosvg_gradient_name_t {
	char id[32];
	int32_t index;
} skb__picosvg_gradient_name_t;

typedef struct skb__picosvg_parser_t {
	skb_icon_t* icon;
	skb_icon_shape_t* current_shape[32];
	int32_t current_shape_idx;
	bool inside_def;
	skb__picosvg_gradient_name_t* gradient_names;
	int32_t gradient_names_count;
	int32_t gradient_names_cap;
} skb__picosvg_parser_t;


skb_icon_handle_t skb__make_icon_handle(int32_t index, uint32_t generation)
{
	assert(index >= 0 && index <= 0xffff);
	assert(generation >= 0 && generation <= 0xffff);
	return index | (generation << 16);
}

skb_icon_t* skb__get_icon_by_handle(const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle)
{
	uint32_t index = icon_handle & 0xffff;
	uint32_t generation = (icon_handle >> 16) & 0xffff;
	if ((int32_t)index < icon_collection->icons_count && icon_collection->icons[index].generation == generation)
		return &icon_collection->icons[index];
	return NULL;
}

skb_vec2_t skb_icon_collection_calc_proportional_scale(const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle, float width, float height)
{
	assert(icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_collection, icon_handle);

	if (!icon)
		return (skb_vec2_t) {0};

	if (width <= 0 && height <= 0) {
		// Auto width and height, use the icon size.
		return (skb_vec2_t) { 1.f, 1.f };
	}
	if (width <= 0) {
		// Auto width
		const float scale = icon->view.height > 0.f ? height / icon->view.height : 0.f;
		return (skb_vec2_t) { scale, scale };
	}
	if (height <= 0) {
		// Auto height
		const float scale = icon->view.width > 0.f ? width / icon->view.width : 0.f;
		return (skb_vec2_t) { scale, scale };
	}

	return (skb_vec2_t) {
		.x = icon->view.width > 0.f ? width / icon->view.width : 0.f,
		.y = icon->view.height > 0.f ? height / icon->view.height : 0.f,
	};
}


skb_vec2_t skb_icon_collection_get_icon_size(const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle)
{
	assert(icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_collection, icon_handle);
	if (!icon) return (skb_vec2_t){0};
	return (skb_vec2_t) {
		.x = icon->view.width,
		.y = icon->view.height,
	};
}

const skb_icon_t* skb_icon_collection_get_icon(const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle)
{
	assert(icon_collection);
	return skb__get_icon_by_handle(icon_collection, icon_handle);
}

static void skb__icon_shape_destroy(skb_icon_shape_t* shape)
{
	for (int32_t i = 0; i < shape->children_count; i++)
		skb__icon_shape_destroy(&shape->children[i]);
	skb_free(shape->path);
	skb_free(shape->children);
}

static void skb__icon_create(skb_icon_t* icon)
{
	memset(icon, 0, sizeof(skb_icon_t));

	// Init root shape.
	icon->root.gradient_idx = SKB_INVALID_INDEX;
	icon->root.opacity = 1.f;
}

static void skb__icon_destroy(skb_icon_t* icon)
{
	if (!icon) return;
	skb_free(icon->name);
	skb__icon_shape_destroy(&icon->root);
}

void skb__add_path_command(skb_icon_shape_t* shape, skb_icon_path_command_t cmd)
{
	SKB_ARRAY_RESERVE(shape->path, shape->path_count+1);
	shape->path[shape->path_count++] = cmd;
}

skb_icon_handle_t skb_icon_collection_add_icon(skb_icon_collection_t* icon_collection, const char* name, float width, float height)
{
	// If icon of same name already exists, fail.
	const uint64_t hash = skb_hash64_append_str(skb_hash64_empty(), name);
	if (skb_hash_table_find(icon_collection->icons_lookup, hash, NULL))
		return (skb_icon_handle_t)0;

	// Try to find free slot.
	int32_t icon_idx = SKB_INVALID_INDEX;
	uint32_t generation = 1;
	if (icon_collection->empty_icons_count > 0 ) {
		// Using linear search as we dont expect to have that many fonts loaded.
		for (int32_t i = 0; i < icon_collection->icons_count; i++) {
			if (icon_collection->icons[i].hash == 0) {
				icon_idx = i;
				icon_collection->empty_icons_count--;
				generation = icon_collection->icons[i].generation;
				break;
			}
		}
	} else {
		SKB_ARRAY_RESERVE(icon_collection->icons, icon_collection->icons_count+1);
		icon_idx = icon_collection->icons_count++;
	}
	assert(icon_idx != SKB_INVALID_INDEX);

	skb_icon_t* icon = &icon_collection->icons[icon_idx];
	skb__icon_create(icon);
	icon->is_color = true;
	icon->hash = skb_hash64_append_str(skb_hash64_empty(), name);

	int32_t name_len = strlen(name);
	icon->name = skb_malloc(name_len+1);
	memcpy(icon->name, name, name_len+1);

	icon->view = (skb_rect2_t) {
			.x = 0.f,
			.y = 0.f,
			.width = width,
			.height = height,
		};

	skb_hash_table_add(icon_collection->icons_lookup, icon->hash, icon_idx);

	icon->generation = generation;
	icon->handle = skb__make_icon_handle(icon_idx, generation);

	return icon->handle;
}

bool skb_icon_collection_remove_icon(skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle)
{
	skb_icon_t* icon = skb__get_icon_by_handle(icon_collection, icon_handle);
	if (!icon)
		return false;

	// Remove from lookup
	skb_hash_table_remove(icon_collection->icons_lookup, icon->hash);

	uint32_t generation = icon->generation + 1;

	skb__icon_destroy(icon);
	memset(icon, 0, sizeof(skb_icon_t));

	icon->generation = generation;

	icon_collection->empty_icons_count++;

	return true;

}

static void skb__picosvg_init(skb__picosvg_parser_t* parser, skb_icon_t* icon)
{
	memset(parser, 0, sizeof(skb__picosvg_parser_t));
	parser->icon = icon;
	parser->current_shape[0] = &parser->icon->root;
}

static void skb__picosvg_destroy(skb__picosvg_parser_t* parser)
{
	if (!parser) return;
	skb_free(parser->gradient_names);
}

skb_icon_shape_t* skb_icon_shape_add_child(skb_icon_shape_t* parent_shape)
{
	SKB_ARRAY_RESERVE(parent_shape->children, parent_shape->children_count + 1);
	skb_icon_shape_t* shape = &parent_shape->children[parent_shape->children_count++];
	memset(shape, 0, sizeof(skb_icon_shape_t));
	shape->gradient_idx = SKB_INVALID_INDEX;
	shape->opacity = 1.f;
	shape->color = skb_rgba(0, 0, 0, 255);
	return shape;
}

static void skb__picosvg_push_shape(skb__picosvg_parser_t* svg)
{
	skb_icon_shape_t* parent_shape = svg->current_shape[svg->current_shape_idx];
	skb_icon_shape_t* shape = skb_icon_shape_add_child(parent_shape);
	if (svg->current_shape_idx < 32) {
		svg->current_shape_idx++;
		svg->current_shape[svg->current_shape_idx] = shape;
	}
}

static void skb__picosvg_pop_shape(skb__picosvg_parser_t* svg)
{
	if (svg->current_shape_idx > 0)
		svg->current_shape_idx--;
}

static bool skb__is_space(char c)
{
	switch (c) {
	case ' ':
	case '\t':
	case '\n':
	case '\v':
	case '\f':
	case '\r':
		return true;
	default:
		return false;
	}
}

static bool skb__is_digit(const char c)
{
	return c >= '0' && c <= '9';
}

static bool skb__is_hex_digit(const char c)
{
	return c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F';
}

// Simple XML parser

#define SKB_XML_TAG 1
#define SKB_XML_CONTENT 2
#define SKB_XML_MAX_ATTRIBS 128

typedef struct skb__xml_str_t {
	const char* begin;
	const char* end;
} skb__xml_str_t;

typedef struct skb__xml_attr_t {
	skb__xml_str_t name;
	skb__xml_str_t value;
} skb__xml_attr_t;

typedef void skb__xml_content_callback_t(skb__xml_str_t data, void* user_data);
typedef void skb__xml_start_elemnent_callback_t(skb__xml_str_t element, skb__xml_attr_t* attribs, int32_t attribs_count, void* user_data);
typedef void skb__xml_end_element_callback_t(skb__xml_str_t element, void* user_data);

static skb__xml_str_t skb__xml_str_skip_space(skb__xml_str_t str)
{
	while (str.begin != str.end && skb__is_space(*str.begin)) str.begin++;
	return str;
}

static bool skb__xml_str_starts_with(skb__xml_str_t str, const char* tag)
{
	while (str.begin != str.end && *tag) {
		if (*str.begin != *tag) return false;
		str.begin++;
		tag++;
	}
	return true;
}

static bool skb__xml_str_equals(skb__xml_str_t str, const char* tag)
{
	while (str.begin != str.end && *tag) {
		if (*str.begin != *tag) return false;
		str.begin++;
		tag++;
	}
	return !*tag && str.begin == str.end;
}

static void skb__xml_parse_content(skb__xml_str_t input, skb__xml_content_callback_t* content, void* user_data)
{
	// Trim start white spaces
	while (input.begin != input.end && skb__is_space(*input.begin)) input.begin++;
	if (input.begin == input.end) return;

	if (content)
		content(input, user_data);
}

static void skb__xml_parse_element(skb__xml_str_t input, skb__xml_start_elemnent_callback_t* start_elem, skb__xml_end_element_callback_t* end_elem, void* user_data)
{
	skb__xml_attr_t attribs[SKB_XML_MAX_ATTRIBS];
	int32_t attribs_count = 0;
	bool found_start = 0;
	bool found_end = 0;

	// Check if the tag is end tag
	if (input.begin != input.end && *input.begin == '/') {
		input.begin++;
		found_end = true;
	} else {
		found_start = true;
	}

	// Skip comments, data and preprocessor stuff.
	if (input.begin == input.end || *input.begin == '?' || *input.begin == '!')
		return;

	// Get tag name
	skb__xml_str_t tag_name = {0};
	tag_name.begin = input.begin;
	while (input.begin != input.end && !skb__is_space(*input.begin)) input.begin++;
	tag_name.end = input.begin;

	// Get attribs
	while (!found_end && input.begin != input.end) {
		// Skip white space before the attrib name
		input = skb__xml_str_skip_space(input);
		if (input.begin == input.end) break;
		if (*input.begin == '/') {
			found_end = true;
			break;
		}

		// Store name and find end of it.
		skb__xml_str_t attr_name = {0};
		attr_name.begin = input.begin;
		while (input.begin != input.end && !skb__is_space(*input.begin) && *input.begin != '=') input.begin++;
		attr_name.end = input.begin;

		// Skip until the beginning of the value.
		while (input.begin != input.end && *input.begin != '\"' && *input.begin != '\'') input.begin++;
		if (input.begin == input.end) break;

		// Step over quote
		const char quote = *input.begin++;

		// Store value and find the end of it.
		skb__xml_str_t attr_value = {0};
		attr_value.begin = input.begin;
		while (input.begin != input.end && *input.begin != quote) input.begin++;
		attr_value.end = input.begin;

		// Step over quote
		if (input.begin != input.end)
			input.begin++;

		if (attribs_count < SKB_XML_MAX_ATTRIBS)
			attribs[attribs_count++] = (skb__xml_attr_t){ .name = attr_name, .value = attr_value};
	}

	// Call callbacks.
	if (found_start && start_elem)
		start_elem(tag_name, attribs, attribs_count, user_data);
	if (found_end && end_elem)
		end_elem(tag_name, user_data);
}

static bool skb__xml_parse(skb__xml_str_t input, skb__xml_start_elemnent_callback_t* start_elem, skb__xml_end_element_callback_t* end_elem, skb__xml_content_callback_t* content, void* user_data)
{
	skb__xml_str_t span = { .begin = input.begin, .end = input.end };
	int32_t state = SKB_XML_CONTENT;
	while (input.begin != input.end) {
		if (*input.begin == '<' && state == SKB_XML_CONTENT) {
			span.end = input.begin;
			skb__xml_parse_content(span, content, user_data);
			span.begin = input.begin;
			state = SKB_XML_TAG;
		} else if (*input.begin == '>' && state == SKB_XML_TAG) {
			// Start of a content or new tag.
			span.end = input.begin;
			if (span.begin != span.end) span.begin++; // skip <
			skb__xml_parse_element(span, start_elem, end_elem, user_data);
			span.begin = input.begin;
			state = SKB_XML_CONTENT;
		} else {
			input.begin++;
		}
	}

	return true;
}

static void skb__picosvg_start_element(skb__xml_str_t element, skb__xml_attr_t* attribs, int32_t attribs_count, void* user_data);
static void skb__picosvg_end_element(skb__xml_str_t element, void* user_data);

static float skb__atof(const char* s)
{
	char* cur = (char*)s;
	char* end = NULL;
	double res = 0.0, sign = 1.0;
	bool has_int_part = false, has_frac_part = false;

	// Parse optional sign
	if (*cur == '+') {
		cur++;
	} else if (*cur == '-') {
		sign = -1;
		cur++;
	}

	// Parse integer part
	if (skb__is_digit(*cur)) {
		// Parse digit sequence
		long long int_part = strtoll(cur, &end, 10);
		if (cur != end) {
			res = (double)int_part;
			has_int_part = true;
			cur = end;
		}
	}

	// Parse fractional part.
	if (*cur == '.') {
		cur++; // Skip '.'
		if (skb__is_digit(*cur)) {
			// Parse digit sequence
			long long frac_part = strtoll(cur, &end, 10);
			if (cur != end) {
				res += (double)frac_part / pow(10.0, (double)(end - cur));
				has_frac_part = true;
				cur = end;
			}
		}
	}

	// A valid number should have integer or fractional part.
	if (!has_int_part && !has_frac_part)
		return 0.f;

	// Parse optional exponent
	if (*cur == 'e' || *cur == 'E') {
		cur++; // skip 'E'
		long long exp_part = strtol(cur, &end, 10); // Parse digit sequence with sign
		if (cur != end) {
			res *= pow(10.0, (double)exp_part);
		}
	}

	return (float)(res * sign);
}

static skb__xml_str_t skb__picosvg_parse_number_buf(skb__xml_str_t str, char* buf, const int32_t buf_size)
{
	const int32_t last = buf_size-1;
	int32_t i = 0;

	// sign
	if (str.begin != str.end && *str.begin == '-' || *str.begin == '+') {
		if (i < last) buf[i++] = *str.begin;
		str.begin++;
	}
	// integer part
	while (str.begin != str.end && skb__is_digit(*str.begin)) {
		if (i < last) buf[i++] = *str.begin;
		str.begin++;
	}
	if (str.begin != str.end && *str.begin == '.') {
		// decimal point
		if (i < last) buf[i++] = *str.begin;
		str.begin++;
		// fraction part
		while (str.begin != str.end && skb__is_digit(*str.begin)) {
			if (i < last) buf[i++] = *str.begin;
			str.begin++;
		}
	}
	// exponent
	if (str.begin != str.end && (*str.begin == 'e' || *str.begin == 'E') && (str.begin[1] != 'm' && str.begin[1] != 'x')) {
		if (i < last) buf[i++] = *str.begin;
		str.begin++;
		if (str.begin != str.end && *str.begin == '-' || *str.begin == '+') {
			if (i < last) buf[i++] = *str.begin;
			str.begin++;
		}
		while (str.begin != str.end && skb__is_digit(*str.begin)) {
			if (i < last) buf[i++] = *str.begin;
			str.begin++;
		}
	}
	buf[i] = '\0';

	return str;
}

static skb__xml_str_t skb__picosvg_parse_number(skb__xml_str_t str, float* value)
{
	char buf[64];
	str = skb__picosvg_parse_number_buf(str, buf, sizeof(buf));
	*value = skb__atof(buf);
	return str;
}

static void skb__picosvg_parse_header(skb__picosvg_parser_t* svg, const skb__xml_attr_t* attribs, int32_t attribs_count)
{
	for (int32_t i = 0; i < attribs_count; i++) {
		skb__xml_attr_t attrib = attribs[i];
		if (skb__xml_str_equals(attrib.name, "viewBox")) {
			skb__xml_str_t str = attrib.value;
			str = skb__picosvg_parse_number(str, &svg->icon->view.x);
			while (str.begin != str.end && (skb__is_space(*str.begin) || *str.begin == '%' || *str.begin == ',')) str.begin++;
			str = skb__picosvg_parse_number(str, &svg->icon->view.y);
			while (str.begin != str.end && (skb__is_space(*str.begin) || *str.begin == '%' || *str.begin == ',')) str.begin++;
			str = skb__picosvg_parse_number(str, &svg->icon->view.width);
			while (str.begin != str.end && (skb__is_space(*str.begin) || *str.begin == '%' || *str.begin == ',')) str.begin++;
			str = skb__picosvg_parse_number(str, &svg->icon->view.height);
		}
	}
}

static int32_t skb__char_to_hex(const char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return 0;
}

static skb_color_t skb__picosvg_parse_color_hex(skb__xml_str_t value)
{
	value.begin++; // skip #

	char hex[6];
	int32_t count = 0;
	while (value.begin != value.end && skb__is_hex_digit(*value.begin) && count < 6 )
		hex[count++] = *value.begin++;

	if (count == 6) {
		return skb_rgba(
			(uint8_t)((skb__char_to_hex(hex[0]) << 4) | skb__char_to_hex(hex[1])),
			(uint8_t)((skb__char_to_hex(hex[2]) << 4) | skb__char_to_hex(hex[3])),
			(uint8_t)((skb__char_to_hex(hex[4]) << 4) | skb__char_to_hex(hex[5])),
			255);
	}
	if (count == 3) {
		return skb_rgba(
			(uint8_t)((skb__char_to_hex(hex[0]) << 4) | skb__char_to_hex(hex[0])),
			(uint8_t)((skb__char_to_hex(hex[1]) << 4) | skb__char_to_hex(hex[1])),
			(uint8_t)((skb__char_to_hex(hex[2]) << 4) | skb__char_to_hex(hex[2])),
			255);
	}
	return skb_rgba(0,0,0,255);
}

static skb_color_t skb__picosvg_parse_color_rgb(skb__xml_str_t value)
{
	value.begin += 4; // skip "rgb("

	const char delimiter[3] = {',', ',', ')'};
	char buf[32];
	float rgb[3] = { 0.f, 0.f, 0.f };

	for (int32_t i = 0; i < 3; i++) {
		// skip leading spaces
		value = skb__xml_str_skip_space(value);
		if (value.begin == value.end) break;

		// skip '+' (don't allow '-')
		if (*value.begin == '+') value.begin++;
		if (value.begin == value.end) break;

		// Parse number
		int32_t j = 0;
		while (value.begin != value.end && j < 31 && (skb__is_digit(*value.begin) || *value.begin == '.')) {
			buf[j++] = *value.begin++;
		}
		buf[j++] = '\0';
		rgb[i] = skb__atof(buf);

		// Handle percentage.
		if (value.begin != value.end && *value.begin == '%') {
			rgb[i] *= 2.5f; // 100% == 255
			value.begin++;
		}
		value = skb__xml_str_skip_space(value);

		// Skip over delimiter
		if (value.begin == value.end || *value.begin != delimiter[i]) break;
		value.begin++;
	}

	return (skb_color_t) {
			.r = (uint8_t)skb_clampf(rgb[0], 0.f, 255.f),
			.g = (uint8_t)skb_clampf(rgb[1], 0.f, 255.f),
			.b = (uint8_t)skb_clampf(rgb[2], 0.f, 255.f),
			.a = 255
		};
}

typedef struct skb__picosvg_named_color_t {
	const char* name;
	skb_color_t color;
} skb__picosvg_named_color_t;

static skb__picosvg_named_color_t g_svg_colors[] = {

	{ "red", { 255, 0, 0, 255 } },
	{ "green", { 0, 128, 0, 255} },
	{ "blue", { 0, 0, 255, 255} },
	{ "yellow", {255, 255, 0, 255} },
	{ "cyan", { 0, 255, 255, 255} },
	{ "magenta", {255, 0, 255, 255} },
	{ "black", { 0, 0, 0, 255} },
	{ "grey", {128, 128, 128, 255} },
	{ "gray", {128, 128, 128, 255} },
	{ "white", {255, 255, 255, 255} },

#ifdef SKB_SVG_ALL_COLOR_KEYWORDS
	{ "aliceblue", {240, 248, 255, 255} },
	{ "antiquewhite", {250, 235, 215, 255} },
	{ "aqua", { 0, 255, 255, 255} },
	{ "aquamarine", {127, 255, 212, 255} },
	{ "azure", {240, 255, 255, 255} },
	{ "beige", {245, 245, 220, 255} },
	{ "bisque", {255, 228, 196, 255} },
	{ "blanchedalmond", {255, 235, 205, 255} },
	{ "blueviolet", {138, 43, 226, 255} },
	{ "brown", {165, 42, 42, 255} },
	{ "burlywood", {222, 184, 135, 255} },
	{ "cadetblue", { 95, 158, 160, 255} },
	{ "chartreuse", {127, 255, 0, 255} },
	{ "chocolate", {210, 105, 30, 255} },
	{ "coral", {255, 127, 80, 255} },
	{ "cornflowerblue", {100, 149, 237, 255} },
	{ "cornsilk", {255, 248, 220, 255} },
	{ "crimson", {220, 20, 60, 255} },
	{ "darkblue", { 0, 0, 139, 255} },
	{ "darkcyan", { 0, 139, 139, 255} },
	{ "darkgoldenrod", {184, 134, 11, 255} },
	{ "darkgray", {169, 169, 169, 255} },
	{ "darkgreen", { 0, 100, 0, 255} },
	{ "darkgrey", {169, 169, 169, 255} },
	{ "darkkhaki", {189, 183, 107, 255} },
	{ "darkmagenta", {139, 0, 139, 255} },
	{ "darkolivegreen", { 85, 107, 47, 255} },
	{ "darkorange", {255, 140, 0, 255} },
	{ "darkorchid", {153, 50, 204, 255} },
	{ "darkred", {139, 0, 0, 255} },
	{ "darksalmon", {233, 150, 1, 2552} },
	{ "darkseagreen", {143, 188, 143, 255} },
	{ "darkslateblue", { 72, 61, 139, 255} },
	{ "darkslategray", { 47, 79, 79, 255} },
	{ "darkslategrey", { 47, 79, 79, 255} },
	{ "darkturquoise", { 0, 206, 209, 255} },
	{ "darkviolet", {148, 0, 211, 255} },
	{ "deeppink", {255, 20, 147, 255} },
	{ "deepskyblue", { 0, 191, 255, 255} },
	{ "dimgray", {105, 105, 105, 255} },
	{ "dimgrey", {105, 105, 105, 255} },
	{ "dodgerblue", { 30, 144, 255, 255} },
	{ "firebrick", {178, 34, 34, 255} },
	{ "floralwhite", {255, 250, 240, 255} },
	{ "forestgreen", { 34, 139, 34, 255} },
	{ "fuchsia", {255, 0, 255, 255} },
	{ "gainsboro", {220, 220, 220, 255} },
	{ "ghostwhite", {248, 248, 255, 255} },
	{ "gold", {255, 215, 0, 255} },
	{ "goldenrod", {218, 165, 32, 255} },
	{ "greenyellow", {173, 255, 47, 255} },
	{ "honeydew", {240, 255, 240, 255} },
	{ "hotpink", {255, 105, 180, 255} },
	{ "indianred", {205, 92, 92, 255} },
	{ "indigo", { 75, 0, 130, 255} },
	{ "ivory", {255, 255, 240, 255} },
	{ "khaki", {240, 230, 140, 255} },
	{ "lavender", {230, 230, 250, 255} },
	{ "lavenderblush", {255, 240, 245, 255} },
	{ "lawngreen", {124, 252, 0, 255} },
	{ "lemonchiffon", {255, 250, 205, 255} },
	{ "lightblue", {173, 216, 230, 255} },
	{ "lightcoral", {240, 128, 128, 255} },
	{ "lightcyan", {224, 255, 255, 255} },
	{ "lightgoldenrodyellow", {250, 250, 210, 255} },
	{ "lightgray", {211, 211, 211, 255} },
	{ "lightgreen", {144, 238, 144, 255} },
	{ "lightgrey", {211, 211, 211, 255} },
	{ "lightpink", {255, 182, 193, 255} },
	{ "lightsalmon", {255, 160, 122, 255} },
	{ "lightseagreen", { 32, 178, 170, 255} },
	{ "lightskyblue", {135, 206, 250, 255} },
	{ "lightslategray", {119, 136, 153, 255} },
	{ "lightslategrey", {119, 136, 153, 255} },
	{ "lightsteelblue", {176, 196, 222, 255} },
	{ "lightyellow", {255, 255, 224, 255} },
	{ "lime", { 0, 255, 0, 255} },
	{ "limegreen", { 50, 205, 50, 255} },
	{ "linen", {250, 240, 230, 255} },
	{ "maroon", {128, 0, 0, 255} },
	{ "mediumaquamarine", {102, 205, 170, 255} },
	{ "mediumblue", { 0, 0, 205, 255} },
	{ "mediumorchid", {186, 85, 211, 255} },
	{ "mediumpurple", {147, 112, 219, 255} },
	{ "mediumseagreen", { 60, 179, 113, 255} },
	{ "mediumslateblue", {123, 104, 238, 255} },
	{ "mediumspringgreen", { 0, 250, 154, 255} },
	{ "mediumturquoise", { 72, 209, 204, 255} },
	{ "mediumvioletred", {199, 21, 133, 255} },
	{ "midnightblue", { 25, 25, 112, 255} },
	{ "mintcream", {245, 255, 250, 255} },
	{ "mistyrose", {255, 228, 225, 255} },
	{ "moccasin", {255, 228, 181, 255} },
	{ "navajowhite", {255, 222, 173, 255} },
	{ "navy", { 0, 0, 128, 255} },
	{ "oldlace", {253, 245, 230, 255} },
	{ "olive", {128, 128, 0, 255} },
	{ "olivedrab", {107, 142, 35, 255} },
	{ "orange", {255, 165, 0, 255} },
	{ "orangered", {255, 69, 0, 255} },
	{ "orchid", {218, 112, 214, 255} },
	{ "palegoldenrod", {238, 232, 170, 255} },
	{ "palegreen", {152, 251, 152, 255} },
	{ "paleturquoise", {175, 238, 238, 255} },
	{ "palevioletred", {219, 112, 147, 255} },
	{ "papayawhip", {255, 239, 213, 255} },
	{ "peachpuff", {255, 218, 185, 255} },
	{ "peru", {205, 133, 63, 255} },
	{ "pink", {255, 192, 203, 255} },
	{ "plum", {221, 160, 221, 255} },
	{ "powderblue", {176, 224, 230, 255} },
	{ "purple", {128, 0, 128, 255} },
	{ "rosybrown", {188, 143, 143, 255} },
	{ "royalblue", { 65, 105, 225, 255} },
	{ "saddlebrown", {139, 69, 19, 255} },
	{ "salmon", {250, 128, 114, 255} },
	{ "sandybrown", {244, 164, 96, 255} },
	{ "seagreen", { 46, 139, 87, 255} },
	{ "seashell", {255, 245, 238, 255} },
	{ "sienna", {160, 82, 45, 255} },
	{ "silver", {192, 192, 192, 255} },
	{ "skyblue", {135, 206, 235, 255} },
	{ "slateblue", {106, 90, 205, 255} },
	{ "slategray", {112, 128, 144, 255} },
	{ "slategrey", {112, 128, 144, 255} },
	{ "snow", {255, 250, 250, 255} },
	{ "springgreen", { 0, 255, 127, 255} },
	{ "steelblue", { 70, 130, 180, 255} },
	{ "tan", {210, 180, 140, 255} },
	{ "teal", { 0, 128, 128, 255} },
	{ "thistle", {216, 191, 216, 255} },
	{ "tomato", {255, 99, 71, 255} },
	{ "turquoise", { 64, 224, 208, 255} },
	{ "violet", {238, 130, 238, 255} },
	{ "wheat", {245, 222, 179, 255} },
	{ "whitesmoke", {245, 245, 245, 255} },
	{ "yellowgreen", {154, 205, 50, 255} },
#endif
};

static skb_color_t skb__picosvg_parse_color_name(skb__xml_str_t value)
{
	for (int32_t i = 0; i < SKB_COUNTOF(g_svg_colors); i++) {
		if (skb__xml_str_equals(value, g_svg_colors[i].name))
			return g_svg_colors[i].color;
	}
	return skb_rgba(128, 128, 128, 255);
}

static skb_color_t skb__picosvg_parse_color(skb__xml_str_t value)
{
	value = skb__xml_str_skip_space(value);
	const int32_t len = (int32_t)(value.end - value.begin);
	if (skb__xml_str_starts_with(value, "#"))
		return skb__picosvg_parse_color_hex(value);
	if (skb__xml_str_starts_with(value, "rgb("))
		return skb__picosvg_parse_color_rgb(value);

	return skb__picosvg_parse_color_name(value);
}

static int32_t skb__picosvg_find_gradient_index(skb__picosvg_parser_t* svg, skb__xml_str_t key)
{
	// parse key from url(#key)

	// Strip from start up to #
	while (key.begin != key.end && *key.begin != '#')
		key.begin++;

	// Skip over #
	if (key.begin != key.end)
		key.begin++;

	// Strip from end up to )
	while (key.begin != key.end && *key.end != ')')
		key.end--;

	// Find gradient by id.
	for (int32_t i = 0; i < svg->gradient_names_count; i++) {
		if (skb__xml_str_equals(key, svg->gradient_names[i].id)) {
			return svg->gradient_names[i].index;
		}
	}
	return SKB_INVALID_INDEX;
}

static skb__xml_str_t skb__picosvg_path_parse_next_item_arc_flags(skb__xml_str_t d, char* buf, const int32_t buf_len)
{
	assert(buf_len >= 2);
	buf[0] = '\0';
	// Skip white spaces and commas
	while (d.begin != d.end && (skb__is_space(*d.begin) || *d.begin == ',')) d.begin++;
	if (d.begin == d.end) return d;
	if (*d.begin == '0' || *d.begin == '1') {
		buf[0] = *d.begin++;
		buf[1] = '\0';
	}
	return d;
}

static skb__xml_str_t skb__picosvg_path_parse_next_item(skb__xml_str_t d, char* buf, const int32_t buf_len)
{
	assert(buf_len >= 2);
	buf[0] = '\0';
	// Skip white spaces and commas
	while (d.begin != d.end && (skb__is_space(*d.begin) || *d.begin == ',')) d.begin++;
	if (d.begin == d.end) return d;
	if (*d.begin == '-' || *d.begin == '+' || *d.begin == '.' || skb__is_digit(*d.begin)) {
		d = skb__picosvg_parse_number_buf(d, buf, buf_len);
	} else {
		// Parse command
		buf[0] = *d.begin++;
		buf[1] = '\0';
	}
	return d;
}

static int skb__picosvg_path_is_coordinate(const char* s)
{
	// optional sign
	if (*s == '-' || *s == '+')
		s++;
	// must have at least one digit, or start by a dot
	return (skb__is_digit(*s) || *s == '.');
}


static int32_t skb__picosvg_path_get_arg_count_per_command(char cmd)
{
	if (cmd == 'M' || cmd == 'L') return 2;
	if (cmd == 'Q') return 4;
	if (cmd == 'C') return 6;
	if (cmd == 'A') return 7;
	if (cmd == 'Z') return 0;
	return -1;
}

static float skb__vecrat(float ux, float uy, float vx, float vy)
{
	return (ux*vx + uy*vy) / (sqrtf(ux*ux + uy*uy) * sqrtf(vx*vx + vy*vy));
}

static float skb__vecang(float ux, float uy, float vx, float vy)
{
	const float r = skb_clampf(skb__vecrat(ux,uy, vx,vy), -1.f, 1.f);
	return ((ux*vy < uy*vx) ? -1.0f : 1.0f) * acosf(r);
}

#define SKB_KAPPA90 (0.5522847493f)	// Length proportional to radius of a cubic bezier handle for 90deg arcs.

static void skb_picosvg_path_add_arc(skb_icon_shape_t* shape, float* args)
{
	if (shape->path_count == 0)
		return;
	skb_icon_path_command_t* prev_command = &shape->path[shape->path_count - 1];

	// Ported from canvg (https://code.google.com/p/canvg/)
	float rx = skb_absf(args[0]); // y radius
	float ry = skb_absf(args[1]); // x radius
	const float rotx = args[2] / 180.0f * SKB_PI; // x rotation angle
	const bool fa = skb_absf(args[3]) > 1e-6 ? true : false; // Large arc
	const bool fs = skb_absf(args[4]) > 1e-6 ? true : false; // Sweep direction
	const float x1 = prev_command->pt.x; // Start pt
	const float y1 = prev_command->pt.y;
	const float x2 = args[5]; // End pt
	const float y2 = args[6];

	const float dir_x = x1 - x2;
	const float dir_y = y1 - y2;
	float d = sqrtf(dir_x*dir_x + dir_y*dir_y);
	if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f) {
		// The arc degenerates to a line
		skb__add_path_command(shape, (skb_icon_path_command_t) {
			.type = SKB_SVG_LINE_TO,
			.pt.x = x2,
			.pt.y = y2,
		});
		return;
	}

	const float sinrx = sinf(rotx);
	const float cosrx = cosf(rotx);

	// Convert to center point parameterization.
	// http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
	// 1) Compute x1', y1'
	float x1p = cosrx * dir_x / 2.0f + sinrx * dir_y / 2.0f;
	float y1p = -sinrx * dir_x / 2.0f + cosrx * dir_y / 2.0f;
	d = skb_squaref(x1p) / skb_squaref(rx) + skb_squaref(y1p) / skb_squaref(ry);
	if (d > 1.f) {
		d = sqrtf(d);
		rx *= d;
		ry *= d;
	}
	// 2) Compute cx', cy'
	float s = 0.0f;
	float sa = skb_squaref(rx) * skb_squaref(ry) - skb_squaref(rx) * skb_squaref(y1p) - skb_squaref(ry) * skb_squaref(x1p);
	float sb = skb_squaref(rx) * skb_squaref(y1p) + skb_squaref(ry) * skb_squaref(x1p);
	if (sa < 0.0f) sa = 0.0f;
	if (sb > 0.0f)
		s = sqrtf(sa / sb);
	if (fa == fs)
		s = -s;
	const float cxp = s * rx * y1p / ry;
	const float cyp = s * -ry * x1p / rx;

	// 3) Compute cx,cy from cx',cy'
	float cx = (x1 + x2)/2.0f + cosrx*cxp - sinrx*cyp;
	float cy = (y1 + y2)/2.0f + sinrx*cxp + cosrx*cyp;

	// 4) Calculate theta1, and delta theta.
	float ux = (x1p - cxp) / rx;
	float uy = (y1p - cyp) / ry;
	float vx = (-x1p - cxp) / rx;
	float vy = (-y1p - cyp) / ry;
	float a1 = skb__vecang(1.0f,0.0f, ux,uy);	// Initial angle
	float da = skb__vecang(ux,uy, vx,vy);		// Delta angle

	if (!fs && da > 0.f)
		da -= 2.f * SKB_PI;
	else if (fs && da < 0.f)
		da += 2.f * SKB_PI;

	// Approximate the arc using cubic spline segments.

	// Split arc into max 90 degree segments.
	// The loop assumes an iteration per end point (including start and end), this +1.
	int32_t num_divs = (int32_t)(skb_absf(da) / (SKB_PI*0.5f) + 1.0f);
	float hda = (da / (float)num_divs) / 2.0f;
	// Fix for ticket #179: division by 0: avoid cotangens around 0 (infinite)
	if ((hda < 1e-3f) && (hda > -1e-3f))
		hda *= 0.5f;
	else
		hda = (1.0f - cosf(hda)) / sinf(hda);
	float kappa = fabsf(4.0f / 3.0f * hda);
	if (da < 0.0f)
		kappa = -kappa;

	float prev_x = 0, prev_y = 0;
	float prev_tanx = 0, prev_tany = 0;

	for (int32_t i = 0; i <= num_divs; i++) {
		const float a = a1 + da * ((float)i/(float)num_divs);
		const float dx = cosf(a);
		const float dy = sinf(a);

		const float lx = dx*rx;
		const float ly = dy*ry;
		const float x = lx*cosrx + ly*-sinrx + cx;
		const float y = lx*sinrx + ly*cosrx + cy;

		const float ltanx = -dy*rx * kappa;
		const float ltany = dx*ry * kappa;
		const float tanx = ltanx*cosrx + ltany*-sinrx;
		const float tany = ltanx*sinrx + ltany*cosrx;

		if (i > 0) {
			skb__add_path_command(shape, (skb_icon_path_command_t) {
				.type = SKB_SVG_CUBIC_TO,
				.cp0 = { prev_x + prev_tanx, prev_y + prev_tany },
				.cp1 = { x - tanx, y - tany },
				.pt = { x, y },
			});
		}

		prev_x = x;
		prev_y = y;
		prev_tanx = tanx;
		prev_tany = tany;
	}
}

static void skb__picosvg_parse_path(skb_icon_shape_t* shape, skb__xml_str_t path_data)
{
	float args[10];
	int32_t arg_count = 0;
	int32_t req_arg_count = 0;

	char item[64];
	char cmd = '\0';

	while (path_data.begin != path_data.end) {
			item[0] = '\0';

			if ((cmd == 'A' || cmd == 'a') && (arg_count == 3 || arg_count == 4))
				path_data = skb__picosvg_path_parse_next_item_arc_flags(path_data, item, sizeof(item));
			else
				path_data = skb__picosvg_path_parse_next_item(path_data, item, sizeof(item));

			if (!item[0]) break;
			if (cmd != '\0') {
				if (arg_count < 10 && skb__picosvg_path_is_coordinate(item))
					args[arg_count++] = skb__atof(item);
				if (arg_count >= req_arg_count) {
					switch (cmd) {
						case 'M':
							skb__add_path_command(shape, (skb_icon_path_command_t) {
								.type = SKB_SVG_MOVE_TO,
								.pt.x = args[0],
								.pt.y = args[1],
							});
							break;
						case 'L':
							skb__add_path_command(shape, (skb_icon_path_command_t) {
								.type = SKB_SVG_LINE_TO,
								.pt.x = args[0],
								.pt.y = args[1],
							});
							break;
						case 'C':
							skb__add_path_command(shape, (skb_icon_path_command_t) {
								.type = SKB_SVG_CUBIC_TO,
								.cp0 = { args[0], args[1] },
								.cp1 = { args[2], args[3] },
								.pt = { args[4], args[5] },
							});
							break;
						case 'Q':
							skb__add_path_command(shape, (skb_icon_path_command_t) {
								.type = SKB_SVG_QUAD_TO,
								.cp0 = { args[0], args[1] },
								.pt = { args[2], args[3] },
							});
							break;
						case 'A':
							skb_picosvg_path_add_arc(shape, args);
							break;
						default:
							break;
					}
					arg_count = 0;
					cmd = '\0';
				}
			} else {
				if (!skb__picosvg_path_is_coordinate(item)) {
					cmd = item[0];
					if (cmd == 'Z') {
						skb__add_path_command(shape, (skb_icon_path_command_t) {
							.type = SKB_SVG_CLOSE_PATH,
						});
						cmd = '\0';
						req_arg_count = 0;
					} else {
						req_arg_count = skb__picosvg_path_get_arg_count_per_command(cmd);
						if (req_arg_count == -1) {
							// Command not recognized
							cmd = '\0';
							req_arg_count = 0;
						}
					}
				}
			}
		}
}

static void skb__picosvg_parse_attribs(skb__picosvg_parser_t* svg, const skb__xml_attr_t* attribs, int32_t attribs_count)
{
	skb_icon_shape_t* shape = svg->current_shape[svg->current_shape_idx];

	for (int32_t i = 0; i < attribs_count; i++) {
		skb__xml_attr_t attrib = attribs[i];
		if (skb__xml_str_equals(attrib.name, "fill")) {
			if (skb__xml_str_starts_with(attrib.value, "url(")) {
				shape->gradient_idx = skb__picosvg_find_gradient_index(svg, attrib.value);
			} else {
				shape->color = skb__picosvg_parse_color(attrib.value);
			}
		} else if (skb__xml_str_equals(attrib.name, "opacity")) {
			skb__picosvg_parse_number(attrib.value, &shape->opacity);
		} else if (skb__xml_str_equals(attrib.name, "d")) {
			skb__picosvg_parse_path(shape, attrib.value);
		}
	}
}

static void skb__picosvg_parse_gradient_stop(skb__picosvg_parser_t* svg, const skb__xml_attr_t* attribs, int32_t attribs_count)
{
	skb_color_t color = skb_rgba(0,0,0,255);
	float opacity = 1.0f;
	float offset = 0.0f;

	for (int32_t i = 0; i < attribs_count; i++) {
		skb__xml_attr_t attrib = attribs[i];
		if (skb__xml_str_equals(attrib.name, "stop-color"))
			color = skb__picosvg_parse_color(attrib.value);
		else if (skb__xml_str_equals(attrib.name, "stop-opacity"))
			skb__picosvg_parse_number(attrib.value, &opacity);
		else if (skb__xml_str_equals(attrib.name, "offset"))
			skb__picosvg_parse_number(attrib.value, &offset);
	}

	if (svg->icon->gradients_count > 0) {
		skb_icon_gradient_t* grad = &svg->icon->gradients[svg->icon->gradients_count-1];
		SKB_ARRAY_RESERVE(grad->stops, grad->stops_count+1);
		skb_color_stop_t* stop = &grad->stops[grad->stops_count++];
		stop->color = skb_color_mul_alpha(color, (uint8_t)skb_clampf(opacity * 255.f, 0.f, 255.f));
		stop->offset = offset;
	}
}

static int32_t skb__picosvg_parse_transform_args(skb__xml_str_t str, float* args, int32_t max_args)
{
	while (str.begin != str.end && *str.begin != '(')
		str.begin++;
	if (str.begin != str.end) // Skip over (
		str.begin++;

	while (str.begin != str.end && *str.end != ')')
		str.end--;

	int32_t count = 0;
	while (str.begin != str.end) {

		if (*str.begin == '-' || *str.begin == '+' || *str.begin == '.' || skb__is_digit(*str.begin)) {
			float val;
			str = skb__picosvg_parse_number(str, &val);
			if (count < max_args)
				args[count++] = val;
		} else {
			++str.begin;
		}
	}
	return count;
}

static void skb__picosvg_parse_matrix(skb__xml_str_t str, skb_mat2_t* xform)
{
	float mat[6];
	const int32_t arg_count = skb__picosvg_parse_transform_args(str, mat, 6);
	if (arg_count == 6) {
		xform->xx = mat[0];
		xform->yx = mat[1];
		xform->xy = mat[2];
		xform->yy = mat[3];
		xform->dx = mat[4];
		xform->dy = mat[5];
	}
}

static void skb__picosvg_parse_gradient(skb__picosvg_parser_t* svg, uint8_t type, const skb__xml_attr_t* attribs, int32_t attribs_count)
{
	SKB_ARRAY_RESERVE(svg->icon->gradients, svg->icon->gradients_count+1);
	skb_icon_gradient_t* grad = &svg->icon->gradients[svg->icon->gradients_count++];
	grad->type = type;
	grad->xform = skb_mat2_make_identity();

	SKB_ARRAY_RESERVE(svg->gradient_names, svg->gradient_names_count+1);
	skb__picosvg_gradient_name_t* grad_name = &svg->gradient_names[svg->gradient_names_count++];
	grad_name->index = svg->icon->gradients_count - 1;

	for (int32_t i = 0; i < attribs_count; i++) {
		skb__xml_attr_t attrib = attribs[i];
		if (skb__xml_str_equals(attrib.name, "id")) {
			// Copy ID
			skb__xml_str_t value = attrib.value;
			int32_t i = 0;
			int32_t max_len = (int32_t)sizeof(grad_name->id)-1;
			while (value.begin != value.end && i < max_len)
				grad_name->id[i++] = *value.begin++;
			grad_name->id[i++] = '\0';
		} else if (skb__xml_str_equals(attrib.name, "cx")) {
			skb__picosvg_parse_number(attrib.value, &grad->p0.x);
		} else if (skb__xml_str_equals(attrib.name, "cy")) {
			skb__picosvg_parse_number(attrib.value, &grad->p0.y);
		} else if (skb__xml_str_equals(attrib.name, "r")) {
			skb__picosvg_parse_number(attrib.value, &grad->radius);
		} else if (skb__xml_str_equals(attrib.name, "fx")) {
			skb__picosvg_parse_number(attrib.value, &grad->p1.x);
		} else if (skb__xml_str_equals(attrib.name, "fy")) {
			skb__picosvg_parse_number(attrib.value, &grad->p1.y);
		} else if (skb__xml_str_equals(attrib.name, "x1")) {
			skb__picosvg_parse_number(attrib.value, &grad->p0.x);
		} else if (skb__xml_str_equals(attrib.name, "y1")) {
			skb__picosvg_parse_number(attrib.value, &grad->p0.y);
		} else if (skb__xml_str_equals(attrib.name, "x2")) {
			skb__picosvg_parse_number(attrib.value, &grad->p1.x);
		} else if (skb__xml_str_equals(attrib.name, "y2")) {
			skb__picosvg_parse_number(attrib.value, &grad->p1.y);
		} else if (skb__xml_str_equals(attrib.name, "gradientTransform")) {
			skb__picosvg_parse_matrix(attrib.value, &grad->xform);
		} else if (skb__xml_str_equals(attrib.name, "spreadMethod")) {
			if (skb__xml_str_equals(attrib.value, "pad"))
				grad->spread = SKB_SPREAD_PAD;
			else if (skb__xml_str_equals(attrib.value, "reflect"))
				grad->spread = SKB_SPREAD_REFLECT;
			else if (skb__xml_str_equals(attrib.value, "repeat"))
				grad->spread = SKB_SPREAD_REPEAT;
		}
	}
}

static void skb__picosvg_start_element(skb__xml_str_t element, skb__xml_attr_t* attribs, int32_t attribs_count, void* user_data)
{
	skb__picosvg_parser_t* svg = (skb__picosvg_parser_t*)user_data;

	if (svg->inside_def) {
		// Skip everything but gradients in defs
		if (skb__xml_str_equals(element, "linearGradient")) {
			skb__picosvg_parse_gradient(svg, SKB_GRADIENT_LINEAR, attribs, attribs_count);
		} else if (skb__xml_str_equals(element, "radialGradient")) {
			skb__picosvg_parse_gradient(svg, SKB_GRADIENT_RADIAL, attribs, attribs_count);
		} else if (skb__xml_str_equals(element, "stop")) {
			skb__picosvg_parse_gradient_stop(svg, attribs, attribs_count);
		}
		return;
	}

	if (skb__xml_str_equals(element, "g")) {
		skb__picosvg_push_shape(svg);
		skb__picosvg_parse_attribs(svg, attribs, attribs_count);
	} else if (skb__xml_str_equals(element, "path")) {
		skb__picosvg_push_shape(svg);
		skb__picosvg_parse_attribs(svg, attribs, attribs_count);
		skb__picosvg_pop_shape(svg);
	} else if (skb__xml_str_equals(element, "defs")) {
		svg->inside_def = 1;
	} else if (skb__xml_str_equals(element, "svg")) {
		skb__picosvg_parse_header(svg, attribs, attribs_count);
	}
}

static void skb__picosvg_end_element(skb__xml_str_t element, void* user_data)
{
	skb__picosvg_parser_t* svg = (skb__picosvg_parser_t*)user_data;

	if (skb__xml_str_equals(element, "g")) {
		skb__picosvg_pop_shape(svg);
	} else if (skb__xml_str_equals(element, "defs")) {
		svg->inside_def = 0;
	}
}


skb_icon_collection_t* skb_icon_collection_create(void)
{
	static uint32_t id = 0;

	skb_icon_collection_t* result = skb_malloc(sizeof(skb_icon_collection_t));
	memset(result, 0, sizeof(skb_icon_collection_t));

	result->icons_lookup = skb_hash_table_create();

	result->id = ++id;

	return result;
}

void skb_icon_collection_destroy(skb_icon_collection_t* icon_collection)
{
	if (!icon_collection) return;
	for (int32_t i = 0; i < icon_collection->icons_count; i++)
		skb__icon_destroy(&icon_collection->icons[i]);
	skb_hash_table_destroy(icon_collection->icons_lookup);
	skb_free(icon_collection);
}

skb_icon_handle_t skb_icon_collection_add_picosvg_icon_from_data(skb_icon_collection_t* icon_collection, const char* name, const char* icon_data, const int32_t icon_data_length)
{
	skb_icon_handle_t icon_handle = skb_icon_collection_add_icon(icon_collection, name, 0.f, 0.f);
	if (!icon_handle) return (skb_icon_handle_t)0;

	skb__picosvg_parser_t svg;
	skb__picosvg_init(&svg, skb__get_icon_by_handle(icon_collection, icon_handle));

	if (!skb__xml_parse((skb__xml_str_t){ icon_data, icon_data + icon_data_length }, &skb__picosvg_start_element, &skb__picosvg_end_element, NULL, &svg)) {
		skb_icon_collection_remove_icon(icon_collection, icon_handle);
		icon_handle = 0;
	}

	skb__picosvg_destroy(&svg);

	return icon_handle;
}

#if !defined(SKB_NO_OPEN)
skb_icon_handle_t skb_icon_collection_add_picosvg_icon(skb_icon_collection_t* icon_collection, const char* name, const char* file_name)
{
	FILE* file = NULL;
	char* buffer = NULL;

	const int32_t name_len = strlen(name);
	if (name_len <= 0) goto error;

	file = fopen(file_name, "rb");
	if (!file) goto error;

	// Get file size
	fseek(file, 0, SEEK_END);
	size_t buffer_length = ftell(file);
	fseek(file, 0, SEEK_SET);

	// Allocate and read
	buffer = skb_malloc(buffer_length);
	if (!buffer) goto error;

	if (fread(buffer, 1, buffer_length, file) != buffer_length)
		goto error;

	fclose(file);
	file = NULL;

	skb_icon_handle_t icon_handle = skb_icon_collection_add_picosvg_icon_from_data(icon_collection, name, buffer, (int32_t)buffer_length);
	if (!icon_handle) goto error;

	skb_free(buffer);

	return icon_handle;

error:
	if (file) fclose(file);
	if (buffer) skb_free(buffer);

	return (skb_icon_handle_t)0;
}
#endif // !defined(SKB_NO_OPEN)

skb_icon_handle_t skb_icon_collection_find_icon(const skb_icon_collection_t* icon_collection, const char* name)
{
	uint64_t hash = skb_hash64_append_str(skb_hash64_empty(), name);
	int32_t icon_index;
	if (skb_hash_table_find(icon_collection->icons_lookup, hash, &icon_index))
		return icon_collection->icons[icon_index].handle;
	return (skb_icon_handle_t)0;
}

void skb_icon_collection_set_is_color(skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle, bool is_color)
{
	assert(icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_collection, icon_handle);
	if (icon)
		icon->is_color = is_color;
}


skb_icon_builder_t skb_icon_builder_make(skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle)
{
	skb_icon_builder_t icon_builder = {0};
	icon_builder.icon_collection = icon_collection;
	icon_builder.icon_handle = icon_handle;
	icon_builder.shape_stack_idx = SKB_INVALID_INDEX;
	return icon_builder;
}

void skb_icon_builder_begin_shape(skb_icon_builder_t* icon_builder)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;

	assert(icon_builder->shape_stack_idx+1 < SKB_ICON_BUILDER_MAX_NESTED_SHAPES);

	skb_icon_shape_t* parent_shape = icon_builder->shape_stack_idx == SKB_INVALID_INDEX ? &icon->root : icon_builder->shape_stack[icon_builder->shape_stack_idx];
	SKB_ARRAY_RESERVE(parent_shape->children, parent_shape->children_count + 1);
	skb_icon_shape_t* shape = &parent_shape->children[parent_shape->children_count++];
	memset(shape, 0, sizeof(skb_icon_shape_t));
	shape->gradient_idx = SKB_INVALID_INDEX;
	shape->opacity = 1.f;

	icon_builder->shape_stack_idx++;
	icon_builder->shape_stack[icon_builder->shape_stack_idx] = shape;
}

void skb_icon_builder_end_shape(skb_icon_builder_t* icon_builder)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;

	assert(icon_builder->shape_stack_idx >= 0);
	icon_builder->shape_stack_idx--;
}

void skb_icon_builder_move_to(skb_icon_builder_t* icon_builder, skb_vec2_t pt)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	skb__add_path_command(shape, (skb_icon_path_command_t) {
		.type = SKB_SVG_MOVE_TO,
		.pt = pt,
	});
}

void skb_icon_builder_line_to(skb_icon_builder_t* icon_builder, skb_vec2_t pt)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	skb__add_path_command(shape, (skb_icon_path_command_t) {
		.type = SKB_SVG_LINE_TO,
		.pt = pt,
	});
}

void skb_icon_builder_quad_to(skb_icon_builder_t* icon_builder, skb_vec2_t cp, skb_vec2_t pt)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	skb__add_path_command(shape, (skb_icon_path_command_t) {
		.type = SKB_SVG_QUAD_TO,
		.cp0 = cp,
		.pt = pt,
	});
}

void skb_icon_builder_cubic_to(skb_icon_builder_t* icon_builder, skb_vec2_t cp0, skb_vec2_t cp1, skb_vec2_t pt)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	skb__add_path_command(shape, (skb_icon_path_command_t) {
		.type = SKB_SVG_CUBIC_TO,
		.cp0 = cp0,
		.cp1 = cp1,
		.pt = pt,
	});
}

void skb_icon_builder_close_path(skb_icon_builder_t* icon_builder)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	skb__add_path_command(shape, (skb_icon_path_command_t) {
		.type = SKB_SVG_CLOSE_PATH,
	});
}

void skb_icon_builder_fill_opacity(skb_icon_builder_t* icon_builder, float opacity)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	shape->opacity = opacity;
}

void skb_icon_builder_fill_color(skb_icon_builder_t* icon_builder, skb_color_t color)
{
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	shape->color = color;
	shape->gradient_idx = SKB_INVALID_INDEX;
}

void skb_icon_builder_fill_linear_gradient(skb_icon_builder_t* icon_builder, skb_vec2_t p0, skb_vec2_t p1, skb_mat2_t xform, skb_gradient_spread_t spread, skb_color_stop_t* stops, int32_t stops_count)
{
	assert(stops);
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	if (stops_count == 1) {
		shape->color = stops[0].color;
		shape->gradient_idx = SKB_INVALID_INDEX;
	} else if (stops_count > 1) {
		// Make gradient
		SKB_ARRAY_RESERVE(icon->gradients, icon->gradients_count+1);
		skb_icon_gradient_t* grad = &icon->gradients[icon->gradients_count++];
		grad->type = SKB_GRADIENT_LINEAR;
		grad->p0 = p0;
		grad->p1 = p1;
		grad->radius = 0.f;
		grad->xform = xform;
		grad->spread = spread;

		SKB_ARRAY_RESERVE(grad->stops, stops_count);
		memcpy(grad->stops, stops, sizeof(skb_color_stop_t) * stops_count);
		grad->stops_count = stops_count;

		shape->gradient_idx = icon->gradients_count-1;
	}
}

void skb_icon_builder_fill_radial_gradient(skb_icon_builder_t* icon_builder, skb_vec2_t p0, skb_vec2_t p1, float radius, skb_mat2_t xform, skb_gradient_spread_t spread, skb_color_stop_t* stops, int32_t stops_count)
{
	assert(stops);
	assert(icon_builder);
	assert(icon_builder->icon_collection);
	skb_icon_t* icon = skb__get_icon_by_handle(icon_builder->icon_collection, icon_builder->icon_handle);
	if (!icon) return;
	assert(icon_builder->shape_stack_idx >= 0);
	skb_icon_shape_t* shape = icon_builder->shape_stack[icon_builder->shape_stack_idx];
	assert(shape);

	if (stops_count == 1) {
		shape->color = stops[0].color;
		shape->gradient_idx = SKB_INVALID_INDEX;
	} else if (stops_count > 1) {
		SKB_ARRAY_RESERVE(icon->gradients, icon->gradients_count+1);
		skb_icon_gradient_t* grad = &icon->gradients[icon->gradients_count++];
		grad->type = SKB_GRADIENT_RADIAL;
		grad->p0 = p0;
		grad->p1 = p1;
		grad->radius = radius;
		grad->xform = xform;
		grad->spread = spread;

		SKB_ARRAY_RESERVE(grad->stops, stops_count);
		memcpy(grad->stops, stops, sizeof(skb_color_stop_t) * stops_count);
		grad->stops_count = stops_count;

		shape->gradient_idx = icon->gradients_count-1;
	}
}
