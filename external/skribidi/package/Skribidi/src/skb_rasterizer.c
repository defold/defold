// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "skb_rasterizer.h"

#include "skb_common.h"
#include "skb_canvas.h"
#include "skb_font_collection.h"
#include "skb_font_collection_internal.h"
#include "skb_icon_collection.h"
#include "skb_icon_collection_internal.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "hb.h"

typedef struct skb_rasterizer_t {
	hb_paint_funcs_t* paint_funcs;
	hb_draw_funcs_t* draw_funcs;
	skb_rasterizer_config_t config;
} skb_rasterizer_t;


static void skb__hb_move_to(hb_draw_funcs_t* dfuncs, void* draw_data, hb_draw_state_t* st, float to_x, float to_y, void* user_data);
static void skb__hb_line_to(hb_draw_funcs_t* dfuncs, void* draw_data, hb_draw_state_t* st, float to_x, float to_y, void* user_data);
static void skb__hb_cubic_to(hb_draw_funcs_t* dfuncs, void* draw_data, hb_draw_state_t* st, float control1_x, float control1_y, float control2_x, float control2_y, float to_x, float to_y, void* user_data);
static void skb__hb_close_path(hb_draw_funcs_t* dfuncs, void* draw_data, hb_draw_state_t* st, void* user_data);

static void skb__hb_push_transform(hb_paint_funcs_t* pfuncs, void* paint_data, float xx, float yx, float xy, float yy, float dx, float dy, void* user_data);
static void skb__hb_pop_transform(hb_paint_funcs_t* pfuncs, void* paint_data, void* user_data);
static void skb__hb_push_clip_glyph(hb_paint_funcs_t* pfuncs, void* paint_data, hb_codepoint_t glyph, hb_font_t* font, void* user_data);
static void skb__hb_push_clip_rectangle(hb_paint_funcs_t* pfuncs, void* paint_data, float xmin, float ymin, float xmax, float ymax, void* user_data);
static void skb__hb_pop_clip(hb_paint_funcs_t* pfuncs, void* paint_data, void* user_data);
static void skb__hb_push_group(hb_paint_funcs_t* pfuncs, void* paint_data, void* user_data);
static void skb__hb_pop_group(hb_paint_funcs_t* pfuncs, void* paint_data, hb_paint_composite_mode_t mode, void* user_data);
static void skb__hb_paint_color(hb_paint_funcs_t* pfuncs, void* paint_data, hb_bool_t use_foreground, hb_color_t color, void* user_data);
static hb_bool_t skb__hb_paint_image(hb_paint_funcs_t* pfuncs, void* paint_data, hb_blob_t* blob, unsigned width, unsigned height, hb_tag_t format, float slant, hb_glyph_extents_t* extents, void* user_data);
static void skb__hb_paint_linear_gradient(hb_paint_funcs_t* pfuncs, void* paint_data, hb_color_line_t* color_line, float x0, float y0, float x1, float y1, float x2, float y2, void* user_data);
static void skb__hb_paint_radial_gradient(hb_paint_funcs_t* pfuncs, void* paint_data, hb_color_line_t* color_line, float x0, float y0, float r0, float x1, float y1, float r1, void* user_data);
static void skb__hb_paint_sweep_gradient(hb_paint_funcs_t* pfuncs, void* paint_data, hb_color_line_t* color_line, float x0, float y0, float start_angle, float end_angle, void *user_data);

skb_rasterizer_t* skb_rasterizer_create(skb_rasterizer_config_t* config)
{
	skb_rasterizer_t* rasterizer = skb_malloc(sizeof(skb_rasterizer_t));
	memset(rasterizer, 0, sizeof(skb_rasterizer_t));

	rasterizer->draw_funcs = hb_draw_funcs_create ();
	hb_draw_funcs_set_move_to_func (rasterizer->draw_funcs, skb__hb_move_to, rasterizer, NULL);
	hb_draw_funcs_set_line_to_func (rasterizer->draw_funcs, skb__hb_line_to, rasterizer, NULL);
	hb_draw_funcs_set_cubic_to_func (rasterizer->draw_funcs, skb__hb_cubic_to, rasterizer, NULL);
	hb_draw_funcs_set_close_path_func (rasterizer->draw_funcs, skb__hb_close_path, rasterizer, NULL);
	hb_draw_funcs_make_immutable (rasterizer->draw_funcs);

	rasterizer->paint_funcs = hb_paint_funcs_create ();
	hb_paint_funcs_set_push_transform_func(rasterizer->paint_funcs, skb__hb_push_transform, rasterizer, NULL);
	hb_paint_funcs_set_pop_transform_func(rasterizer->paint_funcs, skb__hb_pop_transform, rasterizer, NULL);
	hb_paint_funcs_set_push_clip_glyph_func(rasterizer->paint_funcs, skb__hb_push_clip_glyph, rasterizer, NULL);
	hb_paint_funcs_set_push_clip_rectangle_func(rasterizer->paint_funcs, skb__hb_push_clip_rectangle, rasterizer, NULL);
	hb_paint_funcs_set_pop_clip_func(rasterizer->paint_funcs, skb__hb_pop_clip, rasterizer, NULL);
	hb_paint_funcs_set_push_group_func(rasterizer->paint_funcs, skb__hb_push_group, rasterizer, NULL);
	hb_paint_funcs_set_pop_group_func(rasterizer->paint_funcs, skb__hb_pop_group, rasterizer, NULL);
	hb_paint_funcs_set_color_func(rasterizer->paint_funcs, skb__hb_paint_color, rasterizer, NULL);
	hb_paint_funcs_set_image_func(rasterizer->paint_funcs, skb__hb_paint_image, rasterizer, NULL);
	hb_paint_funcs_set_linear_gradient_func(rasterizer->paint_funcs, skb__hb_paint_linear_gradient, rasterizer, NULL);
	hb_paint_funcs_set_radial_gradient_func(rasterizer->paint_funcs, skb__hb_paint_radial_gradient, rasterizer, NULL);
	hb_paint_funcs_set_sweep_gradient_func(rasterizer->paint_funcs, skb__hb_paint_sweep_gradient, rasterizer, NULL);
	hb_paint_funcs_make_immutable(rasterizer->paint_funcs);

	rasterizer->config = config ? *config : skb_rasterizer_get_default_config();

	return rasterizer;
}

skb_rasterizer_config_t skb_rasterizer_get_default_config(void)
{
	return (skb_rasterizer_config_t) {
		.on_edge_value = 128,
		.pixel_dist_scale = 32.f,
	};
}

skb_rasterizer_config_t skb_rasterizer_get_config(const skb_rasterizer_t* rasterizer)
{
	return rasterizer->config;
}

void skb_rasterizer_destroy(skb_rasterizer_t* rasterizer)
{
	if (!rasterizer) return;
	hb_draw_funcs_destroy(rasterizer->draw_funcs);
	hb_paint_funcs_destroy(rasterizer->paint_funcs);

	memset(rasterizer, 0, sizeof(skb_rasterizer_t));

	skb_free(rasterizer);
}


