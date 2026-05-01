// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_canvas.h"
#include "skb_common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// The rasterizer is based on the old stb_truetype rasterizer.
#define SKB_SUBSAMPLES		5
#define SKB_SAMPLEWEIGHT	(255 / SKB_SUBSAMPLES)
#define SKB_FIXSHIFT		10
#define SKB_FIX				(1 << SKB_FIXSHIFT)
#define SKB_FIXMASK			(SKB_FIX-1)

typedef struct skb_image_layer_t {
	skb_color_t* buffer;
	int32_t stride;
} skb_image_layer_t;

typedef struct skb_mask_t {
	uint8_t* buffer;
	int32_t stride;
	skb_rect2i_t region;
} skb_mask_t;

typedef struct skb_edge_t {
	float x0,y0, x1,y1;
	int dir;
} skb_edge_t;

typedef struct skb_active_edge_t {
	int x,dx;
	float ey;
	int dir;
	struct skb_active_edge_t *next;
} skb_active_edge_t;

typedef struct skb_canvas_t {
	uint8_t* scanline;
	int32_t width;
	int32_t height;
	uint8_t bpp;

	skb_vec2_t start_pt;	// path start
	skb_vec2_t pen_pt;	// current pen position

	skb_vec2_t* points;
	int32_t points_count;
	int32_t points_cap;
	int32_t degenerate_path_count;

	skb_rect2_t points_bounds;

	skb_edge_t* edges;
	int32_t edges_count;
	int32_t edges_cap;

	skb_active_edge_t* active_edges;
	int32_t active_edges_count;
	int32_t active_edges_cap;
	skb_active_edge_t* freelist;

	skb_image_layer_t* layers;
	int32_t layers_count;
	int32_t layers_cap;

	skb_mask_t* masks;
	int32_t masks_count;
	int32_t masks_cap;

	skb_mat2_t* transform_stack;
	int32_t transform_stack_count;
	int32_t transform_stack_cap;

	skb_image_t* target;

	skb_temp_alloc_t* alloc;
	skb_temp_alloc_mark_t mark;
} skb_canvas_t;


static void skb_path_add_point_(skb_canvas_t* c, skb_vec2_t pt)
{
	static const float dist_tol = 0.1f;

	if (c->points_count > 0) {
		skb_vec2_t* last_pt = &c->points[c->points_count-1];
		if (skb_vec2_equals(*last_pt, pt, dist_tol))
			return;
	}

	if (c->points_count+1 > c->points_cap) {
		c->points_cap = c->points_cap > 0 ? c->points_cap * 2 : 64;
		skb_vec2_t* new_points = SKB_TEMP_REALLOC(c->alloc, c->points, skb_vec2_t, c->points_cap);
		if (new_points == NULL) return;
		c->points = new_points;
	}

	skb_vec2_t* new_pt = &c->points[c->points_count++];
	*new_pt = pt;
}

static void skb_add_edge_(skb_canvas_t* c, float x0, float y0, float x1, float y1)
{
	// Skip horizontal edges
	if (y0 == y1)
		return;

	if (c->edges_count+1 > c->edges_cap) {
		c->edges_cap = c->edges_cap > 0 ? c->edges_cap * 2 : 64;
		skb_edge_t* new_edges = SKB_TEMP_REALLOC(c->alloc, c->edges, skb_edge_t, c->edges_cap);
		if (new_edges == NULL) return;
		c->edges = new_edges;
	}

	skb_edge_t* e = &c->edges[c->edges_count++];

	if (y0 < y1) {
		e->x0 = x0;
		e->y0 = y0 * SKB_SUBSAMPLES;
		e->x1 = x1;
		e->y1 = y1 * SKB_SUBSAMPLES;
		e->dir = 1;
	} else {
		e->x0 = x1;
		e->y0 = y1 * SKB_SUBSAMPLES;
		e->x1 = x0;
		e->y1 = y0 * SKB_SUBSAMPLES;
		e->dir = -1;
	}
}

