#ifndef DM_DSTRINGS_H
#define DM_DSTRINGS_H

#include <stdio.h>

#if defined(_WIN32)
#define DM_SNPRINTF _snprintf
#else
#define DM_SNPRINTF snprintf
#endif

/**
 * Tokenize strings. Equivalent to BSD strsep_r
 * @param string Pointer to string. For the first call string is the string to tokenize. Subsequent should pass NULL.
 * @param delim Delimiter string
 * @param lasts Internal state pointer
 * @return
 */
char* dmStrTok(char *string, const char *delim, char **lasts);

/**
 * Size-bounded string copying. Same as OpenBSD 2.4 strlcpy. (http://www.manpagez.com/man/3/strlcpy/)
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
 */
size_t dmStrlCpy(char *dst, const char *src, size_t size);

/**
 * Size-bounded string concatenation. Same as OpenBSD 2.4 strlcat. (http://www.manpagez.com/man/3/strlcat)
 * @param dst Destination string
 * @param src Source string
 * @param size Max size
 * @return Total length of the created string
 */
size_t dmStrlCat(char *dst, const char *src, size_t size);

#endif //DM_DSTRINGS_H
