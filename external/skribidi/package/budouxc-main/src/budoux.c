#include "budoux.h"
#include <stdint.h>

// Models
#include "model_ja.h"
#include "model_zh_hans.h"
#include "model_zh_hant.h"
#include "model_th.h"


// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static uint32_t decutf8(uint32_t* state, uint32_t* codep, uint8_t byte)
{
	static const uint8_t utf8d[] = {
		// The first part of the table maps bytes to character classes that
		// to reduce the size of the transition table and create bitmasks.
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

		// The second part is a transition table that maps a combination
		// of a state of the automaton and a character class to a state.
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};

	uint32_t type = utf8d[byte];

	*codep = (*state != UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);

	*state = utf8d[256 + *state + type];
	return *state;
}

static uint32_t get_empty(utf_iter_t* iter)
{
	iter->empty_count--;
	if (iter->empty_count <= 0) 
		iter->done = 1;
	return 0;    
}

static uint32_t get_utf8(utf_iter_t* iter)
{
	uint32_t state = 0;
	uint32_t cp = 0;
	const char* buf = iter->buf;

	// Parse utf-8 code point
	decutf8(&state, &cp, buf[iter->pos++]);
	if (state && iter->pos < iter->buf_len && buf[iter->pos])
		decutf8(&state, &cp, buf[iter->pos++]);
	if (state && iter->pos < iter->buf_len && buf[iter->pos])
		decutf8(&state, &cp, buf[iter->pos++]);
	if (state && iter->pos < iter->buf_len && buf[iter->pos])
		decutf8(&state, &cp, buf[iter->pos++]);
	if (state != UTF8_ACCEPT)
		cp = 0;

	// If reached end of string, start emitting empty code points.
	if (iter->pos >= iter->buf_len || buf[iter->pos] == '\0')
		iter->get = &get_empty;

	return cp;
}

static uint32_t get_utf32(utf_iter_t* iter)
{
	const uint32_t* buf = iter->buf;
	uint32_t cp = buf[iter->pos++];

	// If reached end of string, start emitting empty code points.
	if (iter->pos >= iter->buf_len || buf[iter->pos] == 0)
		iter->get = &get_empty;

	return cp;
}

utf_iter_t make_utf8_iter(const char* buf, int32_t buf_len)
{
	const int32_t is_empty = !buf || buf[0] == '\0' || buf_len == 0;
	return (utf_iter_t) {
		.buf = buf,
		.buf_len = buf_len < 0 ? INT32_MAX : buf_len,
		.pos = 0,
		.done = 0,
		.get = is_empty ? &get_empty : &get_utf8,
	};
}

utf_iter_t make_utf32_iter(const uint32_t* buf, int32_t buf_len)
{
	const int32_t is_empty = !buf || buf[0] == '\0' || buf_len == 0;
	return (utf_iter_t) {
		.buf = buf,
		.buf_len = buf_len < 0 ? INT32_MAX : buf_len,
		.pos = 0,
		.done = 0,
		.get = is_empty ? &get_empty : &get_utf32,
	};
}

static int32_t find_weight(const budoux_weights_t* w, const uint64_t key)
{
	if (key < w->keys[0] || key > w->keys[w->count-1])
		return 0;

	// binary search
	int32_t low = 0;
	int32_t high = w->count - 1;
	while (low != high) {
		const int32_t mid = low + (high - low + 1) / 2; // ceil
		if (w->keys[mid] > key)
			high = mid - 1;
		else
			low = mid;
	}
	if (w->keys[low] == key)
		return w->values[low];
	
	return 0;
}
	
static int32_t find_uni_weight(const budoux_weights_t* w, const uint64_t cp0)
{
	return find_weight(w, cp0);
}

static int32_t find_bi_weight(const budoux_weights_t* w, const uint64_t cp0, const uint64_t cp1)
{
	return find_weight(w, cp0 | (cp1 << 20));
}

static int32_t find_tri_weight(const budoux_weights_t* w, const uint64_t cp0, const uint64_t cp1, const uint64_t cp2)
{
	return find_weight(w, cp0 | (cp1 << 20) | (cp2 << 40));
}

#define ROLL(a) \
	(a)[0] = (a)[1]; \
	(a)[1] = (a)[2]; \
	(a)[2] = (a)[3]; \
	(a)[3] = (a)[4]; \
	(a)[4] = (a)[5]

static boundary_iterator_t boundary_iterator_init(const budoux_model_t* model, utf_iter_t utf_iter)
{
	boundary_iterator_t bit = {
		.model = model,
		.utf_iter = utf_iter,
	};
	bit.utf_iter.empty_count = 2;

	// pre buffer
	for (int32_t i = 0; i < 2; i++) {
		ROLL(bit.offset);
		bit.offset[5] = bit.utf_iter.pos;
		ROLL(bit.buffer);
		bit.buffer[5] = bit.utf_iter.get(&bit.utf_iter);
	}

	return bit;
}