static void skb_path_tess_cubic_bezier_(skb_canvas_t* c, const skb_vec2_t p0, const skb_vec2_t p1, const skb_vec2_t p2, const skb_vec2_t p3, const int level, const float dist_tol_sqr)
{
	if (level > 10) return;

	const skb_vec2_t d30 = skb_vec2_sub(p3, p0);
	const skb_vec2_t d13 = skb_vec2_sub(p1, p3);
	const skb_vec2_t d23 = skb_vec2_sub(p2, p3);
	const float d2 = skb_absf(d13.x * d30.y - d13.y * d30.x);
	const float d3 = skb_absf(d23.x * d30.y - d23.y * d30.x);

	if ((d2 + d3)*(d2 + d3) < dist_tol_sqr * (d30.x*d30.x + d30.y*d30.y)) {
		skb_path_add_point_(c, p3);
		return;
	}

	const skb_vec2_t p01 = skb_vec2_lerp(p0, p1, 0.5f);
	const skb_vec2_t p12 = skb_vec2_lerp(p1, p2, 0.5f);
	const skb_vec2_t p23 = skb_vec2_lerp(p2, p3, 0.5f);
	const skb_vec2_t p012 = skb_vec2_lerp(p01, p12, 0.5f);
	const skb_vec2_t p123 = skb_vec2_lerp(p12, p23, 0.5f);
	const skb_vec2_t p0123 = skb_vec2_lerp(p012, p123, 0.5f);

	skb_path_tess_cubic_bezier_(c, p0, p01, p012, p0123, level+1, dist_tol_sqr);
	skb_path_tess_cubic_bezier_(c, p0123, p123, p23, p3, level+1, dist_tol_sqr);
}

// Used for debugging.
//#define SKB_CANVAS_DUMP_SVG
#ifdef SKB_CANVAS_DUMP_SVG
static void skb__dump_path_svg(skb_canvas_t* c)
{
	static int num = 1;
	char filename[32];
	snprintf(filename, 32, "path_%03d.svg", num++);

	FILE* fp = fopen(filename, "w");

	const int32_t width = c->width;
	const int32_t height = c->height;

	fprintf(fp, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"-50 -50 %d %d\">\n", width,height,width+50,height+50);

	for (int32_t i = 0, j = c->points_count-1; i < c->points_count; j = i++) {
		c->points_bounds = skb_rect2_union_point(c->points_bounds, c->points[j]);
		fprintf(fp, "<line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" stroke=\"black\" />\n",
			c->points[j].x, c->points[j].y, c->points[i].x, c->points[i].y);
	}

	for (int32_t i = 0; i < c->points_count; i++)
		fprintf(fp, "<circle cx=\"%f\" cy=\"%f\" r=\"1\" fill=\"%s\" />\n", c->points[i].x, c->points[i].y, i == 0 ? "red" : "blue");

	fprintf(fp, "</svg>\n");

	fclose(fp);
}
#endif

static void skb_path_commit_(skb_canvas_t* c)
{
	// Need at least 3 points for a filled shape.
	if (c->points_count > 2) {

#ifdef SKB_CANVAS_DUMP_SVG
		skb__dump_path_svg(c);
#endif

		for (int32_t i = 0, j = c->points_count-1; i < c->points_count; j = i++) {
			c->points_bounds = skb_rect2_union_point(c->points_bounds, c->points[j]);
			skb_add_edge_(c, c->points[j].x, c->points[j].y, c->points[i].x, c->points[i].y);
		}
	} else if (c->points_count > 0) {
		// There were points added, but they did not amount to a shape. Mark as degenerate so that we can clear the mask.
		c->degenerate_path_count++;
	}
	c->points_count = 0;
}

void skb_canvas_move_to(skb_canvas_t* c, skb_vec2_t pt)
{
	// Commit any paths in progress
	skb_path_commit_(c);

	// Restart
	pt = skb_mat2_point(c->transform_stack[c->transform_stack_count-1], pt);
	skb_path_add_point_(c, pt);
	c->start_pt = pt;
	c->pen_pt = c->start_pt;
}

void skb_canvas_line_to(skb_canvas_t* c, skb_vec2_t pt)
{
	pt = skb_mat2_point(c->transform_stack[c->transform_stack_count-1], pt);
	skb_path_add_point_(c, pt);
	c->pen_pt = pt;
}

void skb_canvas_quad_to(skb_canvas_t* c, skb_vec2_t cp, skb_vec2_t pt)
{
	static const float dist_tol_sqr = 0.5f * 0.5f;

	cp = skb_mat2_point(c->transform_stack[c->transform_stack_count-1], cp);
	pt = skb_mat2_point(c->transform_stack[c->transform_stack_count-1], pt);

	// Convert to cubic bezier
	const skb_vec2_t cp0 = skb_vec2_mad(c->pen_pt, skb_vec2_sub(cp, c->pen_pt), 2.f/3.f);
	const skb_vec2_t cp1 = skb_vec2_mad(pt, skb_vec2_sub(cp, pt), 2.f/3.f);

	skb_path_tess_cubic_bezier_(c, c->pen_pt, cp0, cp1, pt, 0, dist_tol_sqr);
	c->pen_pt = pt;
}

