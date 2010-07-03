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

#endif //DM_DSTRINGS_H