boundary_iterator_t boundary_iterator_init_ja_utf8(const char* text, int32_t len)
{
	utf_iter_t iter = make_utf8_iter(text, len);
	return boundary_iterator_init(&model_ja, iter);
}

boundary_iterator_t boundary_iterator_init_ja_utf32(const uint32_t* text, int32_t len)
{
	utf_iter_t iter = make_utf32_iter(text, len);
	return boundary_iterator_init(&model_ja, iter);
}

boundary_iterator_t boundary_iterator_init_zh_hans_utf8(const char* text, int32_t len)
{
	utf_iter_t iter = make_utf8_iter(text, len);
	return boundary_iterator_init(&model_zh_hans, iter);
}

boundary_iterator_t boundary_iterator_init_zh_hans_utf32(const uint32_t* text, int32_t len)
{
	utf_iter_t iter = make_utf32_iter(text, len);
	return boundary_iterator_init(&model_zh_hans, iter);
}

boundary_iterator_t boundary_iterator_init_zh_hant_utf8(const char* text, int32_t len)
{
	utf_iter_t iter = make_utf8_iter(text, len);
	return boundary_iterator_init(&model_zh_hant, iter);
}

boundary_iterator_t boundary_iterator_init_zh_hant_utf32(const uint32_t* text, int32_t len)
{
	utf_iter_t iter = make_utf32_iter(text, len);
	return boundary_iterator_init(&model_zh_hant, iter);
}

boundary_iterator_t boundary_iterator_init_th_utf8(const char* text, int32_t len)
{
	utf_iter_t iter = make_utf8_iter(text, len);
	return boundary_iterator_init(&model_th, iter);
}

boundary_iterator_t boundary_iterator_init_th_utf32(const uint32_t* text, int32_t len)
{
	utf_iter_t iter = make_utf32_iter(text, len);
	return boundary_iterator_init(&model_th, iter);
}


int32_t boundary_iterator_next(boundary_iterator_t* bit, int32_t* range_start, int32_t* range_end)
{
	if (!bit->model || !bit->utf_iter.buf || bit->done)
		return 0;
	
	uint32_t* buffer = bit->buffer;
	int32_t* offset = bit->offset;
	const budoux_model_t* model = bit->model;

	*range_start = 0;
	*range_end = 0;
	
	while (!bit->utf_iter.done) {
		ROLL(offset);
		offset[5] = bit->utf_iter.pos;
		ROLL(buffer);
		buffer[5] = bit->utf_iter.get(&bit->utf_iter);

		// buffer[0] is -3, buffer[3] = 0, buffer[5] = +2;
		int score = model->base_score;
		score += find_uni_weight(&model->UW1, buffer[0]);
		score += find_uni_weight(&model->UW2, buffer[1]);
		score += find_uni_weight(&model->UW3, buffer[2]);
		score += find_uni_weight(&model->UW4, buffer[3]);
		score += find_uni_weight(&model->UW5, buffer[4]);
		score += find_uni_weight(&model->UW6, buffer[5]);
		score += find_bi_weight(&model->BW1, buffer[1],buffer[2]);
		score += find_bi_weight(&model->BW2, buffer[2],buffer[3]);
		score += find_bi_weight(&model->BW3, buffer[3],buffer[4]);
		score += find_tri_weight(&model->TW1, buffer[0],buffer[1],buffer[2]);
		score += find_tri_weight(&model->TW1, buffer[1],buffer[2],buffer[3]);
		score += find_tri_weight(&model->TW1, buffer[2],buffer[3],buffer[4]);
		score += find_tri_weight(&model->TW1, buffer[3],buffer[4],buffer[5]);
		
		int32_t pos_end = offset[3];
		if (score > 0 && bit->range_start != pos_end) {
			*range_start = bit->range_start;
			*range_end = pos_end;
			bit->range_start = pos_end;
			break;
		}
	}

	if (bit->utf_iter.done) {
		*range_start = bit->range_start;
		*range_end = offset[3+1]; // 3 is the start of last processed codepoint, 3+1 is null term.
		bit->done = 1;
	}
	
	return 1;
}

int32_t utf8_to_utf32(const char* utf8, uint32_t* utf32)
{
	uint32_t state = 0;
	uint32_t cp = 0;
	int32_t count = 0;
	while (*utf8) {
		if (decutf8(&state, &cp, *utf8++))
			continue;
		if (utf32)
			utf32[count++] = cp;
	}
	if (utf32)
		utf32[count] = 0;
	return count;
}
