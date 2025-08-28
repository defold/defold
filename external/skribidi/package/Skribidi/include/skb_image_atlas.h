// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_IMAGE_ATLAS_H
#define SKB_IMAGE_ATLAS_H

#include <stdint.h>
#include <stdbool.h>
#include "skb_common.h"
#include "skb_font_collection.h"
#include "skb_icon_collection.h"
#include "skb_rasterizer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct skb_font_t skb_font_t;
typedef struct skb_font_collection_t skb_font_collection_t;
typedef struct skb_icon_t skb_icon_t;
typedef struct skb_layout_t skb_layout_t;

/**
 * @defgroup image_atlas Image Atlas
 *
 * The image atlas is used to manage the rasterized images of different sized glyphs, icons, and patterns.
 *
 * The atlas supports multiple textures. Alpha and color items both are laid out in different textures,
 * and a new texture is created if we run out of space in existing ones.
 *
 * The atlas is used in two phases:
 *
 * 1) During frame:
 *  - request glyphs, icons or patterns, get a quad representing the item in the atlas
 *
 * 2) At the end of the frame:
 *	- Render missing requested items
 *	- Update changed atlas textures to the GPU
 *
 * During the frame, the atlas tracks which glyphs (or icons or patterns) are used, packs the item into one of the textures,
 * and returns a quad describing the dimension of a rectangle to draw, and what portion of a texture to use.
 * The data created during this phase is guaranteed to be valid until the end of the frame, even if the atlas dimensions might change.
 * The image coordinates are in pixels, as the actual atlas image might get resized during the frame.
 *
 * You can register to get notified when a new texture is created. In that callback, you should do just enough work in to be able
 * to handle the new texture index returned with the quad mid-frame.
 *
 * At the end of the frame we have a list of glyphs, icons or patterns that need to be rasterized at specific sizes.
 * Once rasterized, we can iterate over the textures to see which portions of the textures need to be updated to the GPU.
 *
 * @{
 */

/** Opaque type for the image atlas. Use skb_image_atlas_create() to create. */
typedef struct skb_image_atlas_t skb_image_atlas_t;

/** Enum describing flags for skb_render_quad_t. */
enum skb_quad_flags_t {
	/** The quad uses color image. */
	SKB_QUAD_IS_COLOR = 1 << 0,
	/** The quad uses SDF. */
	SKB_QUAD_IS_SDF   = 1 << 1,
};

/** Quad representing a glyph or icon. */
typedef struct skb_quad_t {
	/** Screen geometry of the quad to render */
	skb_rect2_t geom;
	/** Portion of the texture to map to the render bounds. */
	skb_rect2_t texture;
	/** Scaling factor between bounds and image bounds. Can be used to adjust the width of the anti-aliasing gradient. */
	float scale;
	/** Index of the atlas texture to draw. */
	uint8_t texture_idx;
	/** Render quad flags (see skb_quad_flags_t). */
	uint8_t flags;
} skb_quad_t;

/** Quad representing a repeating pattern. */
typedef struct skb_pattern_quad_t {
	/** Screen geometry of the quad to render */
	skb_rect2_t geom;
	/** Describes how the geometry maps to the texture bounds, in normalized coordinates.
	 * Rect (0,0,1,1), will map the image to geom 1:1, rect (-0.25,0,2,1) will repeat the image twice horizontally, starting at 1/4 of the image.  */
	skb_rect2_t pattern;
	/** Portion of the texture to render. */
	skb_rect2_t texture;
	/** Scaling factor between geom and texture bounds. Can be used to adjust the width of the anti-aliasing gradient. */
	float scale;
	/** Index of the atlas texture to draw. */
	uint8_t texture_idx;
	/** Render quad flags (see skb_quad_flags_t). */
	uint8_t flags;
} skb_pattern_quad_t;

/**
 * Signature of the image create callback.
 * @param atlas pointer to the image atlas.
 * @param texture_idx index of the new textre in the atlas.
 * @param context context pointer provided for the image atlas when setting up the callback.
 */
typedef void skb_create_texture_func_t(skb_image_atlas_t* atlas, uint8_t texture_idx, void* context);

