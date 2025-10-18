// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_FONT_COLLECTION_H
#define SKB_FONT_COLLECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "skb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// harfbuzz forward declarations
typedef struct hb_font_t hb_font_t;

/**
 * @defgroup font_collection Font Collection
 *
 * A Font collection contains number of fonts which can be matched based on font's properties (weight, style, etc).
 * The matching algorithm is based on https://drafts.csswg.org/css-fonts-3/#font-style-matching.
 *
 * Fonts are arranged in families using an identifier. There are some common identifiers in skb_font_family_t.
 * They are merely suggestions, except SKB_FONT_FAMILY_EMOJI which is automatically assigned to runs of emoji glyphs.
 *
 * SKB_FONT_FAMILY_DEFAULT is zero, so it will match if text attributes is zero initialized without specifying font family.
 *
 * Fonts can be added using skb_font_collection_add_font(). Under the hood it uses Harfbuzz's memory mapped
 * file API, so even if fonts are loaded, they should not take much space until used.
 *
 * @{
 */

//
// Fonts
//

/** Enum describing font style. */
typedef enum {
	/** Normal font */
	SKB_STYLE_NORMAL = 0,
	/** Italic font */
	SKB_STYLE_ITALIC,
	/** Oblique font */
	SKB_STYLE_OBLIQUE,
} skb_style_t;

/** Enum describing some generic font families. */
typedef enum {
	/** Default family, which is used if no family is specified. */
	SKB_FONT_FAMILY_DEFAULT,
	/** Font family automatically assigned for emojis */
	SKB_FONT_FAMILY_EMOJI,
	/** Generic sans-serif family. */
	SKB_FONT_FAMILY_SANS_SERIF,
	/** Generic serif family */
	SKB_FONT_FAMILY_SERIF,
	/** Generic monospace family. */
	SKB_FONT_FAMILY_MONOSPACE,
	/** Generic math family. */
	SKB_FONT_FAMILY_MATH,
} skb_font_family_t;

/** Enum describing font stretch. */
typedef enum {
	/** Normal (1.0), (Default, when zero initialized) */
	SKB_STRETCH_NORMAL = 0,
	/** Ultra condensed (0.5) */
	SKB_STRETCH_ULTRA_CONDENSED,
	/** Extra condensed (0.625) */
	SKB_STRETCH_EXTRA_CONDENSED,
	/** Condensed (0.75) */
	SKB_STRETCH_CONDENSED,
	/** Semi condensed (0.875) */
	SKB_STRETCH_SEMI_CONDENSED,
	/** Semi expanded (1.125) */
	SKB_STRETCH_SEMI_EXPANDED,
	/** Expanded (1.25) */
	SKB_STRETCH_EXPANDED,
	/** Extra expanded (1.5) */
	SKB_STRETCH_EXTRA_EXPANDED,
	/** Ultra expanded (2.0) */
	SKB_STRETCH_ULTRA_EXPANDED,
} skb_stretch_t;

/** Enum describing for weight. */
typedef enum {
	/** Normal (400), (Default, when zero initialized) */
	SKB_WEIGHT_NORMAL,
	/** Thin (100) */
	SKB_WEIGHT_THIN,
	/** Extra light (200) */
	SKB_WEIGHT_EXTRALIGHT,
	/** Ultra light (200) */
	SKB_WEIGHT_ULTRALIGHT,
	/** Light (300) */
	SKB_WEIGHT_LIGHT,
	/** Regular (400) */
	SKB_WEIGHT_REGULAR,
	/** Medium (500) */
	SKB_WEIGHT_MEDIUM,
	/** Demibold (600) */
	SKB_WEIGHT_DEMIBOLD,
	/** Semibold (600) */
	SKB_WEIGHT_SEMIBOLD,
	/** Bold (700) */
	SKB_WEIGHT_BOLD,
	/** Extra bold (800) */
	SKB_WEIGHT_EXTRABOLD,
	/** Ultra bold (800) */
	SKB_WEIGHT_ULTRABOLD,
	/** Black (000) */
	SKB_WEIGHT_BLACK,
	/** Heavy (900) */
	SKB_WEIGHT_HEAVY,
	/** Extra black (950) */
	SKB_WEIGHT_EXTRABLACK,
	/** Ultra black (950) */
	SKB_WEIGHT_ULTRABLACK,
} skb_weight_t;

/** Enum describing specific font baseline. */
typedef enum {
	/** Alphabetic, or roman baseline. */
	SKB_BASELINE_ALPHABETIC,
	/** Ideographic baseline. */
	SKB_BASELINE_IDEOGRAPHIC,
	/** Ideographic central baseline */
	SKB_BASELINE_CENTRAL,
	/** Hanging baseline. */
	SKB_BASELINE_HANGING,
	/** Mathematic baseline. */
	SKB_BASELINE_MATHEMATICAL,
	/** Baseline positioned at the middle of x-height. */
	SKB_BASELINE_MIDDLE,
	/** Text top baseline. */
	SKB_BASELINE_TEXT_TOP,
	/** Text bottom baseline. */
	SKB_BASELINE_TEXT_BOTTOM,
	SKB_BASELINE_MAX,
} skb_baseline_t;

