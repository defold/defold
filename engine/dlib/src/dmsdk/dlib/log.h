// Copyright 2020 The Defold Foundation
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

#ifndef DMSDK_LOG_H
#define DMSDK_LOG_H

#include <stdint.h>

/*# logging functions
 *
 * Logging functions.
 * @note Log functions will be omitted (NOP) for release builds
 * @note Prefer these functions over `printf` since these can print to the platform specific logs
 *
 * @document
 * @name Log
 * @namespace dmLog
 * @path engine/dlib/src/dmsdk/dlib/log.h
*/

/*# macro for debug category logging
 *
 * If DLIB_LOG_DOMAIN is defined the value of the defined is printed after severity.
 * Otherwise DEFAULT will be printed.
 *
 * @note Extensions do not need to set this since they get their own logging domain automatically
 *
 * @macro
 * @name DLIB_LOG_DOMAIN
 * @examples
 *
 * ```cpp
 * #define DLIB_LOG_DOMAIN "MyOwnDomain"
 * #include <dmsdk/dlib/log.h>
 * ```
 */

/*# log with "debug" severity
 *
 * Debug messages are temporary log instances used when debugging a certain behavior
 * Use dmLogOnceDebug for one-shot logging
 *
 * @name dmLogDebug
 * @param format [type:const char*] Format string
 * @param args [type:...] Format string args (variable arg list)
 * @return [type:void]
 */

/*# log with "user" severity
 *
 * Debug messages are temporary log instances used when debugging a certain behavior
 * Use dmLogOnceUserDebug for one-shot logging
 *
 * @name dmLogUserDebug
 * @param format [type:const char*] Format string
 * @param args [type:...] Format string args (variable arg list)
 * @return [type:void]
 */

/*# log with "info" severity
 *
 * Info messages are used to inform the developers of relevant information
 * Use dmLogOnceInfo for one-shot logging
 *
 * @name dmLogInfo
 * @param format [type:const char*] Format string
 * @param args [type:...] Format string args (variable arg list)
 * @return [type:void]
 */

/*# log with "warning" severity
 *
 * Warning messages are used to inform the developers about potential problems which can cause errors.
 * Use dmLogOnceWarning for one-shot logging
 *
 * @name dmLogWarning
 * @param format [type:const char*] Format string
 * @param args [type:...] Format string args (variable arg list)
 * @return [type:void]
 */

/*#log with "error" severity
 *
 * Error messages are used in cases where an recoverable error has occurred.
 * Use dmLogOnceError for one-shot logging
 *
 * @name dmLogError
 * @param format [type:const char*] Format string
 * @param args [type:...] Format string args (variable arg list)
 * @return [type:void]
 */

/*# log with "fatal" severity
 *
 * Fatal messages are used in cases where an unrecoverable error has occurred.
 * Use dmLogOnceFatal for one-shot logging
 *
 * @name dmLogFatal
 * @param format [type:const char*] Format string
 * @param args [type:...] Format string args (variable arg list)
 * @return [type:void]
 */

/** Log severity
 * Log severity
 * @enum
 * @name dmLogSeverity
 */
enum dmLogSeverity
{
    DM_LOG_SEVERITY_DEBUG       = 0,//!< DM_LOG_SEVERITY_DEBUG
    DM_LOG_SEVERITY_USER_DEBUG  = 1,//!< DM_LOG_SEVERITY_USER_DEBUG
    DM_LOG_SEVERITY_INFO        = 2,//!< DM_LOG_SEVERITY_INFO
    DM_LOG_SEVERITY_WARNING     = 3,//!< DM_LOG_SEVERITY_WARNING
    DM_LOG_SEVERITY_ERROR       = 4,//!< DM_LOG_SEVERITY_ERROR
    DM_LOG_SEVERITY_FATAL       = 5,//!< DM_LOG_SEVERITY_FATAL
};

#if defined(NDEBUG)

#define dmLogDebug(format, ... ) do {} while(0);
#define dmLogUserDebug(format, ... ) do {} while(0);
#define dmLogInfo(format, args...) do {} while(0);
#define dmLogWarning(format, args...) do {} while(0);
#define dmLogError(format, args...) do {} while(0);
#define dmLogFatal(format, args...) do {} while(0);

#define dmLogOnceDebug(format, args... ) do {} while(0);
#define dmLogOnceUserDebug(format, args... ) do {} while(0);
#define dmLogOnceInfo(format, args... ) do {} while(0);
#define dmLogOnceWarning(format, args... ) do {} while(0);
#define dmLogOnceError(format, args... ) do {} while(0);
#define dmLogOnceFatal(format, args... ) do {} while(0);

#else

#ifndef DLIB_LOG_DOMAIN
#define DLIB_LOG_DOMAIN "DEFAULT"
#ifdef __GNUC__
#warning "DLIB_LOG_DOMAIN is not defined"
#endif
#endif

#ifdef __GNUC__
void dmLogInternal(dmLogSeverity severity, const char* domain, const char* format, ...)
    __attribute__ ((format (printf, 3, 4)));