void skb_canvas_cubic_to(skb_canvas_t* c, skb_vec2_t cp0, skb_vec2_t cp1, skb_vec2_t pt)
{
	static const float dist_tol_sqr = 0.5f * 0.5f;

	cp0 = skb_mat2_point(c->transform_stack[c->transform_stack_count-1], cp0);
	cp1 = skb_mat2_point(c->transform_stack[c->transform_stack_count-1], cp1);
	pt = skb_mat2_point(c->transform_stack[c->transform_stack_count-1], pt);

	skb_path_tess_cubic_bezier_(c, c->pen_pt, cp0, cp1, pt, 0, dist_tol_sqr);
	c->pen_pt = pt;
}

void skb_canvas_close(skb_canvas_t* c)
{
	skb_path_add_point_(c, (skb_vec2_t){ c->start_pt.x, c->start_pt.y });
}

static inline int32_t skb__round_to_int(float x)
{
	x -= 0.5f;
	const int32_t ix = (int32_t)x;
	return x < 0.f ? ix : (ix+1);
}

static skb_active_edge_t* skb_add_active_edge_(skb_canvas_t* c, const skb_edge_t* e, float start_y)
{
	skb_active_edge_t* z = NULL;

	if (c->freelist != NULL) {
		// Restore from freelist.
		z = c->freelist;
		c->freelist = z->next;
	} else {
		// Alloc new edge.
		if (c->active_edges_count+1 > c->active_edges_cap) return NULL;
		z = &c->active_edges[c->active_edges_count++];
	}

	const float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);

	// round dx down to avoid going too far
	if (dxdy < 0)
		z->dx = -skb__round_to_int(SKB_FIX * -dxdy);
	else
		z->dx = skb__round_to_int(SKB_FIX * dxdy);

	z->x = skb__round_to_int(SKB_FIX * (e->x0 + dxdy * (start_y - e->y0)));

	z->ey = e->y1;
	z->next = 0;
	z->dir = e->dir;

	return z;
}

static void skb_free_active_edge_(skb_canvas_t* c, skb_active_edge_t* z)
{
	z->next = c->freelist;
	c->freelist = z;
}

static void skb_fill_scaline_(uint8_t* scanline, int32_t scanline_width, int32_t x0, int32_t x1, int32_t* xmin, int32_t* xmax)
{
	int32_t start = x0 >> SKB_FIXSHIFT;
	int32_t end = x1 >> SKB_FIXSHIFT;

	if (start < *xmin) *xmin = start;
	if (end > *xmax) *xmax = end;

	if (start < scanline_width && end >= 0) {
		if (start == end) {
			// x0,x1 are the same pixel, so compute combined coverage
			scanline[start] = (uint8_t)(scanline[start] + ((x1 - x0) * SKB_SAMPLEWEIGHT >> SKB_FIXSHIFT));
		} else {
			if (start >= 0) // add antialiasing for x0
				scanline[start] = (uint8_t)(scanline[start] + (((SKB_FIX - (x0 & SKB_FIXMASK)) * SKB_SAMPLEWEIGHT) >> SKB_FIXSHIFT));
			else
				start = -1; // clip

			if (end < scanline_width) // add antialiasing for x1
				scanline[end] = (uint8_t)(scanline[end] + (((x1 & SKB_FIXMASK) * SKB_SAMPLEWEIGHT) >> SKB_FIXSHIFT));
			else
				end = scanline_width; // clip

			for (++start; start < end; ++start) // fill pixels between x0 and x1
				scanline[start] = (uint8_t)(scanline[start] + SKB_SAMPLEWEIGHT);
		}
	}
}

static void skb_fill_active_edges_(uint8_t* scanline, int32_t len, skb_active_edge_t* e, int32_t* xmin, int32_t* xmax)
{
	// Non-zero
	int32_t x0 = 0, w = 0;
	while (e != NULL) {
		if (w == 0) {
			// if we're currently at zero, we need to record the edge start point
			x0 = e->x;
			w += e->dir;
		} else {
			const int32_t x1 = e->x;
			w += e->dir;
			// if we went to zero, we need to draw
			if (w == 0)
				skb_fill_scaline_(scanline, len, x0, x1, xmin, xmax);
		}
		e = e->next;
	}
}

