// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "debug_draw.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>

#include "skb_common.h"

static const char* draw__gl_error_string(int error)
{
	switch (error) {
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	//        case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
	//        case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	//        case GL_CONTEXT_LOST: return "GL_CONTEXT_LOST";
	//        case GL_TABLE_TOO_LARGE1: return "GL_TABLE_TOO_LARGE1";
	default: return "";
	}
}

static void draw__check_error(const char* str)
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		skb_debug_log("Error %08x %s after %s\n", err, draw__gl_error_string(err), str);
		return;
	}
}

static void draw__print_program_log(GLuint program)
{
	if (!glIsProgram(program)) {
		skb_debug_log("Name %d is not a program\n", program);
		return;
	}

	int32_t max_length = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

	char* info_log = skb_malloc(max_length);

	int32_t info_log_length = 0;
	glGetProgramInfoLog(program, max_length, &info_log_length, info_log);
	skb_debug_log("%s\n", info_log);

	skb_free(info_log);
}

static void sraw__print_shader_log(GLuint shader)
{
	//Make sure name is shader
	if (!glIsShader(shader)) {
		skb_debug_log("Name %d is not a shader\n", shader);
		return;
	}

	int32_t max_length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);

	char* info_log = skb_malloc(max_length);

	int32_t info_log_length = 0;
	glGetShaderInfoLog(shader, max_length, &info_log_length, info_log);
	skb_debug_log("%s\n", info_log);

	skb_free(info_log);
}

typedef struct draw__vertex_t {
	skb_vec2_t pos;
	skb_vec2_t uv;
	skb_color_t color;
	float scale;
} draw__vertex_t;

typedef enum {
	STENCIL_DISABLED,
	STENCIL_WINDING,
	STENCIL_FILL,
} draw__stencil_t;

typedef struct draw__batch_t {
	int32_t prim;
	int32_t offset;
	int32_t count;
	draw__stencil_t stencil;
	float line_width;
	uint32_t image_id;
	uint32_t sdf_id;
} draw__batch_t;

typedef struct draw__texture_t {
	uint32_t id;
	GLuint tex_id;
	int32_t width;
	int32_t height;
	uint8_t bpp;
} draw__texture_t;

typedef struct draw__context_t {
	GLuint program;
	GLuint vbo;

	draw__vertex_t* verts;
	int verts_count;
	int verts_cap;

	draw__batch_t* batches;
	int batches_count;
	int batches_cap;

	draw__texture_t* textures;
	int textures_count;
	int textures_cap;

	float line_width;
	draw__stencil_t stencil;

	bool in_polygon;
	skb_vec2_t start;
	skb_vec2_t pen;
	skb_rect2_t poly_bounds;

	uint32_t image_id;
	uint32_t sdf_id;

	GLfloat line_width_range[2];
} draw__context_t;

static draw__context_t g_context = { 0 };


int draw_init(void)
{
	draw__check_error("init");

	GLint success = GL_FALSE;

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	const GLchar* vs_source = "#version 140\n"
		"uniform vec2 viewsize;\n"
		"uniform vec2 tex_size;\n"
		"in vec3 apos;\n"
		"in vec2 auv;\n"
		"in vec4 acolor;\n"
		"in float ascale;\n"
		"out vec4 fcolor;\n"
		"out vec2 fuv;\n"
		"out float fscale;\n"
		"void main() {\n"
		"	fcolor = acolor;\n"
		"	fscale = ascale;\n"
		"	fuv = auv / tex_size;\n"
		"	gl_Position = vec4(2.0*apos.x/viewsize.x - 1.0, 1.0 - 2.0*apos.y/viewsize.y, 0, 1);\n"
		"}\n";

	glShaderSource(vs, 1, &vs_source, NULL);
	glCompileShader(vs);

	glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		skb_debug_log("Unable to compile vertex shader.\n");
		sraw__print_shader_log(vs);
		return 0;
	}

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* fs_source = "#version 140\n"
		"in vec4 fcolor;\n"
		"in vec2 fuv;\n"
		"in float fscale;\n"
		"out vec4 fragment;\n"
		"uniform sampler2D tex;\n"
		"uniform int tex_bpp;\n"
		"uniform int tex_sdf;\n"
		"void main() {\n"
		"	vec4 color = vec4(fcolor.rgb * fcolor.a, fcolor.a); // premult\n"
		"	if (tex_sdf == 0) {\n"
		"		if (tex_bpp == 4) {\n"
		"			color = texture(tex, fuv) * color;"
		"		} else if (tex_bpp == 1) {"
		"			float a = texture(tex, fuv).x;\n"
		"			color = color * a;\n"
		"		}\n"
		"	} else {\n"
		"		if (tex_bpp == 4) {\n"
		"			vec4 tcol = texture(tex, fuv);\n"
		"			float d = (tcol.a - (128./255.)) / (32./255.);\n"
		"			float w = fscale * 0.8;\n"
		"			float a = 1.f - clamp((d + 0.5*w - 0.1) / w, 0., 1.);\n"
		"			color = vec4(tcol.rgb*a,a) * color;\n"
		"		} else if (tex_bpp == 1) {\n"
		"			float d = (texture(tex, fuv).x - (128./255.)) / (32./255.);\n"
		"			float w = fscale * 0.8;\n"
		"			float a = 1.f - clamp((d + 0.5*w - 0.1) / w, 0., 1.);\n"
		"			color = color * a;\n"
		"		}\n"
		"	}\n"
		"	fragment = color;\n"
		"}\n";

	glShaderSource(fs, 1, &fs_source, NULL);
	glCompileShader(fs);

	glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		skb_debug_log("Unable to compile fragment shader.\n");
		sraw__print_shader_log(fs);
		return 0;
	}

	g_context.program = glCreateProgram();
	glAttachShader(g_context.program, vs);
	glAttachShader(g_context.program, fs);

	glBindAttribLocation(g_context.program, 0, "apos");
	glBindAttribLocation(g_context.program, 1, "auv");
	glBindAttribLocation(g_context.program, 2, "acolor");
	glBindAttribLocation(g_context.program, 3, "ascale");

	glLinkProgram(g_context.program);

	glGetProgramiv(g_context.program, GL_LINK_STATUS, &success);
	if (success != GL_TRUE) {
		skb_debug_log("Error linking program %d!\n", g_context.program);
		draw__print_program_log(g_context.program);
		return 0;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	draw__check_error("link");

	glGenBuffers(1, &g_context.vbo);

	draw__check_error("gen vbo");

	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, g_context.line_width_range);

	return 1;
}

void draw_terminate(void)
{
	glDeleteBuffers(1, &g_context.vbo);
	glDeleteProgram(g_context.program);
	for (int32_t i = 0; i < g_context.textures_count; i++) {
		draw__texture_t* tex = &g_context.textures[i];
		glDeleteTextures(1, &tex->tex_id);
	}

	skb_free(g_context.textures);
	skb_free(g_context.verts);
	skb_free(g_context.batches);

	memset(&g_context, 0, sizeof(g_context));
}

static draw__texture_t* draw__find_texture(uint32_t id)
{
	if (id == 0)
		return NULL;
	for (int i = 0; i < g_context.textures_count; i++) {
		if (g_context.textures[i].id == id) {
			return &g_context.textures[i];
		}
	}
	return NULL;
}