/**
 * Configuration for rendering specific image type.
 * The rounding and clamping is used to control how many size variations of the images are rendered.
 * SDF images are much more reusable, and may have more coarse rounding.
 * The requested size is calculated from the paramters by first applying view_scale, then rounding, and finally clamping, based on the settings.
 * The requested size is used to lookup existing size.
 * The padding is applied when calculating the final image size for rasterization.
 */
typedef struct skb_image_item_config_t {
	/** The size is rounded up to the next multiple of rounding. */
	float rounding;
	/** Minimum size of a requested image. */
	float min_size;
	/** Maximum size of a requested image. */
	float max_size;
	/** How much padding to add around the image. */
	int32_t padding;
} skb_image_item_config_t;

/** Enum describing flags for skb_image_cache_config_t. */
enum skb_image_atlas_config_flags_t {
	/** The space in atlas for removed items is cleared. This makes it easier to see which parts of the atlas are unused. */
	SKB_IMAGE_ATLAS_DEBUG_CLEAR_REMOVED = 1 << 0,
};

/**
 * Image atlas configuration.
 * Note: Tall atlas performs much better than wide, as it can support more size variations.
 */
typedef struct skb_image_atlas_config_t {
	/** Initial width of a newly create atlas. Default: 1024 */
	int32_t init_width;
	/** Initial height of a newly create atlas: Default 1024 */
	int32_t init_height;
	/** Increment of how much atlas is grown when running out of space. Default 512. */
	int32_t expand_size;
	/** Maximum atlas width. Default 1024. */
	int32_t max_width;
	/** Maximum atlas height. Default 4096. */
	int32_t max_height;
	/** The height of an item added to the atlas is rounded up to a multiple of this value Allows better resue of the atlas rows. Default: 8. */
	int32_t item_height_rounding;
	/** How much bigger or smaller and item can be so it may get added to too big or too small row in the atlas. E.g. if row size is 20 and fit factor is 0.25, then items from 15 to 25 are considered to be added to the row. Default: 0.25.*/
	float fit_max_factor;
	/** Defines after which duration inactive items are removed from the cache. Each call to skb_image_atlas_compact() bumps the counter. Default: 0.25. */
	int32_t evict_inactive_duration;
	/** Render cache config flags (see skb_image_atlas_config_flags_t). */
	uint8_t flags;
	/** Image config for SDF glyphs */
	skb_image_item_config_t glyph_sdf;
	/** Image config for alpha glyphs */
	skb_image_item_config_t glyph_alpha;
	/** Image config for SDF icons */
	skb_image_item_config_t icon_sdf;
	/** Image config for alpha icons */
	skb_image_item_config_t icon_alpha;
	/** Image config for SDF patterns (e.g decoration patterns) */
	skb_image_item_config_t pattern_sdf;
	/** Image config for alpha patterns (e.g decoration patterns) */
	skb_image_item_config_t pattern_alpha;
} skb_image_atlas_config_t;

/**
 * Creates a new image atlas with specified config.
 * @param config configuration to use for the new image atlas.
 * @return pointer to the created image atlas.
 */
skb_image_atlas_t* skb_image_atlas_create(const skb_image_atlas_config_t* config);

/**
 * Destroys a image atlas created with skb_image_atlas_create().
 * @param atlas pointer to the atlas to destroy.
 */
void skb_image_atlas_destroy(skb_image_atlas_t* atlas);

/**
 * Returns default values for image atlas config. Useful if you only want to change some specific values.
 * @return image atlas config default values.
 */
skb_image_atlas_config_t skb_image_atlas_get_default_config(void);

/**
 * Returns the config used to create the image atlas.
 * @param atlas atlas to use.
 * @return config for the specified image atlas.
 */
skb_image_atlas_config_t skb_image_atlas_get_config(skb_image_atlas_t* atlas);

/**
 * Sets the texture creation callback of the image atlas.
 * @param atlas atlas to use.
 * @param create_texture_callback pointer to the callback function.
 * @param context pointer passed to the callback function each time it is called.
 */
void skb_image_atlas_set_create_texture_callback(skb_image_atlas_t* atlas, skb_create_texture_func_t* create_texture_callback, void* context);

/** @return number of textures in the atlas. */
int32_t skb_image_atlas_get_texture_count(skb_image_atlas_t* atlas);