//
// SDF
//

#define SKB_SQRT2 1.4142136f	// sqrt(2)

static float skb__edgedf(float gx, float gy, float a)
{
	float df = 0.f;
	if ((gx == 0.f) || (gy == 0.f)) {
		// Either A) gu or gv are zero, or B) both
		// Linear approximation is A) correct or B) a fair guess
		df = 0.5f - a;
	} else {
		// Everything is symmetric wrt sign and transposition,
		// so move to first octant (gx>=0, gy>=0, gx>=gy) to
		// avoid handling all possible edge directions.
		gx = skb_absf(gx);
		gy = skb_absf(gy);
		if (gx < gy) {
			float temp = gx;
			gx = gy;
			gy = temp;
		}
		float a1 = 0.5f*gy / gx;
		if (a < a1) { // 0 <= a < a1
			df = 0.5f*(gx + gy) - sqrtf(2.0f*gx*gy*a);
		} else if (a < (1.0-a1)) { // a1 <= a <= 1-a1
			df = (0.5f-a)*gx;
		} else { // 1-a1 < a <= 1
			df = -0.5f*(gx + gy) + sqrtf(2.0f*gx*gy*(1.0f-a));
		}
	}
	return df;
}

// This code is useful for testing contour point issues, keeping it around.
//#define DUMP_CONTOUR_SVG
#ifdef DUMP_CONTOUR_SVG
static void dump_svg(skb_image_t* mask, float* dist, skb_vec2_t* contour_pts, float max_dist)
{
	static int num = 1;
	char filename[32];
	snprintf(filename, 32, "sdf_%03d.svg", num++);

	FILE* fp = fopen(filename, "w");

	uint8_t* mask_buffer = mask->buffer;
	const int32_t mask_stride = mask->stride_bytes;

	const int32_t width = mask->width;
	const int32_t height = mask->height;
	const int32_t stride = mask->width;

	fprintf(fp, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"-50 -50 %d %d\">\n", width*10,height*10,width*10+50,height*10+50);

	// Draw grid
	for (int32_t y = 1; y < height-1; y++) {
		fprintf(fp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"grey\" />\n", 0, y*10, width*10, y*10);
	}
	for (int32_t x = 1; x < width-1; x++) {
		fprintf(fp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"grey\" />\n", x*10, 0, x*10, height*10);
	}

	// Draw contour points.
	for (int32_t y = 1; y < height-1; y++) {
		const uint8_t* x_mask = &mask_buffer[y * mask_stride + 1];
		skb_vec2_t* x_contour_pts = &contour_pts[y * stride + 1];
		for (int32_t x = 1; x < width-1; x++) {
			float d = dist[x + y * stride];
			skb_vec2_t cpt = contour_pts[x + y * stride];
			if (d < max_dist) {
				fprintf(fp, "<circle cx=\"%f\" cy=\"%f\" r=\"2\" fill=\"blue\" />\n", cpt.x*10.f, cpt.y*10.f);
			}
		}
	}

	fprintf(fp, "</svg>\n");

	fclose(fp);
}
#endif

void skb__mask_to_sdf(skb_temp_alloc_t* temp_alloc, skb_image_t* mask, uint8_t on_edge_value, float pixel_dist_scale)
{
	assert(temp_alloc);
	assert(mask);

	float* dist = SKB_TEMP_ALLOC(temp_alloc, float, mask->width * mask->height);
	skb_vec2_t* contour_pts = SKB_TEMP_ALLOC(temp_alloc, skb_vec2_t, mask->width * mask->height);

	uint8_t* mask_buffer = mask->buffer;
	const int32_t mask_stride = mask->stride_bytes;

	// Initialize buffers
	const int32_t width = mask->width;
	const int32_t height = mask->height;
	const int32_t stride = mask->width;

	memset(contour_pts, 0, width * height * sizeof(skb_vec2_t));

	const float max_dist = skb_squaref((float)skb_maxi(width, height) * 2.f);
	for (int32_t i = 0; i < width * height; i++)
		dist[i] = max_dist;

	static const float half = 0.5f;

	// Calculate position of the anti-aliased pixels and distance to the boundary of the shape.
	skb_vec2_t c;
	for (int32_t y = 1; y < height-1; y++) {
		c.x = 1.f + half;
		c.y = (float)y + half;

		const uint8_t* x_mask = &mask_buffer[y * mask_stride + 1];
		float* x_dist = &dist[y * stride + 1];
		skb_vec2_t* x_contour_pts = &contour_pts[y * stride + 1];

		for (int32_t x = 1; x < width-1; x++) {

			// Skip flat areas.
			if (*x_mask != 0) {

				//  0 | 1 | 2
				// ---+---+---
				//  3 | 4 | 5
				// ---+---+---
				//  6 | 7 | 8

				const int32_t img1 = x_mask[-mask_stride];
				const int32_t img3 = x_mask[-1];
				const int32_t img4 = x_mask[0];
				const int32_t img5 = x_mask[1];
				const int32_t img7 = x_mask[mask_stride];

				if (*x_mask == 255) {
					// Handle the case where we have sharp edge between 0 and 255 coverage.
					int32_t igx = 0;
					if (img3 == 0) igx--;
					if (img5 == 0) igx++;
					int32_t igy = 0;
					if (img1 == 0) igy--;
					if (img7 == 0) igy++;

					if (igx != 0 || igy != 0) {
						static const float d = 0.5f;
						skb_vec2_t cpt = (skb_vec2_t) { c.x + (float)igx*d, c.y + (float)igy*d };
						*x_contour_pts = cpt;
						*x_dist = skb_vec2_dist_sqr(c, cpt);
					}
				} else {
					const int32_t img0 = x_mask[-mask_stride - 1];
					const int32_t img2 = x_mask[-mask_stride + 1];
					const int32_t img6 = x_mask[mask_stride - 1];
					const int32_t img8 = x_mask[mask_stride + 1];

					const int32_t igx = (img2 + img8 - img0 - img6) * 32 + (img5 - img3) * 45; // 45/32 ~ sqrt(2), gotta be careful with the bits, since we're squaring below.
					const int32_t igy = (img6 + img8 - img0 - img2) * 32 + (img7 - img1) * 45;
					const int32_t ig_len = igx*igx + igy*igy;

					if (ig_len > 0) {
						const float s = 1.0f / sqrtf((float)ig_len);
						const float gx = (float)igx * s;
						const float gy = (float)igy * s;

						// Find nearest point on contour.
						const float d = skb__edgedf(gx, gy, (float)img4 / 255.0f);
						skb_vec2_t cpt = (skb_vec2_t) { c.x + gx*d, c.y + gy*d };

						*x_contour_pts = cpt;
						*x_dist = skb_vec2_dist_sqr(c, cpt);
					}
				}

				// Calculate normalized gradient direction
				/*			float gx = -(float)img0 - SKB_SQRT2*(float)img3 - (float)img6 + (float)img2 + SKB_SQRT2*(float)img5 + (float)img8;
							float gy = -(float)img0 - SKB_SQRT2*(float)img1 - (float)img2 + (float)img6 + SKB_SQRT2*(float)img7 + (float)img8;
							float glen = gx*gx + gy*gy;

							// Skip if the gradient is very weak.
							if (glen < 0.00001f)
								continue;
							const float s = 1.0f / sqrtf(glen);
							gx *= s;
							gy *= s;*/


			}
			x_mask++;
			x_dist++;
			x_contour_pts++;
			c.x += 1.f;
		}
	}

#ifdef DUMP_CONTOUR_SVG
	dump_svg(mask, dist, contour_pts, max_dist);
#endif
	// Calculate dead-reckoning distance transform.

	// Top-left to bottom-right.
	for (int32_t y = 1; y < height-1; y++) {
		c.y = (float)y + half;
		c.x = 1.f + half;

		float* x_dist = &dist[y * stride + 1];
		skb_vec2_t* x_contour_pts = &contour_pts[y * stride + 1];

		for (int32_t x = 1; x < width-1; x++) {
			// (-1,-1)
			const int32_t kn_1_1 = -1 - stride;
//			if (x_dist[kn_1_1] + SDF_SQRT2 <= *x_dist) {
			if (x_dist[kn_1_1] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn_1_1];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}
			// (0,-1)
			const int32_t kn0_1 = -stride;
//			if (x_dist[kn0_1] + 1.f <= *x_dist) {
			if (x_dist[kn0_1] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn0_1];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}
			// (1,-1)
			const int32_t kn1_1 = 1 - stride;
//			if (x_dist[kn1_1] + SKB_SQRT2 <= *x_dist) {
			if (x_dist[kn1_1] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn1_1];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}
			// (-1,0)
			const int32_t kn_10 = -1;