static void draw__resize_texture(draw__texture_t* tex, int32_t width, int32_t height, int32_t stride_bytes, const uint8_t* image, uint8_t bpp)
{
	// Delete old of exists.
	if (tex->tex_id) {
		glDeleteTextures(1, &tex->tex_id);
		tex->tex_id = 0;
	}

	tex->width = width;
	tex->height = height;
	tex->bpp = bpp;

	// upload image
	glGenTextures(1, &tex->tex_id);
	glBindTexture(GL_TEXTURE_2D, tex->tex_id);

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_bytes / bpp);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	assert(bpp == 4 || bpp == 1);
	if (bpp == 4)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	else if (bpp == 1)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	draw__check_error("resize texture");
}

static draw__texture_t* draw__add_texture(int32_t width, int32_t height, int32_t stride_bytes, const uint8_t* image, uint8_t bpp)
{
	// Not found, create.
	SKB_ARRAY_RESERVE(g_context.textures, g_context.textures_count + 1);
	draw__texture_t* tex = &g_context.textures[g_context.textures_count++];
	memset(tex, 0, sizeof(draw__texture_t));

	tex->id = g_context.textures_count;

	draw__resize_texture(tex, width, height, stride_bytes, image, bpp);

	draw__check_error("add texture");

	return tex;
}


static void draw__update_texture(draw__texture_t* tex, int32_t offset_x, int32_t offset_y, int32_t width, int32_t height, int32_t stride_bytes, const uint8_t* image)
{
	// upload image
	glBindTexture(GL_TEXTURE_2D, tex->tex_id);

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_bytes / tex->bpp);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, offset_x);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, offset_y);

	if (tex->bpp == 4)
		glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image);
	else if (tex->bpp == 1)
		glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, GL_RED, GL_UNSIGNED_BYTE, image);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	draw__check_error("update texture");
}

static void draw__add_command(int32_t prim)
{
	if (g_context.batches_count > 0) {
		draw__batch_t* prevBatch = &g_context.batches[g_context.batches_count-1];
		if (prevBatch->prim == prim && prevBatch->line_width == g_context.line_width && prevBatch->stencil == g_context.stencil
			&& prevBatch->image_id == g_context.image_id && prevBatch->sdf_id == g_context.sdf_id)
			return;
		prevBatch->count = g_context.verts_count - prevBatch->offset;
	}

	SKB_ARRAY_RESERVE(g_context.batches, g_context.batches_count+1);
	draw__batch_t* batch = &g_context.batches[g_context.batches_count];
	batch->prim = prim;
	batch->offset = g_context.verts_count;
	batch->count = 0;
	batch->line_width = g_context.line_width;
	batch->stencil = g_context.stencil;
	batch->image_id = g_context.image_id;
	batch->sdf_id = g_context.sdf_id;

	g_context.batches_count++;
}

static void draw__add_vertex_uv_scale(skb_vec2_t pos, skb_color_t col, skb_vec2_t uv, float scale)
{
	SKB_ARRAY_RESERVE(g_context.verts, g_context.verts_count+1);
	g_context.verts[g_context.verts_count++] = (draw__vertex_t){ .pos = pos, .color = col, .uv = uv, .scale = scale };
}

static void draw__add_vertex_uv(skb_vec2_t pos, skb_color_t col, skb_vec2_t uv)
{
	SKB_ARRAY_RESERVE(g_context.verts, g_context.verts_count+1);
	g_context.verts[g_context.verts_count++] = (draw__vertex_t){ .pos = pos, .color = col, .uv = uv };
}

static void draw__add_vertex(skb_vec2_t pos, skb_color_t col)
{
	SKB_ARRAY_RESERVE(g_context.verts, g_context.verts_count+1);
	g_context.verts[g_context.verts_count++] = (draw__vertex_t){ .pos = pos, .color = col, .uv = (skb_vec2_t){0} };
}

void draw_line_width(float w)
{
	g_context.line_width = skb_clampf(w, g_context.line_width_range[0], g_context.line_width_range[1]);
}

void draw_tick(float x, float y, float s, skb_color_t col)
{
	draw__add_command(GL_LINES);

	float hs = s * 0.5f + g_context.line_width * 0.5f;

	draw__add_vertex((skb_vec2_t){ x-hs, y }, col);
	draw__add_vertex((skb_vec2_t){ x+hs, y }, col);
	draw__add_vertex((skb_vec2_t){ x, y-hs }, col);
	draw__add_vertex((skb_vec2_t){ x, y+hs }, col);
}

static float draw__get_dir(float x0, float y0, float x1, float y1, float* dirx, float* diry)
{
	float dx = x1 - x0;
	float dy = y1 - y0;
	float dd = dx * dx + dy * dy;
	float d = sqrtf(dd);
	float id = d > 0.f ? 1.f / d : 0.f;
	*dirx = dx * id;
	*diry = dy * id;
	return d;
}

void draw_line(float x0, float y0, float x1, float y1, skb_color_t col)
{
	draw__add_command(GL_LINES);
	if (g_context.line_width > 0.f) {
		float dx, dy;
		draw__get_dir(x0, y0, x1, y1, &dx, &dy);
		dx *= g_context.line_width * 0.5f;
		dy *= g_context.line_width * 0.5f;
		draw__add_vertex((skb_vec2_t){ x0 - dx, y0 - dy }, col);
		draw__add_vertex((skb_vec2_t){ x1 + dx, y1 + dy }, col);
	} else {
		draw__add_vertex((skb_vec2_t){ x0, y0 }, col);
		draw__add_vertex((skb_vec2_t){ x1, y1 }, col);
	}
}

void draw_dashed_line(float x0, float y0, float x1, float y1, float dash, skb_color_t col)
{
	float dx, dy;
	float len = draw__get_dir(x0, y0, x1, y1, &dx, &dy);
	len += g_context.line_width;

	int tick_count = (int)floorf(len / dash) | 1;
	if (tick_count < 1) tick_count = 1;
	if (tick_count > 1000) tick_count = 1000;

	float d = len / (float)tick_count;

	x0 -= g_context.line_width * 0.5f;
	y0 -= g_context.line_width * 0.5f;

	draw__add_command(GL_LINES);

	for (int i = 0; i < tick_count; i += 2) {
		const float d0 = (float)i * d;
		const float d1 = d0 + d;
		draw__add_vertex((skb_vec2_t){ x0 + dx*d0, y0 + dy*d0 }, col);
		draw__add_vertex((skb_vec2_t){ x0 + dx*d1, y0 + dy*d1 }, col);
	}

}

void draw_arrow(float x0, float y0, float x1, float y1, float as, skb_color_t col)
{
	float dx, dy;
	draw__get_dir(x0, y0, x1, y1, &dx, &dy);
	float nx = -dy;
	float ny = dx;

	draw_line(x0, y0, x1, y1, col);
	draw_line(x1, y1, x1 - dx*as - nx*as*.5f, y1 - dy*as - ny*as*.5f, col);
	draw_line(x1, y1, x1 - dx*as + nx*as*.5f, y1 - dy*as + ny*as*.5f, col);
}

#define STEPS 16

