// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_RASTERIZER_H
#define SKB_RASTERIZER_H

#include <stdint.h>
#include <stdbool.h>
#include "skb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct skb_font_t skb_font_t;
typedef struct skb_icon_t skb_icon_t;

/**
 * @defgroup rasterizer Rasterizer
 *
 * The rasterizer is used to rasterize glyphs and icons. It holds some internal state that is needed for rasterization.
 * It's not thread safe, each thread should hold their own rasterizer and temp allocator.
 *
 * The rasterizer allows icons and glyphs to be rasterizer with a signed distance field (SDF) or mask as alpha channel.
 * The SDF allows the shapes to be rasterized at different sizes while maintaining scrisp outline.
 * Color images can also be rasterized with SDF alpha channel, in which case the colors inside the image are interpolated, but the outline can be crisp under scaling.
 *
 * The following code is used to convert the floating point distance field to 8-bit alpha channel:
 * ```
 *  uint8_t alpha = clamp(on_edge_value + distance * pixel_dist_scale, 0, 255);
 * ```
 * The _on_edge_value_ defines the location of SDF zero in 8-bit alpha, and _pixel_dist_scale_ defines the resolution. It allows to tune how much of the SDF range is inside or outside of the image.
 * Smaller values of _pixel_dist_scale_ will cause jagginess when the SDF image is scaled, and larger values will reduce the range of the SDF (e.g. when used for effects).
 * See skb_rasterizer_config_t.
 *
 * @{
 */

/** Opaque type for the rasterizer. Use skb_rasterizer_create() to create. */
typedef struct skb_rasterizer_t skb_rasterizer_t;

/** Rasterizer configuration. */
typedef struct skb_rasterizer_config_t {
	/** Defines the zero of the SDF when converted to alpha [0..255]. Default: 128 */
	uint8_t on_edge_value;
	/** Defines the scale of one SDF pixel when converted to alpha [0...255]. Default: 32.0f */
	float pixel_dist_scale;
} skb_rasterizer_config_t;

/** Enum describing how the alpha channel should be created. */
typedef enum {
	SKB_RASTERIZE_ALPHA_MASK, /**< Rasterize alpha channel as mask. */
	SKB_RASTERIZE_ALPHA_SDF, /**< Rasterize alpha channel as sign distance field. */
} skb_rasterize_alpha_mode_t;

/**
 * Creates a rasterizer.
 * @param config pointer to rasterizer configuration.
 * @return pointer to the created rasterizer.
 */
skb_rasterizer_t* skb_rasterizer_create(skb_rasterizer_config_t* config);

/**
 * Returns default values for the rasterizer config. Can be used if you only want to modify a specific value.
 * @return default config.
 */
skb_rasterizer_config_t skb_rasterizer_get_default_config(void);

/**
 * Returns the config the rasterizer was initialized with.
 * @param rasterizer pointer to the rasterizer
 * @return config for the specified rasterizer.
 */
skb_rasterizer_config_t skb_rasterizer_get_config(const skb_rasterizer_t* rasterizer);

/**
 * Destroys a rasterizer previously created using skb_rasterizer_create().
 * @param rasterizer pointer to the rasterizer.
 */
void skb_rasterizer_destroy(skb_rasterizer_t* rasterizer);

/**
 * Calculates the dimensions required to rasterize a specific glyph at spcified size.
 * The width and height of the returned rectangle defines the image size, and origin defines the offset the glyph should be rasterized at.
 * @param glyph_id glyph id to rasterize
 * @param font font where to get the glyph data
 * @param font_size font size
 * @param padding padding to leave around the glyph.
 * @return rect describing size and offset required to rasterize the glyph.
 */
skb_rect2i_t skb_rasterizer_get_glyph_dimensions(uint32_t glyph_id, const skb_font_t* font, float font_size, int32_t padding);

/**
 * Rasterizes a glyph as alpha mask.
 * The offset and image size can be obtained using skb_rasterizer_get_glyph_dimensions().
 * @param rasterizer pointer to rasterizer.
 * @param temp_alloc pointer to temp alloc used during the rasterization.
 * @param glyph_id glyph id to rasterize.
 * @param font font where to get the glyph data.
 * @param font_size font size.
 * @param alpha_mode alpha mode, defines if the alpha channel of the result is SDF or alpha mask.
 * @param offset_x offset x where to rasterize the glyph.
 * @param offset_y offset y where to rasterize the glyph.
 * @param target target image to rasterize to. The image must be 1 byte-per-pixel.
 * @return true of the rasterization succeeded.
 */
bool skb_rasterizer_draw_alpha_glyph(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	uint32_t glyph_id, const skb_font_t* font, float font_size, skb_rasterize_alpha_mode_t alpha_mode,
	float offset_x, float offset_y, skb_image_t* target);