/** Font metrics */
typedef struct skb_font_metrics_t {
	/** Space above baseline. */
	float ascender;
	/** Space below baseline. */
	float descender;
	/** Extra space between lines. */
	float line_gap;
	/** Height of upper case letter. */
	float cap_height;
	/** Height of lower case x. */
	float x_height;
	/** Underline offset. */
	float underline_offset;
	/** Underline height. */
	float underline_size;
	/** Strikeout offset. */
	float strikeout_offset;
	/** Strikeout height. */
	float strikeout_size;
} skb_font_metrics_t;

/** Struct describing all the baselines of a font. Based on baseline tables of CSS: https://www.w3.org/TR/css-inline-3/#baseline-table */
typedef struct skb_baseline_set_t {
	union {
		// Baselines that can be index using skb_baseline_t.
		float baselines[SKB_BASELINE_MAX];
		struct {
			/** Alphabetic baseline, used in Latin, Cyrillic, Greek, and many other scripts. Corresponds to the bottom of most lower case glyphs. Often represented as zero in font design coordinate systems. */
			float alphabetic;
			/** Ideographic baseline, used in CJK (Han/Hangul/Kana), corresponds to the bottom design edge of the glyph. */
			float ideographic;
			/** Central baseline, middle of cap-height and alphabetic baseline. Generally the nicest choice to center align items. */
			float central;
			/** Hanging baseline, used in Tibetan and similar unicameral scripts. Corresponds to top edge the glyphs seem to "hang" from */
			float hanging;
			/** Mathematical baseline, corresponds to center baseline of mathematical glyphs. */
			float mathematical;
			/** Middle baseline, middle of x-height and alphabetic baseline. */
			float middle;
			/** Text bottom baseline, equals to descender. */
			float text_bottom;
			/** Text top baseline, equals to ascender. */
			float text_top;
		};
	};
	/** Script the baseline set was build for. */
	uint8_t script;
	/** Text direction the baseline set was build for. */
	uint8_t direction;
} skb_baseline_set_t;

/** Caret metrics */
typedef struct skb_caret_metrics_t {
	/** Caret X offset from current position. */
	float offset;
	/** How much X changes per Y. */
	float slope;
} skb_caret_metrics_t;

/** Handle to a font. */
typedef uint32_t skb_font_handle_t;

/** Opaque type for the font collection. Use skb_font_collection_create() to create. */
typedef struct skb_font_collection_t skb_font_collection_t;

/** Opaque type for the font. Use skb_font_collection_add*() to create. */
typedef struct skb_font_t skb_font_t;

/**
 * Signature font fallback function callback.
 * The font fallback function is called when font selection fails. Use skb_font_collection_set_on_font_fallback() to set the callback.
 * @param font_collection font collection to use.
 * @param lang language of the failed font selection.
 * @param script script of the failed font selection, use skb_script_to_iso15924_tag() to get ISO-15924 tag of the script.
 * @param font_family font family of the failed font selection
 * @param context context pointer, which was passed to skb_font_collection_set_on_font_fallback() when the callback was set up.
 * @return true if we should retry font selection after the callback.
 */
typedef bool skb_font_fallback_func_t(skb_font_collection_t* font_collection, const char* lang, uint8_t script, uint8_t font_family, void* context);

/**
 * Creates a new font collection.
 * @return created font collection.
 */
skb_font_collection_t* skb_font_collection_create(void);

/**
 * Destroys font collection.
 * Note: layouts store pointers to the font collection. The font collection should be destroyed only after all layouts are destroyed.
 * @param font_collection font collection to destroy.
 */
void skb_font_collection_destroy(skb_font_collection_t* font_collection);

/**
 * Sets callback function that is called when we cannot match font. The callback allows the user to either collect missing fonts, or to load them.
 * @param font_collection font collection to use
 * @param fallback_func function to call when font selection fails.
 * @param context Pointer passed to the callback function.
 */
void skb_font_collection_set_on_font_fallback(skb_font_collection_t* font_collection, skb_font_fallback_func_t* fallback_func, void* context);

/**
 * Adds OTF or TTF font to the collection from memory.
 * The font collection retains a reference to the data until skb_font_collection_remove_font() or skb_font_collection_destroy is called.
 * The destroy_func() is called with the provided context pointer when the data is destroyed.
 * @param font_collection font collection to use.
 * @param name used to uniquely identify the font.
 * @param font_family font family identifier.
 * @param font_data pointer to the font data in memory.
 * @param font_data_length length of the font data in bytes.
 * @param context pointer passed to the destroy function, when the font data is no longer used. null can be passed in.
 * @param destroy_func function to call, when the font data is longer used. null can be passed in if no callback is desired.
 * @return pointer to the added font, on NULL if failed to load the font.
 */
skb_font_handle_t skb_font_collection_add_font_from_data(
	skb_font_collection_t* font_collection,
	const char* name,
	uint8_t font_family,
	const void* font_data,
	size_t font_data_length,
	void* context,
	skb_destroy_func_t* destroy_func
);