/**
 * Returns texture at specified index. See skb_image_atlas_get_texture_count() to get number of textures.
 * @param atlas atlas to use.
 * @param texture_idx index of the texture to get.
 * @return pointer to the spcified image.
 */
const skb_image_t* skb_image_atlas_get_texture(skb_image_atlas_t* atlas, int32_t texture_idx);

/**
 * Returns the bounding rect of the modified portion of the specified texture. See skb_image_atlas_get_textures_count() to get number of textures.
 * @param atlas atlas to use.
 * @param texture_idx index of the image to query.
 * @return bounding rect of the modified portion of the image.
 */
skb_rect2i_t skb_image_atlas_get_texture_dirty_bounds(skb_image_atlas_t* atlas, int32_t texture_idx);

/**
 * Returns the bounding rect of the modified portion of the specified texture, and resets the bounding rectangle.
 * See skb_image_atlas_get_texture_count() to get number of textures.
 * @param atlas atlas to use.
 * @param texture_idx index of the image to query.
 * @return bounding rect of the modified portion of the texture.
 */
skb_rect2i_t skb_image_atlas_get_and_reset_texture_dirty_bounds(skb_image_atlas_t* atlas, int32_t texture_idx);

/**
 * Sets user data for a specified texture.
 * See skb_image_atlas_get_texture_count() to get number of textures.
 * @param atlas atlas to use.
 * @param texture_idx index of the image.
 * @param user_data user date to store.
 */
void skb_image_atlas_set_texture_user_data(skb_image_atlas_t* atlas, int32_t texture_idx, uintptr_t user_data);

/**
 * Gets user data set for specified image.
 * See skb_image_atlas_set_texture_user_data() to set the user data.
 * @param image_atlas atlas to use.
 * @param texture_idx index of the texture.
 * @return user data.
 */
uintptr_t skb_image_atlas_get_texture_user_data(skb_image_atlas_t* image_atlas, int32_t texture_idx);

/**
 * Signature of rectangle iterator functions.
 * @param x x location of the rectangle in the image.
 * @param y y location of the rectangle in the image.
 * @param width with of the rectangle.
 * @param height height of the rectangle.
 * @param context context that was passed to the iterator function.
 */
typedef void skb_debug_rect_iterator_func_t(int32_t x, int32_t y, int32_t width, int32_t height, void* context);

/**
 * Iterates all the free space in specified image. Used for debugging.
 * @param atlas atlas to use.
 * @param texture_idx index of the texture.
 * @param callback callback that is called for each free space rectangle in the atlas.
 * @param context contest pointer passed to the callback.
 */
void skb_image_atlas_debug_iterate_free_rects(skb_image_atlas_t* atlas, int32_t texture_idx, skb_debug_rect_iterator_func_t* callback, void* context);

/**
 * Iterates over all the used rectangles in a specific image. Used for debugging.
 * @param atlas atlas to use.
 * @param texture_idx index of the texture.
 * @param callback callback that is called for each used rectangle in the atlas.
 * @param context contest pointer passed to the callback.
 */
void skb_image_atlas_debug_iterate_used_rects(skb_image_atlas_t* atlas, int32_t texture_idx, skb_debug_rect_iterator_func_t* callback, void* context);

/**
 * Returns previous non-empty dirty bounds of the specified texture. Can be used to visualize the last update region.
 * @param atlas atlas to use.
 * @param texture_idx index of the texture.
 * @return previous dirty rectangle.
 */
skb_rect2i_t skb_image_atlas_debug_get_texture_prev_dirty_bounds(skb_image_atlas_t* atlas, int32_t texture_idx);


/**
 * Get a quad representing the geometry and texture portion of the specified glyph.
 *
 * The pixel scale is used to control the ratio between glyph geometry size and image size.
 * For example, if font_size 12 is requested, and pixel_scale is 2, then the geometry of the quad is based on 12 units, but the requested image will be twice the size.
 * This is useful for cases where geometry will later go through a separate transformation process, and we want to match the pixel density.
 *
 * The function will return an existing glyph or request a new glyph to be rendered if one does not exist.
 *
 * @param atlas atlas to use
 * @param x position x to render the glyph at.
 * @param y position y to render the glyph at.
 * @param pixel_scale the size of a pixel compared to the geometry.
 * @param font_collection font collection to use.
 * @param font_handle handle to the font in the font collection.
 * @param glyph_id glyph id to render.
 * @param font_size font size.
 * @param alpha_mode whether to render the glyph as SDF or alpha mask.
 * @return quad representing the geometry to render, and portion of an image to use.
 */