static inline float interpolate_quad_bez(float x0, float x1, float x2, float t)
{
	float x01 = skb_lerpf(x0, x1, t);
	float x12 = skb_lerpf(x1, x2, t);
	return skb_lerpf(x01, x12, t);
}

static inline float interpolate_cubic_bez(float x0, float x1, float x2, float x3, float t)
{
	float x01 = skb_lerpf(x0, x1, t);
	float x12 = skb_lerpf(x1, x2, t);
	float x23 = skb_lerpf(x2, x3, t);
	float x012 = skb_lerpf(x01, x12, t);
	float x123 = skb_lerpf(x12, x23, t);
	return skb_lerpf(x012, x123, t);
}

void draw_quad_bez(float x0, float y0, float x1, float y1, float x2, float y2, skb_color_t col)
{
	draw__add_command(GL_LINES);

	if (g_context.line_width > 0.f) {
		float dx, dy;
		draw__get_dir(x0, y0, x1, y1, &dx, &dy);
		draw__add_vertex((skb_vec2_t){ x0 - dx * g_context.line_width * 0.5f, y0 - dy * g_context.line_width * 0.5f }, col);
		draw__add_vertex((skb_vec2_t){ x0, y0 }, col);
	}

	float px = x0;
	float py = y0;
	for (int i = 0; i <= STEPS; i++) {
		float t = (float)i / (float)STEPS;
		float x = interpolate_quad_bez(x0, x1, x2, t);
		float y = interpolate_quad_bez(y0, y1, y2, t);
		draw__add_vertex((skb_vec2_t){ px, py }, col);
		draw__add_vertex((skb_vec2_t){ x, y }, col);
		px = x;
		py = y;
	}

	if (g_context.line_width > 0.f) {
		float dx, dy;
		draw__get_dir(x1, y1, x2, y2, &dx, &dy);
		draw__add_vertex((skb_vec2_t){ x2, y2 }, col);
		draw__add_vertex((skb_vec2_t){ x2 + dx * g_context.line_width * 0.5f, y2 + dy * g_context.line_width * 0.5f }, col);
	}
}

void draw_cubic_bez(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, skb_color_t col)
{
	draw__add_command(GL_LINES);
	float px = x0;
	float py = y0;
	for (int i = 0; i <= STEPS; i++) {
		float t = (float)i / (float)STEPS;
		float x = interpolate_cubic_bez(x0, x1, x2, x3, t);
		float y = interpolate_cubic_bez(y0, y1, y2, y3, t);
		draw__add_vertex((skb_vec2_t){ px, py }, col);
		draw__add_vertex((skb_vec2_t){ x, y }, col);
		px = x;
		py = y;
	}
}

void draw_rect(float x, float y, float w, float h, skb_color_t col)
{
	draw__add_command(GL_LINES);
	float lw = g_context.line_width * 0.5f;

	draw__add_vertex((skb_vec2_t){ x-lw, y }, col);
	draw__add_vertex((skb_vec2_t){ x+w+lw, y }, col);

	draw__add_vertex((skb_vec2_t){ x+w, y-lw }, col);
	draw__add_vertex((skb_vec2_t){ x+w, y+h+lw }, col);

	draw__add_vertex((skb_vec2_t){ x+w+lw, y+h }, col);
	draw__add_vertex((skb_vec2_t){ x-lw, y+h }, col);

	draw__add_vertex((skb_vec2_t){ x, y+h+lw }, col);
	draw__add_vertex((skb_vec2_t){ x, y-lw }, col);
}

void draw_filled_rect(float x, float y, float w, float h, skb_color_t col)
{
	draw__add_command(GL_TRIANGLES);

	draw__add_vertex((skb_vec2_t){ x, y }, col);
	draw__add_vertex((skb_vec2_t){ x+w, y }, col);
	draw__add_vertex((skb_vec2_t){ x+w, y+h }, col);

	draw__add_vertex((skb_vec2_t){ x, y }, col);
	draw__add_vertex((skb_vec2_t){ x+w, y+h }, col);
	draw__add_vertex((skb_vec2_t){ x, y+h }, col);

}

void draw_tri(float x0, float y0, float x1, float y1, float x2, float y2, skb_color_t col)
{
	draw__add_command(GL_TRIANGLES);
	draw__add_vertex((skb_vec2_t){ x0, y0 }, col);
	draw__add_vertex((skb_vec2_t){ x1, y1 }, col);
	draw__add_vertex((skb_vec2_t){ x2, y2 }, col);
}

// Line based debug font
typedef struct draw__line_glyph_t {
	char num;
	char advance;
	char verts[2*40];
} draw__line_glyph_t;