static void skb_rasterize_sorted_edges_(skb_canvas_t* c, skb_mask_t* mask)
{
	// Make sure we have enough active edges
	c->freelist = NULL;
	c->active_edges_count = 0;
	if (c->edges_count > c->active_edges_cap) {
		c->active_edges_cap = c->edges_count;
		skb_active_edge_t* new_active_edge = SKB_TEMP_REALLOC(c->alloc, c->active_edges, skb_active_edge_t, c->active_edges_cap);
		if (new_active_edge == NULL) return;
		c->active_edges = new_active_edge;
	}

	skb_active_edge_t *active = NULL;
	int32_t e = 0;

	uint8_t* mask_buffer = mask->buffer + (mask->region.y * mask->stride);
	uint8_t* scanline = c->scanline;

	for (int32_t y = mask->region.y; y < (mask->region.y + mask->region.height); ++y) {
		memset(scanline + mask->region.x, 0, mask->region.width);
		int32_t xmin = c->width - 1;
		int32_t xmax = 0;
		for (int32_t s = 0; s < SKB_SUBSAMPLES; ++s) {
			// find center of pixel for this scanline
			float scan_y = (float)(y*SKB_SUBSAMPLES + s) + 0.5f;
			skb_active_edge_t **step = &active;

			// update all active edges;
			// remove all active edges that terminate before the center of this scanline
			while (*step) {
				skb_active_edge_t* z = *step;
				if (z->ey <= scan_y) {
					*step = z->next; // delete from list
					skb_free_active_edge_(c, z);
					z = NULL;
				} else {
					z->x += z->dx; // advance to position for current scanline
					step = &((*step)->next); // advance through list
				}
			}

			// resort the list if needed
			for (;;) {
				bool changed = 0;
				step = &active;
				while (*step && (*step)->next) {
					if ((*step)->x > (*step)->next->x) {
						skb_active_edge_t* t = *step;
						skb_active_edge_t* q = t->next;
						t->next = q->next;
						q->next = t;
						*step = q;
						changed = true;
					}
					step = &(*step)->next;
				}
				if (!changed) break;
			}

			// insert all edges that start before the center of this scanline -- omit ones that also end on this scanline
			while (e < c->edges_count && c->edges[e].y0 <= scan_y) {
				if (c->edges[e].y1 > scan_y) {
					skb_active_edge_t* z = skb_add_active_edge_(c, &c->edges[e], scan_y);
					if (z == NULL) break;
					// find insertion point
					if (active == NULL) {
						active = z;
					} else if (z->x < active->x) {
						// insert at front
						z->next = active;
						active = z;
					} else {
						// find thing to insert AFTER
						skb_active_edge_t* p = active;
						while (p->next && p->next->x < z->x)
							p = p->next;
						// at this point, p->next->x is NOT < z->x
						z->next = p->next;
						p->next = z;
					}
				}
				e++;
			}

			// now process all active edges in non-zero fashion
			if (active != NULL) {
				skb_fill_active_edges_(scanline, c->width, active, &xmin, &xmax);
			}
		}

		// Blit.
		xmin = skb_maxi(xmin, mask->region.x);
		xmax = skb_mini(xmax, mask->region.x + mask->region.width - 1);
		if (xmin <= xmax) {
			// Mask out the part before the rendered shape
			for (int32_t x = mask->region.x; x < xmin; x++)
				mask_buffer[x] = 0;
			// Combine rendered mask with existing one.
			for (int32_t x = xmin; x <= xmax; ++x)
				mask_buffer[x] = (uint8_t)skb_mul255((int32_t)mask_buffer[x], (int32_t)scanline[x]);
			// Mask out the part after the rendered shape
			for (int32_t x = xmax+1; x < (mask->region.x + mask->region.width); x++)
				mask_buffer[x] = 0;
		} else {
			// Nothing got rendered,
			memset(mask_buffer + mask->region.x, 0, mask->region.width);
		}

		mask_buffer += mask->stride;
	}
}

static int skb_cmp_edge_(const void *p, const void *q)
{
	const skb_edge_t* a = (const skb_edge_t*)p;
	const skb_edge_t* b = (const skb_edge_t*)q;
	if (a->y0 < b->y0) return -1;
	if (a->y0 > b->y0) return  1;
	return 0;
}