//			if (x_dist[kn_10] + 1.f <= *x_dist) {
			if (x_dist[kn_10] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn_10];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}

			x_dist++;
			x_contour_pts++;
			c.x += 1.f;
		}
	}

	// Bottom-right to top-left.
	for (int32_t y = height-2; y > 0 ; y--) {
		c.y = (float)y + half;
		c.x = (float)(width-2) + half;

		float* x_dist = &dist[y * stride + width-2];
		skb_vec2_t* x_contour_pts = &contour_pts[y * stride + width-2];

		for (int32_t x = width-2; x > 0; x--) {
			// (1,0)
			const int32_t kn10 = 1;
//			if (x_dist[kn10] + 1.f <= *x_dist) {
			if (x_dist[kn10] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn10];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}
			// (-1,1)
			const int32_t kn_11 = -1 + stride;
//			if (x_dist[kn_11] + SKB_SQRT2 <= *x_dist) {
			if (x_dist[kn_11] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn_11];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}
			// (0,1)
			const int32_t kn01 = stride;
//			if (x_dist[kn01] + 1.f <= *x_dist) {
			if (x_dist[kn01] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn01];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}
			// (1,1)
			const int32_t kn11 = 1 + stride;
//			if (x_dist[kn11] + SDF_SQRT2 <= *x_dist) {
			if (x_dist[kn11] < *x_dist) {
				const skb_vec2_t cpt = x_contour_pts[kn11];
				const float d = skb_vec2_dist_sqr(c, cpt);
				if (d < *x_dist) {
					*x_contour_pts = cpt;
					*x_dist = d;
				}
			}

			x_dist--;
			x_contour_pts--;
			c.x -= 1.f;
		}
	}

	// Cheap approximation for the boundary
	const int32_t k_top = 0;
	for (int32_t x = 1; x < width-1; x++)
		dist[k_top + x] = skb_squaref(sqrtf(dist[k_top + x + stride]) + 1.f);

	const int32_t k_bot = (height-1) * stride;
	for (int32_t x = 1; x < width-1; x++)
		dist[k_bot+x] = skb_squaref(sqrtf(dist[k_bot + x - stride]) + 1.f);

	const int32_t k_left = 0;
	for (int32_t y = 1; y < height-1; y++)
		dist[k_left + y*width] = skb_squaref(sqrtf(dist[k_left + y*stride + 1]) + 1.f);

	const int32_t k_right = width-1;
	for (int32_t y = 1; y < height-1; y++)
		dist[k_right + y*width] = skb_squaref(sqrtf(dist[k_right + y*stride - 1]) + 1.f);

	dist[k_top + k_left] = skb_squaref(sqrtf(dist[k_top + k_left + 1 + stride]) + SKB_SQRT2);
	dist[k_top + k_right] = skb_squaref(sqrtf(dist[k_top + k_right - 1 + stride]) + SKB_SQRT2);
	dist[k_bot + k_left] = skb_squaref(sqrtf(dist[k_bot + k_left + 1 - stride]) + SKB_SQRT2);
	dist[k_bot + k_right] = skb_squaref(sqrtf(dist[k_bot + k_right - 1 - stride]) + SKB_SQRT2);

	// Adjust distance sign and map to requested output range.
	for (int32_t y = 0; y < height; y++) {

		const float* x_dist = &dist[y * stride];
		uint8_t* x_mask = &mask_buffer[y * mask_stride];

		for (int32_t x = width-1; x >= 0; x--) {
			float d = sqrtf(*x_dist);
			if (*x_mask > 127) d = -d;
			const float val = (float)on_edge_value + d * pixel_dist_scale;
			*x_mask = (uint8_t)skb_clampf(val, 0.f, 255.f);
			x_dist++;
			x_mask++;
		}
	}

	SKB_TEMP_FREE(temp_alloc, contour_pts);
	SKB_TEMP_FREE(temp_alloc, dist);
}

