// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include "skb_common.h"

int draw_init(void);
void draw_terminate(void);

void draw_line_width(float w);
void draw_tick(float x, float y, float s, skb_color_t col);
void draw_line(float x0, float y0, float x1, float y1, skb_color_t col);
void draw_dashed_line(float x0, float y0, float x1, float y1, float dash, skb_color_t col);
void draw_arrow(float x0, float y0, float x1, float y1, float as, skb_color_t col);
void draw_quad_bez(float x0, float y0, float x1, float y1, float x2, float y2, skb_color_t col);
void draw_cubic_bez(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, skb_color_t col);
void draw_rect(float x, float y, float w, float h, skb_color_t col);
void draw_filled_rect(float x, float y, float w, float h, skb_color_t col);
void draw_tri(float x0, float y0, float x1, float y1, float x2, float y2, skb_color_t col);
float draw_text(float x, float y, float size, float align, skb_color_t col, const char* fmt, ...);
float draw_text_width(float size, const char* fmt, ...);
float draw_char(float x, float y, float size, skb_color_t col, char chr);

uint32_t draw_create_texture(int32_t img_width, int32_t img_height, int32_t img_stride_bytes, const uint8_t* img_data, uint8_t bpp);
void draw_update_texture(uint32_t tex_id, int32_t offset_x, int32_t offset_y, int32_t width, int32_t height, int32_t img_width, int32_t img_height, int32_t img_stride_bytes, const uint8_t* img_data);

void draw_image_quad(skb_rect2_t geom, skb_rect2_t image, skb_color_t tint, uint32_t tex_id);
void draw_image_quad_sdf(skb_rect2_t geom, skb_rect2_t image, float scale, skb_color_t tint, uint32_t tex_id);
void draw_image_pattern_quad_sdf(skb_rect2_t geom, skb_rect2_t pattern, skb_rect2_t image, float scale, skb_color_t tint, uint32_t tex_id);

void draw_path_begin();
void draw_path_move_to(float x, float y);
void draw_path_line_to(float x1, float y1);
void draw_path_quad_to(float x1, float y1, float x2, float y2);
void draw_path_cubic_to(float x1, float y1, float x2, float y2, float x3, float y3);
void draw_path_end(skb_color_t color);

void draw_flush(int w, int h);


#endif // DRAW_H
