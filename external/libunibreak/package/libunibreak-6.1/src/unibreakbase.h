/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Break processing in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2015-2024 Wu Yongwei <wuyongwei at gmail dot com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software.  If you use this
 *    software in a product, an acknowledgement in the product
 *    documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file    unibreakbase.h
 *
 * Header file for common definitions in the libunibreak library.
 *
 * @author  Wu Yongwei
 */

#ifndef UNIBREAKBASE_H
#define UNIBREAKBASE_H

#ifdef __cplusplus
extern "C" {
#endif

#define UNIBREAK_VERSION   0x0601   /**< Version of the libunibreak */
extern const int unibreak_version;

#ifndef UNIBREAK_UTF_TYPES_DEFINED
#define UNIBREAK_UTF_TYPES_DEFINED
typedef unsigned char   utf8_t;     /**< Type for UTF-8 data points */
typedef unsigned short  utf16_t;    /**< Type for UTF-16 data points */
typedef unsigned int    utf32_t;    /**< Type for UTF-32 data points */
#endif

#ifdef __cplusplus
}
#endif

#endif /* UNIBREAKBASE_H */