static void skb__unpremultiply_and_dilate(skb_temp_alloc_t* temp_alloc, skb_image_t* image)
{
	assert(temp_alloc);
	assert(image);
	assert(image->bpp == 4);

	uint16_t* dist = SKB_TEMP_ALLOC(temp_alloc, uint16_t, image->width * image->height);
	int32_t dist_stride = image->width;

	int32_t image_stride = image->stride_bytes / 4;
	skb_color_t* image_buffer = (skb_color_t*)image->buffer;

	// Un-premultiply and initialize distance field.
	for (int32_t y = 0; y < image->height; y++) {
		skb_color_t* x_image = &image_buffer[y * image_stride];
		uint16_t* x_dist = &dist[y * dist_stride];
		for (int32_t x = 0; x < image->width; x++) {
			const skb_color_t c = *x_image;
			if (c.a > 128) {
				skb_color_t res = {0};
				res.r = (uint8_t)((int32_t)c.r * 255 / (int32_t)c.a);
				res.g = (uint8_t)((int32_t)c.g * 255 / (int32_t)c.a);
				res.b = (uint8_t)((int32_t)c.b * 255 / (int32_t)c.a);
				res.a = 255;
				*x_image = res;
				*x_dist = 0;
			} else {
				*x_image = (skb_color_t){0};
				*x_dist = 0xffff;
			}
			x_dist++;
			x_image++;
		}
	}

	for (int32_t y = 1; y < image->height; y++) {
		skb_color_t* x_image = &image_buffer[1 + y * image_stride];
		uint16_t* x_dist = &dist[1 + y * dist_stride];
		for (int32_t x = 1; x < image->width; x++) {
			uint32_t cur_d = *x_dist;
			if (cur_d != 0) {
				const uint32_t d1 = (uint32_t)x_dist[-dist_stride-1] + 3;
				if (d1 < cur_d) {
					*x_image = x_image[-image_stride-1];
					*x_dist = (uint16_t)d1;
					cur_d = d1;
				}
				const uint32_t d2 = (uint32_t)x_dist[-dist_stride] + 2;
				if (d2 < cur_d) {
					*x_image = x_image[-image_stride];
					*x_dist = (uint16_t)d2;
					cur_d = d2;
				}
				const uint32_t d3 = (uint32_t)x_dist[-1] + 2;
				if (d3 < cur_d) {
					*x_image = x_image[-1];
					*x_dist = (uint16_t)d3;
				}
			}

			x_image++;
			x_dist++;
		}
	}

	for (int32_t y = image->height-2; y >= 0; y--) {
		skb_color_t* x_image = &image_buffer[image->width-2 + y * image_stride];
		uint16_t* x_dist = &dist[image->width-2 + y * dist_stride];
		for (int32_t x = image->width-2; x >= 0; x--) {
			uint32_t cur_d = *x_dist;
			if (cur_d != 0) {
				const uint32_t d1 = (uint32_t)x_dist[dist_stride+1] + 3;
				if (d1 < cur_d) {
					*x_image = x_image[image_stride+1];
					*x_dist = (uint16_t)d1;
					cur_d = d1;
				}
				const uint32_t d2 = (uint32_t)x_dist[dist_stride] + 2;
				if (d2 < cur_d) {
					*x_image = x_image[image_stride];
					*x_dist = (uint16_t)d2;
					cur_d = d2;
				}
				const uint32_t d3 = (uint32_t)x_dist[1] + 2;
				if (d3 < cur_d) {
					*x_image = x_image[1];
					*x_dist = (uint16_t)d3;
				}
			}
			x_image--;
			x_dist--;
		}
	}

	// Handle boundary
	const int32_t k_top = 0;
	for (int32_t x = 1; x < image->width-1; x++)
		image_buffer[k_top + x] = image_buffer[k_top + x + image_stride];

	const int32_t k_bot = (image->height-1) * image_stride;
	for (int32_t x = 1; x < image->width-1; x++)
		image_buffer[k_bot+x] = image_buffer[k_bot + x - image_stride];

	const int32_t k_left = 0;
	for (int32_t y = 1; y < image->height-1; y++)
		image_buffer[k_left + y*image_stride] = image_buffer[k_left + y*image_stride + 1];

	const int32_t k_right = image->width-1;
	for (int32_t y = 1; y < image->height-1; y++)
		image_buffer[k_right + y*image_stride] = image_buffer[k_right + y*image_stride - 1];

	image_buffer[k_top + k_left] = image_buffer[k_top + k_left + 1 + image_stride];
	image_buffer[k_top + k_right] = image_buffer[k_top + k_right - 1 + image_stride];
	image_buffer[k_bot + k_left] = image_buffer[k_bot + k_left + 1 - image_stride];
	image_buffer[k_bot + k_right] = image_buffer[k_bot + k_right - 1 - image_stride];

	SKB_TEMP_FREE(temp_alloc, dist);
}



//
// Font
//

static void skb__hb_move_to(
	hb_draw_funcs_t* dfuncs,
	void* draw_data,
	hb_draw_state_t* st,
	float to_x, float to_y,
	void* user_data)
{
	SKB_UNUSED(dfuncs);
	SKB_UNUSED(st);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)draw_data;

	skb_canvas_move_to(c, skb_vec2_make(to_x, to_y));
}

static void skb__hb_line_to (
	hb_draw_funcs_t* dfuncs,
	void* draw_data,
	hb_draw_state_t* st,
	float to_x, float to_y,
	void* user_data)
{
	SKB_UNUSED(dfuncs);
	SKB_UNUSED(st);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)draw_data;

	skb_canvas_line_to(c, skb_vec2_make(to_x, to_y));
}

static void skb__hb_cubic_to (
	hb_draw_funcs_t* dfuncs,
	void* draw_data,
	hb_draw_state_t* st,
	float control1_x, float control1_y,
	float control2_x, float control2_y,
	float to_x, float to_y,
	void* user_data)
{
	SKB_UNUSED(dfuncs);
	SKB_UNUSED(st);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)draw_data;

	skb_canvas_cubic_to(c, skb_vec2_make(control1_x, control1_y), skb_vec2_make(control2_x, control2_y), skb_vec2_make(to_x, to_y));
}

static void skb__hb_close_path (
	hb_draw_funcs_t* dfuncs,
	void* draw_data,
	hb_draw_state_t* st,
	void* user_data)
{
	SKB_UNUSED(dfuncs);
	SKB_UNUSED(st);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)draw_data;

	skb_canvas_close(c);
}

static void skb__hb_push_transform (
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	float xx, float yx,
	float xy, float yy,
	float dx, float dy,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_mat2_t t = {
		.xx = xx, .yx = yx,
		.xy = xy, .yy = yy,
		.dx = dx, .dy = dy,
	};
	skb_canvas_push_transform(c, t);
}

static void skb__hb_pop_transform (
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_canvas_pop_transform(c);
}

static void skb__hb_push_clip_glyph (
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	hb_codepoint_t glyph,
	hb_font_t* font,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;
	skb_rasterizer_t* rasterizer = (skb_rasterizer_t*)user_data;

	skb_canvas_push_mask(c);

	hb_font_draw_glyph(font, glyph, rasterizer->draw_funcs, c);
	skb_canvas_fill_mask(c);
}

static void skb__hb_push_clip_rectangle (
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	float xmin, float ymin, float xmax, float ymax,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_canvas_push_mask(c);

	skb_canvas_move_to(c, skb_vec2_make(xmin, ymin));
	skb_canvas_line_to(c, skb_vec2_make(xmax, ymin));
	skb_canvas_line_to(c, skb_vec2_make(xmax, ymax));
	skb_canvas_line_to(c, skb_vec2_make(xmin, ymax));
	skb_canvas_close(c);

	skb_canvas_fill_mask(c);
}

static void skb__hb_pop_clip (
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_canvas_pop_mask(c);
}

static void skb__hb_push_group (
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_canvas_push_layer(c);
}

static void skb__hb_pop_group (
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	hb_paint_composite_mode_t mode,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	if (mode != HB_PAINT_COMPOSITE_MODE_SRC_OVER) {
		skb_debug_log("Unsupported blend mode: %d\n", mode);
	}

	skb_canvas_pop_layer(c);
}