skb_quad_t skb_image_atlas_get_glyph_quad(
	skb_image_atlas_t* atlas, float x, float y, float pixel_scale,
	skb_font_collection_t* font_collection, skb_font_handle_t font_handle, uint32_t glyph_id, float font_size, skb_rasterize_alpha_mode_t alpha_mode);

/**
 * Get a quad representing the geometry and texture portion of the specified icon.
 *
 * The pixel scale is used to control the ratio between icon geometry size and image size.
 * For example, if icon of size 20 is requested, and pixel_scale is 2, then the geometry of the quad is based on the 20 units, but the requested image will be twice the size.
 * This is useful for cases where geometry will later go through a separate transformation process, and we want to match the pixel density.
 *
 * The function will return an existing icon or request a new icon to be rendered if one does not exist.
 *
 * @param atlas atlas to use.
 * @param x position x to render the icon at.
 * @param y position y to render the icon at.
 * @param pixel_scale the size of a pixel compared to the geometry.
 * @param icon_collection icon collection to use.
 * @param icon_handle handle to icon in the icon collection.
 * @param icon_scale scale of the icon to render.
 * @param alpha_mode whether to render the icon as SDF or alpha mask.
 * @return quad representing the geometry to render, and portion of an image to use.
 */
skb_quad_t skb_image_atlas_get_icon_quad(
	skb_image_atlas_t* atlas, float x, float y, float pixel_scale,
	const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle, skb_vec2_t icon_scale, skb_rasterize_alpha_mode_t alpha_mode);

/**
 * Get a quad representing the geometry and texture portion of the specified decoration pattern.
 *
 * The pixel scale is used to control the ratio between icon geometry size and image size.
 * For example, if icon of size 20 is requested, and pixel_scale is 2, then the geometry of the quad is based on the 20 units, but the requested image will be twice the size.
 * This is useful for cases where geometry will later go through a separate transformation process, and we want to match the pixel density.
 *
 * The function will return an existing pattern or request a new pattern to be rendered if one does not exist.
 *
 * @param atlas atlas to use.
 * @param x position x to render the glyph at.
 * @param y position y to render the glyph at.
 * @param width width of the decoration line to render (in same units as x and y).
 * @param offset_x offset of the pattern (in same units as x and y).
 * @param pixel_scale the size of a pixel compared to the geometry.
 * @param style style of the decoration to render.
 * @param thickness thickness of the decoration to render.
 * @param alpha_mode whether to render the pattern as SDF or alpha mask.
 * @return quad representing the geometry to render, and portion of an image to use.
 */
skb_pattern_quad_t skb_image_atlas_get_decoration_quad(
	skb_image_atlas_t* atlas, float x, float y, float width, float offset_x, float pixel_scale,
	uint8_t style, float thickness, skb_rasterize_alpha_mode_t alpha_mode);

/**
 * Compacts the atlas based on usage.
 * Items in the atlas that have not been queried for number of frames are removed (see config evict_inactive_duration).
 * This function should be called early in the frame so that we can free space for any new items that get requested during the frame.
 * @param atlas atlas to use.
 * @return true if any items were removed.
 */
bool skb_image_atlas_compact(skb_image_atlas_t* atlas);

/**
 * Rasterizes glyphs and icons that have been requested.
 * This function should be called after all the glyphs and icons have been requested.
 *
 * If the function returns true, you can use skb_image_atlas_get_texture_count() and skb_image_atlas_get_and_reset_textire_dirty_bounds() iterate
 * over all the images and see which ones, and what portions need to be uploaded to the GPU.
 *
 * @param atlas atlas to use.
 * @param temp_alloc temp alloc to use during rasterization.
 * @param rasterizer renderer to use during rasterization.
 * @return true if any images were changed.
 */
bool skb_image_atlas_rasterize_missing_items(skb_image_atlas_t* atlas, skb_temp_alloc_t* temp_alloc, skb_rasterizer_t* rasterizer);

/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_IMAGE_ATLAS_H