void skb_canvas_fill_mask(skb_canvas_t* c)
{
	// Commit any pending paths.
	skb_path_commit_(c);

	bool is_empty = c->edges_count == 0 && c->degenerate_path_count == 0;

	skb_mask_t* mask = &c->masks[c->masks_count - 1];

	// Rasterize edges
	if (c->edges_count > 0) {

		qsort(c->edges, c->edges_count, sizeof(skb_edge_t), skb_cmp_edge_);

		// The mask is intersection of the existing masking area and new path to render to the mask.
		skb_rect2i_t shape_bounds;
		shape_bounds.x = (int32_t)floorf(c->points_bounds.x);
		shape_bounds.y = (int32_t)floorf(c->points_bounds.y);
		shape_bounds.width = (int32_t)ceilf(c->points_bounds.x + c->points_bounds.width) - shape_bounds.x;
		shape_bounds.height = (int32_t)ceilf(c->points_bounds.y + c->points_bounds.height) - shape_bounds.y;

		mask->region = skb_rect2i_intersection(mask->region, shape_bounds);

		if (!skb_rect2i_is_empty(mask->region))
			skb_rasterize_sorted_edges_(c, mask);

	} else if (c->degenerate_path_count > 0) {
		// We tried to rasterize a degenerate path, nothing got drawn, so nothing should be visible.
		mask->region = (skb_rect2i_t){0};
	}

	// If we're reandering to the mask, we need to clear out the unused region.
	if (c->bpp == 1 && mask->buffer == c->target->buffer) {
		uint8_t* mask_buffer = mask->buffer;
		const int32_t mask_stride = mask->stride;
		if (is_empty || mask->region.width <= 0 || mask->region.height <= 0) {
			// Clear the whole mask
			for (int32_t y = 0; y < c->height; y++)
				memset(mask_buffer + y * mask_stride, 0, c->width);
		} else {
			// Clear the outside of the region.
			for (int32_t y = 0; y < mask->region.y; y++)
				memset(mask_buffer + y * mask_stride, 0, c->width);
			for (int32_t y = mask->region.y; y < (mask->region.y + mask->region.height); y++) {
				memset(mask_buffer + y * mask_stride, 0, mask->region.x);
				memset(mask_buffer + y * mask_stride + mask->region.x + mask->region.width, 0, c->width - (mask->region.x + mask->region.width));
			}
			for (int32_t y = mask->region.y + mask->region.height; y < c->height; y++)
				memset(mask_buffer + y * mask_stride, 0, c->width);
		}
	}

	// Reset bounding box collection.
	c->points_bounds = skb_rect2_make_undefined();

	c->edges_count = 0;
	c->degenerate_path_count = 0;
}

void skb_canvas_push_transform(skb_canvas_t* c, skb_mat2_t t)
{
	if (c->transform_stack_count+1 > c->transform_stack_cap) {
		c->transform_stack_cap = c->transform_stack_cap > 0 ? c->transform_stack_cap * 2 : 4;
		skb_mat2_t* new_stack = SKB_TEMP_REALLOC(c->alloc, c->transform_stack, skb_mat2_t, c->transform_stack_cap);
		assert(new_stack);
		c->transform_stack = new_stack;
	}

	if (c->transform_stack_count > 0)
		c->transform_stack[c->transform_stack_count] = skb_mat2_multiply(t, c->transform_stack[c->transform_stack_count-1]);
	else
		c->transform_stack[c->transform_stack_count] = t;
	c->transform_stack_count++;
}

void skb_canvas_pop_transform(skb_canvas_t* c)
{
	if (c->transform_stack_count > 1)
		c->transform_stack_count--;
}

void skb_canvas_push_mask(skb_canvas_t* c)
{
	SKB_ARRAY_RESERVE(c->masks, c->masks_count+1);

	skb_mask_t* cur_mask = c->masks_count > 0 ? &c->masks[c->masks_count-1] : NULL;
	skb_mask_t* mask = &c->masks[c->masks_count++];

	if (!mask->buffer) {
		mask->buffer = SKB_TEMP_ALLOC(c->alloc, uint8_t, c->width * c->height);
		mask->stride = c->width;
	}

	if (cur_mask) {
		// Inherit mask
		const uint8_t* src = cur_mask->buffer + cur_mask->region.x + (cur_mask->region.y * cur_mask->stride);
		uint8_t* dst = mask->buffer + cur_mask->region.x + (cur_mask->region.y * mask->stride);
		const int32_t copy_w = cur_mask->region.width;
		for (int32_t y = cur_mask->region.y; y < (cur_mask->region.y + cur_mask->region.height); y++) {
			memcpy(dst, src, copy_w);
			src += cur_mask->stride;
			dst += mask->stride;
		}
		mask->region = cur_mask->region;
	} else {
		// Init mask to full canvas size.
		for (int32_t y = 0; y < c->height; y++)
			memset(mask->buffer + y * mask->stride, 255, c->width * sizeof(uint8_t));
		mask->region.x = 0;
		mask->region.y = 0;
		mask->region.width = c->width;
		mask->region.height = c->height;
	}
}

void skb_canvas_pop_mask(skb_canvas_t* c)
{
	if (c->masks_count > 1) // first mask is for the whole image.
		c->masks_count--;
}