static draw__line_glyph_t g_simplex[95] = {
	// Space (32)
	{ .num = 0, .advance = 16, .verts = { 0 }, },
	// !
	{ .num = 8, .advance = 10, .verts = { 5,21, 5,7, -1,-1, 5,2, 4,1, 5,0, 6,1, 5,2 }, },
	// "
	{ .num = 5, .advance = 16, .verts = { 4,21, 4,14, -1,-1, 12,21, 12,14 }, },
	// #
	{ .num = 11, .advance = 21, .verts = { 9,20, 6,-2,-1,-1,15,20,12,-2,-1,-1, 4,12,18,12,-1,-1, 3, 6,17, 6 }, },
	// $
	{ .num = 23, .advance = 20, .verts = { 12,25, 9,-4, -1,-1, 17,18, 15,20, 12,21, 8,21, 5,20, 3,18, 3,16, 4,14, 5,13, 7,12, 13,10, 15,9, 16,8, 17,6, 17,3, 15,1, 12,0, 8,0, 5,1, 3,3, }, },
	// %
	{ .num = 26, .advance = 24, .verts = { 17,21, 7,0, -1,-1, 8,21, 10,19, 10,17, 9,15, 7,14, 5,14, 3,16, 3,18, 4,20, 6,21, 8,21, -1,-1, 17,7, 15,6, 14,4, 14,2, 16,0, 18,0, 20,1, 21,3, 21,5, 19,7, 17,7 }, },
	// &
	{ .num = 23, .advance = 24, .verts = { 19,8, 15,3, 13,1, 11,0, 7,0, 5,1, 4,2, 3,4, 3,6, 4,8, 5,9, 12,13, 13,14, 14,16, 14,18, 13,20, 11,21, 9,20, 8,18, 8,16, 9,13, 11,10, 19,0 }, },
	// '
	{ .num = 7, .advance = 10, .verts = { 5,19, 4,20, 5,21, 6,20, 6,18, 5,16, 4,15 }, },
	// (
	{ .num = 8, .advance = 12, .verts = { 8,22, 7,20, 5,16, 4,11, 4,7, 5,2, 7,-2, 8,-4 }, },
	// )
	{ .num = 8, .advance = 12, .verts = { 4,22, 5,20, 7,16, 8,11, 8,7, 7,2, 5,-2, 4,-4 }, },
	// *
	{ .num = 8, .advance = 16, .verts = { 8,21, 8,9, -1,-1, 3,18, 13,12, -1,-1, 13,18, 3,12 }, },
	// +
	{ .num = 5, .advance = 22, .verts = { 11,16, 11,2, -1,-1, 4,9, 18,9 }, },
	// ,
	{ .num = 8, .advance = 10, .verts = { 6, 1, 5, 0, 4, 1, 5, 2, 6, 1, 6,-1, 5,-3, 4,-4 }, },
	// -
	{ .num = 2, .advance = 22, .verts = { 4, 9,18, 9 }, },
	// .
	{ .num = 5, .advance = 10, .verts = { 5, 2, 4, 1, 5, 0, 6, 1, 5, 2 }, },
	// /
	{ .num = 2, .advance = 16, .verts = { 12,21, 2, 0 }, },
	// 0
	{ .num = 17, .advance = 20, .verts = { 9,21, 6,20, 4,17, 3,12, 3, 9, 4, 4, 6, 1, 9, 0,11, 0,14, 1,16, 4,17, 9,17,12,16,17,14,20,11,21, 9,21 }, },
	// 1
	{ .num = 4, .advance = 20, .verts = { 6,17, 8,18,11,21,11,0 }, },
	// 2
	{ .num = 14, .advance = 20, .verts = { 4,16, 4,17, 5,19, 6,20, 8,21,12,21,14,20,15,19,16,17,16,15,15,13,13, 10, 3, 0,17, 0 }, },
	// 3
	{ .num = 15, .advance = 20, .verts = { 5,21,16,21,10,13,13,13,15,12,16,11,17, 8,17, 6,16, 3,14, 1,11, 0, 8, 0, 5, 1, 4, 2, 3, 4 }, },
	// 4
	{ .num = 6, .advance = 20, .verts = { 13,21, 3, 7,18, 7,-1,-1,13,21,13, 0 }, },
	// 5
	{ .num = 17, .advance = 20, .verts = { 15,21, 5,21, 4,12, 5,13, 8,14,11,14,14,13,16,11,17, 8,17, 6,16, 3,14,1,11, 0, 8, 0, 5, 1, 4, 2, 3, 4 }, },
	// 6
	{ .num = 23, .advance = 20, .verts = { 16,18,15,20,12,21,10,21, 7,20, 5,17, 4,12, 4, 7, 5, 3, 7, 1,10, 0,11, 0,14, 1,16, 3,17, 6,17, 7,16,10,14,12,11,13,10,13, 7,12, 5,10, 4, 7 }, },
	// 7
	{ .num = 5, .advance = 20, .verts = { 17,21, 7, 0,-1,-1, 3,21,17,21 }, },
	// 8
	{ .num = 29, .advance = 20, .verts = { 8,21, 5,20, 4,18, 4,16, 5,14, 7,13,11,12,14,11,16, 9,17, 7,17, 4,16,2,15, 1,12, 0, 8, 0, 5, 1, 4, 2, 3, 4, 3, 7, 4, 9, 6,11, 9,12,13,13,15,14,16,16,16,18,15,20,12,21, 8,21 }, },
	// 9
	{ .num = 23, .advance = 20, .verts = { 16,14,15,11,13, 9,10, 8, 9, 8, 6, 9, 4,11, 3,14, 3,15, 4,18, 6,20, 9, 21,10,21,13,20,15,18,16,14,16, 9,15, 4,13, 1,10, 0, 8, 0, 5, 1, 4, 3 }, },
	// :
	{ .num = 11, .advance = 10, .verts = { 5,14, 4,13, 5,12, 6,13, 5,14,-1,-1, 5, 2, 4, 1, 5, 0, 6, 1, 5, 2 }, },
	// ,
	{ .num = 14, .advance = 10, .verts = { 5,14, 4,13, 5,12, 6,13, 5,14,-1,-1, 6, 1, 5, 0, 4, 1, 5, 2, 6, 1, 6,-1, 5,-3, 4,-4 }, },
	// <
	{ .num = 3, .advance = 24, .verts = { 20,18, 4, 9,20, 0 }, },
	// =
	{ .num = 5, .advance = 26, .verts = { 4,12,22,12,-1,-1, 4, 6,22, 6 }, },
	// >
	{ .num = 3, .advance = 24, .verts = { 4,18,20, 9, 4, 0 }, },
	// ?
	{ .num = 20, .advance = 18, .verts = { 3,16, 3,17, 4,19, 5,20, 7,21,11,21,13,20,14,19,15,17,15,15,14,13,13,12, 9,10, 9, 7,-1,-1, 9, 2, 8, 1, 9, 0,10, 1, 9, 2 }, },
	// @
	{ .num = 40, .advance = 27, .verts = { 18,13, 17,15, 15,16, 12,16, 10,15, 9,14, 8,11, 8,8, 9,6, 11,5, 14,5, 16,6, 17,8, -1,-1, 18,16, 17,8, 17,6, 19,5, 21,5, 23,7, 24,10, 24,12, 23,15, 22,17, 20,19, 18,20, 15,21, 12,21, 9,20, 7,19, 5,17, 4,15, 3,12, 3,9, 4,6, 5,4, 7,2, 9,1 ,12,0, 16,0, }, },
	// A
	{ .num = 8, .advance = 18, .verts = { 9,21, 1, 0,-1,-1, 9,21,17, 0,-1,-1, 4, 7,14, 7 }, },
	// B
	{ .num = 23, .advance = 21, .verts = { 4,21, 4, 0,-1,-1, 4,21,13,21,16,20,17,19,18,17,18,15,17,13,16,12,13,11,-1,-1, 4,11,13,11,16,10,17, 9,18, 7,18, 4,17, 2,16, 1,13, 0, 4, 0 }, },
	// C
	{ .num = 18, .advance = 21, .verts = { 18,16,17,18,15,20,13,21, 9,21, 7,20, 5,18, 4,16, 3,13, 3, 8, 4, 5, 5,3, 7, 1, 9, 0,13, 0,15, 1,17, 3,18, 5 }, },
	// D
	{ .num = 15, .advance = 21, .verts = { 4,21, 4, 0,-1,-1, 4,21,11,21,14,20,16,18,17,16,18,13,18, 8,17, 5,16,3,14, 1,11, 0, 4, 0 }, },
	// E
	{ .num = 11, .advance = 19, .verts = { 4,21, 4, 0,-1,-1, 4,21,17,21,-1,-1, 4,11,12,11,-1,-1, 4, 0,17, 0 }, },
	// F
	{ .num = 8, .advance = 18, .verts = { 4,21, 4, 0,-1,-1, 4,21,17,21,-1,-1, 4,11,12,11 }, },
	// G
	{ .num = 22, .advance = 21, .verts = { 18,16,17,18,15,20,13,21, 9,21, 7,20, 5,18, 4,16, 3,13, 3, 8, 4, 5, 5,3, 7, 1, 9, 0,13, 0,15, 1,17, 3,18, 5,18, 8,-1,-1,13, 8,18, 8 }, },
	// H
	{ .num = 8, .advance = 22, .verts = { 4,21, 4, 0,-1,-1,18,21,18, 0,-1,-1, 4,11,18,11 }, },
	// I
	{ .num = 2, .advance = 8, .verts = { 4,21, 4, 0 }, },
	// J
	{ .num = 10, .advance = 16, .verts = { 12,21,12, 5,11, 2,10, 1, 8, 0, 6, 0, 4, 1, 3, 2, 2, 5, 2, 7 }, },
	// K
	{ .num = 8, .advance = 21, .verts = { 4,21, 4, 0,-1,-1,18,21, 4, 7,-1,-1, 9,12,18, 0 }, },
	// L
	{ .num = 5, .advance = 17, .verts = { 4,21, 4, 0,-1,-1, 4, 0,16, 0 }, },
	// M
	{ .num = 11, .advance = 24, .verts = { 4,21, 4, 0,-1,-1, 4,21,12, 0,-1,-1,20,21,12, 0,-1,-1,20,21,20, 0 }, },
	// N
	{ .num = 8, .advance = 22, .verts = { 4,21, 4, 0,-1,-1, 4,21,18, 0,-1,-1,18,21,18, 0 }, },
	// O
	{ .num = 21, .advance = 22, .verts = { 9,21, 7,20, 5,18, 4,16, 3,13, 3, 8, 4, 5, 5, 3, 7, 1, 9, 0,13, 0,15,1,17, 3,18, 5,19, 8,19,13,18,16,17,18,15,20,13,21, 9,21 }, },
	// P
	{ .num = 13, .advance = 21, .verts = { 4,21, 4, 0,-1,-1, 4,21,13,21,16,20,17,19,18,17,18,14,17,12,16,11,13,10, 4,10 }, },
	// Q
	{ .num = 24, .advance = 22, .verts = { 9,21, 7,20, 5,18, 4,16, 3,13, 3, 8, 4, 5, 5, 3, 7, 1, 9, 0,13, 0,15,1,17, 3,18, 5,19, 8,19,13,18,16,17,18,15,20,13,21, 9,21,-1,-1,12, 4,18,-2 }, },
	// R
	{ .num = 16, .advance = 21, .verts = { 4,21, 4, 0,-1,-1, 4,21,13,21,16,20,17,19,18,17,18,15,17,13,16,12,13,11, 4,11,-1,-1,11,11,18, 0 }, },
	// S
	{ .num = 20, .advance = 20, .verts = { 17,18,15,20,12,21, 8,21, 5,20, 3,18, 3,16, 4,14, 5,13, 7,12,13,10,15,9,16, 8,17, 6,17, 3,15, 1,12, 0, 8, 0, 5, 1, 3, 3 }, },
	// T
	{ .num = 5, .advance = 16, .verts = { 8,21, 8, 0,-1,-1, 1,21,15,21 }, },
	// U
	{ .num = 10, .advance = 22, .verts = { 4,21, 4, 6, 5, 3, 7, 1,10, 0,12, 0,15, 1,17, 3,18, 6,18,21 }, },
	// V
	{ .num = 5, .advance = 18, .verts = { 1,21, 9, 0,-1,-1,17,21, 9, 0 }, },
	// W
	{ .num = 11, .advance = 24, .verts = { 2,21, 7, 0,-1,-1,12,21, 7, 0,-1,-1,12,21,17, 0,-1,-1,22,21,17, 0 }, },
	// X
	{ .num = 5, .advance = 20, .verts = { 3,21,17, 0,-1,-1,17,21, 3, 0 }, },
	// Y
	{ .num = 6, .advance = 18, .verts = { 1,21, 9,11, 9, 0,-1,-1,17,21, 9,11 }, },
	// Z
	{ .num = 8, .advance = 20, .verts = { 17,21, 3, 0,-1,-1, 3,21,17,21,-1,-1, 3, 0,17, 0 }, },
	// [
	{ .num = 4, .advance = 11, .verts = { 8,22, 4,22, 4,-2, 8,-2, }, },
	// backslash
	{ .num = 2, .advance = 14, .verts = { 2,21, 12, 0 }, },
	// ]
	{ .num = 4, .advance = 11, .verts = { 4,22, 8,22, 8,-2, 4,-2 }, },
	// ^
	{ .num = 3, .advance = 16, .verts = { 3,15, 8,20, 13,15 }, },
	// _
	{ .num = 2, .advance = 16, .verts = { 0,-2,16,-2 }, },
	// `
	{ .num = 7, .advance = 10, .verts = { 6,21, 5,20, 4,18, 4,16, 5,15, 6,16, 5,17 }, },
	// a
	{ .num = 17, .advance = 19, .verts = { 15,14,15, 0,-1,-1,15,11,13,13,11,14, 8,14, 6,13, 4,11, 3, 8, 3, 6, 4,3, 6, 1, 8, 0,11, 0,13, 1,15, 3 }, },
	// b
	{ .num = 17, .advance = 19, .verts = { 4,21, 4, 0,-1,-1, 4,11, 6,13, 8,14,11,14,13,13,15,11,16, 8,16, 6,15,3,13, 1,11, 0, 8, 0, 6, 1, 4, 3 }, },
	// c
	{ .num = 14, .advance = 18, .verts = { 15,11,13,13,11,14, 8,14, 6,13, 4,11, 3, 8, 3, 6, 4, 3, 6, 1, 8, 0,11,0,13, 1,15, 3 }, },
	// d
	{ .num = 17, .advance = 19, .verts = { 15,21,15, 0,-1,-1,15,11,13,13,11,14, 8,14, 6,13, 4,11, 3, 8, 3, 6, 4,3, 6, 1, 8, 0,11, 0,13, 1,15, 3 }, },
	// e
	{ .num = 17, .advance = 18, .verts = { 3, 8,15, 8,15,10,14,12,13,13,11,14, 8,14, 6,13, 4,11, 3, 8, 3, 6, 4,3, 6, 1, 8, 0,11, 0,13, 1,15, 3 }, },
	// f
	{ .num = 8, .advance = 12, .verts = { 10,21, 8,21, 6,20, 5,17, 5, 0,-1,-1, 2,14, 9,14 }, },
	// g
	{ .num = 22, .advance = 19, .verts = { 15,14,15,-2,14,-5,13,-6,11,-7, 8,-7, 6,-6,-1,-1,15,11,13,13,11,14, 8,14, 6,13, 4,11, 3, 8, 3, 6, 4, 3, 6, 1, 8, 0,11, 0,13, 1,15, 3 }, },
	// h
	{ .num = 10, .advance = 19, .verts = { 4,21, 4, 0,-1,-1, 4,10, 7,13, 9,14,12,14,14,13,15,10,15, 0 }, },
	// i
	{ .num = 8, .advance = 8, .verts = { 3,21, 4,20, 5,21, 4,22, 3,21,-1,-1, 4,14, 4, 0 }, },
	// j
	{ .num = 11, .advance = 10, .verts = { 5,21, 6,20, 7,21, 6,22, 5,21,-1,-1, 6,14, 6,-3, 5,-6, 3,-7, 1,-7 }, },
	// k
	{ .num = 8, .advance = 17, .verts = { 4,21, 4, 0,-1,-1,14,14, 4, 4,-1,-1, 8, 8,15, 0 }, },
	// l
	{ .num = 2, .advance = 8, .verts = { 4,21, 4, 0 }, },
	// m
	{ .num = 18, .advance = 30, .verts = { 4,14, 4, 0,-1,-1, 4,10, 7,13, 9,14,12,14,14,13,15,10,15, 0,-1,-1,15,10,18,13,20,14,23,14,25,13,26,10,26, 0 }, },
	// n
	{ .num = 10, .advance = 19, .verts = { 4,14, 4, 0,-1,-1, 4,10, 7,13, 9,14,12,14,14,13,15,10,15, 0 }, },
	// o
	{ .num = 17, .advance = 19, .verts = { 8,14, 6,13, 4,11, 3, 8, 3, 6, 4, 3, 6, 1, 8, 0,11, 0,13, 1,15, 3,16,6,16, 8,15,11,13,13,11,14, 8,14 }, },
	// p
	{ .num = 17, .advance = 19, .verts = { 4,14, 4,-7,-1,-1, 4,11, 6,13, 8,14,11,14,13,13,15,11,16, 8,16, 6,15,3,13, 1,11, 0, 8, 0, 6, 1, 4, 3 }, },
	// q
	{ .num = 17, .advance = 19, .verts = { 15,14,15,-7,-1,-1,15,11,13,13,11,14, 8,14, 6,13, 4,11, 3, 8, 3, 6, 4,3, 6, 1, 8, 0,11, 0,13, 1,15, 3 }, },
	// r
	{ .num = 8, .advance = 13, .verts = { 4,14, 4, 0,-1,-1, 4, 8, 5,11, 7,13, 9,14,12,14 }, },
	// s
	{ .num = 17, .advance = 17, .verts = { 14,11,13,13,10,14, 7,14, 4,13, 3,11, 4, 9, 6, 8,11, 7,13, 6,14, 4,14,3,13, 1,10, 0, 7, 0, 4, 1, 3, 3 }, },
	// t
	{ .num = 8, .advance = 12, .verts = { 5,21, 5, 4, 6, 1, 8, 0,10, 0,-1,-1, 2,14, 9,14 }, },
	// u
	{ .num = 10, .advance = 19, .verts = { 4,14, 4, 4, 5, 1, 7, 0,10, 0,12, 1,15, 4,-1,-1,15,14,15, 0 }, },
	// v
	{ .num = 5, .advance = 16, .verts = { 2,14, 8, 0,-1,-1,14,14, 8, 0 }, },
	// w
	{ .num = 11, .advance = 22, .verts = { 3,14, 7, 0,-1,-1,11,14, 7, 0,-1,-1,11,14,15, 0,-1,-1,19,14,15, 0 }, },
	// x
	{ .num = 5, .advance = 17, .verts = { 3,14,14, 0,-1,-1,14,14, 3, 0 }, },
	// y
	{ .num = 9, .advance = 16, .verts = { 2,14, 8, 0,-1,-1,14,14, 8, 0, 6,-4, 4,-6, 2,-7, 1,-7 }, },
	// z
	{ .num = 8, .advance = 17, .verts = { 14,14, 3, 0,-1,-1, 3,14,14,14,-1,-1, 3, 0,14, 0 }, },
	// {
	{ .num = 11, .advance = 14, .verts = { 9,22, 7,21, 6,19, 6,12, 5,10, 4,9, 5,8, 6,6, 6,-1, 7,-3, 9,-4, }, },
	// |
	{ .num = 2, .advance = 8, .verts = { 4,22, 4,-4 }, },
	// }
	{ .num = 11, .advance = 14, .verts = { 4,22, 6,21, 7,19, 7,12, 8,10, 9,9, 8,8, 7,6, 7,-1, 6,-3, 4,-4 }, },
	// ~
	{ .num = 9, .advance = 24, .verts = { 3, 8, 4,10, 6,11, 8,11, 14,8, 16,7, 18,7, 20,8, 21,10 }, },
};