#define GRAST_MAX_COLOR_STOPS 64

static int skb__hb_cmp_color_stop(const void *p1, const void *p2)
{
	const hb_color_stop_t *c1 = (const hb_color_stop_t *)p1;
	const hb_color_stop_t *c2 = (const hb_color_stop_t *)p2;
	if (c1->offset < c2->offset)
		return -1;
	if (c1->offset > c2->offset)
		return 1;
	return 0;
}

static void skb__hb_prepare_color_stops(hb_color_line_t* color_line, skb_color_stop_t* stops, int32_t* stops_count, float* offset_min, float* offset_max)
{
	hb_color_stop_t hb_stops[GRAST_MAX_COLOR_STOPS];
	uint32_t hb_stops_count = GRAST_MAX_COLOR_STOPS;
	hb_color_line_get_color_stops(color_line, 0, &hb_stops_count, hb_stops);

	if (hb_stops_count == 0) {
		*stops_count = 0;
		return;
	}

	qsort(hb_stops, hb_stops_count, sizeof(hb_color_stop_t), skb__hb_cmp_color_stop);

	float omin = hb_stops[0].offset;
	float omax = hb_stops[0].offset;
	for (uint32_t i = 1; i < hb_stops_count; i++) {
		omin = skb_minf(omin, hb_stops[i].offset);
		omax = skb_maxf(omax, hb_stops[i].offset);
	}
	float orange = omax - omin;
	float oscale = orange > 0.f ? 1.f / orange : 0.f;

	*offset_min = omin;
	*offset_max = omax;

	for (uint32_t i = 0; i < hb_stops_count; i++) {
		stops[i].offset = (hb_stops[i].offset - omin) * oscale;
		 skb_color_t col = {
			.r = hb_color_get_red(hb_stops[i].color),
			.g = hb_color_get_green(hb_stops[i].color),
			.b = hb_color_get_blue(hb_stops[i].color),
			.a = hb_color_get_alpha(hb_stops[i].color),
		};
		stops[i].color = skb_color_premult(col);
	}
	*stops_count = (int32_t)hb_stops_count;
}

static void skb__hb_paint_color(
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	hb_bool_t use_foreground,
	hb_color_t color,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(use_foreground);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_color_t col = {
		.r = hb_color_get_red(color),
		.g = hb_color_get_green(color),
		.b = hb_color_get_blue(color),
		.a = hb_color_get_alpha(color),
	};

	skb_canvas_fill_solid_color(c, skb_color_premult(col));
}

static hb_bool_t skb__hb_paint_image(
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	hb_blob_t* blob,
	unsigned width,
	unsigned height,
	hb_tag_t format,
	float slant,
	hb_glyph_extents_t* extents,
	void* user_data)
{
	SKB_UNUSED(paint_data);
	SKB_UNUSED(blob);
	SKB_UNUSED(width);
	SKB_UNUSED(height);
	SKB_UNUSED(format);
	SKB_UNUSED(slant);
	SKB_UNUSED(extents);
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

//	skb_canvas_t* c = (skb_canvas_t*)paint_data;
	// Not supporting image for the time being.
	return false;
}

static void skb__hb_paint_linear_gradient(
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	hb_color_line_t* color_line,
	float x0, float y0,
	float x1, float y1,
	float x2, float y2,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_color_stop_t stops[GRAST_MAX_COLOR_STOPS] = {0};
	int32_t stops_count = 0;
	float offset_min = 0.f;
	float offset_max = 0.f;
	skb__hb_prepare_color_stops (color_line, stops, &stops_count, &offset_min, &offset_max);
	if (stops_count == 0)
		return;

	// Normalize gradient and convert to 2 point gradient.
	skb_vec2_t orig = skb_vec2_make(x0, y0);
	skb_vec2_t target = {0};

	const skb_vec2_t q2 = { x2 - x0, y2 - y0 };
	const skb_vec2_t q1 = { x1 - x0, y1 - y0 };
	const float s = skb_vec2_dot(q2, q2);
	if (s < 0.000001f) {
		target = skb_vec2_make(x1, y1);
	} else {
		const float k = (q2.x * q1.x + q2.y * q1.y) / s;
		target.x = x1 - k * q2.x;
		target.y = y1 - k * q2.y;
	}

	skb_vec2_t delta = skb_vec2_sub(target, orig);
	skb_vec2_t p0 = skb_vec2_mad(orig, delta, offset_min);
	skb_vec2_t p1 = skb_vec2_mad(orig, delta, offset_max);

	skb_canvas_fill_linear_gradient(c, p0, p1, SKB_SPREAD_PAD, stops, stops_count);
}

static void skb__hb_paint_radial_gradient(
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	hb_color_line_t* color_line,
	float x0, float y0, float r0,
	float x1, float y1, float r1,
	void* user_data)
{
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	skb_canvas_t* c = (skb_canvas_t*)paint_data;

	skb_color_stop_t stops[GRAST_MAX_COLOR_STOPS] = {0};
	int32_t stops_count = 0;
	float offset_min = 0.f;
	float offset_max = 0.f;
	skb__hb_prepare_color_stops (color_line, stops, &stops_count, &offset_min, &offset_max);
	if (stops_count == 0)
		return;

	// Normalize gradient
	const skb_vec2_t orig = { x0, y0 };
	const skb_vec2_t target = { x1, y1 };
	const skb_vec2_t delta = skb_vec2_sub(target, orig);

	const float orig_r = r0;
	const float delta_r = r1 - r0;

	const skb_vec2_t p0 = skb_vec2_mad(orig, delta, offset_min);
	r0 = orig_r + delta_r * offset_min;

	const skb_vec2_t p1 = skb_vec2_mad(orig, delta, offset_max);
	r1 = orig_r + delta_r * offset_max;

	skb_canvas_fill_radial_gradient(c, p0, r0, p1, r1, SKB_SPREAD_PAD, stops, stops_count);
}

static void skb__hb_paint_sweep_gradient(
	hb_paint_funcs_t* pfuncs,
	void* paint_data,
	hb_color_line_t* color_line,
	float x0, float y0,
	float start_angle, float end_angle,
	void *user_data)
{
	SKB_UNUSED(paint_data);
	SKB_UNUSED(color_line);
	SKB_UNUSED(x0);
	SKB_UNUSED(y0);
	SKB_UNUSED(start_angle);
	SKB_UNUSED(end_angle);
	SKB_UNUSED(pfuncs);
	SKB_UNUSED(user_data);

	// TODO
	skb_debug_log("Unsupported paint_sweep_gradient\n");
}