#else
void dmLogInternal(dmLogSeverity severity, const char* domain, const char* format, ...);
#endif

#ifdef _MSC_VER
#define dmLogDebug(format, ... ) dmLogInternal(DM_LOG_SEVERITY_DEBUG, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogUserDebug(format, ... ) dmLogInternal(DM_LOG_SEVERITY_USER_DEBUG, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogInfo(format, ... ) dmLogInternal(DM_LOG_SEVERITY_INFO, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogWarning(format, ... ) dmLogInternal(DM_LOG_SEVERITY_WARNING, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogError(format, ... ) dmLogInternal(DM_LOG_SEVERITY_ERROR, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogFatal(format, ... ) dmLogInternal(DM_LOG_SEVERITY_FATAL, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#else
#define dmLogDebug(format, args...) dmLogInternal(DM_LOG_SEVERITY_DEBUG, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogUserDebug(format, args...) dmLogInternal(DM_LOG_SEVERITY_USER_DEBUG, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogInfo(format, args...) dmLogInternal(DM_LOG_SEVERITY_INFO, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogWarning(format, args...) dmLogInternal(DM_LOG_SEVERITY_WARNING, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogError(format, args...) dmLogInternal(DM_LOG_SEVERITY_ERROR, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogFatal(format, args...) dmLogInternal(DM_LOG_SEVERITY_FATAL, DLIB_LOG_DOMAIN, format, ## args);
#endif

#define dmLogOnceIdentifier __dmLogOnce
#define __DM_LOG_PASTE(x, y) x ## y
#define DM_LOG_PASTE(x, y) __DM_LOG_PASTE(x, y)

#ifdef _MSC_VER
#define dmLogOnceInternal(method, format, ... )                     \
    {                                                               \
        static int DM_LOG_PASTE(dmLogOnceIdentifier, __LINE__) = 0; \
        if (DM_LOG_PASTE(dmLogOnceIdentifier, __LINE__) == 0)       \
        {                                                           \
            DM_LOG_PASTE(dmLogOnceIdentifier, __LINE__) = 1;        \
            method(format, __VA_ARGS__ );                           \
        }                                                           \
    }
#define dmLogOnceDebug(format, ... ) dmLogOnceInternal(dmLogDebug, format, __VA_ARGS__ )
#define dmLogOnceUserDebug(format, ... ) dmLogOnceInternal(dmLogUserDebug, format, __VA_ARGS__ )
#define dmLogOnceInfo(format, ... ) dmLogOnceInternal(dmLogInfo, format, __VA_ARGS__ )
#define dmLogOnceWarning(format, ... ) dmLogOnceInternal(dmLogWarning, format, __VA_ARGS__ )
#define dmLogOnceError(format, ... ) dmLogOnceInternal(dmLogError, format, __VA_ARGS__ )
#define dmLogOnceFatal(format, ... ) dmLogOnceCritical(dmLogFatal, format, __VA_ARGS__ )
#else
#define dmLogOnceInternal(method, format, args... )                 \
    {                                                               \
        static int DM_LOG_PASTE(dmLogOnceIdentifier, __LINE__) = 0; \
        if (DM_LOG_PASTE(dmLogOnceIdentifier, __LINE__) == 0)       \
        {                                                           \
            DM_LOG_PASTE(dmLogOnceIdentifier, __LINE__) = 1;        \
            method(format, ## args );                               \
        }                                                           \
    }
#define dmLogOnceDebug(format, args... ) dmLogOnceInternal(dmLogDebug, format, ## args )
#define dmLogOnceUserDebug(format, args... ) dmLogOnceInternal(dmLogUserDebug, format, ## args )
#define dmLogOnceInfo(format, args... ) dmLogOnceInternal(dmLogInfo, format, ## args )
#define dmLogOnceWarning(format, args... ) dmLogOnceInternal(dmLogWarning, format, ## args )
#define dmLogOnceError(format, args... ) dmLogOnceInternal(dmLogError, format, ## args )
#define dmLogOnceFatal(format, args... ) dmLogOnceCritical(dmLogFatal, format, ## args )
#endif

/*# dmLogListener callback typedef
 *
 * dmLog listener function type. Provides all logs from dmLog* functions and print/pprint Lua functions.
 * Used with RegisterLogListener() and UnregisterLogListener()
 *
 * @typedef
 * @name dmLogListener
 * @param severity [type:dmLogSeverity]
 * @param domain [type:const char*]
 * @param formatted_string [type:char*]
 */
typedef void (*dmLogListener)(dmLogSeverity severity, const char* domain, const char* format, ...);

/*# register dmLog listener.
 *
 * Registers a dmLog listener.
 *
 * @name RegisterLogListener
 * @param listener [type:dmLogListener] 
 */
void RegisterLogListener(dmLogListener listener);

/*# unregister dmLog listener.
 *
 * Unregisters a dmLog listener.
 *
 * @name UnregisterLogListener
 * @param [type:dmLogListener] listener
 */
void UnregisterLogListener(dmLogListener listener);

#endif

#endif // DMSDK_LOG_H