static float draw__glyph_width(float size, char chr)
{
	float scale = size / 30.f;
	int32_t idx = chr - 32;
	if (idx >= 0 && idx < 95) {
		const draw__line_glyph_t* g = &g_simplex[idx];
		return (float)g->advance * scale;
	}
	return 0.f;
}

float draw_char(float x, float y, float size, skb_color_t col, char chr)
{
	// Ascender 24
	// Descender -8

	float scale = size / 30.f;

	int idx = chr - 32;
	if (idx >= 0 && idx < 95) {
		draw__line_glyph_t* g = &g_simplex[idx];
		int px = -1;
		int py = -1;
		for (int j = 0; j < g->num; j++) {
			int cx = g->verts[j*2];
			int cy = -g->verts[j*2+1];
			if (px != -1 && cx != -1) {
				float x0 = x + (float)px * scale;
				float y0 = y + (float)py * scale;
				float x1 = x + (float)cx * scale;
				float y1 = y + (float)cy * scale;
				draw_line(x0, y0, x1, y1, col);
			}
			px = cx;
			py = cy;
		}

		return (float)g->advance * scale;
	}
	return 0.f;
}

float draw_text(float x, float y, float size, float align, skb_color_t col, const char* fmt, ...)
{
	char s[1025];
	va_list args;
	va_start(args, fmt);
	vsnprintf(s, sizeof(s) - 1, fmt, args);
	va_end(args);

	float w = 0.f;
	char* str = s;
	while (*str) {
		w += draw__glyph_width(size, *str);
		str++;
	}
	x -= w * align;

	str = s;
	while (*str) {
		x += draw_char(x, y, size, col,*str);
		str++;
	}
	return x;
}