#if !defined(SKB_NO_OPEN)
/**
 * Adds OTF or TTF font to the collection.
 * @param font_collection font collection to use.
 * @param file_name file name of the font to add.
 * @param font_family font family identifier.
 * @return pointer to the added font, on NULL if failed to load the font.
 */
skb_font_handle_t skb_font_collection_add_font(skb_font_collection_t* font_collection, const char* file_name, uint8_t font_family);
#endif

/**
 * Adds OTF or TTF font to the collection.
 * @param font_collection font collection to use.
 * @param hb_font a harfbuzz font instance. will be incref'd
 * @param name used to uniquely identify the font.
 * @param font_family font family identifier.
 * @return pointer to the added font, on NULL if failed to load the font.
 */
skb_font_handle_t skb_font_collection_add_hb_font(skb_font_collection_t* font_collection, hb_font_t* hb_font, const char* name, uint8_t font_family);

/**
 * Removes font from the collection.
 * @param font_collection font collection to change.
 * @param font_handle handle to the font to remove.
 * @return true if the remove succeeded.
 */
bool skb_font_collection_remove_font(skb_font_collection_t* font_collection, skb_font_handle_t font_handle);

/**
 * Returns fonts matching specific font properties.
 * Script and font family are hard constraints, for the rest, we do best effort to find something compatible.
 * Script is ignored for emoji font family.
 * The matching algorithm is based on: https://drafts.csswg.org/css-fonts-3/#font-style-matching
 * @param font_collection font collection to use.
 * @param lang the languages of the requested text.
 * @param script script to match.
 * @param font_family font family to match.
 * @param weight weight to match.
 * @param style style to match.
 * @param stretch stretch to match.
 * @param results pointer to results array.
 * @param results_cap results array capacity.
 * @return number of fonts matching the parameters.
 */
int32_t skb_font_collection_match_fonts(
	skb_font_collection_t* font_collection,
	const char* lang, uint8_t script, uint8_t font_family,
	skb_weight_t weight, skb_style_t style, skb_stretch_t stretch,
	skb_font_handle_t* results, int32_t results_cap);

/**
 * Returns default font for specified font family.
 * The default font is the first added font.
 * @param font_collection font collection to change.
 * @param font_family font family to query.
 * @return handle to the default font.
 */
skb_font_handle_t skb_font_collection_get_default_font(skb_font_collection_t* font_collection, uint8_t font_family);

/**
 * Returns font from the collection. The font pointer should not be stored for longer duration. Use skb_font_handle_t instead.
 * @param font_collection font collection to use.
 * @param font_handle handle to the font to get from the collection.
 * @return pointer to the specified font, or NULL if not found.
 */
skb_font_t* skb_font_collection_get_font(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle);

/**
 * Returns the id of the fonc collection, each font collection has unique index.
 * @param font_collection font collection to use.
 * @return
 */
uint32_t skb_font_collection_get_id(const skb_font_collection_t* font_collection);

/**
 * Returns the bounding rect of the specified glyph.
 * @param font_collection font collection to use.
 * @param font_handle font to use.
 * @param glyph_id id of the glyph to query.
 * @param font_size size of the font.
 * @return rectangle descriging the bounding rect of the glyph.
 */
skb_rect2_t skb_font_get_glyph_bounds(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle, uint32_t glyph_id, float font_size);

/**
 * Returns fotn metrics.
 * @param font_collection font collection to use.
 * @param font_handle font to use.
 * @return font metrics.
 */
skb_font_metrics_t skb_font_get_metrics(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle);

/**
 * Returns font caret metrics
 * @param font_collection font collection to use.
 * @param font_handle font to use.
 * @return caret metrics.
 */
skb_caret_metrics_t skb_font_get_caret_metrics(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle);

/**
 * Returns Harfbuzz representation of the font.
 * @param font_collection font collection to use.
 * @param font_handle font to use.
 * @return pointer to the Harfbuzz representation.
 */
hb_font_t* skb_font_get_hb_font(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle);

/**
 * Returns specific baseline of the specified font.
 * @param font_collection font collection to use.
 * @param font_handle font to use.
 * @param baseline which baseline to query.
 * @param direction direction of the text.
 * @param script script of the use cases.
 * @param font_size size of the font in use.
 * @return vertical location of the baseline.
 */
float skb_font_get_baseline(const skb_font_collection_t* font_collection, skb_font_handle_t font_handle, skb_baseline_t baseline, skb_text_direction_t direction, uint8_t script, float font_size);

/**
 * Returns the baseline set of the specified font and font_size.
 * @param font_collection font collection to use.
 * @param font_handle font to use.
 * @param direction direction of the text.
 * @param script script of the use cases.
 * @param font_size size of the font in use.
 * @return
 */
skb_baseline_set_t skb_font_get_baseline_set(const skb_font_collection_t* font_collection, const skb_font_handle_t font_handle, skb_text_direction_t direction, uint8_t script, float font_size);

/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_FONT_COLLECTION_H
