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
 * @namespace dmStrings
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
 */

#ifdef __GNUC__
	int DM_SNPRINTF(char *buffer, size_t count, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
#else
	int DM_SNPRINTF(char *buffer, size_t count, const char *format, ...);
#endif

/*# Tokenize strings.
 *
 * Tokenize strings. Equivalent to BSD strsep_r
 *
 * @name dmStrTok
 * @param string Pointer to string. For the first call string is the string to tokenize. Subsequent should pass NULL.
 * @param delim Delimiter string
 * @param lasts Internal state pointer
 * @return
 */
char* dmStrTok(char *string, const char *delim, char **lasts);

/*# Size-bounded string copying.
 * 
 * Size-bounded string copying. Same as OpenBSD 2.4 [strlcpy](http://www.manpagez.com/man/3/strlcpy/). 
 *
 * @name dmStrlCpy
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
 */
size_t dmStrlCpy(char *dst, const char *src, size_t size);

/*# Size-bounded string concatenation.
 * 
 * Size-bounded string concatenation. Same as OpenBSD 2.4 [strlcat](http://www.manpagez.com/man/3/strlcat).
 *
 * @name dmStrlCat
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
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
 */
int dmStrCaseCmp(const char *s1, const char *s2);

#endif //DMSDK_DSTRINGS_H
