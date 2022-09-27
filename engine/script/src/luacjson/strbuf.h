/* strbuf - String buffer routines
 *
 * Copyright (c) 2010-2012  Mark Pulford <mark@kyne.com.au>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdarg.h>

/* Size: Total bytes allocated to *buf
 * Length: String length, excluding optional NULL terminator.
 * Increment: Allocation increments when resizing the string buffer.
 * Dynamic: True if created via strbuf_new()
 */

typedef struct {
    char *buf;
    int size;
    int length;
    int increment;
    int dynamic;
    int reallocs;
    int debug;
} strbuf_t;

#ifndef STRBUF_DEFAULT_SIZE
#define STRBUF_DEFAULT_SIZE 1023
#endif
#ifndef STRBUF_DEFAULT_INCREMENT
#define STRBUF_DEFAULT_INCREMENT -2
#endif

/* Initialise */
extern strbuf_t *strbuf_new(int len);
extern int strbuf_init(strbuf_t *s, int len);
extern int strbuf_set_increment(strbuf_t *s, int increment);

/* Release */
extern void strbuf_free(strbuf_t *s);
extern char *strbuf_free_to_string(strbuf_t *s, int *len);

/* Management */
extern int strbuf_resize(strbuf_t *s, int len);
static int strbuf_empty_length(strbuf_t *s);
static int strbuf_length(strbuf_t *s);
static char *strbuf_string(strbuf_t *s, int *len);
static int strbuf_ensure_empty_length(strbuf_t *s, int len);
static char *strbuf_empty_ptr(strbuf_t *s);
static void strbuf_extend_length(strbuf_t *s, int len);

/* Update */
extern int strbuf_append_fmt(strbuf_t *s, int len, const char *fmt, ...);
extern int strbuf_append_fmt_retry(strbuf_t *s, const char *format, ...);
static int strbuf_append_mem(strbuf_t *s, const char *c, int len);
extern int strbuf_append_string(strbuf_t *s, const char *str);
static int strbuf_append_char(strbuf_t *s, const char c);
static void strbuf_ensure_null(strbuf_t *s);

/* Reset string for before use */
static inline void strbuf_reset(strbuf_t *s)
{
    s->length = 0;
}

static inline int strbuf_allocated(strbuf_t *s)
{
    return s->buf != NULL;
}

/* Return bytes remaining in the string buffer
 * Ensure there is space for a NULL terminator. */
static inline int strbuf_empty_length(strbuf_t *s)
{
    return s->size - s->length - 1;
}

static inline int strbuf_ensure_empty_length(strbuf_t *s, int len)
{
    if (len > strbuf_empty_length(s))
        return strbuf_resize(s, s->length + len);
    return 1;
}

static inline char *strbuf_empty_ptr(strbuf_t *s)
{
    return s->buf + s->length;
}

static inline void strbuf_extend_length(strbuf_t *s, int len)
{
    s->length += len;
}

static inline int strbuf_length(strbuf_t *s)
{
    return s->length;
}

static inline int strbuf_append_char(strbuf_t *s, const char c)
{
    if (strbuf_ensure_empty_length(s, 1) == 0)
        return 0;
    s->buf[s->length++] = c;
    return 1;
}

static inline void strbuf_append_char_unsafe(strbuf_t *s, const char c)
{
    s->buf[s->length++] = c;
}

static inline int strbuf_append_mem(strbuf_t *s, const char *c, int len)
{
    if (strbuf_ensure_empty_length(s, len) == 0)
        return 0;
    memcpy(s->buf + s->length, c, len);
    s->length += len;
    return 1;
}

static inline void strbuf_append_mem_unsafe(strbuf_t *s, const char *c, int len)
{
    memcpy(s->buf + s->length, c, len);
    s->length += len;
}

static inline void strbuf_ensure_null(strbuf_t *s)
{
    s->buf[s->length] = 0;
}

// +++++
static inline char *strbuf_string(strbuf_t *s, int *len)
{
    if (len)
        *len = s->length;

    return s->buf;
}

/* vi:ai et sw=4 ts=4:
 */
