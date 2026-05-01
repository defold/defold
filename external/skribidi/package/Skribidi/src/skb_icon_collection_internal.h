// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_ICON_COLLECTION_INTERNAL_H
#define SKB_ICON_COLLECTION_INTERNAL_H

#include <stdint.h>
#include "skb_canvas.h"
#include "skb_icon_collection.h"

typedef enum {
	SKB_SVG_MOVE_TO,
	SKB_SVG_LINE_TO,
	SKB_SVG_QUAD_TO,
	SKB_SVG_CUBIC_TO,
	SKB_SVG_CLOSE_PATH,
} skb_icon_path_command_type_t;

typedef struct skb_icon_path_command_t {
	skb_vec2_t cp0;
	skb_vec2_t cp1;
	skb_vec2_t pt;
	skb_icon_path_command_type_t type;
} skb_icon_path_command_t;

typedef enum {
	SKB_GRADIENT_LINEAR,
	SKB_GRADIENT_RADIAL,
} skb_icon_gradient_type_t;

typedef struct skb_icon_gradient_t {
	skb_color_stop_t* stops;	// Color stops array.
	int32_t stops_count;		// Number of color stops.
	int32_t stops_cap;			// Capacity of the stops array.
	skb_vec2_t p0;				// Starting point of the gradient.
	skb_vec2_t p1;				// End point of the gradient.
	float radius;				// Radius of the gradient (for radial gradient)
	uint8_t type;				// Type of the gradient.
	uint8_t spread;				// Spread type of the gradient.
	skb_mat2_t xform;			// Gradient transform.
} skb_icon_gradient_t;

typedef struct skb_icon_shape_t {
	struct skb_icon_shape_t* children;	// Array of child shapes.
	int32_t children_count;			// Number of children
	int32_t children_cap;			// Capacity of the children array.
	skb_icon_path_command_t* path;	// Path commands of the shape.
	int32_t path_count;				// Number of path commands.
	int32_t path_cap;				// Capccity of the path array.
	float opacity;					// Opacity of the shape and it's children.
	skb_color_t color;				// Color of the shape.
	int32_t gradient_idx;			// Gradient index of the shape (gradients stored in icon), or SKB_INVALID_INDEX if color should be used instead.
} skb_icon_shape_t;

typedef struct skb_icon_t {
	skb_icon_gradient_t* gradients;	// Array of gradients in the icon.
	int32_t gradients_count;		// Number of gradients in the icon.
	int32_t gradients_cap;			// Capacity of the gradients array.
	skb_icon_shape_t root;			// Root shape
	skb_rect2_t view;				// View rectangle, which defines size and origin offset of the icon.
	uint64_t hash;					// Hash used to identify the icon.
	bool is_color;			// If true, render the icon as alpha mask only.
	char* name;						// Icon name string.
	skb_icon_handle_t handle;		// Icon handle
	uint32_t generation;			// Icon generation, bumped when icon is deleted.
} skb_icon_t;

typedef struct skb_icon_collection_t {
	uint32_t id;					// Unique id of the icon collection.
	skb_hash_table_t* icons_lookup;	// Lookup used to find an icon by name,
	skb_icon_t* icons;				// Array of icons in the collection.
	int32_t icons_count;			// Number of icons in the collection.
	int32_t icons_cap;				// Capacity of the icons array.
	int32_t empty_icons_count;		// Number of empty icons, used to try to find empty icon slot.
} skb_icon_collection_t;

#endif // SKB_ICON_COLLECTION_INTERNAL_H
