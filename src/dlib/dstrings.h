#ifndef __DSTRINGS_H__
#define __DSTRINGS_H__

#include <stdio.h>

#if defined(_WIN32)
#define SNPRINTF _snprintf
#else
#define SNPRINTF snprintf
#endif


#endif //__DSTRINGS_H__
