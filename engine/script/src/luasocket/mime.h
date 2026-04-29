#ifndef MIME_H 
#define MIME_H 
/*=========================================================================*\
* Core MIME support
* LuaSocket toolkit
*
* This module provides functions to implement transfer content encodings
* and formatting conforming to RFC 2045. It is used by mime.lua, which
* provide a higher level interface to this functionality. 
\*=========================================================================*/
#include <dmsdk/dlua/dlua.h>

/*-------------------------------------------------------------------------*\
* Current MIME library version
\*-------------------------------------------------------------------------*/
#define MIME_VERSION    "MIME 1.0.3"
#define MIME_COPYRIGHT  "Copyright (C) 2004-2013 Diego Nehab"
#define MIME_AUTHORS    "Diego Nehab"

/*-------------------------------------------------------------------------*\
* This macro prefixes all exported API functions
\*-------------------------------------------------------------------------*/
#ifndef MIME_API
#define MIME_API extern
#endif

MIME_API int luaopen_mime_core(dlua_State *L);

#endif /* MIME_H */