void skb_canvas_push_layer(skb_canvas_t* c)
{
	SKB_ARRAY_RESERVE(c->layers, c->layers_count+1);
	skb_image_layer_t* layer = &c->layers[c->layers_count++];

	if (!layer->buffer) {
		layer->buffer = SKB_TEMP_ALLOC(c->alloc, skb_color_t, c->width * c->height);
		layer->stride = c->width;
	}

	skb_canvas_push_mask(c);

	// Clear rendering region.
	skb_mask_t* mask = &c->masks[c->masks_count-1];
	skb_color_t* dst = layer->buffer + mask->region.x + (mask->region.y * c->width);
	const int32_t clear_w = mask->region.width * 4;
	for (int32_t y = mask->region.y; y < (mask->region.y + mask->region.height); y++) {
		memset(dst, 0, clear_w);
		dst += layer->stride;
	}
}

static void skb__blend_over(const skb_image_layer_t* src, skb_image_layer_t* dst, int32_t offset_x, int32_t offset_y, int32_t width, int32_t height)
{
	const int32_t src_stride = src->stride;
	const int32_t dst_stride = dst->stride;
	const int32_t blend_w = width;

	for (int32_t y = offset_y; y < (offset_y + height); y++) {
		const skb_color_t* x_src = src->buffer + offset_x + (y * src_stride);
		skb_color_t* x_dst = dst->buffer + offset_x + (y * dst_stride);
		for (int32_t x = 0; x < blend_w; x++) {
			if (x_src->a == 255)
				*x_dst = *x_src;
			else if (x_src->a > 0)
				*x_dst = skb_color_blend(*x_dst, *x_src);
			x_src++;
			x_dst++;
		}
	}
}

void skb_canvas_pop_layer(skb_canvas_t* c)
{
	if (c->layers_count <= 1) return; // First item is the final layer, do not remove.
	c->layers_count--;

	// Blend down
	skb_image_layer_t* bg_layer = &c->layers[c->layers_count-1];
	skb_image_layer_t* fg_layer = &c->layers[c->layers_count];
	skb_mask_t* mask = &c->masks[c->masks_count-1];

	skb__blend_over(fg_layer, bg_layer, mask->region.x, mask->region.y, mask->region.width, mask->region.height);

	skb_canvas_pop_mask(c);
}

enum {
	SKB_GRADIENT_CACHE_SIZE = 256,	// Must be power of 2
};

// Allowing the gradient indexing to go up to SKB_GRADIENT_CACHE_SIZE inclusive, makes the spread mode calculation simpler.
typedef struct skb_gradient_cache_t {
	skb_color_t colors[SKB_GRADIENT_CACHE_SIZE+1];
} skb_gradient_cache_t;

static void skb_gradient_cache_init_(skb_gradient_cache_t* cache, const skb_color_stop_t* stops, int32_t stops_count)
{
	if (stops_count == 0) {
		skb_color_t black = {0};
		for (int32_t i = 0; i <= SKB_GRADIENT_CACHE_SIZE; i++)
			cache->colors[i] = black;
	} else if (stops_count == 1) {
		for (int32_t i = 0; i <= SKB_GRADIENT_CACHE_SIZE; i++)
			cache->colors[i] = stops[0].color;
	} else {
		// Scale offsets, so that first stop is at 0, and last at 1.

		// Colors before the first stop.
		int32_t idx = skb_clampi((int32_t)(stops[0].offset * SKB_GRADIENT_CACHE_SIZE), 0, SKB_GRADIENT_CACHE_SIZE);
		skb_color_t color = stops[0].color;
		for (int32_t i = 0; i < idx; i++)
			cache->colors[i] = color;

		// Colors in-between
		for (int32_t stop_idx = 1; stop_idx < stops_count; stop_idx++) {

			int32_t prev_idx = idx;
			skb_color_t prev_color = color;

			idx = skb_clampi((int32_t)(stops[stop_idx].offset * SKB_GRADIENT_CACHE_SIZE), 0, SKB_GRADIENT_CACHE_SIZE);
			color = stops[stop_idx].color;

			const int32_t count = idx - prev_idx;
			if (count > 0) {
				float t = 0.f;
				const float dt = count > 0 ? 1.f / (float)(count-1) : 0.f;
				for (int32_t i = prev_idx; i < idx; i++) {
					cache->colors[i] = skb_color_lerpf(prev_color, color, t);
					t += dt;
				}
			}
		}

		// Colors after the first stop.
		for (int32_t i = idx; i <= SKB_GRADIENT_CACHE_SIZE; i++)
			cache->colors[i] = color;
	}
}