skb_rect2i_t skb_rasterizer_get_glyph_dimensions(uint32_t glyph_id, const skb_font_t* font, float font_size, int32_t padding)
{
	// Calc size
	hb_glyph_extents_t extents;
	hb_font_get_glyph_extents(font->hb_font, glyph_id, &extents);

	const float scale = font_size * font->upem_scale;

	const int32_t x = (int32_t)floorf((float)extents.x_bearing * scale);
	const int32_t y = (int32_t)floorf(-(float)extents.y_bearing * scale);
	const int32_t width = (int32_t)ceilf((float)(extents.x_bearing + extents.width) * scale) - x;
	const int32_t height = (int32_t)ceilf(-(float)(extents.y_bearing + extents.height) * scale) - y;

	// Do not pad empty or degenerate rectangles.
	if (width == 0 || height == 0)
		return (skb_rect2i_t) { .x = x, .y = y, .width = width, .height = height };

	return (skb_rect2i_t) {
		.x = x - padding,
		.y = y - padding,
		.width = width + padding * 2,
		.height = height + padding * 2,
	};
}


bool skb_rasterizer_draw_alpha_glyph(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	uint32_t glyph_id, const skb_font_t* font, float font_size, skb_rasterize_alpha_mode_t alpha_mode,
	float offset_x, float offset_y, skb_image_t* target)
{
	assert(rasterizer);
	assert(temp_alloc);
	assert(target);

	if (!target->buffer || target->width <= 0 || target->height <= 0 || target->bpp != 1)
		return false;

	int64_t t_start = skb_perf_timer_get();

	skb_canvas_t* canvas = skb_canvas_create(temp_alloc, target);

	// Create transform to convert from the font coordinates to the canvas.
	const float scale = font_size * font->upem_scale;

	const skb_mat2_t scale_xfrorm = skb_mat2_make_scale(scale, -scale);
	const skb_mat2_t trans_xform = skb_mat2_make_translation(-offset_x, -offset_y);
	const skb_mat2_t xform = skb_mat2_multiply(scale_xfrorm, trans_xform);
	skb_canvas_push_transform(canvas, xform);

	hb_font_draw_glyph(font->hb_font, glyph_id, rasterizer->draw_funcs, canvas);

	skb_canvas_fill_mask(canvas);

	if (alpha_mode == SKB_RASTERIZE_ALPHA_SDF) {
		// SDF
		skb__mask_to_sdf(temp_alloc, target, rasterizer->config.on_edge_value, rasterizer->config.pixel_dist_scale);
	}

	skb_canvas_destroy(canvas);

	int64_t t_end = skb_perf_timer_get();
	int64_t elapsed_us = skb_perf_timer_elapsed_us(t_start, t_end);

	// skb_debug_log("Rasterize %s glyph %s [%d], size: %.1f, canvas: (%d x %d), time: %dus\n", alpha_mode == SKB_RENDER_ALPHA_SDF ? "SDF" : "mask", font->name, glyph_id, font_size, target->width, target->height, (int32_t)elapsed_us);

	return true;
}

bool skb_rasterizer_draw_color_glyph(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	uint32_t glyph_id, const skb_font_t* font, float font_size, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target)
{
	assert(rasterizer);
	assert(temp_alloc);

	if (!target->buffer || target->width <= 0 || target->height <= 0 || target->bpp != 4)
		return false;

	int64_t t_start = skb_perf_timer_get();

	skb_canvas_t* canvas = skb_canvas_create(temp_alloc, target);

	// Create transform to convert from the font coordinates to the canvas.
	const float scale = font_size * font->upem_scale;

	const skb_mat2_t scale_xfrorm = skb_mat2_make_scale(scale, -scale);
	const skb_mat2_t trans_xform = skb_mat2_make_translation((float)-offset_x, (float)-offset_y);
	const skb_mat2_t xform = skb_mat2_multiply(scale_xfrorm, trans_xform);
	skb_canvas_push_transform(canvas, xform);

	hb_font_paint_glyph(font->hb_font, glyph_id, rasterizer->paint_funcs, canvas, 0, HB_COLOR(255,255,255,255)); // BGRA

	if (alpha_mode == SKB_RASTERIZE_ALPHA_SDF) {
		// SDF
		uint8_t* mask_buffer = SKB_TEMP_ALLOC(temp_alloc, uint8_t, target->width * target->height);
		const int32_t mask_stride = target->width;

		skb_image_t mask = {
			.buffer = mask_buffer,
			.width = target->width,
			.height = target->height,
			.stride_bytes = target->width,
			.bpp = 1,
		};

		// Copy alpha mask.
		for (int32_t y = 0; y < target->height; y++) {
			uint8_t* x_alpha = target->buffer + y * target->stride_bytes + 3; // Alpha is 3rd component
			uint8_t* x_mask = mask_buffer + y * mask_stride;
			for (int32_t x = 0; x < target->width; x++) {
				*x_mask = *x_alpha;
				x_mask++;
				x_alpha += 4;
			}
		}

		skb__mask_to_sdf(temp_alloc, &mask, rasterizer->config.on_edge_value, rasterizer->config.pixel_dist_scale);
		skb__unpremultiply_and_dilate(temp_alloc, target);

		// Copy SDF to alpha channel
		for (int32_t y = 0; y < target->height; y++) {
			uint8_t* x_alpha = target->buffer + y * target->stride_bytes + 3; // Alpha is 3rd component
			uint8_t* x_mask = mask_buffer + y * mask_stride;
			for (int32_t x = 0; x < target->width; x++) {
				*x_alpha = *x_mask;
				x_mask++;
				x_alpha += 4;
			}
		}

		SKB_TEMP_FREE(temp_alloc, mask_buffer);
	}

	skb_canvas_destroy(canvas);

	int64_t t_end = skb_perf_timer_get();
    int64_t elapsed_us = skb_perf_timer_elapsed_us(t_start, t_end);

    // skb_debug_log("Rasterize color glyph %s [%d], size: %.1f, canvas: (%d x %d), time: %dus\n", font->name, glyph_id, font_size, target->width, target->height, (int32_t)elapsed_us);

	return true;
}


//
// Icon
//

static void skb__icon_draw_shape(skb_canvas_t* c, const skb_icon_t* icon, const skb_icon_shape_t* shape, float opacity)
{
	opacity *= shape->opacity;

	if (shape->path_count > 0) {
		skb_canvas_push_layer(c);
		for (int32_t i = 0; i < shape->path_count; i++) {
			skb_icon_path_command_t cmd = shape->path[i];
			if (cmd.type == SKB_SVG_MOVE_TO)
				skb_canvas_move_to(c, cmd.pt);
			else if (cmd.type == SKB_SVG_LINE_TO)
				skb_canvas_line_to(c, cmd.pt);
			else if (cmd.type == SKB_SVG_QUAD_TO)
				skb_canvas_quad_to(c, cmd.cp0, cmd.pt);
			else if (cmd.type == SKB_SVG_CUBIC_TO)
				skb_canvas_cubic_to(c, cmd.cp0, cmd.cp1, cmd.pt);
			else if (cmd.type == SKB_SVG_CLOSE_PATH)
				skb_canvas_close(c);
		}

		if (shape->gradient_idx != SKB_INVALID_INDEX) {
			const skb_icon_gradient_t* gradient = &icon->gradients[shape->gradient_idx];

			skb_canvas_push_transform(c, gradient->xform);

			if (gradient->type == SKB_GRADIENT_LINEAR) {
				skb_canvas_fill_linear_gradient(c, gradient->p0, gradient->p1, gradient->spread, gradient->stops, gradient->stops_count);
			} else if (gradient->type == SKB_GRADIENT_RADIAL) {
				skb_canvas_fill_radial_gradient(c, gradient->p0, 0.f, gradient->p1, gradient->radius, gradient->spread, gradient->stops, gradient->stops_count);
			}

			skb_canvas_pop_transform(c);
		} else {
			const skb_color_t color = skb_color_mul_alpha(shape->color, (uint8_t)skb_clampf(opacity * 255.f, 0.f, 255.f));
			skb_canvas_fill_solid_color(c, color);
		}

		skb_canvas_pop_layer(c);
	}

	for (int32_t i = 0; i < shape->children_count; i++)
		skb__icon_draw_shape(c, icon, &shape->children[i], opacity);
}