/**
 * Rasterizes a glyph as RGBA.
 * The offset and image size can be obtained using skb_rasterizer_get_glyph_dimensions().
 * @param rasterizer pointer to rasterizer.
 * @param temp_alloc pointer to temp alloc used during the rasterization.
 * @param glyph_id glyph id to rasterize.
 * @param font font.
 * @param font_size font size .
 * @param alpha_mode alpha mode, defines if the alpha channel of the result is SDF or alpha mask.
 * @param offset_x offset x where to rasterize the glyph.
 * @param offset_y offset y where to rasterize the glyph.
 * @param target target image to rasterize to. The image must be 4 bytes-per-pixel.
 * @return true of the rasterization succeeded.
 */
bool skb_rasterizer_draw_color_glyph(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	uint32_t glyph_id, const skb_font_t* font, float font_size, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target);

/**
 * Calculates the dimensions required to rasterize a specific icon at spcified size.
 * The width and height of the returned rectangle defines the image size, and origin defines the offset the icon should be rasterized at.
 * @param icon icon to rasterize.
 * @param icon_scale icon scale (see skb_rasterizer_calc_proportional_icon_scale()).
 * @param padding padding to leave around the icon.
 * @return rect describing size and offset required to rasterize the icon.
 */
skb_rect2i_t skb_rasterizer_get_icon_dimensions(const skb_icon_t* icon, skb_vec2_t icon_scale, int32_t padding);

/**
 * Rasterizes an icon as alpha mask.
 * The offset and image size can be obtained using skb_rasterizer_get_icon_dimensions().
 * @param rasterizer pointer to rasterizer.
 * @param temp_alloc pointer to temp alloc used during the rasterization.
 * @param icon icon to rasterize.
 * @param icon_scale scale of the icon.
 * @param alpha_mode alpha mode, defines if the alpha channel of the result is SDF or alpha mask.
 * @param offset_x offset x where to rasterize the icon.
 * @param offset_y offset y where to rasterize the icon.
 * @param target target image to draw to. The image must be 4 bytes-per-pixel.
 * @return true of the rasterization succeeded.
 */
bool skb_rasterizer_draw_alpha_icon(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	const skb_icon_t* icon, skb_vec2_t icon_scale, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target);

/**
 * Rasterizes an icon as RGBA.
 * The offset and image size can be obtained using skb_rasterizer_get_icon_dimensions().
 * @param rasterizer pointer to rasterizer.
 * @param temp_alloc pointer to temp alloc used during the rasterization.
 * @param icon icon to rasterize.
 * @param icon_scale scale of the icon.
 * @param alpha_mode alpha mode, defines if the alpha channel of the result is SDF or alpha mask.
 * @param offset_x offset x where to rasterize the icon.
 * @param offset_y offset y where to rasterize the icon.
 * @param target target image to draw to. The image must be 4 bytes-per-pixel.
 * @return true of the rasterization succeeded.
 */
bool skb_rasterizer_draw_color_icon(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	const skb_icon_t* icon, skb_vec2_t icon_scale, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target);

/**
 * Calculates the size of the decoration pattern at specified thickness.
 * The thickness of the line is consistent on all patterns, but some patterns like wavy, can be higher due to the geometry.
 * Note: The width the pattern is clamped to an integer size, so that it is possible to create repeating texture out of the pattern.
 * @param style style of the decoration.
 * @param thickness thickness of the decoration line.
 * @return size of the pattern.
 */
skb_vec2_t skb_rasterizer_get_decoration_pattern_size(skb_decoration_style_t style, float thickness);

/**
 * Calculates the dimensions of a canvas needed to rasterize specified pattern.
 * @param style style of the decoration.
 * @param thickness thickness of the decoration line.
 * @param padding vertical padding to leave around the pattern. Note: horizontal padding is always 1, so that the pattern can be repeated horizontally.
 * @return rect describing size and offset required to rasterize the icon.
 */
skb_rect2i_t skb_rasterizer_get_decoration_pattern_dimensions(skb_decoration_style_t style, float thickness, int32_t padding);

/**
 * Rasterizes specified repeatable decoration pattern as alpha image.
 * The offset and image size can be obtained using skb_rasterizer_get_pattern_dimensions().
 * @param rasterizer pointer to rasterizer.
 * @param temp_alloc pointer to temp alloc used during the rasterization.
 * @param style style of the decoration.
 * @param thickness thickness of the decoration line.
 * @param alpha_mode alpha mode, defines if the alpha channel of the result is SDF or alpha mask.
 * @param offset_x offset x where to rasterize the pattern.
 * @param offset_y offset y where to rasterize the pattern.
 * @param target target image to draw to. The image must be 1 bytes-per-pixel.
 * @return true of the rasterization succeeded.
 */
bool skb_rasterizer_draw_decoration_pattern(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	skb_decoration_style_t style, float thickness, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target);

/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_RASTERIZER_H