void skb_canvas_fill_solid_color(skb_canvas_t* c, skb_color_t color)
{
	// Fill mask
	skb_canvas_fill_mask(c);

	assert(c->layers_count > 0);
	assert(c->masks_count > 0);
	skb_image_layer_t* layer = &c->layers[c->layers_count - 1];
	skb_mask_t* mask = &c->masks[c->masks_count - 1];
	const int32_t layer_stride = layer->stride;
	const int32_t mask_stride = mask->stride;
	const int32_t fill_w = mask->region.width;

	for (int32_t y = mask->region.y; y < (mask->region.y + mask->region.height); y++) {

		const uint8_t* x_mask = mask->buffer + mask->region.x + (y * mask_stride);
		skb_color_t* x_layer = layer->buffer + mask->region.x + (y * layer_stride);

		for (int32_t x = 0; x < fill_w; x++) {
			if (*x_mask > 0)
				*x_layer = skb_color_lerp(*x_layer, color, *x_mask);
			x_mask++;
			x_layer++;
		}
	}
}

static inline int32_t skb__apply_spread(int32_t index, skb_gradient_spread_t spread)
{
	if (spread == SKB_SPREAD_PAD)
		return skb_clampi(index, 0, SKB_GRADIENT_CACHE_SIZE);
	if (spread == SKB_SPREAD_REPEAT)
		// Sawtooth wave: mod(index, period)
		return (index & (SKB_GRADIENT_CACHE_SIZE-1));
	if (spread == SKB_SPREAD_REFLECT)
		// Triangle wave: period/2 - abs(mod(index, period) - period/2)
		return SKB_GRADIENT_CACHE_SIZE - skb_absi((index & (SKB_GRADIENT_CACHE_SIZE*2-1)) - SKB_GRADIENT_CACHE_SIZE);
	return skb_clampi(index, 0, SKB_GRADIENT_CACHE_SIZE);
}

void skb_canvas_fill_linear_gradient(skb_canvas_t* c, skb_vec2_t p0, skb_vec2_t p1, skb_gradient_spread_t spread, const skb_color_stop_t* stops, int32_t stops_count)
{
	// Fill mask
	skb_canvas_fill_mask(c);

	const skb_mat2_t inv = skb_mat2_inverse(c->transform_stack[c->transform_stack_count-1]);

	skb_gradient_cache_t cache;
	skb_gradient_cache_init_(&cache, stops, stops_count);

	assert(c->layers_count > 0);
	assert(c->masks_count > 0);
	skb_image_layer_t* layer = &c->layers[c->layers_count - 1];
	skb_mask_t* mask = &c->masks[c->masks_count - 1];
	const int32_t layer_stride = layer->stride;
	const int32_t mask_stride = mask->stride;
	const int32_t fill_w = mask->region.width;

	const float dir_x = p1.x - p0.x;
	const float dir_y = p1.y - p0.y;
	const float dir_d = dir_x*dir_x + dir_y*dir_y;
	const float dir_s = dir_d > 0.f ? 1.f / dir_d : 1.f;

	for (int32_t y = mask->region.y; y < (mask->region.y + mask->region.height); y++) {
		// Calculate gradient space location at the beginning of the line.
		const float px = (float)mask->region.x + 0.5f;
		const float py = (float)y + 0.5f;
		float fx = px * inv.xx + py * inv.xy + inv.dx;
		float fy = px * inv.yx + py * inv.yy + inv.dy;
		// Offset to gradient start position.
		fx -= p0.x;
		fy -= p0.y;

		const uint8_t* x_mask = mask->buffer + mask->region.x + (y * mask_stride);
		skb_color_t* x_layer = layer->buffer + mask->region.x + (y * layer_stride);

		for (int32_t x = 0; x < fill_w; x++) {
			if (*x_mask > 0) {
				// Calculate gradient position
				const float t = (dir_x * fx + dir_y * fy) * dir_s;
				// Sample color
				const int32_t ti = skb__apply_spread((int32_t)(t * (SKB_GRADIENT_CACHE_SIZE)), spread);
				const skb_color_t col = cache.colors[ti];
				// Blend
				*x_layer = skb_color_lerp(*x_layer, col, *x_mask);
			}
			x_mask++;
			x_layer++;
			// Update gradient space position.
			fx += inv.xx;
			fy += inv.yx;
		}
	}
}

