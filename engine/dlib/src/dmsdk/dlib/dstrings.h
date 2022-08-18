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

#ifndef DMSDK_DSTRINGS_H
#define DMSDK_DSTRINGS_H

#include <stdio.h>

/*# String functions.
 *
 * SDK Defold String Utils API documentation
 *
 * @document
 * @namespace dmStringFunc
 * @name DStrings
 * @path engine/dlib/src/dmsdk/dlib/dstrings.h
 */

/*# Size-bounded string formating.
 *
 * Size-bounded string formating. Resulting string is guaranteed to be 0-terminated.
 *
 * @name dmSnPrintf
 * @param buffer Buffer to write to
 * @param count Size of the buffer
 * @param format String format
 * @return Size of the resulting string (excl terminating 0) if it fits, -1 otherwise
 * @examples
 *
 * ```cpp
 * char path[64];
 * dmSnPrintf(path, 64, PATH_FORMAT, filename);
 * ```
 */

#ifdef __GNUC__
	int dmSnPrintf(char *buffer, size_t count, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
#else
	int dmSnPrintf(char *buffer, size_t count, const char *format, ...);
#endif

/*# Tokenize strings.
 *
 * Tokenize strings. Equivalent to BSD strsep_r. Thread-save version of strtok.
 *
 * @name dmStrTok
 * @param string Pointer to string. For the first call string is the string to tokenize. Subsequent should pass NULL.
 * @param delim Delimiter string
 * @param lasts Internal state pointer
 * @return Each call to dmStrTok() returns a pointer to a null-terminated string containing the next token. This string does not include the delimiting byte. If no more tokens are found, dmStrTok() returns NULL
 * @examples
 *
 * ```cpp
 * char* string = strdup("a b c");
 * char* s, *last;
 * s = dmStrTok(string, " ", &last); // a
 * s = dmStrTok(0, " ", &last); // b
 * s = dmStrTok(0, " ", &last); // c
 * ```
 */
char* dmStrTok(char *string, const char *delim, char **lasts);

/*# Size-bounded string copying.
 *
 * Size-bounded string copying. Same as OpenBSD 2.4 [strlcpy](http://www.manpagez.com/man/3/strlcpy/).
 * Copy src to string dst of size siz.  At most siz-1 characters will be copied.
 * Always NUL terminates (unless siz == 0).Returns strlen(src); if retval >= siz, truncation occurred.
 *
 * @name dmStrlCpy
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
 * @examples
 *
 * ```cpp
 * const char* src = "foo";
 * char dst[3];
 * dmStrlCpy(dst, src, sizeof(dst)); // dst = "fo"
 * ```
 */
size_t dmStrlCpy(char *dst, const char *src, size_t size);

/*# Size-bounded string concatenation.
 *
 * Size-bounded string concatenation. Same as OpenBSD 2.4 [strlcat](http://www.manpagez.com/man/3/strlcat).
 * Appends src to string dst of size siz (unlike strncat, siz is the full size of dst, not space left).
 * At most siz-1 characters will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 *
 * @name dmStrlCat
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
 * @examples
 *
 * ```cpp
 * const char* src = "foo";
 * char dst[3] = { 0 };
 * dmStrlCat(dst, src, sizeof(dst)); // dst = "fo"
 * ```
 */
size_t dmStrlCat(char *dst, const char *src, size_t size);

/*# Case-insensitive string comparison
 *
 * Case-insensitive string comparison
 *
 * @name dmStrCaseCmp
 * @param s1 First string to compare
 * @param s2 Second string to compare
 * @return an integer greater than, equal to, or less than 0 after lexicographically comparison of s1 and s2
 * @examples
 *
 * ```cpp
 * dmStrCaseCmp("a", "b"); // <0
 * dmStrCaseCmp("b", "a"); // >0
 * dmStrCaseCmp("a", "a"); // 0
 * ```
 */
int dmStrCaseCmp(const char *s1, const char *s2);

/*# Error code to string representation
 *
 * Error code to string representation. Wrapper for thread-safe strerror_s/r variants.
 * If the size of the buffer is too small, the message will be truncated to fit the buffer.
 * If the buffer is null, or if size is zero, nothing will happen.
 *
 * @name dmStrError
 * @param dst Destination string that carries the error message
 * @param size Max size of destination string in bytes
 * @return a null-terminated error message
 * @examples
 *
 * ```cpp
 * char buf[128];
 * dmStrError(buf, sizeof(buf), ENOENT); // buf => "No such file or directory"
 * ```
 */
void dmStrError(char* dst, size_t size, int err);

#endif //DMSDK_DSTRINGS_H
