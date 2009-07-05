#ifndef DM_DSTRINGS_H
#define DM_DSTRINGS_H

#include <stdio.h>

#if defined(_WIN32)
#define DM_SNPRINTF _snprintf
#else
#define DM_SNPRINTF snprintf
#endif


#endif //DM_DSTRINGS_H
