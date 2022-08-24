// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "dstrings.h"

#if defined(_WIN32)
#include <stdlib.h>
#endif

int dmSnPrintf(char *buffer, size_t count, const char *format, ...)
{
    // MS-compliance
    if (buffer == 0x0 || count == 0 || format == 0x0)
        return -1;
    va_list argp;
    va_start(argp, format);
#if defined(_WIN32)
    int result = _vsnprintf_s(buffer, count, _TRUNCATE, format, argp);
#else
    int result = vsnprintf(buffer, count, format, argp);
#endif
    va_end(argp);
    // MS-compliance
    if (count == 0 || (count > 0 && result >= (int)count))
        return -1;
    return result;
}

/*      $NetBSD: strtok_r.c,v 1.9 2003/08/07 16:43:53 agc Exp $ */

/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * thread-save version of strtok
 */
char* dmStrTok(char *s, const char *delim, char **lasts)
{
    const char *spanp;
    int c, sc;
    char *tok;

    /* s may be NULL */
    assert(delim != NULL);
    assert(lasts != NULL);

    if (s == NULL && (s = *lasts) == NULL)
        return (NULL);

    /*
     * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
     */
cont:
    c = *s++;
    for (spanp = delim; (sc = *spanp++) != 0;) {
        if (c == sc)
            goto cont;
    }

    if (c == 0) {           /* no non-delimiter characters */
        *lasts = NULL;
        return (NULL);
    }
    tok = s - 1;

    /*
     * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
     * Note that delim must have one NUL; we stop if we see that, too.
     */
    for (;;) {
        c = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *lasts = s;
                return (tok);
            }
        } while (sc != 0);
    }
    /* NOTREACHED */
}

/*      $NetBSD: strlcpy.c,v 1.5 1999/09/20 04:39:47 lukem Exp $        */
/*      from OpenBSD: strlcpy.c,v 1.4 1999/05/01 18:56:41 millert Exp   */

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */

size_t dmStrlCpy(char *dst, const char *src, size_t siz)
{
    char *d = dst;
    const char *s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);
}

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0';              /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return(s - src - 1);    /* count does not include NUL */
}

#include <string.h>

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
dmStrlCat(char *dst, const char *src, size_t siz)
{
        char *d = dst;
        const char *s = src;
        size_t n = siz;
        size_t dlen;

        /* Find the end of dst and adjust bytes left but don't go past end */
        while (*d != '\0' && n-- != 0)
                d++;
        dlen = d - dst;
        n = siz - dlen;

        if (n == 0)
                return(dlen + strlen(s));
        while (*s != '\0') {
                if (n != 1) {
                        *d++ = *s;
                        n--;
                }
                s++;
        }
        *d = '\0';

        return(dlen + (s - src));       /* count does not include NUL */
}

int dmStrCaseCmp(const char *s1, const char *s2)
{
#ifdef _WIN32
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

#if defined(ANDROID)
    #if defined(__USE_GNU) && __ANDROID_API__ >= 23
        #define DM_STRERROR_USE_GNU
    #else
        #define DM_STRERROR_USE_POSIX
    #endif
#elif defined(__EMSCRIPTEN__)
    // Emscripten wraps strerror_r as strerror anyway
    #define DM_STRERROR_USE_UNSAFE
#else
    #if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE || defined(_WIN32) || defined(__MACH__)
        #define DM_STRERROR_USE_POSIX
    #else
        #define DM_STRERROR_USE_GNU
    #endif
#endif

#ifdef DM_STRERROR_USE_POSIX
    #if defined(_WIN32)
        #define DM_STRERROR_FN(buf, size, errval) (int) strerror_s(buf, size, errval)
    #else
        #define DM_STRERROR_FN(buf, size, errval) strerror_r(errval, buf, size)
    #endif
#endif

void dmStrError(char* dst, size_t size, int err)
{
    if (dst == 0 || size == 0)
    {
        return;
    }

    char scratch[256];
    const size_t scratch_size = sizeof(scratch);
    size_t err_msg_len = 0;
    int old_errno = errno;
    char* retstr = 0;

#ifdef DM_STRERROR_USE_POSIX
    int ret = DM_STRERROR_FN(scratch, scratch_size, err);

    if (ret == 0 || ret == ERANGE)
    {
    #if defined(_WIN32)
        // apparently windows returns a success ret value and a "Unknown error" string
        // even if it's not a supported errno
        if (dmStrCaseCmp("Unknown error", scratch) == 0)
        {
            dmSnPrintf(scratch, scratch_size, "Unknown error %d", err);
        }
    #endif
    }
    else
    {
        if (ret == EINVAL)
        {
            dmSnPrintf(scratch, scratch_size, "Unknown error %d", err);
        }
        else
        {
            dmSnPrintf(scratch, scratch_size, "Failed getting error (code %d)", ret);
        }
    }

    retstr = &scratch[0];
#endif

#ifdef DM_STRERROR_USE_GNU
    // GNU version, char* returned
    retstr = strerror_r(err, scratch, scratch_size);
#endif

#ifdef DM_STRERROR_USE_UNSAFE
    retstr = strerror(err);
#endif

    err_msg_len = strlen(retstr) + 1;

    // Restore errno in case strerror variants raised error
    errno = old_errno;

    if (size < err_msg_len)
    {
        err_msg_len = size;
    }

    memcpy(dst, retstr, err_msg_len);
    dst[err_msg_len-1] = '\0';
}


#undef DM_STRERROR_USE_GNU
#undef DM_STRERROR_USE_POSIX
#undef DM_STRERROR_USE_UNSAFE
#undef DM_STRERROR_FN
