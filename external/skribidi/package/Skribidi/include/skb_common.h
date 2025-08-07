// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef SKB_COMMON_H
#define SKB_COMMON_H

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup common Common
 * Common functionality used across the different features.
 * @{
 */

// TODO: this feels out of place here, but it is needed in multiple places.
/** Enum describing how the decoration should be drawn. */
typedef enum {
	/** Solid line. */
	SKB_DECORATION_STYLE_SOLID = 0,
	/** Double line. */
	SKB_DECORATION_STYLE_DOUBLE,
	/** Dotted line. */
	SKB_DECORATION_STYLE_DOTTED,
	/** Dashed line. */
	SKB_DECORATION_STYLE_DASHED,
	/** Wave line. */
	SKB_DECORATION_STYLE_WAVY,
} skb_decoration_style_t;

/**
 * Logs a debug message.
 * @param format printf style format string.
 * @param ... paramters to format.
 */
void skb_debug_log(const char* format, ...);

/** Counts the number of items in an array. */
#define SKB_COUNTOF(arr) (sizeof((arr)) / sizeof((arr)[0]))

/** Used to mark unused function parameters. */
#define SKB_UNUSED(x) (void)(x)

/** Max 32bit floating point number */
#define SKB_FLT_MAX (3.402823466e+38f)

/** Math constant PI */
#define SKB_PI (3.14159265f)

enum {
	/** Invalid value used for indices. */
	SKB_INVALID_INDEX = -1,
};

/** Defines fourcc tag */
#define SKB_TAG(c1, c2, c3, c4) ((((uint32_t)(c1)&0xff)<<24) | (((uint32_t)(c2) & 0xff)<<16) | (((uint32_t)(c3)&0xff)<<8) | ((uint32_t)(c4)&0xff))

/** Defines fourcc tag from a string */
#define SKB_TAG_STR(tag) SKB_TAG((tag)[0], (tag)[1], (tag)[2], (tag)[3])

/** "Untags" a fourcc tag, useful e.g. for debug printing. */
#define SKB_UNTAG(tag)   (uint8_t)(((tag)>>24)&0xFF), (uint8_t)(((tag)>>16)&0xFF), (uint8_t)(((tag)>>8)&0xFF), (uint8_t)((tag)&0xFF)

/**
 * Allocates memory.
 * The malloc will assert if the allocation failed.
 * @param size number of bytes to allocate
 * @return pointer to the allocated memory.
 */
void* skb_malloc(size_t size);

/**
 * Reallocates existing memory to fit new size.
 * @param ptr pointer to existing memory to allocate, if NULL, realloc will behave like malloc
 * @param new_size new size
 * @return return pointer to the allocated memory
 */
void* skb_realloc(void* ptr, size_t new_size);

/**
 * Frees previously allocated memory.
 * @param ptr pointer to the memory to free.
 */
void skb_free(void* ptr);

/** Signature of destroy function */
typedef void skb_destroy_func_t(void *context);

/**
 * Helper macro to reserve space in an allocated array.
 * The macro relies on naming convention, if your array is called 'apples', then it is expected
 * that there is variable 'apples_cap' which is the current allocation capacity (in items).
 * The array is grown so that it can fit N items, and the new items are initialzed to 0.
 * @param arr array to grow
 * @param N number of items to reserve
 */
#define SKB_ARRAY_RESERVE(arr, N) \
	if ((N) > arr##_cap) { \
		const int32_t new_cap = skb_maxi((N), arr##_cap ? (arr##_cap + arr##_cap/2) : 4); \
		(arr) = skb_realloc((arr), sizeof((arr)[0]) * new_cap); \
		assert(arr); \
		memset(&(arr)[arr##_cap], 0, sizeof((arr)[0]) * (new_cap - arr##_cap)); \
		arr##_cap = new_cap; \
	}

/**
 * @defgroup math Math
 * Common math functions for different types.
 * @{
 */
static inline float skb_minf(float a, float b) { return a < b ? a : b; }
static inline float skb_maxf(float a, float b) { return a > b ? a : b; }
static inline float skb_absf(float a) { return a < 0.f ? -a : a; }
static inline float skb_clampf(float a, float mn, float mx) { return skb_minf(skb_maxf(a, mn), mx); }
static inline float skb_lerpf(float a, float b, float t) { return a + t * (b - a); }
static inline float skb_squaref(float x) { return x * x; }
static inline int32_t skb_mini(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t skb_maxi(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t skb_absi(int32_t a) { return a < 0 ? -a : a; }
static inline int32_t skb_clampi(int32_t a, int32_t mn, int mx) { return skb_mini(skb_maxi(a, mn), mx); }
static inline uint32_t skb_minu(uint32_t a, uint32_t b) { return a < b ? a : b; }
static inline uint32_t skb_maxu(uint32_t a, uint32_t b) { return a > b ? a : b; }
static inline uint32_t skb_clampu(uint32_t a, uint32_t mn, uint32_t mx) { return skb_minu(skb_maxu(a, mn), mx); }

static inline int32_t skb_mul255(int32_t a, int32_t b)
{
	a *= b;
	a += 0x80;
	a += a >> 8;
	return a >> 8;
}

static inline int skb_align(int32_t v, int32_t align)
{
	return (v + align - 1) & ~(align - 1);
}

static inline void skb_swapf(float* a, float* b)
{
	float tmp = *a;
	*a = *b;
	*b = tmp;
}

static inline void skb_swapi(int32_t* a, int32_t* b)
{
	int32_t tmp = *a;
	*a = *b;
	*b = tmp;
}

static inline int32_t skb_next_pow2(int32_t v)
{
	assert(v >= 0 && v < 0x40000000);
	if (v == 0) return 1;
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

static inline bool skb_equalsf(float lhs, float rhs, float eps)
{
	return skb_absf(lhs - rhs) < eps;
}


/** Range - defines range of items, end is non-inclusive. */
typedef struct skb_range_t {
	/** Start of the range (inclusive). */
	int32_t start;
	/** End of the range (non-inclusive) */
	int32_t end;
} skb_range_t;


/** @return true if the ranges overlap. */
static inline bool skb_range_overlap(skb_range_t a, skb_range_t b)
{
	return skb_maxi(a.start, b.start) < skb_mini(a.end, b.end);
}

/** @return true if the range contains the index. */
static inline bool skb_range_contains(skb_range_t r, int32_t idx)
{
	return idx >= r.start && idx < r.end;
}


/** 8-bits per component color, sRGB. */
typedef struct skb_color_t {
	uint8_t r,g,b,a;
} skb_color_t;

/** @returns new color constructed from RGBA components. */
static inline skb_color_t skb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	skb_color_t res;
	res.r = r;
	res.g = g;
	res.b = b;
	res.a = a;
	return res;
}

/** @return true if the colors are equal. */
static inline bool skb_color_equals(skb_color_t lhs, skb_color_t rhs)
{
	const uint32_t u = *(uint32_t*)&lhs;
	const uint32_t v = *(uint32_t*)&rhs;
	return u == v;
}

/** @return the color multiplied by the alpha (per component). */
static inline skb_color_t skb_color_mul_alpha(skb_color_t col, uint8_t alpha)
{
	const uint32_t u = *(uint32_t*)&col;
	uint32_t rb = u & 0x00ff00ff;
	rb *= (uint32_t)alpha;
	rb += 0x00800080;
	rb += (rb >> 8) & 0x00ff00ff;
	rb &= 0xff00ff00;

	uint32_t ga = (u >> 8) & 0x00ff00ff;
	ga *= (uint32_t)alpha;
	ga += 0x00800080;
	ga += (ga >> 8) & 0x00ff00ff;
	ga &= 0xff00ff00;

	const uint32_t r = ga | (rb >> 8);
	return *(const skb_color_t*)&r;
}

/** @return the colors added together (per component). */
static inline skb_color_t skb_color_add(skb_color_t a, skb_color_t b)
{
	const uint32_t au = *(uint32_t*)&a;
	const uint32_t bu = *(uint32_t*)&b;

	uint32_t rb = au & 0x00ff00ff;
	rb += bu & 0x00ff00ff;
	rb &= 0x00ff00ff;

	uint32_t ga = (au >> 8) & 0x00ff00ff;
	ga += (bu >> 8) & 0x00ff00ff;
	ga &= 0x00ff00ff;

	const uint32_t r = (ga << 8) | rb;
	return *(const skb_color_t*)&r;
}

/** @return the average of the two colors. */
static inline skb_color_t skb_color_average(skb_color_t a, skb_color_t b)
{
	uint32_t au = *(uint32_t*)&a;
	uint32_t bu = *(uint32_t*)&b;
	uint32_t acc = (au & 0x01010101) + (bu & 0x01010101);
	au = (au >> 1) & 0x7f7f7f7f;
	bu = (bu >> 1) & 0x7f7f7f7f;
	acc = (acc >> 1) & 0x7f7f7f7f;

	const uint32_t r = au + bu + acc;

	return *(const skb_color_t*)&r;
}

/** @return linearly interpolate color between a and b. */
static inline skb_color_t skb_color_lerp(skb_color_t a, skb_color_t b, uint8_t t)
{
	b = skb_color_mul_alpha(b, t);
	if (b.a == 255)
		return b;
	if (b.a == 0)
		return a;
	a = skb_color_mul_alpha(a, 255 - b.a);
	return skb_color_add(a, b);
}

/** @return linearly interpolate color between a and b (floating point t). */
static inline skb_color_t skb_color_lerpf(skb_color_t a, skb_color_t b, float t)
{
	const int32_t ti = (int32_t)(t * 255.0f);
	return skb_color_lerp(a, b, (uint8_t)ti);
}

/** @return dst blended over src (premultiplied colors). */
static inline skb_color_t skb_color_blend(skb_color_t dst, skb_color_t src)
{
	dst = skb_color_mul_alpha(dst, 255 - src.a);
	return skb_color_add(dst, src);
}

/** @return premultiplied color of col (RGB components multiplied by alpha). */
static inline skb_color_t skb_color_premult(skb_color_t col)
{
	const int32_t a = col.a;
	const int32_t r = skb_mul255(col.r, a);
	const int32_t g = skb_mul255(col.g, a);
	const int32_t b = skb_mul255(col.b, a);
	skb_color_t res;
	res.r = (uint8_t)r;
	res.g = (uint8_t)g;
	res.b = (uint8_t)b;
	res.a = (uint8_t)a;
	return res;
}


/** RGBA or alpha image. The RGBA image is sRGB colorspace with premultiplied alpha. */
typedef struct skb_image_t {
	/** buffer that stores the pixels */
	uint8_t* buffer;
	/** width of the image in pixels */
	int32_t width;
	/** height of the image in pixeles */
	int32_t height;
	/** number of bytes between rows, must to multiple of 'bpp'. */
	int32_t stride_bytes;
	/** bytes per pixel, 4 = RGBA, 1 = Alpha */
	uint8_t bpp;
} skb_image_t;


/** 2D floating point vector */
typedef struct skb_vec2_t {
	float x, y;
} skb_vec2_t;

static inline skb_vec2_t skb_vec2_make(float x, float y)
{
	skb_vec2_t res;
	res.x = x;
	res.y = y;
	return res;
}

static inline skb_vec2_t skb_vec2_add(skb_vec2_t a, skb_vec2_t b)
{
	skb_vec2_t res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	return res;
}

static inline skb_vec2_t skb_vec2_sub(skb_vec2_t a, skb_vec2_t b)
{
	skb_vec2_t res;
	res.x = a.x - b.x;
	res.y = a.y - b.y;
	return res;
}

static inline skb_vec2_t skb_vec2_scale(skb_vec2_t a, float s)
{
	skb_vec2_t res;
	res.x = a.x * s;
	res.y = a.y * s;
	return res;
}

static inline skb_vec2_t skb_vec2_mad(skb_vec2_t a, skb_vec2_t b, float s)
{
	skb_vec2_t res;
	res.x = a.x + b.x * s;
	res.y = a.y + b.y * s;
	return res;
}

static inline skb_vec2_t skb_vec2_lerp(skb_vec2_t a, skb_vec2_t b, float t)
{
	skb_vec2_t res;
	res.x = a.x + (b.x - a.x) * t;
	res.y = a.y + (b.y - a.y) * t;
	return res;
}

static inline float skb_vec2_dot(skb_vec2_t a, skb_vec2_t b)
{
	return a.x * b.x + a.y * b.y;
}

static inline float skb_vec2_length(skb_vec2_t a)
{
	return sqrtf(a.x*a.x + a.y*a.y);
}

static inline float skb_vec2_dist_sqr(skb_vec2_t a, skb_vec2_t b)
{
	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	return dx * dx + dy * dy;
}

static inline float skb_vec2_dist(skb_vec2_t a, skb_vec2_t b)
{
	return sqrtf(skb_vec2_dist_sqr(a, b));
}

static inline bool skb_vec2_equals(skb_vec2_t a, skb_vec2_t b, float tol)
{
	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	return dx*dx + dy*dy < tol*tol;
}


/** 2D affine transformation matrix. */
typedef struct skb_mat2_t {
	float xx, yx;
	float xy, yy;
	float dx, dy;
} skb_mat2_t;

static inline skb_mat2_t skb_mat2_make_identity()
{
	skb_mat2_t res;
	res.xx = 1.0f;
	res.yx = 0.0f;
	res.xy = 0.0f;
	res.yy = 1.0f;
	res.dx = 0.0f;
	res.dy = 0.0f;
	return res;
}

static inline skb_mat2_t skb_mat2_make_translation(float tx, float ty)
{
	skb_mat2_t res;
	res.xx = 1.0f;
	res.yx = 0.0f;
	res.xy = 0.0f;
	res.yy = 1.0f;
	res.dx = tx;
	res.dy = ty;
	return res;
}

static inline skb_mat2_t skb_mat2_make_scale(float sx, float sy)
{
	skb_mat2_t res;
	res.xx = sx;
	res.yx = 0.0f;
	res.xy = 0.0f;
	res.yy = sy;
	res.dx = 0.0f;
	res.dy = 0.0f;
	return res;
}

static inline skb_mat2_t skb_mat2_make_rotation(float a)
{
	const float cs = cosf(a), sn = sinf(a);
	skb_mat2_t res;
	res.xx = cs;
	res.yx = sn;
	res.xy = -sn;
	res.yy = cs;
	res.dx = 0.0f;
	res.dy = 0.0f;
	return res;
}

static inline skb_mat2_t skb_mat2_multiply(skb_mat2_t t, skb_mat2_t s)
{
	skb_mat2_t res;
	res.xx = t.xx * s.xx + t.yx * s.xy;
	res.yx = t.xx * s.yx + t.yx * s.yy;
	res.xy = t.xy * s.xx + t.yy * s.xy;
	res.yy = t.xy * s.yx + t.yy * s.yy;
	res.dx = t.dx * s.xx + t.dx * s.xy + s.dx;
	res.dy = t.dy * s.yx + t.dy * s.yy + s.dy;
	return res;
}

static inline skb_vec2_t skb_mat2_point(skb_mat2_t t, skb_vec2_t pt)
{
	skb_vec2_t res;
	res.x = pt.x * t.xx + pt.y * t.xy + t.dx;
	res.y = pt.x * t.yx + pt.y * t.yy + t.dy;
	return res;
}

skb_mat2_t skb_mat2_inverse(skb_mat2_t t);


/** 2D floating point rectangle.  */
typedef struct skb_rect2_t {
	float x;
	float y;
	float width;
	float height;
} skb_rect2_t;

/** Makes undefined rectangle. Undefined rectangle is set up so that it is empty, and allows efficiently union with points and other rectangles. */
static inline skb_rect2_t skb_rect2_make_undefined(void)
{
	// These values are chosen so that min_x = SKB_FLT_MAX / 2.f and max_x = -SKB_FLT_MAX / 2.f,
	// which allows to call union on an initially invalid rect.
	skb_rect2_t res;
	res.x = SKB_FLT_MAX / 2.f;
	res.y = SKB_FLT_MAX / 2.f;
	res.width = -SKB_FLT_MAX;
	res.height = -SKB_FLT_MAX;
	return res;
}

static inline skb_rect2_t skb_rect2_union_point(skb_rect2_t r, skb_vec2_t pt)
{
	const float min_x = skb_minf(r.x, pt.x);
	const float min_y = skb_minf(r.y, pt.y);
	const float max_x = skb_maxf(r.x + r.width, pt.x);
	const float max_y = skb_maxf(r.y + r.height, pt.y);
	skb_rect2_t res;
	res.x = min_x;
	res.y = min_y;
	res.width = max_x - min_x;
	res.height = max_y - min_y;
	return res;
}

static inline skb_rect2_t skb_rect2_union(const skb_rect2_t a, const skb_rect2_t b)
{
	const float min_x = skb_minf(a.x, b.x);
	const float min_y = skb_minf(a.y, b.y);
	const float max_x = skb_maxf(a.x + a.width, b.x + b.width);
	const float max_y = skb_maxf(a.y + a.height, b.y + b.height);
	skb_rect2_t res;
	res.x = min_x;
	res.y = min_y;
	res.width = max_x - min_x;
	res.height = max_y - min_y;
	return res;
}

static inline skb_rect2_t skb_rect2_intersection(const skb_rect2_t a, const skb_rect2_t b)
{
	const float min_x = skb_maxf(a.x, b.x);
	const float min_y = skb_maxf(a.y, b.y);
	const float max_x = skb_minf(a.x + a.width, b.x + b.width);
	const float max_y = skb_minf(a.y + a.height, b.y + b.height);
	skb_rect2_t res;
	res.x = min_x;
	res.y = min_y;
	res.width = max_x - min_x;
	res.height = max_y - min_y;
	return res;
}

static inline skb_rect2_t skb_rect2_translate(const skb_rect2_t r, const skb_vec2_t d)
{
	skb_rect2_t res;
	res.x = r.x + d.x;
	res.y = r.y + d.y;
	res.width = r.width;
	res.height = r.height;
	return res;
}

static inline bool skb_rect2_is_empty(const skb_rect2_t r)
{
	return r.width <= 0.f || r.height <= 0.f;
}

static inline bool skb_rect2_pt_inside(const skb_rect2_t r, const skb_vec2_t pt)
{
	return pt.x >= r.x && pt.y >= r.y && pt.x <= (r.x + r.width) && pt.y <= (r.y + r.height);
}


/** 2D integer rectangle. */
typedef struct skb_rect2i_t {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
} skb_rect2i_t;

/** Makes undefined rectangle. Undefined rectangle is set up so that it is empty, and allows efficiently union with points and other rectangles. */
static inline skb_rect2i_t skb_rect2i_make_undefined(void)
{
	skb_rect2i_t res;
	res.x = INT32_MAX/2;
	res.y = INT32_MAX/2;
	res.width = INT32_MIN;
	res.height = INT32_MIN;
	return res;
}

static inline skb_rect2i_t skb_rect2i_union_point(skb_rect2i_t r, int32_t x, int32_t y)
{
	const int32_t min_x = skb_mini(r.x, x);
	const int32_t min_y = skb_mini(r.y, y);
	const int32_t max_x = skb_maxi(r.x + r.width, x);
	const int32_t max_y = skb_maxi(r.y + r.height, y);
	skb_rect2i_t res;
	res.x = min_x;
	res.y = min_y;
	res.width = max_x - min_x;
	res.height = max_y - min_y;
	return res;
}

static inline skb_rect2i_t skb_rect2i_union(const skb_rect2i_t a, const skb_rect2i_t b)
{
	const int32_t min_x = skb_mini(a.x, b.x);
	const int32_t min_y = skb_mini(a.y, b.y);
	const int32_t max_x = skb_maxi(a.x + a.width, b.x + b.width);
	const int32_t max_y = skb_maxi(a.y + a.height, b.y + b.height);
	skb_rect2i_t res;
	res.x = min_x;
	res.y = min_y;
	res.width = max_x - min_x;
	res.height = max_y - min_y;
	return res;
}

static inline skb_rect2i_t skb_rect2i_intersection(const skb_rect2i_t a, const skb_rect2i_t b)
{
	const int32_t min_x = skb_maxi(a.x, b.x);
	const int32_t min_y = skb_maxi(a.y, b.y);
	const int32_t max_x = skb_mini(a.x + a.width, b.x + b.width);
	const int32_t max_y = skb_mini(a.y + a.height, b.y + b.height);
	skb_rect2i_t res;
	res.x = min_x;
	res.y = min_y;
	res.width = max_x - min_x;
	res.height = max_y - min_y;
	return res;
}

static inline bool skb_rect2i_is_empty(const skb_rect2i_t r)
{
	return r.width <= 0 || r.height <= 0;
}

/** @} */


/**
 * @defgroup hash Hash
 * 64 bit FNV1a hash with various utilities
 * @{
 */

/** @returns hash for empty content, can be used to initialize a hash value which will be conditionally appended to. */
static inline uint64_t skb_hash64_empty(void)
{
	return 0xcbf29ce484222325;
}

static inline uint64_t skb_hash64_append(uint64_t hash, const void* key, const size_t len)
{
	if (!key) return hash;
	static const uint64_t prime = 0x100000001b3;
	const uint8_t* data = (const uint8_t*)key;
	for (size_t i = 0; i < len; ++i) {
		const uint8_t value = data[i];
		hash = hash ^ value;
		hash *= prime;
	}
	return hash;
}

static inline uint64_t skb_hash64_append_float(uint64_t hash, float value)
{
	return skb_hash64_append(hash, &value, sizeof(value));
}

static inline uint64_t skb_hash64_append_int32(uint64_t hash, int32_t value)
{
	return skb_hash64_append(hash, &value, sizeof(value));
}

static inline uint64_t skb_hash64_append_uint32(uint64_t hash, uint32_t value)
{
	return skb_hash64_append(hash, &value, sizeof(value));
}

static inline uint64_t skb_hash64_append_uint64(uint64_t hash, uint64_t value)
{
	return skb_hash64_append(hash, &value, sizeof(value));
}

static inline uint64_t skb_hash64_append_uint8(uint64_t hash, uint8_t value)
{
	return skb_hash64_append(hash, &value, sizeof(value));
}

static inline uint64_t skb_hash64_append_str(uint64_t hash, const char* key)
{
	if (!key) return hash;
	static const uint64_t prime = 0x100000001b3;
	while (*key) {
		const uint8_t value = (uint8_t)*key;
		hash = hash ^ value;
		hash *= prime;
		key++;
	}
	return hash;
}

/** @} */


/**
 * @defgroup tempalloc Temp Alloc
 * Temporary allocator, bump allocator which with pages. Not thread safe.
 * @{
 */

enum {
	/** Default block size if none specified */
	SKB_TEMPALLOC_DEFAULT_BLOCK_SIZE = (128*1024),
	/** Alignment for all allocations. */
	SKB_TEMPALLOC_ALIGN = 8,
};

/** Opaque type for the temp allocator. Use skb_temp_alloc_create() to create. */
typedef struct skb_temp_alloc_t skb_temp_alloc_t;

/** Mark used to store the allocator state. Can be used to rewind the allocator. */
typedef struct skb_temp_alloc_mark_t {
	int32_t block_num;
	int32_t offset;
} skb_temp_alloc_mark_t;

/** Statistics about the current state of the allocator. */
typedef struct skb_temp_alloc_stats_t {
	/** Number of bytes allocated for the buffers. */
	int32_t allocated;
	/** Number of bytes currently allocated. */
	int32_t used;
} skb_temp_alloc_stats_t;

/**
 * Initializes allocator.
 * @param alloc allocator to init
 * @param default_block_size default blocks size to use, unless more memory is requested.
 * @return pointer to newly created allocator.
 */
skb_temp_alloc_t* skb_temp_alloc_create(int32_t default_block_size);

/**
 * Frees all memory allocated by the allocator and clears the allocator.
 * @param alloc allocator to destroy
 */
void skb_temp_alloc_destroy(skb_temp_alloc_t* alloc);

/**
 * Reports statistics about the current allocation.
 * @param alloc allocator to report
 * @return struct containing statistics about the current state of the allocator.
 */
skb_temp_alloc_stats_t skb_temp_alloc_stats(skb_temp_alloc_t* alloc);

/**
 * Resets all the allocated memory blocks as free.
 * @param alloc allocator to reset
 */
void skb_temp_alloc_reset(skb_temp_alloc_t* alloc);

/**
 * Returns a mark, which can be used to restore the allocators state later.
 * This function can be used together with skb_tempalloc_restore() to release all allocations between the save and restore.
 * Use with caution.
 * @param alloc allocator which location to save
 * @return mark which can be used to restore the allocator to current state later
 */
skb_temp_alloc_mark_t skb_temp_alloc_save(skb_temp_alloc_t* alloc);

/**
 * Restores the alloctors state to a previous set mark.
 * If memory has been freed past the allocation mark, restore will not do anything.
 * @param alloc allocator to restore
 * @param mark state to restore
 */
void skb_temp_alloc_restore(skb_temp_alloc_t* alloc, skb_temp_alloc_mark_t mark);

/**
 * Allocates a requested size of memory. The returned memory is aligned to SKB_TEMPALLOC_ALIGN.
 * @param alloc allocator to allocate from.
 * @param size size of the allocation in bytes.
 * @return pointer to the allocated memory.
 */
void* skb_temp_alloc_alloc(skb_temp_alloc_t* alloc, int32_t size);

/**
 * Reallocates existing allocation to new size. The returned memory is aligned to SKB_TEMPALLOC_ALIGN.
 * If the allocation is the last allocation, and fits into the current block, the allocation is grown in situ.
 * Otherwise, new memory is allocated and old data copied over.
 * @param alloc allocator to allocate from.
 * @param ptr pointer to prevoous allocation (can be NULL, in which case skb_tempalloc_alloc() is called).
 * @param new_size new size of the allocation in bytes.
 * @return pointer to the allocated memory.
 */
void* skb_temp_alloc_realloc(skb_temp_alloc_t* alloc, void* ptr, int32_t new_size);

/**
 * Frees allocated memory.
 * If the allocation was the last allocation, the used memory is returned as free memory.
 * Matching allocations are frees carefully, allows the allocator to be used as a stack, reducing needed memory.
 * @param alloc allocator where the memory was allocated from.
 * @param ptr pointer to the allocation.
 */
void skb_temp_alloc_free(skb_temp_alloc_t* alloc, void* ptr);

/**
 * Helper macro to allocate number of items of specified type.
 * @param temp_alloc pointer to the temp allocator to use.
 * @param type type to allocate.
 * @param count numner of items to allocate.
 * @returns pointer to the allocated.
 */
#define SKB_TEMP_ALLOC(temp_alloc, type, count) \
	(type*)skb_temp_alloc_alloc(temp_alloc, (int32_t)sizeof(type) * (count))

/**
 * Helper macro to reallocate number of items of specified type.
 * @param temp_alloc pointer to the temp allocator to use
 * @param ptr pointer to the array to reallocate.
 * @param type type to allocate.
 * @param count numner of items to allocate.
 * @returns pointer to the resized array.
 */
#define SKB_TEMP_REALLOC(temp_alloc, ptr, type, count) \
	(type*)skb_temp_alloc_realloc(temp_alloc, ptr, (int32_t)sizeof(type)*(count))
/**
 * Helper macro to free array of items allocated with temp allocator.
 * @param temp_alloc pointer to the temp allocator to use.
 * @param ptr pointer to the array to free.
 */
#define SKB_TEMP_FREE(temp_alloc, ptr) \
	skb_temp_alloc_free(temp_alloc, ptr)

/**
 * Helper macro to reserve space in a temp allocated array.
 * The macro relies on naming convention, if your array is called 'apples', then it is expected
 * that there is variable 'apples_cap' which is the current allocation capacity (in items).
 * The array is grown so that it can fit N items, and the new items are initialized to 0.
 * @param arr array to grow
 * @param N number of items to reserve
 */
#define SKB_TEMP_RESERVE(temp_alloc, arr, N) \
	if ((N) > arr##_cap) { \
		const int32_t new_cap = skb_maxi((N), arr##_cap ? (arr##_cap + arr##_cap/2) : 4); \
		(arr) = skb_temp_alloc_realloc(temp_alloc, arr, sizeof((arr)[0]) * new_cap); \
		assert(arr); \
		memset(&(arr)[arr##_cap], 0, sizeof((arr)[0]) * (new_cap - arr##_cap)); \
		arr##_cap = new_cap; \
	}

/** @} */


/**
 * @defgroup hashtable Hashtable
 * Hashtable that can map 64bit hashed to 32bit ints. Not thread safe.
 * The hash table is generally used as an index to quickly locate data in a separate array.
 * @{
 */

/** Opaque type for the hash table. Use skb_hashtable_create() to create. */
typedef struct skb_hash_table_t skb_hash_table_t;

/**
 * Creates an empty hash table. Use skb_hashtable_destroy() to destroy the hash table.
 * @return initialized empty hash table.
 */
skb_hash_table_t* skb_hash_table_create(void);

/**
 * Cleans up hash table and frees any allocated memory.
 * @param ht hash table to clean.
 */
void skb_hash_table_destroy(skb_hash_table_t* ht);

/**
 * Adds 'value' with key 'hash' into the hash table.
 * If item with same hash exists, the value is updated and function returns true.
 * @param ht hash table where to add the item.
 * @param hash unique hash representing the data.
 * @param value data to place in the cache.
 * @return true if 'hash' already exists in the table.
 */
bool skb_hash_table_add(skb_hash_table_t* ht, uint64_t hash, int32_t value);

/**
 * Tries to get 'data' from hash table based on hash.
 * @param ht hash table where to look up the data from.
 * @param hash hash associated with the data.
 * @param value pointer to store the found value (can be NULL).
 * @return the true if found.
 */
bool skb_hash_table_find(skb_hash_table_t* ht, uint64_t hash, int32_t* value);

/**
 * Removes item from the hash table associated with 'hash'.
 * @param ht cache where the data is removed from.
 * @param hash hash associated with the data to remove.
 * @return true if data was removed.
 */
bool skb_hash_table_remove(skb_hash_table_t* ht, uint64_t hash);

/** @} */


/**
 * @defgroup list List
 * Doubly linked non-intrusive list. Uses indices as pointers to the backing store.
 * The list is meant to be used with an array, which stores all the items.
 * Can be used for example for LRU cache's latest list, where the list items are sotred in an array, whose address might change due to reallocation.
 *
 * ```
 *	struct foo_t {
 *		const char* filename;
 *		skb_list_item_t link;
 *	}
 *
 *  struct foo_array_t {
 *		foo_t* foos;
 *		int32_t foos_count;
 *		skb_list_t list;
 *	}
 *
 *	skb_list_item_t* get_foo(int32_t item_idx, void* context) {
 *		foo_array_t* arr = (foo_array_t*)context;
 *		assert(item_idx >= 0 && item_idx < arr->foos_count);
 *		return &arr->foos[item_idx].link;
 *	}
 *
 *	foo_array_t arr = {...};
 *	skb_list_move_to_front(&arr.list, item_idx, get_foo, &arr);
 *	```
 * @{
 */

/** Links to adjacent items, each item in the list should contain this struct. */
typedef struct skb_list_item_t {
	int32_t prev;
	int32_t next;
} skb_list_item_t;

/** Head and tail of the list */
typedef struct skb_list_t {
	int32_t head;
	int32_t tail;
} skb_list_t;

/** Signature of function used to return the links of a specified item. */
typedef skb_list_item_t* skb_list_get_item_func_t(int32_t item_idx, void* context);

/** @returm a list initialized to empty. */
static inline skb_list_t skb_list_make(void)
{
	skb_list_t res;
	res.head = SKB_INVALID_INDEX;
	res.tail = SKB_INVALID_INDEX;
	return res;
}

/** @return a list item initialized to empty. */
static inline skb_list_item_t skb_list_item_make(void)
{
	skb_list_item_t res;
	res.prev = SKB_INVALID_INDEX;
	res.next = SKB_INVALID_INDEX;
	return res;
}

/**
 * Removes specified item from the list.
 * @param list list where the item belongs to.
 * @param item_idx index of the item to remove.
 * @param get_item callback used to retrieve links of a specified item.
 * @param context context passed to the get_item callback.
 */
static inline void skb_list_remove(skb_list_t* list, int32_t item_idx, skb_list_get_item_func_t* get_item, void* context)
{
	skb_list_item_t* item = get_item(item_idx, context);

	if (item->prev != SKB_INVALID_INDEX)
		get_item(item->prev, context)->next = item->next;
	else if (list->head == item_idx)
		list->head = item->next;

	if (item->next != SKB_INVALID_INDEX)
		get_item(item->next, context)->prev = item->prev;
	else if (list->tail == item_idx)
		list->tail = item->prev;

	item->prev = SKB_INVALID_INDEX;
	item->next = SKB_INVALID_INDEX;
}

/**
 * Moves speciied item to the front (head) of the list.
 * @param list list where the item belongs to.
 * @param item_idx index of the item to move.
 * @param get_item callback used to retrieve links of a specified item.
 * @param context context passed to the get_item callback.
 */
static inline void skb_list_move_to_front(skb_list_t* list, int32_t item_idx, skb_list_get_item_func_t* get_item, void* context)
{
	skb_list_remove(list, item_idx, get_item, context);

	skb_list_item_t* item = get_item(item_idx, context);

	item->next = list->head;
	list->head = item_idx;

	if (item->next != SKB_INVALID_INDEX)
		get_item(item->next, context)->prev = item_idx;
	else
		list->tail = item_idx;
}

/** @} */


/**
 * @defgroup text Text
 * Unicode helpers and text related code.
 * See https://www.unicode.org/reports/tr51/ for further detail on the emoji functions.
 *
 * @{
 */

/** Enum describing text writing direction. */
typedef enum {
	/** Auto, infer from the text. */
	SKB_DIRECTION_AUTO,
	/** Left-to-right */
	SKB_DIRECTION_LTR,
	/** Right-to-left */
	SKB_DIRECTION_RTL,
} skb_text_direction_t;

/** @retur true if the text direction is right-to-left. */
static inline bool skb_is_rtl(skb_text_direction_t direction)
{
	return direction == SKB_DIRECTION_RTL;
}


/** Commonly references unicode codepoints. */
typedef enum {
	SKB_CHAR_COMBINING_ENCLOSING_CIRCLE_BACKSLASH = 0x20E0,
	SKB_CHAR_COMBINING_ENCLOSING_KEYCAP = 0x20E3,
	SKB_char_VARIATION_SELECTOR15 = 0xFE0E,
	SKB_CHAR_VARIATION_SELECTOR16 = 0xFE0F,
	SKB_CHAR_ZERO_WIDTH_JOINER = 0x200D,
	SKB_CHAR_REGIONAL_INDICATOR_BASE = 0x1F3F4,
	SKB_CHAR_CANCEL_TAG = 0xE007F,
	SKB_CHAR_LINE_FEED = 0x0A,
	SKB_CHAR_VERTICAL_TAB = 0x0B,
	SKB_CHAR_FORM_FEED = 0x0C,
	SKB_CHAR_CARRIAGE_RETURN = 0x0D,
	SKB_CHAR_NEXT_LINE = 0x85,
	SKB_CHAR_LINE_SEPARATOR = 0x2028,
	SKB_CHAR_PARAGRAPH_SEPARATOR = 0x2029,
	SKB_CHAR_REPLACEMENT_OBJECT = 0xFFFC,
} skb_character_t;

/** @returns true of the character serve as a base for emoji modifiers (EBase). */
bool skb_is_emoji_modifier_base(uint32_t codepoint);
/** @returns true of the character has emoji presentation by default (EPres). */
bool skb_is_emoji_presentation(uint32_t codepoint);
/** @returns true if the character is emoji (Emoji). */
bool skb_is_emoji(uint32_t codepoint);
/** @returns true if the character is emoji modifier (Emod). */
bool skb_is_emoji_modifier(uint32_t codepoint);
/** @returns true if the character is emoji modifier (Emod). */
bool skb_is_regional_indicator_symbol(uint32_t codepoint);
/** @returns true if the character is variation selector (VS15 or VS16). */
bool skb_is_variation_selector(uint32_t codepoint);
/** @returns true if the character is keycap. */
bool skb_is_keycap_base(uint32_t codepoint);
/** @returns true if the character is tag specifier (tag_spec). */
bool skb_is_tag_spec_char(uint32_t codepoint);
/** @returns true if the character is paragraph separator. */
bool skb_is_paragraph_separator(uint32_t codepoint);

/** Emoji iterator state, see skb_emoji_run_iterator_make(). */
typedef struct skb_emoji_run_iterator_t {
	uint8_t* emoji_category;
	int32_t count;
	int32_t pos;
	int32_t start;
	int32_t offset;
	bool has_emoji;
} skb_emoji_run_iterator_t;

/**
 * Initializes an emoji iterator for range of utf-32 text.
 * @param range range of codepoints to process (end is non-inclusive).
 * @param text pointer to the utf-32 text.
 * @param emoji_category_buffer buffer used internally to categorize the codepoints, length should be at least the length of the range (range.end - range.start).
 * @return initializes iterator.
 */
skb_emoji_run_iterator_t skb_emoji_run_iterator_make(skb_range_t range, const uint32_t* text, uint8_t* emoji_category_buffer);

/**
 * Gets the next range of emojis or normal text. This function is meant to be used in a while loop to iterator over all the emoji and text ranges in the input text.
 * ```
 *	skb_emoji_run_iterator_t iter = skb_emoji_run_iterator_make(...);
 *	skb_range_t range = {0};
 *	bool range_has_emojis = false;
 *	while (skb_emoji_run_iterator_next(&iter, &range, &range_has_emojis) {
 *		...
 *	}
 * ```
 * @param iter pointer to the iterator to advance.
 * @param range (out) range of the text analyzed
 * @param range_has_emojis (out) true of the output range has emojis
 * @return true if there are more ranges to come.
 */
bool skb_emoji_run_iterator_next(skb_emoji_run_iterator_t* iter, skb_range_t* range, bool* range_has_emojis);

/**
 * Converts utf-8 string to utf-32 string.
 * Does not zero terminate.
 * You can use this function (with null result buffer) or skb_utf8_to_utf32_count() to calculate the size of the result buffer.
 * @param utf8 pointer to a utf-8 to convert.
 * @param utf8_len length of the utf-8 string.
 * @param utf32 pointer to the result utf-32 string (can be null).
 * @param utf32_cap capacity of the result utf-32 string.
 * @return total number of codeunits in the utf-32 string.
 */
int32_t skb_utf8_to_utf32(const char* utf8, int32_t utf8_len, uint32_t* utf32, int32_t utf32_cap);

/**
 * Counts number of utf-32 codeunits in an utf-8 string.
 * @param utf8 pointer to a string in utf-8 encoding.
 * @param utf8_len length of the utf-8 string.
 * @return total number of codepoints (equals to utf-32 code units) in the inputs string.
 */
int32_t skb_utf8_to_utf32_count(const char* utf8, int32_t utf8_len);

/**
 * Returns an offset in the utf8 string to the specified codepoint offset.
 * @param utf8 string where to find the codepoint from.
 * @param utf8_len length of the string.
 * @param codepoint_offset offset of the codepoint to find.
 * @return returns start offset in the string to the specified codepoint.
 */
int32_t skb_utf8_codepoint_offset(const char* utf8, int32_t utf8_len, int32_t codepoint_offset);

/** @return number of utf-8 code units in a codepoint. */
int32_t skb_utf8_num_units(uint32_t cp);

/**
 * Encodes a codepoint to utf-8 string (max 4 utf-8 code units).
 * @param cp codepoint to encode
 * @param utf8 pointer to the result string
 * @param utf8_cap capacity of the utf-8 string.
 * @return number of code units in the result string.
 */
int32_t skb_utf8_encode(uint32_t cp, char* utf8, int32_t utf8_cap);

/**
 * Converts utf-32 string into utf-8 string.
 * Does not zero terminate.
 * You can use this function (with null result buffer) or skb_utf32_to_utf8_count() to calculate the size of the result buffer.
 * @param utf32 pointer to a utf-32 string to convert.
 * @param utf32_len length of the utf-32 string.
 * @param utf8 pointer to the result utf-8 string (can be null)
 * @param utf8_cap capacity of the result utf-8 string.
 * @return total number of codeunits in the utf-8 string.
 */
int32_t skb_utf32_to_utf8(const uint32_t* utf32, int32_t utf32_len, char* utf8, int32_t utf8_cap);

/**
 * Counts number of utf-8 codeunits in an utf-32 string.
 * @param utf32 pointer to a utf-32 string to convert.
 * @param utf32_len length of the utf-32 string.
 * @param utf8 pointer to the result utf-8 string (can be null)
 * @param utf8_cap capacity of the result utf-8 string.
 * @return total number of codeunits in the utf-8 string.
 */
int32_t skb_utf32_to_utf8_count(const uint32_t* utf32, int32_t utf32_len);

/** @return number of code units in null terminated utf-32 string. */
int32_t skb_utf32_strlen(const uint32_t* utf32);

/** @} */

/**
 * @defgroup perf_timer Performance timer
 *
 * @{
 */

/** @return performance timer time stamp. */
int64_t skb_perf_timer_get(void);

/**
 * Calculates elapsed time in micro seconds between two performance timer samples.
 * @param start start time sample
 * @param end  start time sample
 * @return elapsed time between samples in seconds.
 */
int64_t skb_perf_timer_elapsed_us(int64_t start, int64_t end);

/** @} */

#if defined( linux ) || defined( __linux__ ) || defined( __FreeBSD__ ) ||                       \
	defined( __OpenBSD__ ) || defined( __NetBSD__ ) || defined( __DragonFly__ ) ||              \
	defined( __SVR4 ) || defined( __sun ) || defined( __APPLE_CC__ ) || defined( __APPLE__ ) || \
	defined( __HAIKU__ ) || defined( __BEOS__ ) || defined( __emscripten__ ) ||                 \
	defined( EMSCRIPTEN )
#define SKB_PLATFORM_POSIX
#endif

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // SKB_COMMON_H
