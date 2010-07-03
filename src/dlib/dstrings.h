#ifndef DM_DSTRINGS_H
#define DM_DSTRINGS_H

#include <stdio.h>

#if defined(_WIN32)
#define DM_SNPRINTF _snprintf
#else
#define DM_SNPRINTF snprintf
#endif

/**
 * Separate strings. Equivialent to BSD strsep
 * @param stringp Pointer to string
 * @param delim Delimiter string
 * @return Next token. NULL if end-of-tokens.
 */
char* dmStrSep(char** stringp, const char* delim);

#endif //DM_DSTRINGS_H
