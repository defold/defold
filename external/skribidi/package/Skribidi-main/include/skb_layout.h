// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_LAYOUT_H
#define SKB_LAYOUT_H

#include <stdint.h>
#include <stdbool.h>
#include "skb_common.h"
#include "skb_font_collection.h"

#ifdef __cplusplus
extern "C" {
#endif

// Harfbuzz forward declarations
typedef struct hb_font_t hb_font_t;
typedef const struct hb_language_impl_t *hb_language_t;

/**
 * @defgroup layout Layout
 *
 * The layout takes text with attributes, and fonts as input, and gives an array of glyphs to render as output.
 *
 * To build the layout, the text is first split into runs based on the Unicode bidirectional algorithm.
 * Then the text is itemized into runs of same script (writing system), style, and direction.
 * Next the runs of text are shaped, arranging and combining the glyphs based on the rules of the script,
 * and finally the rungs of glyphs are arranged into lines.
 *
 * Some units are marked as pixels (px), but they can be interpreted as a generic units too.
 * If you are using the renderer or render cache, the values will correspond to pixels.
 *
 * Layout represents the text internally as utf-32 (codepoints) to avoid extra layer of offset translations.
 * Functions and structs that describe text position have the offset as utf-32. If it is needed to
 * convert back to utf-8, use skb_utf8_codepoint_offset().
 *
 * @{
 */

/** Enum describing horizontal alignment of a line of the text. */
typedef enum {
	/** Align to the language specific start. Left for LTR and right for RTL. */
	SKB_ALIGN_START = 0,
	/** Align to the language specific end. Right for LTR and left for RTL. */
	SKB_ALIGN_END,
	/** Center align the lines. */
	SKB_ALIGN_CENTER,
} skb_align_t;

/** Enum describing how line height is calculated */
typedef enum {
	/** Line height comes from font metrics. No scaling applied. */
	SKB_LINE_HEIGHT_NORMAL = 0,
	/** Line height is multiple of line height specified in font metrics. */
	SKB_LINE_HEIGHT_METRICS_RELATIVE,
	/** Line height is multiple of font size. */
	SKB_LINE_HEIGHT_FONT_SIZE_RELATIVE,
	/** Line height is the specified value.  */
	SKB_LINE_HEIGHT_ABSOLUTE,
} skb_line_height_t;

/** Enum describing text decoration position. */
typedef enum {
	/** Under the alphabetic baseline. */
	SKB_DECORATION_UNDERLINE = 0,
	/** Under the descender. */
	SKB_DECORATION_BOTTOMLINE,
	/** Over the ascender */
	SKB_DECORATION_OVERLINE,
	/** Through the text. */
	SKB_DECORATION_THROUGHLINE,
} skb_decoration_position_t;

//
// Text attributes
//

/**
 * Writing mode text attribute.
 * If multiple writing attributes are defined, only the first one is used.
 * Subset of https://drafts.csswg.org/css-writing-modes-4/
 */
typedef struct skb_attribute_writing_t {
	// Attribute kind tag, must be first.
	uint32_t kind;
	/** Text writing direction, no change if SKB_DIRECTION_AUTO. */
	uint8_t direction;
	/** BCP 47 language tag, e.g. fi-FI. If NULL, do not override language.  */
	const char* lang;
} skb_attribute_writing_t;

/**
 * Font text attribute.
 * If multiple font attributes are defined, only the first one is used.
 * Subset of https://drafts.csswg.org/css-fonts/
 */
typedef struct skb_attribute_font_t {
	// Attribute kind tag, must be first.
	uint32_t kind;
	/** Font size (px). Default 16.0 */
	float size;
	/** Font family, see skb_font_family_t. Default SKB_FONT_FAMILY_DEFAULT. */
	uint8_t family;
	/** Font weight, see skb_weight_t. Default SKB_WEIGHT_NORMAL. */
	uint8_t weight;
	/** Font style, see skb_style_t. Default SKB_STYLE_NORMAL. */
	uint8_t style;
	/** Font stretch see skb_stretch_t. Default SKB_STRETCH_NORMAL. */
	uint8_t stretch;
} skb_attribute_font_t;

/**
 * Font feature text attribute.
 * The attribute array can contain multiple font feature attributes.
 * See https://learn.microsoft.com/en-us/typography/opentype/spec/featuretags
 */
typedef struct skb_attribute_font_feature_t {
	// Attribute kind tag, must be first.
	uint32_t kind;
	/** OpenType font feature tag */
	uint32_t tag;
	/** Taga value, often 1 = on, 0 = off. */
	uint32_t value;
} skb_attribute_font_feature_t;

/**
 * Text spacing attribute.
 * If multiple spacing attributes are defined, only the first one is used.
 * Subset of https://drafts.csswg.org/css-text/
 */
typedef struct skb_attribute_spacing_t {
	// Attribute kind tag, must be first.
	uint32_t kind;
	/** Letter spacing (px)  */
	float letter;
	/** Word spacing (px) */
	float word;
} skb_attribute_spacing_t;

/**
 * Line height attribute.
 * If multiple line height attributes are defined, only the first one is used.
 */
typedef struct skb_attribute_line_height_t {
	// Attribute kind tag, must be first.
	uint32_t kind;
	/** Line height type. See skb_line_height_t for types. */
	uint8_t type;
	/** Line height, see line_height_type how the value is interpreted. */
	float height;
} skb_attribute_line_height_t;

/**
 * Text fill color attribute.
 * It is up to the client code to decide if multiple fill attributes are supported.
 */
typedef struct skb_attribute_fill_t {
	// Attribute kind tag, must be first.
	uint32_t kind;
	/** Color of the text */
	skb_color_t color;
} skb_attribute_fill_t;

/**
 * Text line decoration attribute.
 * It is up to the client code to decide if multiple decoration attributes are supported.
 * Loosely based on https://drafts.csswg.org/css-text-decor-4/
 */
typedef struct skb_attribute_decoration_t {
	// Attribute kind tag, must be first.
	uint32_t kind;
	/** Position of the decoration line relative to the text. See skb_decoration_position_t. */
	uint8_t position;
	/** Style of the decoration line. See skb_decoration_style_t. */
	uint8_t style;
	/** Thickness of the decoration line to draw. If left to 0.0, the thickness will be based on the font. */
	float thickness;
	/** Offset of the decoration line relative to the position. For under and bottom the offset grows down, and for through and top line the offset grows up. */
	float offset;
	/** Color of the decoration line. */
	skb_color_t color;
} skb_attribute_decoration_t;


/** Enum describing tags for each of the attributes. */
typedef enum {
	/** Tag for skb_attribute_writing_t */
	SKB_ATTRIBUTE_WRITING = SKB_TAG('w','r','i','t'),
	/** Tag for skb_attribute_font_t */
	SKB_ATTRIBUTE_FONT = SKB_TAG('f','o','n','t'),
	/** Tag for skb_attribute_font_feature_t */
	SKB_ATTRIBUTE_FONT_FEATURE = SKB_TAG('f','e','a','t'),
	/** tag for skb_attribute_spacing_t */
	SKB_ATTRIBUTE_SPACING = SKB_TAG('s','p','c','e'),
	/** Tag for skb_attribute_line_height_t */
	SKB_ATTRIBUTE_LINE_HEIGHT = SKB_TAG('l','n','h','e'),
	/** Tag for skb_attribute_fill_t */
	SKB_ATTRIBUTE_FILL = SKB_TAG('f','i','l','l'),
	/** Tag for skb_attribute_decoration_t */
	SKB_ATTRIBUTE_DECORATION = SKB_TAG('d','e','c','o'),
} skb_attribute_type_t;

/**
 * Tagged union which can hold any text attribute.
 */
typedef union skb_attribute_t {
	uint32_t kind;
	skb_attribute_writing_t writing;
	skb_attribute_font_t font;
	skb_attribute_font_feature_t font_feature;
	skb_attribute_spacing_t spacing;
	skb_attribute_line_height_t line_height;
	skb_attribute_fill_t fill;
	skb_attribute_decoration_t decoration;
} skb_attribute_t;

/** @returns new writing mode text attribute. See skb_attribute_writing_t */
skb_attribute_t skb_attribute_make_writing(const char* lang, skb_text_direction_t direction);

/** @returns new font text attribute. See skb_attribute_font_t */
skb_attribute_t skb_attribute_make_font(skb_font_family_t family, float size, skb_weight_t weight, skb_style_t style, skb_stretch_t stretch);

/** @returns new font feature text attribute. See skb_attribute_font_feature_t */
skb_attribute_t skb_attribute_make_font_feature(uint32_t tag, uint32_t value);

/** @returns new spacing text attribute. See skb_attribute_spacing_t */
skb_attribute_t skb_attribute_make_spacing(float letter, float word);

/** @returns new line height text attribute. See skb_attribute_line?height_t */
skb_attribute_t skb_attribute_make_line_height(skb_line_height_t type, float height);

/** @returns new fill color text attribute. See skb_attribute_fill_t */
skb_attribute_t skb_attribute_make_fill(skb_color_t color);

/** @returns new text decoration attribute. See skb_attribute_decoration_t */
skb_attribute_t skb_attribute_make_decoration(skb_decoration_position_t position, skb_decoration_style_t style, float thickness, float offset, skb_color_t color);


/**
 * Returns first text writing attribute or default value.
 * The default value is empty, which causes no change.
 * @return first found attribute or default avalue.
 */
skb_attribute_writing_t skb_attributes_get_writing(const skb_attribute_t* attributes, int32_t attributes_count);

/**
 * Returns first font text attribute or default value.
 * The default value is: font size 16.0, SKB_FONT_FAMILY_DEFAULT, SKB_WEIGHT_NORMAL, SKB_STYLE_NORMAL, SKB_STRETCH_NORMAL.
 * @return first found attribute or default value.
 */
skb_attribute_font_t skb_attributes_get_font(const skb_attribute_t* attributes, int32_t attributes_count);

/**
 * Returns first spacing attribute or default value.
 * The default value is: letter spacing 0, word spacing 0.
 * @return first found attribute or default value.
 */
skb_attribute_spacing_t skb_attributes_get_spacing(const skb_attribute_t* attributes, int32_t attributes_count);

/**
 * Returns first line height attribute or default value.
 * The default value is: line spacing multiplier 1.0.
 * @return first found attribute or default value.
 */
skb_attribute_line_height_t skb_attributes_get_line_height(const skb_attribute_t* attributes, int32_t attributes_count);

/**
 * Returns first fill attribute or default value.
 * The default value is opaque black.
 * @return first found attribute or default value.
 */
skb_attribute_fill_t skb_attributes_get_fill(const skb_attribute_t* attributes, int32_t attributes_count);


/** Enum describing flags for skb_layout_params_t. */
enum skb_layout_params_flags_t {
	/** Ignored line breaks from control characters. */
	SKB_LAYOUT_PARAMS_IGNORE_MUST_LINE_BREAKS = 1 << 0,
};

/** Struct describing parameters that apply to the whole text layout. */
typedef struct skb_layout_params_t {
	/** Pointer to font collection to use. */
	skb_font_collection_t* font_collection;
	/** BCP 47 language tag, e.g. fi-FI. */
	const char* lang;
	/** Line break width. If 0.0, line width is unbounded. */
	float line_break_width;
	/** Origin of the text layout. */
	skb_vec2_t origin;
	/** base writing direction. */
	uint8_t base_direction;
	/** Horizontal alignment. */
	uint8_t align;
	/** Baseline alignment. Works similarly as dominant-baseline in CSS. */
	uint8_t baseline;
	/** Layout parameter flags (see skb_layout_params_flags_t). */
	uint8_t flags;
} skb_layout_params_t;

/** Struct describing attributes assigned to a range of text. */
typedef struct skb_text_attributes_span_t {
	/** Range of text the attribues apply to. */
	skb_range_t text_range;
	/** The text attributes. */
	skb_attribute_t* attributes;
	int32_t attributes_count;
} skb_text_attributes_span_t;

/** Struct describing a run of utf-8 text with attributes. */
typedef struct skb_text_run_utf8_t {
	/** Text as utf-8 */
	const char* text;
	/** Length of the text, or -1 if text is null terminated. */
	int32_t text_count;
	/** Pointer to the text attributes. */
	const skb_attribute_t* attribs;
	int32_t attribs_count;
} skb_text_run_utf8_t;

/** Struct describing a run of utf-32 text with attributes. */
typedef struct skb_text_run_utf32_t {
	/** Text as utf-32 */
	const uint32_t* text;
	/** Length of the text, or -1 if text is null terminated. */
	int32_t text_count;
	/** Pointer to the text attributes. */
	const skb_attribute_t* attributes;
	int32_t attributes_count;
} skb_text_run_utf32_t;

/** Struct describing shaped and positioned glyph. */
typedef struct skb_glyph_t {
	/** X offset of the glyph (including layout origin). */
	float offset_x;
	/** Y offset of the glyph (including layout origin). */
	float offset_y;
	/** Typographic advancement to the next glyph. */
	float advance_x;
	/** Original visual index of the glyph. Used internally for word wrapping. */
	int32_t visual_idx;
	/** Range of the text (codepoints) the glyph covers. End exclusive. */
	skb_range_t text_range;
	/** Glyph ID to render. */
	uint16_t gid;
	/** Index of the attribute span. */
	uint16_t span_idx;
	/** Index of the font in font collection. */
	skb_font_handle_t font_handle;
} skb_glyph_t;

/** Struct describing text decoration  */
typedef struct skb_decoration_t {
	/** X offset of the decoration (including layout origin). */
	float offset_x;
	/** Y offset of the decoration (including layout origin). */
	float offset_y;
	/** Length of the decoration. */
	float length;
	/** Offset of the start of the pattern. */
	float pattern_offset;
	/** Thickness of the decoration. */
	float thickness;
	/** Index of the attribute span. */
	uint16_t span_idx;
	/** Index to the decoration attribute within the span. */
	uint16_t attribute_idx;
	/** Range of glyphs the decoration relates to. */
	skb_range_t glyph_range;
} skb_decoration_t;

/** Enum describing flags for skb_text_property_t. */
enum skb_text_prop_flags_t {
	/** Grapheme break after the codepoint. */
	SKB_TEXT_PROP_GRAPHEME_BREAK   = 1 << 0,
	/** Word break after the codepoint. */
	SKB_TEXT_PROP_WORD_BREAK       = 1 << 1,
	/** Must break line after the code point. */
	SKB_TEXT_PROP_MUST_LINE_BREAK  = 1 << 2,
	/** Allow line break after the codepoint. */
	SKB_TEXT_PROP_ALLOW_LINE_BREAK = 1 << 3,
	/** The codepoint is an emoji. */
	SKB_TEXT_PROP_EMOJI            = 1 << 4,
	/** The codepoint is a control character. */
	SKB_TEXT_PROP_CONTROL          = 1 << 5,
	/** The codepoint is a white space character. */
	SKB_TEXT_PROP_WHITESPACE       = 1 << 6,
	/** The codepoint is a punctuation character. */
	SKB_TEXT_PROP_PUNCTUATION      = 1 << 7,
};

/** Struct describing properties if a single codepoint. */
typedef struct skb_text_property_t {
	/** Text property flags (see skb_text_prop_flags_t). */
	uint8_t flags;
	/** Script of the codepoint. */
	uint8_t script;
	/** Text direction. */
	uint8_t direction;
} skb_text_property_t;

/** Struct describing a line of text. */
typedef struct skb_layout_line_t {
	/** Range of glyphs that belong to the line. */
	skb_range_t glyph_range;
	/** Range of text (codepoints) that belong to the line. */
	skb_range_t text_range;
	/** Range of decorations that belong to the line. */
	skb_range_t decorations_range;
	/** Text offset (codepoints) of the start of the last codepoint on the line. */
	int32_t last_grapheme_offset;
	/** Combined ascender of the line. */
	float ascender;
	/** Combined descender of the line. */
	float descender;
	/** Baseline offset from line top. */
	float baseline;
	/** Bounding rectangle of the line. */
	skb_rect2_t bounds;
} skb_layout_line_t;

/** Opaque type for the text layout. Use skb_layout_create*() to create. */
typedef struct skb_layout_t skb_layout_t;

/**
 * Appends the hash of the layout paramgs to the provided hash.
 * @param hash hash to append to.
 * @param params pointer to the paramters to hash.
 * @return combined hash.
 */
uint64_t skb_layout_params_hash_append(uint64_t hash, const skb_layout_params_t* params);

/**
 * Appends the hash of the text attributes to the provided hash.
 * @param hash hash to append to.
 * @param attribs pointer to the attributes to hash.
 * @return combined hash.
 */
uint64_t skb_attributes_hash_append(uint64_t hash, const skb_attribute_t* attribs, int32_t attribs_count);

/**
 * Creates empty layout with specified parameters.
 * @param params paramters to use for the layout.
 * @return newly create empty layout.
 */
skb_layout_t* skb_layout_create(const skb_layout_params_t* params);

/**
 * Creates new layout from the provided parameters, text and text attributes.
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param text text to layout as utf-8.
 * @param text_count length of the text, or -1 is nul terminated.
 * @param attribs attributes to apply for the text.
 * @return newly create layout.
 */
skb_layout_t* skb_layout_create_utf8(
	skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const char* text, int32_t text_count,
	const skb_attribute_t* attribs, int32_t attribs_count);

/**
 * Creates new layout from the provided parameters, text and text attributes.
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param text text to layout as utf-32.
 * @param text_count length of the text, or -1 is nul terminated.
 * @param attribs attributes to apply for the text.
 * @return newly create layout.
 */
skb_layout_t* skb_layout_create_utf32(
	skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const uint32_t* text, int32_t text_count,
	const skb_attribute_t* attribs, int32_t attribs_count);

/**
 * Creates new layout from the provided parameters and text runs.
 * The text runs are combined into one attributes string and laid out as one.
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param runs utf-8 text runs to combine into continuous text.
 * @param runs_count number of runs.
 * @return newly create layout.
 */
skb_layout_t* skb_layout_create_from_runs_utf8(
	skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const skb_text_run_utf8_t* runs, int32_t runs_count);

/**
 * Creates new layout from the provided parameters and text runs.
 * The text runs are combined into one attributes string and laid out as one.
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param runs utf-32 text runs to combine into continuous text.
 * @param runs_count number of runs.
 * @return newly create layout.
 */
skb_layout_t* skb_layout_create_from_runs_utf32(
	skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const skb_text_run_utf32_t* runs, int32_t runs_count);

/**
 * Sets the layout from the provided parameters, text and text attributes.
 * @param layout layout to set up
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param text text to layout as utf-8.
 * @param text_count length of the text, or -1 is nul terminated.
 * @param attribs attributes to apply for the text.
 * @return newly create layout.
 */
void skb_layout_set_utf8(
	skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const char* text, int32_t text_count,
	const skb_attribute_t* attribs, int32_t attribs_count);

/**
 * Sets the layout from the provided parameters, text and text attributes.
 * @param layout layout to set up
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param text text to layout as utf-8.
 * @param text_count length of the text, or -1 is nul terminated.
 * @param attribs attributes to apply for the text.
 * @return newly create layout.
 */
void skb_layout_set_utf32(
	skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const uint32_t* text, int32_t text_count,
	const skb_attribute_t* attribs, int32_t attribs_count);

/**
 * Sets the layout from the provided parameters and text runs.
 * The text runs are combined into one attributes string and laid out as one.
 * @param layout layout to set up
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param runs utf-8 text runs to combine into continuous text.
 * @param runs_count number of runs.
 */
void skb_layout_set_from_runs_utf8(
	skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const skb_text_run_utf8_t* runs, int32_t runs_count);

/**
 * Sets the layout from the provided parameters and text runs.
 * The text runs are combined into one attributes string and laid out as one.
 * @param layout layout to set up
 * @param temp_alloc temp alloc to use during building the layout.
 * @param params paramters to use for the layout.
 * @param runs utf-32 text runs to combine into continuous text.
 * @param runs_count number of runs.
 */
void skb_layout_set_from_runs_utf32(
	skb_layout_t* layout, skb_temp_alloc_t* temp_alloc, const skb_layout_params_t* params,
	const skb_text_run_utf32_t* runs, int32_t runs_count);

/**
 * Empties the specified layout. Keeps the existing allocations.
 * @param layout layout to reset.
 */
void skb_layout_reset(skb_layout_t* layout);

/**
 * Destroys specified layout.
 * @param layout layout to destroy.
 */
void skb_layout_destroy(skb_layout_t* layout);

/**
 * Returns parameters that were used to create th elayout.
 * @param layout layout to use
 * @return const pointer to the parameters.
 */
const skb_layout_params_t* skb_layout_get_params(const skb_layout_t* layout);

/** @return number of codepoints in the layout text. */
int32_t skb_layout_get_text_count(const skb_layout_t* layout);

/** @return const pointer to the codepoints of the text. See skb_layout_get_text_count() to get text length. */
const uint32_t* skb_layout_get_text(const skb_layout_t* layout);

/** @return const pointer to the codepoint properties of the text. See skb_layout_get_text_count() to get text length. */
const skb_text_property_t* skb_layout_get_text_properties(const skb_layout_t* layout);

/** @return number of glyphs in the layout. */
int32_t skb_layout_get_glyphs_count(const skb_layout_t* layout);

/** @return const pointer to the glyphs. See skb_layout_get_glyphs_count() to get number of glyphs. */
const skb_glyph_t* skb_layout_get_glyphs(const skb_layout_t* layout);

int32_t skb_layout_get_decorations_count(const skb_layout_t* layout);
const skb_decoration_t* skb_layout_get_decorations(const skb_layout_t* layout);

/** @return number of lines in the layout. */
int32_t skb_layout_get_lines_count(const skb_layout_t* layout);

/** @return const pointer to the lines. See skb_layout_get_lines_count() to get number of lines. */
const skb_layout_line_t* skb_layout_get_lines(const skb_layout_t* layout);

/** @return number of attribute spans in the layout. */
int32_t skb_layout_get_attribute_spans_count(const skb_layout_t* layout);

/** @return const pointer to the attribute spans. See skb_layout_get_attribute_spans_count() to get number of spans. */
const skb_text_attributes_span_t* skb_layout_get_attribute_spans(const skb_layout_t* layout);

/** @return typographic bunds of the layout. */
skb_rect2_t skb_layout_get_bounds(const skb_layout_t* layout);

/** @return text direction of the layout, if the direction was auto, the direction inferred from the text. */
skb_text_direction_t skb_layout_get_resolved_direction(const skb_layout_t* layout);

/**
 * Get the start of the next grapheme in the layout based on text offset.
 * @param layout layout to use
 * @param offset offset (codepoints) in the text where to start looking.
 * @return offset (codepoints) to the start of the next grapheme.
 */
int32_t skb_layout_next_grapheme_offset(const skb_layout_t* layout, int32_t offset);

/**
 * Get the start of the previous grapheme in the layout based on text offset.
 * @param layout layout to use
 * @param offset offset (codepoints) in the text where to start looking.
 * @return offset (codepoints) to the start of the previous grapheme.
 */
int32_t skb_layout_prev_grapheme_offset(const skb_layout_t* layout, int32_t offset);

/**
 * Get the start of the current grapheme in the layout based on text offset.
 * @param layout layout to use
 * @param offset offset (codepoints) in the text where to start looking.
 * @return offset (codepoints) to the start of the current grapheme.
 */
int32_t skb_layout_align_grapheme_offset(const skb_layout_t* layout, int32_t offset);

//
// Text Selection
//

/** Enum describing the caret's position in relation a codepoint, in logical text order. */
enum skb_caret_affinity_t {
	/** Not specified. Generally translates to SKB_AFFINITY_TRAILING. */
	SKB_AFFINITY_NONE,
	/** The caret is at the trailing edge of the codepoint. */
	SKB_AFFINITY_TRAILING,
	/** The caret is at the leading edge of the codepoint. */
	SKB_AFFINITY_LEADING,
	/** The caret is at the start of the line. This can be different than trailing when line direction and text direction do not match. */
	SKB_AFFINITY_SOL,
	/** The caret is at the end of the line. This can be different than leading when line direction and text direction do not match. */
	SKB_AFFINITY_EOL,
};

/** Struct describing position within the text in a layout. */
typedef struct skb_text_position_t {
	/** Offset (codepoints) within the text. */
	int32_t offset;
	/** Relation to the codepoint. See skb_caret_affinity_t */
	uint8_t affinity;
} skb_text_position_t;

/** Struct describing a selection range of the text in a layout. There's no expectation of the order of start and end positions. */
typedef struct skb_text_selection_t {
	/** Start position of the selection. */
	skb_text_position_t start_pos;
	/** End position of the selection. */
	skb_text_position_t end_pos;
} skb_text_selection_t;

/** Struct describing visual caret location.
 * The caret line can be described as: (x+width, y) - (x, y+height).
 * Where, (x,y) is the top left corner of the rectangle containing the caret.
 */
typedef struct skb_visual_caret_t {
	/** X location of the caret */
	float x;
	/** Y location of the caret */
	float y;
	/** Height of the caret */
	float height;
	/** Width of the caret (slant) */
	float width;
	/** Text direction at caret location. */
	uint8_t direction;
} skb_visual_caret_t;

/**
 * Returns the line number where the text position lies.
 * @param layout layout to use
 * @param pos position within the text.
 * @return zero based line number.
 */
int32_t skb_layout_get_line_index(const skb_layout_t* layout, skb_text_position_t pos);

/**
 * Returns the text offset (codepoint) if specific text position, taking affinity into account.
 * @param layout layout to use
 * @param pos position within the text.
 * @return text offset.
 */
int32_t skb_layout_get_text_offset(const skb_layout_t* layout, skb_text_position_t pos);

/**
 * Returns text direction at the specified text postition.
 * @param layout layout to use
 * @param pos position within the text.
 * @return text direction at the specified text postition.
 */
skb_text_direction_t skb_layout_get_text_direction_at(const skb_layout_t* layout, skb_text_position_t pos);


/** Enum describing intended movement. Caret movement and selection cursor movement have diffent behavior at the end of hte line. */
typedef enum {
	/** Moving the caret. */
	SKB_MOVEMENT_CARET,
	/** Moving selection end. */
	SKB_MOVEMENT_SELECTION,
} skb_movement_type_t;

/**
 * Returns text position under the hit location on the specified line.
 * Start or end of the line is returned if the position is outside the line bounds.
 * @param layout layout to use.
 * @param type type of interaction.
 * @param line_idx index of the line to test.
 * @param hit_x hit X location.
 * @return text position under the specified hit location.
 */
skb_text_position_t skb_layout_hit_test_at_line(const skb_layout_t* layout, skb_movement_type_t type, int32_t line_idx, float hit_x);

/**
 * Returns text position under the hit location.
 * First or last line is tested if the hit location is outside the vertical bounds.
 * Start or end of the line is returned if the hit location is outside the horizontal bounds.
 * @param layout layout to use.
 * @param type type of interaction
 * @param hit_x hit X location
 * @param hit_y hit Y location
 * @return text position under the specified hit location.
 */
skb_text_position_t skb_layout_hit_test(const skb_layout_t* layout, skb_movement_type_t type, float hit_x, float hit_y);

/**
 * Returns visual caret location of the text position at specified line.
 * @param layout layout to use
 * @param line_idx index of the line where the text position is.
 * @param pos text position to use.
 * @return visual caret location.
 */
skb_visual_caret_t skb_layout_get_visual_caret_at_line(const skb_layout_t* layout, int32_t line_idx, skb_text_position_t pos);

/**
 * Returns visual caret location of the text position.
 * @param layout layout to use
 * @param pos text position to use.
 * @return visual caret location.
 */
skb_visual_caret_t skb_layout_get_visual_caret_at(const skb_layout_t* layout, skb_text_position_t pos);

/** @return text position of nearest start of the line, starting from specified text position. */
skb_text_position_t skb_layout_get_line_start_at(const skb_layout_t* layout, skb_text_position_t pos);

/** @return text position of nearest end of the line, starting from specified text position. */
skb_text_position_t skb_layout_get_line_end_at(const skb_layout_t* layout, skb_text_position_t pos);

/** @return text position of nearest start of a word, starting from specified text position. */
skb_text_position_t skb_layout_get_word_start_at(const skb_layout_t* layout, skb_text_position_t pos);

/** @return text position of nearest end of a word, starting from specified text position. */
skb_text_position_t skb_layout_get_word_end_at(const skb_layout_t* layout, skb_text_position_t pos);

/** @return text position of selection start, which is first in text order. */
skb_text_position_t skb_layout_get_selection_ordered_start(const skb_layout_t* layout, skb_text_selection_t selection);

/** @return text position of selection end, which is last in text order. */
skb_text_position_t skb_layout_get_selection_ordered_end(const skb_layout_t* layout, skb_text_selection_t selection);

/** @return ordered range of text (codepoints) representing the selection. End exclusive. */
skb_range_t skb_layout_get_selection_text_offset_range(const skb_layout_t* layout, skb_text_selection_t selection);

/** @return number of codepoints in the selection. */
int32_t skb_layout_get_selection_count(const skb_layout_t* layout, skb_text_selection_t selection);

/**
 * Signature of selection bounds getter callback.
 * @param rect rectangle that has part of the selection.
 * @param context context passed to skb_layout_get_selection_bounds()
 */
typedef void skb_selection_rect_func_t(skb_rect2_t rect, void* context);

/**
 * Returns set of rectangles that represent the selection.
 * Due to bidirectional text the selection in logical order can span across multiple visual rectangles.
 * @param layout layout to use.
 * @param selection selection to get.
 * @param callback callback to call on each rectangle
 * @param context context passed to the callback.
 */
void skb_layout_get_selection_bounds(const skb_layout_t* layout, skb_text_selection_t selection, skb_selection_rect_func_t* callback, void* context);

/**
 * Returns set of rectangles that represent the selection.
 * Due to bidirectional text the selection in logical order can span across multiple visual rectangles.
 * @param layout layout to use.
 * @param offset_y y-offset added to each rectangle.
 * @param selection selection to get.
 * @param callback callback to call on each rectangle
 * @param context context passed to the callback.
 */
void skb_layout_get_selection_bounds_with_offset(const skb_layout_t* layout, float offset_y, skb_text_selection_t selection, skb_selection_rect_func_t* callback, void* context);

//
// Caret iterator
//

/** Struct describing result of caret iterator. */
typedef struct skb_caret_iterator_result_t {
	/** Text position of the caret */
	skb_text_position_t text_position;
	/** Glyph index of the caret. */
	int32_t glyph_idx;
	/** Text direction at the text position. */
	uint8_t direction;
} skb_caret_iterator_result_t;

/** Struct holding state for iterating over all caret locations in a layout. */
typedef struct skb_caret_iterator_t {
	// Internal
	const skb_layout_t* layout;

	float advance;
	float x;

	int32_t glyph_pos;
	int32_t glyph_end;
	uint8_t glyph_direction;

	int32_t grapheme_pos;
	int32_t grapheme_end;

	bool end_of_glyph;
	bool end_of_line;

	int32_t line_first_grapheme_offset;
	int32_t line_last_grapheme_offset;

	skb_caret_iterator_result_t pending_left;
} skb_caret_iterator_t;

/**
 * Make a caret iterator for specific line in the layout.
 * The caret iterator iterates between all grapheme boundaries (also before and after the first and last) from left to right along a line (even inside ligatures).
 * @param layout laout to use
 * @param line_idx index of the line to iterate.
 * @return initialized caret iterator.
 */
skb_caret_iterator_t skb_caret_iterator_make(const skb_layout_t* layout, int32_t line_idx);

/**
 * Advances to the next caret location.
 * @param iter iterator to advance.
 * @param x (out) x location of the caret betweem two graphemes.
 * @param advance (out) distance to the next caret location.
 * @param left text position left of 'x'.
 * @param left_is_rtl true if left text position is right-to-left.
 * @param right text position right of 'x'.
 * @param right_is_rtl true if right text position is right-to-left.
 * @return true as long as the output values are valid.
 */
bool skb_caret_iterator_next(skb_caret_iterator_t* iter, float* x, float* advance, skb_caret_iterator_result_t* left, skb_caret_iterator_result_t* right);

/**
 * Returns four-letter ISO 15924 script tag of the specified script.
 * @param script scrip to covert.
 * @return four letter tag.
 */
uint32_t skb_script_to_iso15924_tag(uint8_t script);

/**
 * Returns script from four-letter ISO 15924 script tag.
 * @param ISO 15924 script tag scrip to covert.
 * @return script.
 */
uint8_t skb_script_from_iso15924_tag(uint32_t script_tag);


//
// Glyph run iterator
//

/** Struct holding the state of the glyph iterator */
typedef struct skb_glyph_run_iterator_t {
	const skb_glyph_t* glyphs;
	int32_t glyphs_count;
	skb_range_t glyph_range;
	int32_t pos;
} skb_glyph_run_iterator_t;

/**
 * Makes new glyph run iterator.
 * Glyph iterator can be used to iterate over a contiguous range of glyphs which have the same font and attributes.
 * Use skb_glyph_run_iterator_next() in a while loop to iterate the ranges.
 * @param glyphs const pointer to the glyphs to iterate over.
 * @param glyphs_count Number of glyphs.
 * @param start Start index of the glyphs to iterate.
 * @param end One past the end index of the glyphs to iterate (half open range).
 * @return initialized iterator.
 */
static inline skb_glyph_run_iterator_t skb_glyph_run_iterator_make(const skb_glyph_t* glyphs, int32_t glyphs_count, int32_t start, int32_t end)
{
	assert(end >= start);
	assert(start >= 0 && start <= glyphs_count);
	assert(end >= 0 && end <= glyphs_count);

	skb_glyph_run_iterator_t iter = {0};
	iter.glyphs = glyphs;
	iter.glyphs_count = glyphs_count;
	iter.glyph_range.start = start;
	iter.glyph_range.end = end;
	iter.pos = start;

	return iter;
}

/**
 * Advances to the next glyph range of same font and attributes.
 * @param iter pointer to iterator to advance.
 * @param range (out) range of the glyphs with same font and attributes.
 * @param font_handle (out) the font of the glyph run.
 * @param span_idx (out) index to the span describing the attributes of the run.
 * @return true if the return values are valid.
 */
static inline bool skb_glyph_run_iterator_next(skb_glyph_run_iterator_t* iter, skb_range_t* range, skb_font_handle_t* font_handle, uint16_t* span_idx)
{
	if (iter->pos == iter->glyph_range.end)
		return false;

	int32_t start_pos = iter->pos;

	// Find continuous range of same font and attribute span.
	skb_font_handle_t cur_font_handle = iter->glyphs[iter->pos].font_handle;
	uint16_t cur_span_idx = iter->glyphs[iter->pos].span_idx;
	iter->pos++;

	while (iter->pos < iter->glyph_range.end) {
		if (iter->glyphs[iter->pos].font_handle != cur_font_handle || iter->glyphs[iter->pos].span_idx != cur_span_idx)
			break;
		iter->pos++;
	}

	range->start = start_pos;
	range->end = iter->pos;
	*font_handle = cur_font_handle;
	*span_idx = cur_span_idx;

	return true;
}


/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_LAYOUT_H
