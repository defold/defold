// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_CANVAS_H
#define SKB_CANVAS_H

#include "skb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup canvas Canvas
 *
 * Canvas is lightweight disposable vector drawing context.
 * The user is expected to create a canvas when they want to draw something to an image, and then free the canvas.
 *
 * The canvas is build the rendering model of OpenType COLR v1 [https://learn.microsoft.com/en-us/typography/opentype/spec/colr],
 * and as such may not be easy to use as general purpose canvas.
 *
 * The vector shapes are rasterized into masks, which in turn are used to apply the paints over the image.
 * Each time a mask is rasterized, it will intersect with the previous mask as if the previous mask was the clipping mask.
 * For that reason manual drawing would look something like this:
 *
 *		// Create copy of the (clipping) mask, and create temporary layer to draw to.
 *		skb_canvas_push_layer(...);
 *
 *		// Define shape to draw
 *		skb_canvas_move_to(...);
 *		skb_canvas_line_to(...);
 *		skb_canvas_close(...);
 *
 *		// Rasterize the mask, intersecting with the mask before call to skb_canvas_push_layer(),
 *		// and apply fill using the mask.
 *		skb_canvas_fill_solid_color(...);
 *
 *		// Blend the temporary layer over the main image.
 *		skb_canvas_pop_layer(...);
 *
 * The canvas supports drawing to an RGBA or Alpha images. In case of Alpha image, just the mask is drawn.
 *
 * The drawing is done in sRGB color space, and the output colors are premultiplied.
 *
 * @{
 */

/** Defines how the gradient fill behaves outside the defined edges. */
typedef enum {
	/** The gradient color is clamped to the nearest edge. */
	SKB_SPREAD_PAD,
	/** The gradient repeats beyond the defined edges. */
	SKB_SPREAD_REPEAT,
	/** The gradient repeats mirrored beyond the defined edges. */
	SKB_SPREAD_REFLECT,
} skb_gradient_spread_t;

/** Opaque type for the canvas. Use skb_canvas_create() to create. */
typedef struct skb_canvas_t skb_canvas_t;

/**
 * Defines gradient color stop.
 * The colors stops differ from OpenType COLR v1. The offsets are in range [0..1], and the whole range is defined interval.
 * The defined interval is used for the spread mode. The font rasterizer will normalize the ranges coming from the fonts.
 */
typedef struct skb_color_stop_t {
	/** Offset of the color, in range [0..1] */
	float offset;
	/** Color of the gradient stop. */
	skb_color_t color;
} skb_color_stop_t;

/**
 * Creates new canvas to draw to.
 * The canvas is expected to be disposable. You create canvas, draw some shapes, and dispose it.
 * The canvas will hold on to the temp allocator until skb_canvas_destroy() is called.
 * Note: the canvas itself is allocated using the temp allocator.
 * @param temp_alloc temp allocator to use during drawing.
 * @param target target to draw to.
 * @return pointer to the new canvas.
 */
skb_canvas_t* skb_canvas_create(skb_temp_alloc_t* temp_alloc, skb_image_t* target);

/**
 * Destroys canvas and frees any memory associated with it.
 * Note: the canvas is allocated using the temp allocator passed in the skb_canvas_create().
 * @param c pointer to the canvas to destroy.
 */
void skb_canvas_destroy(skb_canvas_t* c);

/**
 * Starts a new shape and move the pen position to 'pt'.
 * @param c canvas to draw to
 * @param pt
 */
void skb_canvas_move_to(skb_canvas_t* c, skb_vec2_t pt);

/**
 * Draws line from the pen position to 'pt'.
 * Sets the new pen position to 'pt'.
 * @param c canvas to draw to
 * @param pt
 */
void skb_canvas_line_to(skb_canvas_t* c, skb_vec2_t pt);

/**
 * Draws quadratic bezier segment from the pen position, using control point 'cp' to end segment point 'pt'.
 * Sets the new pen position to 'pt'.
 * @param c canvas to draw to
 * @param cp bezier segment control point
 * @param pt end point of the bezier segment.
 */
void skb_canvas_quad_to(skb_canvas_t* c, skb_vec2_t cp, skb_vec2_t pt);

/**
 * Draws cubi bezier segment from the pen position, using control points 'cp0' and 'cp1' to end segment point 'pt'.
 * Sets the new pen position to 'pt'.
 * @param c canvas to draw to
 * @param cp0 first control point (pen position out tangent)
 * @param cp1 first control point (pt in tangent)
 * @param pt end point
 */
void skb_canvas_cubic_to(skb_canvas_t* c, skb_vec2_t cp0, skb_vec2_t cp1, skb_vec2_t pt);

/**
 * Closes the shape by drawing line to the start point.
 * @param c canvas to draw to
 */
void skb_canvas_close(skb_canvas_t* c);

/**
 * Multiplies the top of the transform stack with 't' and pushes it to the top of the transform stack.
 * @param c canvas to draw to
 * @param t transform to apply.
 */
void skb_canvas_push_transform(skb_canvas_t* c, skb_mat2_t t);

/**
 * Pops the top of the transform stack.
 * @param c canvas to draw to
 */
void skb_canvas_pop_transform(skb_canvas_t* c);

/**
 * Takes a copy of the current mask, and pushes it to the top of the mask stack.
 * Shapes drawn after the push will be clipped to the mask.
 * Use skb_canvas_pop_mask() to restore to the earlier mask.
 * @param c canvas to draw to
 */
void skb_canvas_push_mask(skb_canvas_t* c);

/**
 * Pops top of the mask stack and makes the earlier mask effective.
 * @param c canvas to draw to
 */
void skb_canvas_pop_mask(skb_canvas_t* c);

/**
 * Pushes new image layer and mask to the lasyer stac.
 * @param c canvas to draw to
 */
void skb_canvas_push_layer(skb_canvas_t* c);

/**
 * Removes the image layer from the top of the layer stack and blends it over the previous image.
 * @param c canvas to draw to
 */
void skb_canvas_pop_layer(skb_canvas_t* c);

/**
 * Rasterizes the vector shape defined earlier into the top of the mask stack.
 * The mask will be intersected with the mask at the top of the mask stack using.
 * That is, the earlier mask is the clipping mask for the drawn shape.
 * If you call the fill methods multiple times without push/pop mask, the filled area will get smaller and smaller.
 * This fill method is used when drawing to an Alpha image.
 * @param c canvas to draw to
 */
void skb_canvas_fill_mask(skb_canvas_t* c);

/**
 * Applies solid color using the topmost mask over the topmost image layer.
 * Any pending masks are rasterized using skb_canvas_fill_mask() before applying the paint.
 * @param c canvas to draw to
 * @param color color to fill.
 */
void skb_canvas_fill_solid_color(skb_canvas_t* c, skb_color_t color);

/**
 * Applies linear gradient using the topmost mask over the topmost image layer.
 * Any pending masks are rasterized using skb_canvas_fill_mask() before applying the paint.
 * @param c canvas to draw to
 * @param p0 start point of the gradient
 * @param p1 end point of the gradient
 * @param spread defines how the gradient behaves outside the start and end point (not implemented yet)
 * @param stops color stops defining the gradient
 * @param stops_count number of gradient color stops
 */
void skb_canvas_fill_linear_gradient(skb_canvas_t* c, skb_vec2_t p0, skb_vec2_t p1, skb_gradient_spread_t spread, const skb_color_stop_t* stops, int32_t stops_count);

/**
 * Applies radial gradient using the topmost mask over the topmost image layer.
 * Any pending masks are rasterized using skb_canvas_fill_mask() before applying the paint.
 * Note: does not yet support the full spec.
 * @param c canvas to draw to
 * @param p0 center of the gradient
 * @param r0 radius around the center where the gradient starts
 * @param p1 offset end point of the gradient (not implemented yet)
 * @param r1 radius relative to the offset end point where the gradient ends
 * @param spread defines how the gradient behaves outside the start and end point (not implemented yet)
 * @param stops color stops defining the gradient
 * @param stops_count number of gradient color stops
 */
void skb_canvas_fill_radial_gradient(skb_canvas_t* c, skb_vec2_t p0, float r0, skb_vec2_t p1, float r1, skb_gradient_spread_t spread, const skb_color_stop_t* stops, int32_t stops_count);

/** @} */

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_CANVAS_H