static void skb__icon_draw_shape_alpha(skb_canvas_t* c, const skb_icon_t* icon, const skb_icon_shape_t* shape)
{
	if (shape->path_count > 0) {
		for (int32_t i = 0; i < shape->path_count; i++) {
			skb_icon_path_command_t cmd = shape->path[i];
			if (cmd.type == SKB_SVG_MOVE_TO)
				skb_canvas_move_to(c, cmd.pt);
			else if (cmd.type == SKB_SVG_LINE_TO)
				skb_canvas_line_to(c, cmd.pt);
			else if (cmd.type == SKB_SVG_QUAD_TO)
				skb_canvas_quad_to(c, cmd.cp0, cmd.pt);
			else if (cmd.type == SKB_SVG_CUBIC_TO)
				skb_canvas_cubic_to(c, cmd.cp0, cmd.cp1, cmd.pt);
			else if (cmd.type == SKB_SVG_CLOSE_PATH)
				skb_canvas_close(c);
		}
	}

	for (int32_t i = 0; i < shape->children_count; i++)
		skb__icon_draw_shape_alpha(c, icon, &shape->children[i]);
}

skb_rect2i_t skb_rasterizer_get_icon_dimensions(const skb_icon_t* icon, skb_vec2_t icon_scale, int32_t padding)
{
	assert(icon);
	const int32_t width = (int32_t)ceilf(icon->view.width * icon_scale.x);
	const int32_t height = (int32_t)ceilf(icon->view.height * icon_scale.y);

	return (skb_rect2i_t) {
		.x = -padding,
		.y = -padding,
		.width = width + padding * 2,
		.height = height + padding * 2,
	};
}

bool skb_rasterizer_draw_alpha_icon(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	const skb_icon_t* icon, skb_vec2_t icon_scale, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target)
{
	assert(rasterizer);
	assert(temp_alloc);

	if (!target) return false;

	int64_t t_start = skb_perf_timer_get();

	skb_canvas_t* canvas = skb_canvas_create(temp_alloc, target);

	// Create transform to convert from the font coordinates to the canvas.
	const skb_mat2_t scale_xfrorm = skb_mat2_make_scale(icon_scale.x, icon_scale.y);
	const skb_mat2_t trans_xform = skb_mat2_make_translation(-icon->view.x * icon_scale.x - (float)offset_x, -icon->view.y * icon_scale.y - (float)offset_y);
	const skb_mat2_t xform = skb_mat2_multiply(scale_xfrorm, trans_xform);
	skb_canvas_push_transform(canvas, xform);

	skb__icon_draw_shape_alpha(canvas, icon, &icon->root);

	skb_canvas_fill_mask(canvas);

	if (alpha_mode == SKB_RASTERIZE_ALPHA_SDF) {
		// SDF
		skb__mask_to_sdf(temp_alloc, target, rasterizer->config.on_edge_value, rasterizer->config.pixel_dist_scale);
	}

	skb_canvas_destroy(canvas);

	int64_t t_end = skb_perf_timer_get();
	int64_t elapsed_us = skb_perf_timer_elapsed_us(t_start, t_end);

	// skb_debug_log("Rasterize alpha icon %s, scale: (%.1f, %.1f), canvas: (%d x %d), time: %dus\n", icon->name, icon_scale.x, icon_scale.y, target->width, target->height, (int32_t)elapsed_us);

	return true;
}

bool skb_rasterizer_draw_color_icon(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	const skb_icon_t* icon, skb_vec2_t icon_scale, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target)
{
	assert(rasterizer);
	assert(temp_alloc);

	if (!target) return false;

	int64_t t_start = skb_perf_timer_get();

	skb_canvas_t* canvas = skb_canvas_create(temp_alloc, target);

	// Create transform to convert from the font coordinates to the canvas.
	const skb_mat2_t scale_xfrorm = skb_mat2_make_scale(icon_scale.x, icon_scale.y);
	const skb_mat2_t trans_xform = skb_mat2_make_translation(-icon->view.x * icon_scale.x - (float)offset_x, -icon->view.y * icon_scale.y - (float)offset_y);
	const skb_mat2_t xform = skb_mat2_multiply(scale_xfrorm, trans_xform);
	skb_canvas_push_transform(canvas, xform);

	skb__icon_draw_shape(canvas, icon, &icon->root, 1.f);

	if (alpha_mode == SKB_RASTERIZE_ALPHA_SDF) {
		uint8_t* mask_buffer = SKB_TEMP_ALLOC(temp_alloc, uint8_t, target->width * target->height);
		const int32_t mask_stride = target->width;
		skb_image_t mask = {
			.buffer = mask_buffer,
			.width = target->width,
			.height = target->height,
			.stride_bytes = mask_stride,
			.bpp = 1,
		};

		// Copy alpha mask.
		for (int32_t y = 0; y < target->height; y++) {
			uint8_t* x_alpha = target->buffer + y * target->stride_bytes + 3; // Alpha is 3rd component
			uint8_t* x_mask = mask_buffer + y * mask_stride;
			for (int32_t x = 0; x < target->width; x++) {
				*x_mask = *x_alpha;
				x_mask++;
				x_alpha += 4;
			}
		}

		skb__mask_to_sdf(temp_alloc, &mask, rasterizer->config.on_edge_value, rasterizer->config.pixel_dist_scale);
		skb__unpremultiply_and_dilate(temp_alloc, target);

		// Copy SDF to alpha channel
		for (int32_t y = 0; y < target->height; y++) {
			uint8_t* x_alpha = target->buffer + y * target->stride_bytes + 3; // Alpha is 3rd component
			uint8_t* x_mask = mask_buffer + y * mask_stride;
			for (int32_t x = 0; x < target->width; x++) {
				*x_alpha = *x_mask;
				x_mask++;
				x_alpha += 4;
			}
		}

		SKB_TEMP_FREE(temp_alloc, mask_buffer);
	}

	skb_canvas_destroy(canvas);

	int64_t t_end = skb_perf_timer_get();
	int64_t elapsed_us = skb_perf_timer_elapsed_us(t_start, t_end);

	// skb_debug_log("Rasterize color icon %s, scale: (%.1f, %.1f), canvas: (%d x %d), time: %dus\n", icon->name, icon_scale.x, icon_scale.y, target->width, target->height, (int32_t)elapsed_us);

	return true;
}