void skb_canvas_fill_radial_gradient(skb_canvas_t* c, skb_vec2_t p0, float r0, skb_vec2_t p1, float r1, skb_gradient_spread_t spread, const skb_color_stop_t* stops, int32_t stops_count)
{
	SKB_UNUSED(p0);

	static const float eps = 0.25f;

	if (!stops_count)
		return;

	if (r1 < eps) {
		skb_canvas_fill_solid_color(c, stops[stops_count-1].color);
		return;
	}

	// Fill mask
	skb_canvas_fill_mask(c);

	// This implementation only supports the simple radial case of the radial gradient.

	const skb_mat2_t inv = skb_mat2_inverse(c->transform_stack[c->transform_stack_count-1]);

	// @TODO: handle the clamping better.
	// Clamp r0 to be inside the r1.
	r0 = skb_clampf(r0, 0.f, r1 - eps);
	const float r_scale = 1.f / (r1 - r0);

	skb_gradient_cache_t cache;
	skb_gradient_cache_init_(&cache, stops, stops_count);

	assert(c->layers_count > 0);
	assert(c->masks_count > 0);
	skb_image_layer_t* layer = &c->layers[c->layers_count - 1];
	skb_mask_t* mask = &c->masks[c->masks_count - 1];
	const int32_t layer_stride = layer->stride;
	const int32_t mask_stride = mask->stride;
	const int32_t fill_w = mask->region.width;

	for (int32_t y = mask->region.y; y < (mask->region.y + mask->region.height); y++) {
		// Calculate gradient space location at the beginning of the line.
		const float px = (float)mask->region.x + 0.5f;
		const float py = (float)y + 0.5f;
		float fx = px * inv.xx + py * inv.xy + inv.dx;
		float fy = px * inv.yx + py * inv.yy + inv.dy;
		// Offset to gradient center.
		fx -= p0.x;
		fy -= p0.y;

		const uint8_t* x_mask = mask->buffer + mask->region.x + (y * mask_stride);
		skb_color_t* x_layer = layer->buffer + mask->region.x + (y * layer_stride);

		for (int32_t x = 0; x < fill_w; x++) {
			if (*x_mask > 0) {
				// Calculate gradient position.
				const float t = (sqrtf(fx*fx + fy*fy) - r0) * r_scale;
				// Sample gradient.
				const int32_t ti = skb__apply_spread((int32_t)(t * (SKB_GRADIENT_CACHE_SIZE)), spread);
				const skb_color_t col = cache.colors[ti];
				// Blend
				*x_layer = skb_color_lerp(*x_layer, col, *x_mask);
			}
			x_mask++;
			x_layer++;
			// Update gradient space position.
			fx += inv.xx;
			fy += inv.yx;
		}
	}
}

skb_canvas_t* skb_canvas_create(skb_temp_alloc_t* alloc, skb_image_t* target)
{
	assert(alloc);
	assert(target->width > 0 && target->height > 0);
	assert(target->bpp == 1 || target->bpp == 4);

	skb_temp_alloc_mark_t mark = skb_temp_alloc_save(alloc);

	skb_canvas_t* c = SKB_TEMP_ALLOC(alloc, skb_canvas_t, 1);
	memset(c, 0, sizeof(skb_canvas_t));

	c->alloc = alloc;
	c->mark = mark;

	c->width = target->width;
	c->height = target->height;
	c->bpp = target->bpp;
	c->target = target;

	c->scanline = SKB_TEMP_ALLOC(c->alloc, uint8_t, c->width);

	c->points_bounds = skb_rect2_make_undefined();

	skb_canvas_push_transform(c, skb_mat2_make_identity());

	// Push target layer or mask
	if (target->bpp == 4) {
		SKB_ARRAY_RESERVE(c->layers, c->layers_count+1);
		skb_image_layer_t* layer = &c->layers[c->layers_count++];

		layer->buffer = (skb_color_t*)target->buffer;
		layer->stride = target->stride_bytes / 4;

		// Clear rendering region.
		skb_color_t* dst = layer->buffer;
		for (int32_t y = 0; y < c->height; y++) {
			memset(dst, 0, c->width * 4);
			dst += layer->stride;
		}

		skb_canvas_push_mask(c);

	} else {
		// Render to just mask.
		SKB_ARRAY_RESERVE(c->masks, c->masks_count+1);
		skb_mask_t* mask = &c->masks[c->masks_count++];
		mask->buffer = target->buffer;
		mask->stride = target->stride_bytes;

		// Init mask to full canvas size.
		for (int32_t y = 0; y < c->height; y++)
			memset(mask->buffer + y * mask->stride, 255, c->width * sizeof(uint8_t));
		mask->region.x = 0;
		mask->region.y = 0;
		mask->region.width = c->width;
		mask->region.height = c->height;
	}

	return c;
}

void skb_canvas_destroy(skb_canvas_t* c)
{
	if (!c) return;
	skb_temp_alloc_t* alloc = c->alloc;
	skb_temp_alloc_mark_t mark = c->mark;
	skb_temp_alloc_restore(alloc, mark);
}
