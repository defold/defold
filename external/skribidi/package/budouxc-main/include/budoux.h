#ifndef BUDOUX_H
#define BUDOUX_H

#include <stdint.h>

typedef struct budoux_weights_t {
	uint64_t* keys;
	int16_t* values;
	int32_t count;
} budoux_weights_t;

typedef struct budoux_model_t  {
	int32_t base_score;
	budoux_weights_t UW1;
	budoux_weights_t UW2;
	budoux_weights_t UW3;
	budoux_weights_t UW4;
	budoux_weights_t UW5;
	budoux_weights_t UW6;
	budoux_weights_t BW1;
	budoux_weights_t BW2;
	budoux_weights_t BW3;
	budoux_weights_t TW1;
	budoux_weights_t TW2;
	budoux_weights_t TW3;
	budoux_weights_t TW4;
} budoux_model_t;

struct utf_iter_t;

typedef uint32_t (*get_func_t)(struct utf_iter_t* iter);

typedef struct utf_iter_t {
	get_func_t get;
	const void* buf;
	int32_t buf_len;
	int32_t pos;
	int32_t done;
	int32_t empty_count;
} utf_iter_t;

// Boundary iterator state, place in stack, and init with one of the initialization functions.
// Does not allocate, no need to tear down.
typedef struct boundary_iterator_t {
	const budoux_model_t* model;
	utf_iter_t utf_iter;
	uint32_t buffer[6];
	int32_t offset[6];
	int32_t range_start;
	int32_t done;
} boundary_iterator_t;

// Initialize boundary iterator for Japanese
boundary_iterator_t boundary_iterator_init_ja_utf8(const char* text, int32_t len);
boundary_iterator_t boundary_iterator_init_ja_utf32(const uint32_t* text, int32_t len);

// Initialize boundary iterator for Simplified Chinese
boundary_iterator_t boundary_iterator_init_zh_hans_utf8(const char* text, int32_t len);
boundary_iterator_t boundary_iterator_init_zh_hans_utf32(const uint32_t* text, int32_t len);

// Initialize boundary iterator for Traditional Chinese
boundary_iterator_t boundary_iterator_init_zh_hant_utf8(const char* text, int32_t len);
boundary_iterator_t boundary_iterator_init_zh_hant_utf32(const uint32_t* text, int32_t len);

// Initialize boundary iterator for Thai
boundary_iterator_t boundary_iterator_init_th_utf8(const char* text, int32_t len);
boundary_iterator_t boundary_iterator_init_th_utf32(const uint32_t* text, int32_t len);

// Gets next word boundary range. Returns 1 until all ranges have been iterated.
// Value stored in range_end is non-inclusive, that is len = range_end - range_start;
//
//	iter = boundary_iterator_init_ja_utf8(sentence);
//	while (boundary_iterator_next(&iter, &range_start, &range_end))
//		for (int32_t i = range_start; i < range_end; i++)
//          printf("%c", sentence[i]);
//
int32_t boundary_iterator_next(boundary_iterator_t* bit, int32_t* range_start, int32_t* range_end);

// Convert utf8 to utf32, returns number of codepoints.
// Param utf32 can be NULL, in which case just the length is calculated (does not include zero term).
int32_t utf8_to_utf32(const char* utf8, uint32_t* utf32);

#endif // BODOUX_H