skb_vec2_t skb_rasterizer_get_decoration_pattern_size(skb_decoration_style_t style, float thickness)
{
	skb_vec2_t res = {0};

	// Width of the pattern is rounded so that we can create repeating texture from the pattern.

	if (style == SKB_DECORATION_STYLE_SOLID) {
		// Solid
		res.x = ceilf(thickness * 10.f);
		res.y = thickness;
	} else if (style == SKB_DECORATION_STYLE_DOUBLE) {
		// Double
		res.x = ceilf(thickness * 10.f);
		res.y = thickness * 2.5f; // 2 lines of same thickess, and half the thickness space in-between.
	} else if (style == SKB_DECORATION_STYLE_DOTTED) {
		// Dotted
		res.x = ceilf(thickness * 10.f); // 5 dot per 10 units.
		res.y = thickness;
	} else if (style == SKB_DECORATION_STYLE_DASHED) {
		// Dashed
		res.x = ceilf(thickness * 10.f);	// 2 dashes per 10 units.
		res.y = thickness;
	} else {
		// Wavy
		res.x = ceilf(thickness * 10.f);	// 2 waves per 10 units.
		res.y = thickness * 3.0f;
	}
	return res;
}

skb_rect2i_t skb_rasterizer_get_decoration_pattern_dimensions(skb_decoration_style_t style, float thickness, int32_t padding)
{
	skb_vec2_t size = skb_rasterizer_get_decoration_pattern_size(style, thickness);

	int32_t width = (int32_t)ceilf(size.x);
	int32_t height = (int32_t)ceilf(size.y);

	// Only 1 padding on X since we assume that the pattern tiles horizontally. Image cache will inset the 1px padding to avoid interpolation bleeding.
	return (skb_rect2i_t) {
		.x = -1,
		.y = -padding,
		.width = width + 2,
		.height = height + padding * 2,
	};
}

static void skb__canvas_rect(skb_canvas_t* canvas, float x, float y, float width, float height)
{
	skb_canvas_move_to(canvas, skb_vec2_make(x, y));
	skb_canvas_line_to(canvas, skb_vec2_make(x + width, y));
	skb_canvas_line_to(canvas, skb_vec2_make(x + width, y + height));
	skb_canvas_line_to(canvas, skb_vec2_make(x, y + height));
	skb_canvas_close(canvas);
}

bool skb_rasterizer_draw_decoration_pattern(
	skb_rasterizer_t* rasterizer, skb_temp_alloc_t* temp_alloc,
	skb_decoration_style_t style, float thickness, skb_rasterize_alpha_mode_t alpha_mode,
	int32_t offset_x, int32_t offset_y, skb_image_t* target)
{

	assert(rasterizer);
	assert(temp_alloc);
	assert(target);

	if (!target->buffer || target->width <= 0 || target->height <= 0 || target->bpp != 1)
		return false;

	skb_canvas_t* canvas = skb_canvas_create(temp_alloc, target);

	skb_vec2_t size = skb_rasterizer_get_decoration_pattern_size(style, thickness);

	const skb_mat2_t trans_xform = skb_mat2_make_translation(-(float)offset_x, -(float)offset_y);
	skb_canvas_push_transform(canvas, trans_xform);

	if (style == SKB_DECORATION_STYLE_SOLID) {
		// Solid
		skb__canvas_rect(canvas, -1.f, 0.f, size.x+2.f, size.y);
	} else if (style == SKB_DECORATION_STYLE_DOUBLE) {
		// Double
		const float line_h = size.y / 2.5f;
		skb__canvas_rect(canvas, -1.f, 0.f, size.x+2.f, line_h);
		skb__canvas_rect(canvas, -1.f, size.y - line_h, size.x+2.f, line_h);
	} else if (style == SKB_DECORATION_STYLE_DOTTED) {
		// Dotted
		const float dot_spacing = size.x / 5.f;
		const float dot_width = size.x / 10.f;
		for (int32_t i = 0; i < 5; i++)
			skb__canvas_rect(canvas, (float)i * dot_spacing, 0.f, dot_width, size.y);
	} else if (style == SKB_DECORATION_STYLE_DASHED) {
		// Dashed
		float dash_spacing = size.x / 2.f;
		float dash_width = size.x / 2.f * (3.f / 5.f); // dash is 3 thickesses wide, gap is 2.
		for (int32_t i = 0; i < 2; i++)
			skb__canvas_rect(canvas, (float)i * dash_spacing, 0.f, dash_width, size.y);
	} else {
		// Wavy
		float unit_x = size.x / 10.f; // wave is 5 thicknesses wide, 3 high.
		float unit_y = size.y / 3.f;

		// The pattern can hold 2 waves, but we draw one before and one after to have valid data on the padding area too.
		for (int32_t i = -1; i <= 3; i++) {
			const skb_mat2_t offset_xform = skb_mat2_make_translation((float)i * unit_x * 5.f, 0.f);
			skb_canvas_push_transform(canvas, offset_xform);
			// Wave top
			skb_canvas_move_to(canvas, skb_vec2_make(0.f, unit_y * 2.f));
			skb_canvas_cubic_to(canvas, skb_vec2_make(unit_x * 1.f, unit_y * 2.f), skb_vec2_make(unit_x * 1.f, 0.f), skb_vec2_make(unit_x * 2.5f, 0.f));
			skb_canvas_cubic_to(canvas, skb_vec2_make(unit_x * 4.f, 0.f), skb_vec2_make(unit_x * 4.f, unit_y * 2.f), skb_vec2_make(unit_x * 5.f, unit_y * 2.f));
			// Wave bottom
			skb_canvas_line_to(canvas, skb_vec2_make(unit_x * 5.f, unit_y * 3.f));
			skb_canvas_cubic_to(canvas, skb_vec2_make(unit_x * 3.5f, unit_y * 3.f), skb_vec2_make(unit_x * 3.5f, unit_y * 1.f), skb_vec2_make(unit_x * 2.5f, unit_y * 1.f));
			skb_canvas_cubic_to(canvas, skb_vec2_make(unit_x * 1.5f, unit_y * 1.f), skb_vec2_make(unit_x * 1.5f, unit_y * 3.f), skb_vec2_make(0.f, unit_y * 3.f));

			skb_canvas_pop_transform(canvas);
		}
		skb_canvas_close(canvas);
	}

	skb_canvas_fill_mask(canvas);

	if (alpha_mode == SKB_RASTERIZE_ALPHA_SDF) {
		// SDF
		skb__mask_to_sdf(temp_alloc, target, rasterizer->config.on_edge_value, rasterizer->config.pixel_dist_scale);
	}

	skb_canvas_destroy(canvas);

	return true;
}
