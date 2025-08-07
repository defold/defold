// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_ICON_COLLECTION_H
#define SKB_ICON_COLLECTION_H

#include "skb_canvas.h"
#include "skb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup icon_collection Icon Collection
 *
 * An Icon Collection contains number of icons, which can be queried based by name.
 *
 * The collection supports icons in PicoSVG format. It is a tiny SVG subset, see https://github.com/googlefonts/picosvg
 *
 * Alternatively the icons can be created in code.
 * @{
 */

/** Opaque type for the icon collection. Use skb_icon_collection_create() to create. */
typedef struct skb_icon_collection_t skb_icon_collection_t;

/** Opaque type for the icon. Use skb_icon_collection_add_icon() to create. */
typedef struct skb_icon_t skb_icon_t;

/** Opaque type for the icon shape. Use skb_icon_shape_add() to create. */
typedef struct skb_icon_shape_t skb_icon_shape_t;

/** Handle to an icon. */
typedef uint32_t skb_icon_handle_t;

/**
 * Create new icon collection.
 * @return create icon collection.
 */
skb_icon_collection_t* skb_icon_collection_create(void);

/**
 * Destroy icon collection.
 * @param icon_collection icon collection to destroy.
 */
void skb_icon_collection_destroy(skb_icon_collection_t* icon_collection);

/**
 * Adds PicoSVG to the icon collection from memory.
 * Note: the icon is parsed during the function and icon_data is not stored for later use.
 * @param icon_collection icon collection to use.
 * @param name name of the icon (used for querying).
 * @param icon_data pointer to the icon data to add.
 * @param icon_data_length length of the data in icon_data in bytes.
 * @return pointer to the added icon, or NULL if failed.
 */
skb_icon_handle_t skb_icon_collection_add_picosvg_icon_from_data(skb_icon_collection_t* icon_collection, const char* name, const char* icon_data, const int32_t icon_data_length);

#if !defined(SKB_NO_OPEN)
/**
 * Adds PicoSVG to the icon collection.
 * @param icon_collection icon collection to use.
 * @param name name of the icon (used for querying).
 * @param file_name name of the icon to add.
 * @return pointer to the added icon, or NULL if failed.
 */
skb_icon_handle_t skb_icon_collection_add_picosvg_icon(skb_icon_collection_t* icon_collection, const char* name, const char* file_name);
#endif // !defined(SKB_NO_OPEN)

/**
 * Adds an empty icon of specified name and size.
 * If icon of same name already exists, the function will return 0 as failure.
 * @param icon_collection icon collection to use.
 * @param name name of the icon (used for querying).
 * @param width width of the icon to create.
 * @param height height of the icon to create.
 * @return pointer to the added icon, or 0 if failed.
 */
skb_icon_handle_t skb_icon_collection_add_icon(skb_icon_collection_t* icon_collection, const char* name, float width, float height);

/**
 * Removes icons from collection.
 * @param icon_collection icon collection to use.
 * @param icon_handle handle to icon to remove.
 * @return true if the icon was removes.
 */
bool skb_icon_collection_remove_icon(skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle);

/**
 * Finds an icon based on name.
 * @param icon_collection icon collection to use.
 * @param name name of the icon to query.
 * @return pointer to the icon, or NULL if not found.
 */
skb_icon_handle_t skb_icon_collection_find_icon(const skb_icon_collection_t* icon_collection, const char* name);

/**
 * Set whether the icon should be rendered as RGBA or alpha mask.
 * Icons are created as color icons.
 * @param icon_collection icon collection to use.
 * @param icon_handle handle to icon to modify.
 * @param is_color if true, the icon will be rendered as RGBA, if false, the icon will be rendered as alpha mask.
 */
void skb_icon_collection_set_is_color(skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle, bool is_color);

/**
 * Calculates propertional scale to render icon at specific size.
 * For example, if icon size is 20, and you request 30, the scaling will be 1.5.
 * If width or height is set to -1, then uniform scaling is used.
 * @param icon_collection icon collection to use.
 * @param icon_handle handle to the icon to query.
 * @param width requested with, if -1 the result x scale is same as y scale.
 * @param height requested with, if -1 the result y scale is same as x scale.
 * @return how much to scale the icon to get requested size.
 */
skb_vec2_t skb_icon_collection_calc_proportional_scale(const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle, float width, float height);

/** @return size of the icon. */
skb_vec2_t skb_icon_collection_get_icon_size(const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle);

/**
 * Returns pointer to an icon. The icon pointer should not be stored for longer duration. Use skb_font_handle_t instead.
 * @param icon_collection icon collection to use.
 * @param icon_handle handle to the icon to query.
 * @return pointer to the specified icon.
 */
const skb_icon_t* skb_icon_collection_get_icon(const skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle);

//
// Icon Builder
//

enum {
	/** Number of nested levels of shapes that can be build using the builder. */
	SKB_ICON_BUILDER_MAX_NESTED_SHAPES = 8,
};

/** Struct describing icon builder. */
typedef struct skb_icon_builder_t {
	// Internal
	skb_icon_collection_t* icon_collection;
	skb_icon_handle_t icon_handle;
	skb_icon_shape_t* shape_stack[SKB_ICON_BUILDER_MAX_NESTED_SHAPES];
	int32_t shape_stack_idx;
} skb_icon_builder_t;

/**
 * Makes a icon builder struct.
 * @param icon_collection pointer to icon collection.
 * @param icon_handle icon handle to build.
 * @return
 */
skb_icon_builder_t skb_icon_builder_make(skb_icon_collection_t* icon_collection, skb_icon_handle_t icon_handle);

/**
 * Begin new shape. Can be nested to create groups.
 * @param icon_builder pointer to icon builder.
 */
void skb_icon_builder_begin_shape(skb_icon_builder_t* icon_builder);

/**
 * Ends shape or group.
 * @param icon_builder pointer to icon builder.
 */
void skb_icon_builder_end_shape(skb_icon_builder_t* icon_builder);

/**
 * Starts a new path in the current shape.
 * @param icon_builder pointer to icon builder.
 * @param pt starting point of the path.
 */
void skb_icon_builder_move_to(skb_icon_builder_t* icon_builder, skb_vec2_t pt);

/**
 * Appends line to the current path.
 * @param icon_builder
 * @param pt line end position.
 */
void skb_icon_builder_line_to(skb_icon_builder_t* icon_builder, skb_vec2_t pt);

/**
 * Appends quadratic bezier segment to the current path.
 * @param icon_builder pointer to icon builder.
 * @param cp control point of the curve.
 * @param pt curve end position.
 */
void skb_icon_builder_quad_to(skb_icon_builder_t* icon_builder, skb_vec2_t cp, skb_vec2_t pt);

/**
 * Appends cubic bezier segment to the current path.
 * @param icon_builder pointer to icon builder.
 * @param cp0 first control point of the curve.
 * @param cp1 second control point of the curve.
 * @param pt curve end position.
 */
void skb_icon_builder_cubic_to(skb_icon_builder_t* icon_builder, skb_vec2_t cp0, skb_vec2_t cp1, skb_vec2_t pt);

/**
 * Closes the current path byt connect the last point to start point.
 * @param icon_builder pointer to icon builder.
 */
void skb_icon_builder_close_path(skb_icon_builder_t* icon_builder);

/**
 * Sets the opacity of current shape.
 * @param icon_builder pointer to icon builder.
 * @param opacity
 */
void skb_icon_builder_fill_opacity(skb_icon_builder_t* icon_builder, float opacity);

/**
 * Sets the fill of the current shape to solid color.
 * @param icon_builder pointer to icon builder.
 * @param color color to set
 */
void skb_icon_builder_fill_color(skb_icon_builder_t* icon_builder, skb_color_t color);

/**
 * Sets the fill of the current shape to a linear gradient.
 * @param icon_builder pointer to icon builder.
 * @param p0 start point of the gradient.
 * @param p1 end point of the gradient.
 * @param xform gradient transform.
 * @param spread spread mode of the gradient.
 * @param stops color stops for the gradient.
 * @param stops_count number of color stops.
 */
void skb_icon_builder_fill_linear_gradient(skb_icon_builder_t* icon_builder, skb_vec2_t p0, skb_vec2_t p1, skb_mat2_t xform, skb_gradient_spread_t spread, skb_color_stop_t* stops, int32_t stops_count);

/**
 * Sets the fill of the current shape to a radial gradient.
 * @param icon_builder pointer to icon builder.
 * @param p0 start point of the gradient.
 * @param p1 end point of the gradient.
 * @param radius radius of the gradient at start point.
 * @param xform gradient transform.
 * @param spread spread mode of the gradient.
 * @param stops color stops for the gradient.
 * @param stops_count number of color stops.
 */
void skb_icon_builder_fill_radial_gradient(skb_icon_builder_t* icon_builder, skb_vec2_t p0, skb_vec2_t p1, float radius, skb_mat2_t xform, skb_gradient_spread_t spread, skb_color_stop_t* stops, int32_t stops_count);

/** @} */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SKB_ICON_COLLECTION_H