float draw_text_width(float size, const char* fmt, ...)
{
	char s[1025];
	va_list args;
	va_start(args, fmt);
	vsnprintf(s, sizeof(s) - 1, fmt, args);
	va_end(args);

	float w = 0.f;
	char* str = s;
	while (*str) {
		w += draw__glyph_width(size, *str);
		str++;
	}
	return w;
}


uint32_t draw_create_texture(int32_t img_width, int32_t img_height, int32_t img_stride_bytes, const uint8_t* img_data, uint8_t bpp)
{
	draw__texture_t* tex = draw__add_texture(img_width, img_height, img_stride_bytes, img_data, bpp);
	return tex ? tex->id : 0;
}

void draw_update_texture(uint32_t tex_id, int32_t offset_x, int32_t offset_y, int32_t width, int32_t height, int32_t img_width, int32_t img_height, int32_t img_stride_bytes, const uint8_t* img_data)
{
	draw__texture_t* tex = draw__find_texture(tex_id);
	if (tex) {
		if (tex->width != img_width || tex->height != img_height) {
			draw__resize_texture(tex, img_width, img_height, img_stride_bytes, img_data, tex->bpp);
		} else {
			draw__update_texture(tex, offset_x, offset_y, width, height, img_stride_bytes, img_data);
		}
	}
}

//void draw_image_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, skb_color_t tint, uint32_t tex_id)
void draw_image_quad(skb_rect2_t geom, skb_rect2_t image, skb_color_t tint, uint32_t tex_id)
{
	g_context.image_id = tex_id;

	const float x0 = geom.x;
	const float y0 = geom.y;
	const float x1 = geom.x + geom.width;
	const float y1 = geom.y + geom.height;
	const float u0 = image.x;
	const float v0 = image.y;
	const float u1 = image.x + image.width;
	const float v1 = image.y + image.height;

	draw__add_command(GL_TRIANGLES);

	draw__add_vertex_uv((skb_vec2_t){ x0, y0 }, tint, (skb_vec2_t){u0,v0});
	draw__add_vertex_uv((skb_vec2_t){ x1, y0 }, tint, (skb_vec2_t){u1,v0});
	draw__add_vertex_uv((skb_vec2_t){ x1, y1 }, tint, (skb_vec2_t){u1,v1});

	draw__add_vertex_uv((skb_vec2_t){ x0, y0 }, tint, (skb_vec2_t){u0,v0});
	draw__add_vertex_uv((skb_vec2_t){ x1, y1 }, tint, (skb_vec2_t){u1,v1});
	draw__add_vertex_uv((skb_vec2_t){ x0, y1 }, tint, (skb_vec2_t){u0,v1});

	g_context.image_id = 0;
}

