#ifndef DMSDK_DSTRINGS_H
#define DMSDK_DSTRINGS_H

#include <stdio.h>

/*# SDK Defold String Utils API documentation
 * [file:<dmsdk/dlib/dstrings.h>]
 *
 * String functions.
 *
 * @document
 * @name DStrings
 * @namespace dmStringFunc
 */

/*# Size-bounded string formating.
 *
 * Size-bounded string formating. Resulting string is guaranteed to be 0-terminated.
 *
 * @name DM_SNPRINTF
 * @param buffer Buffer to write to
 * @param count Size of the buffer
 * @param format String format
 * @return Size of the resulting string (excl terminating 0) if it fits, -1 otherwise
 * @examples
 *
 * ```cpp
 * char path[64];
 * DM_SNPRINTF(path, 64, PATH_FORMAT, filename);
 * ```
 */

#ifdef __GNUC__
	int DM_SNPRINTF(char *buffer, size_t count, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
#else
	int DM_SNPRINTF(char *buffer, size_t count, const char *format, ...);
#endif

/*# Tokenize strings.
 *
 * Tokenize strings. Equivalent to BSD strsep_r
 * Each call to dmStrTok() returns a pointer to a null-terminated string containing the next token. 
 * This string does not include the delimiting byte. If no more tokens are found, dmStrTok() returns NULL"
 *
 * @name dmStrTok
 * @param string Pointer to string. For the first call string is the string to tokenize. Subsequent should pass NULL.
 * @param delim Delimiter string
 * @param lasts Internal state pointer
 * @return
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

#endif //DMSDK_DSTRINGS_H