//void draw_image_quad_sdf(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float scale, skb_color_t tint, uint32_t tex_id)
void draw_image_quad_sdf(skb_rect2_t geom, skb_rect2_t image, float scale, skb_color_t tint, uint32_t tex_id)
{
	g_context.sdf_id = tex_id;

	const float x0 = geom.x;
	const float y0 = geom.y;
	const float x1 = geom.x + geom.width;
	const float y1 = geom.y + geom.height;
	const float u0 = image.x;
	const float v0 = image.y;
	const float u1 = image.x + image.width;
	const float v1 = image.y + image.height;

	draw__add_command(GL_TRIANGLES);

	draw__add_vertex_uv_scale((skb_vec2_t){ x0, y0 }, tint, (skb_vec2_t){u0,v0}, scale);
	draw__add_vertex_uv_scale((skb_vec2_t){ x1, y0 }, tint, (skb_vec2_t){u1,v0}, scale);
	draw__add_vertex_uv_scale((skb_vec2_t){ x1, y1 }, tint, (skb_vec2_t){u1,v1}, scale);

	draw__add_vertex_uv_scale((skb_vec2_t){ x0, y0 }, tint, (skb_vec2_t){u0,v0}, scale);
	draw__add_vertex_uv_scale((skb_vec2_t){ x1, y1 }, tint, (skb_vec2_t){u1,v1}, scale);
	draw__add_vertex_uv_scale((skb_vec2_t){ x0, y1 }, tint, (skb_vec2_t){u0,v1}, scale);

	g_context.sdf_id = 0;
}


static float skb__wrap_offset(float x)
{
	// Wrap the offset so that it is always between -1..0
	const float mx = fmodf(x, 1.f);
	return mx > 0.f ? mx - 1.f : mx;
}

void draw_image_pattern_quad_sdf(skb_rect2_t geom, skb_rect2_t pattern, skb_rect2_t image, float scale, skb_color_t tint, uint32_t tex_id)
{
	// In order to draw the pattern, we need to tile the part of the textture described by "image" rectangle.
	// This could be done on the shader, but will require us to pass the mapping to the shader (e.g. via vertex attribs).
	// In order to keep the vertex format and pixel shader simple, here we instead slice quad into smaller pieces which can be rendered with the simpler px shader.

	//
	// sx  [==================] geom
	//  +-----+-----+-----+-----+  repeat_x
	//     |--+-----+-----+---|	(ux,uwidth)
	//        [====] image

	const float sx = skb__wrap_offset(pattern.x);
	const float sy = skb__wrap_offset(pattern.y);
	const int32_t repeat_x = (int32_t)ceilf(skb_absf(sx) + pattern.width);
	const int32_t repeat_y = (int32_t)ceilf(skb_absf(sy) + pattern.height);

	// Size of the image to repeat.
	const float unit_pat_width = geom.width / pattern.width;
	const float unit_pat_height	= geom.height / pattern.height;

	for (int32_t y = 0; y < repeat_y; y++) {
		for (int32_t x = 0; x < repeat_x; x++) {
			// Start position in normalized coordinates
			const float cx = sx + (float)x;
			const float cy = sy + (float)y;
			// Position and size of the current tile on normalized coordinates.
			const float ux = skb_maxf(0.f, cx);
			const float uy = skb_maxf(0.f, cy);
			const float uwidth = skb_minf(cx + 1.f, pattern.width) - ux;
			const float uheight = skb_minf(cy + 1.f, pattern.height) - uy;

			// Quad to draw
			skb_rect2_t sub_geom = {
				.x = geom.x + ux * unit_pat_width,
				.y = geom.y + uy * unit_pat_height,
				.width = uwidth * unit_pat_width,
				.height = uheight * unit_pat_height,
			};

			// Portion of image to map to the quad.
			// The first tile of row or col may be truncated from the start edge, otherwise we always truncate from end.
			skb_rect2_t sub_image = {
				.x = image.x + skb_maxf(0.f, -cx) * image.width,
				.y = image.y + skb_maxf(0.f, -cy) * image.height,
				.width = uwidth * image.width,
				.height = uheight * image.height,
			};

			draw_image_quad_sdf(sub_geom, sub_image, scale, tint, tex_id);
		}
	}
}

static void draw__poly_bounds_pt(float x, float y)
{
	g_context.poly_bounds = skb_rect2_union_point(g_context.poly_bounds, (skb_vec2_t){x,y});
}

void draw_path_begin()
{
	if (g_context.in_polygon) return;
	g_context.stencil = STENCIL_WINDING;
	g_context.pen.x = 0.f;
	g_context.pen.y = 0.f;
	g_context.poly_bounds = skb_rect2_make_undefined();
	g_context.in_polygon = true;
}

void draw_path_move_to(float x, float y)
{
	if (!g_context.in_polygon) return;
	g_context.start.x = x;
	g_context.start.y = y;
	g_context.pen.x = x;
	g_context.pen.y = y;
	draw__poly_bounds_pt(x, y);
}

void draw_path_line_to(float x1, float y1)
{
	if (!g_context.in_polygon) return;

	draw_tri(g_context.start.x, g_context.start.y, g_context.pen.x, g_context.pen.y, x1,y1, skb_rgba(255,0,0,255));

	g_context.pen.x = x1;
	g_context.pen.y = y1;
	draw__poly_bounds_pt(x1, y1);
}

static void draw__quad_bezier_fill(float cx, float cy, float x0, float y0, float x1, float y1, float x2, float y2, int level, float dist_tol_sqr)
{
	if (level > 10) return;

	const float dx = x2 - x0;
	const float dy = y2 - y0;
	const float d = fabsf(((x1 - x2) * dy - (y1 - y2) * dx));

	if ((d * d) < dist_tol_sqr * (dx*dx + dy*dy)) {
		draw_tri(cx,cy, x0,y0,x2,y2,skb_rgba(255,0,0,255));
		return;
	}

	const float x01 = (x0+x1)*0.5f;
	const float y01 = (y0+y1)*0.5f;
	const float x12 = (x1+x2)*0.5f;
	const float y12 = (y1+y2)*0.5f;
	const float x012 = (x01+x12)*0.5f;
	const float y012 = (y01+y12)*0.5f;

	draw__quad_bezier_fill(cx,cy, x0,y0, x01,y01, x012,y012, level+1, dist_tol_sqr);
	draw__quad_bezier_fill(cx,cy, x012,y012, x12,y12, x2,y2, level+1, dist_tol_sqr);
}

void draw_path_quad_to(float x1, float y1, float x2, float y2)
{
	if (!g_context.in_polygon) return;

	const float dist_tol_sqr = 0.75f * 0.75f;

	draw__quad_bezier_fill(g_context.start.x, g_context.start.y, g_context.pen.x, g_context.pen.y, x1,y1, x2,y2, 0, dist_tol_sqr);

	g_context.pen.x = x2;
	g_context.pen.y = y2;
	draw__poly_bounds_pt(x1, y1);
	draw__poly_bounds_pt(x2, y2);
}


static void draw__cubic_bezier_fill(float cx, float cy, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int level, float dist_tol_sqr)
{
	if (level > 10) return;

	float dx = x3 - x0;
	float dy = y3 - y0;
	float d2 = fabsf(((x1 - x3) * dy - (y1 - y3) * dx));
	float d3 = fabsf(((x2 - x3) * dy - (y2 - y3) * dx));

	if ((d2 + d3)*(d2 + d3) < dist_tol_sqr * (dx*dx + dy*dy)) {
		draw_tri(cx,cy, x0,y0, x3,y3,skb_rgba(255,0,0,255));
		return;
	}

	float x01 = (x0+x1)*0.5f;
	float y01 = (y0+y1)*0.5f;
	float x12 = (x1+x2)*0.5f;
	float y12 = (y1+y2)*0.5f;
	float x23 = (x2+x3)*0.5f;
	float y23 = (y2+y3)*0.5f;
	float x012 = (x01+x12)*0.5f;
	float y012 = (y01+y12)*0.5f;
	float x123 = (x12+x23)*0.5f;
	float y123 = (y12+y23)*0.5f;
	float x0123 = (x012+x123)*0.5f;
	float y0123 = (y012+y123)*0.5f;

	draw__cubic_bezier_fill(cx,cy, x0,y0, x01,y01, x012,y012, x0123,y0123, level+1, dist_tol_sqr);
	draw__cubic_bezier_fill(cx,cy, x0123,y0123, x123,y123, x23,y23, x3,y3, level+1, dist_tol_sqr);
}
void draw_path_cubic_to(float x1, float y1, float x2, float y2, float x3, float y3)
{
	if (!g_context.in_polygon) return;

	const float dist_tol_sqr = 0.75f * 0.75f;

	draw__cubic_bezier_fill(g_context.start.x, g_context.start.y, g_context.pen.x, g_context.pen.y, x1,y1, x2,y2, x3,y3, 0, dist_tol_sqr);

	g_context.pen.x = x3;
	g_context.pen.y = y3;
	draw__poly_bounds_pt(x1, y1);
	draw__poly_bounds_pt(x2, y2);
	draw__poly_bounds_pt(x3, y3);
}

void draw_path_end(skb_color_t col)
{
	if (!g_context.in_polygon) return;

	g_context.in_polygon = false;

	if (!skb_rect2_is_empty(g_context.poly_bounds)) {
		g_context.stencil = STENCIL_FILL;

		// Draw quad to fill the polygon
		const float min_x = g_context.poly_bounds.x;
		const float min_y = g_context.poly_bounds.y;
		const float max_x = g_context.poly_bounds.x + g_context.poly_bounds.width;
		const float max_y = g_context.poly_bounds.y + g_context.poly_bounds.height;
		draw_tri(min_x,min_y, max_x,min_y,max_x,max_y, col);
		draw_tri(min_x,min_y, max_x,max_y,min_x,max_y, col);
	}

	g_context.stencil = STENCIL_DISABLED;
}


void draw_flush(int w, int h)
{
	if (g_context.verts_count == 0 || g_context.batches_count == 0) {
		g_context.verts_count = 0;
		g_context.batches_count = 0;
		return;
	}

	draw__check_error("start");

	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premult alpha
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	draw__check_error("state");

	glUseProgram(g_context.program);
	glUniform2f(glGetUniformLocation(g_context.program, "viewsize"), (float)w, (float)h);

	draw__check_error("prog");

	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	draw__check_error("vao");

	glBindBuffer(GL_ARRAY_BUFFER, g_context.vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(draw__vertex_t), (GLvoid*)(0 + offsetof(draw__vertex_t, pos)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(draw__vertex_t), (GLvoid*)(0 + offsetof(draw__vertex_t, uv)));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(draw__vertex_t), (GLvoid*)(0 + offsetof(draw__vertex_t, color)));
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(draw__vertex_t), (GLvoid*)(0 + offsetof(draw__vertex_t, scale)));
	glBufferData(GL_ARRAY_BUFFER, g_context.verts_count * sizeof(draw__vertex_t), g_context.verts, GL_STREAM_DRAW);

	draw__check_error("vbo");

	// Close last batch
	g_context.batches[g_context.batches_count-1].count = g_context.verts_count - g_context.batches[g_context.batches_count-1].offset;

	int32_t prev_stencil = -1;
	for (int32_t i = 0; i < g_context.batches_count; i++) {
		const draw__batch_t* b = &g_context.batches[i];
		if (b->stencil != prev_stencil) {
			if (b->stencil == STENCIL_WINDING) {
				// For drawing polygon winding
				glEnable(GL_STENCIL_TEST);
				glStencilMask(0xff);
				glStencilFunc(GL_ALWAYS, 0, 0xff);
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
				glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
			} else if (b->stencil == STENCIL_FILL) {
				// For drawing polygon fill
				glEnable(GL_STENCIL_TEST);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glStencilFunc(GL_NOTEQUAL, 0x0, 0xff);
				glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
			} else {
				// Normal drawing, disable stencil stuff.
				glDisable(GL_STENCIL_TEST);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
			prev_stencil = b->stencil;
		}

		const draw__texture_t* image_tex = draw__find_texture(b->image_id);
		const draw__texture_t* sdf_tex = draw__find_texture(b->sdf_id);

		if (image_tex && image_tex->tex_id) {
			glUniform1i(glGetUniformLocation(g_context.program, "tex_bpp"), image_tex->bpp);
			glUniform1i(glGetUniformLocation(g_context.program, "tex"), 0);
			glUniform1i(glGetUniformLocation(g_context.program, "tex_sdf"), 0);
			glUniform2f(glGetUniformLocation(g_context.program, "tex_size"), image_tex->width, image_tex->height);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, image_tex->tex_id);
			draw__check_error("tex2");
		} else if (sdf_tex && sdf_tex->tex_id) {
			glUniform1i(glGetUniformLocation(g_context.program, "tex_bpp"), sdf_tex->bpp);
			glUniform1i(glGetUniformLocation(g_context.program, "tex"), 0);
			glUniform1i(glGetUniformLocation(g_context.program, "tex_sdf"), 1);
			glUniform2f(glGetUniformLocation(g_context.program, "tex_size"), sdf_tex->width, sdf_tex->height);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sdf_tex->tex_id);
			draw__check_error("tex2");
		} else {
			glUniform1i(glGetUniformLocation(g_context.program, "tex_bpp"), 0);
			glUniform2f(glGetUniformLocation(g_context.program, "tex_size"), 1.f, 1.f);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
			draw__check_error("no tex");
		}

		glLineWidth(b->line_width);

		glDrawArrays(b->prim, b->offset, b->count);
	}

	draw__check_error("draw");

	//    glUseProgram(0);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);

	draw__check_error("end");

	g_context.verts_count = 0;
	g_context.batches_count = 0;
	g_context.line_width = 0.f;
}
