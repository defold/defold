// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

/*# Log severity
 *
 * Log severity
 *
 * @enum
 * @name LogSeverity
 * @member LOG_SEVERITY_DEBUG
 * @member LOG_SEVERITY_USER_DEBUG
 * @member LOG_SEVERITY_INFO
 * @member LOG_SEVERITY_WARNING
 * @member LOG_SEVERITY_ERROR
 * @member LOG_SEVERITY_FATAL
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LogSeverity
{
    LOG_SEVERITY_DEBUG       = 0,//!< LOG_SEVERITY_DEBUG
    LOG_SEVERITY_USER_DEBUG  = 1,//!< LOG_SEVERITY_USER_DEBUG
    LOG_SEVERITY_INFO        = 2,//!< LOG_SEVERITY_INFO
    LOG_SEVERITY_WARNING     = 3,//!< LOG_SEVERITY_WARNING
    LOG_SEVERITY_ERROR       = 4,//!< LOG_SEVERITY_ERROR
    LOG_SEVERITY_FATAL       = 5,//!< LOG_SEVERITY_FATAL
} LogSeverity;

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
void LogInternal(LogSeverity severity, const char* domain, const char* format, ...)
    __attribute__ ((format (printf, 3, 4)));
#else
void LogInternal(LogSeverity severity, const char* domain, const char* format, ...);
#endif

#ifdef _MSC_VER
#define dmLogDebug(format, ... ) LogInternal(LOG_SEVERITY_DEBUG, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogUserDebug(format, ... ) LogInternal(LOG_SEVERITY_USER_DEBUG, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogInfo(format, ... ) LogInternal(LOG_SEVERITY_INFO, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogWarning(format, ... ) LogInternal(LOG_SEVERITY_WARNING, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogError(format, ... ) LogInternal(LOG_SEVERITY_ERROR, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogFatal(format, ... ) LogInternal(LOG_SEVERITY_FATAL, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#else
#define dmLogDebug(format, args...) LogInternal(LOG_SEVERITY_DEBUG, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogUserDebug(format, args...) LogInternal(LOG_SEVERITY_USER_DEBUG, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogInfo(format, args...) LogInternal(LOG_SEVERITY_INFO, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogWarning(format, args...) LogInternal(LOG_SEVERITY_WARNING, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogError(format, args...) LogInternal(LOG_SEVERITY_ERROR, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogFatal(format, args...) LogInternal(LOG_SEVERITY_FATAL, DLIB_LOG_DOMAIN, format, ## args);
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
#define dmLogOnceFatal(format, ... ) dmLogOnceInternal(dmLogFatal, format, __VA_ARGS__ )
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
#define dmLogOnceFatal(format, args... ) dmLogOnceInternal(dmLogFatal, format, ## args )
#endif

/*# dmLog:LogListener callback typedef
 *
 * dmLog listener function type. Provides all logs from dmLog* functions and print/pprint Lua functions.
 * Used with dmLogRegisterListener() and dmLogUnregisterListener()
 *
 * @typedef
 * @name dmLog:LogListener
 * @param severity [type:LogSeverity]
 * @param domain [type:const char*]
 * @param formatted_string [type:const char*] null terminated string
 */
typedef void (*FLogListener)(LogSeverity severity, const char* domain, const char* formatted_string);

/*# register a log listener.
 *
 * Registers a log listener.
 * This listener receive logs even in release bundle.
 *
 * @note Any calls to dmLogInfo et al from within the calllback will be ignored
 *
 * @name dmLogRegisterListener
 * @param listener [type:FLogListener]
 */
void dmLogRegisterListener(FLogListener listener);

/*# unregister a log listener.
 *
 * Unregisters a log listener.
 *
 * @name dmLogUnregisterListener
 * @param [type:FLogListener] listener
 */
void dmLogUnregisterListener(FLogListener listener);

/*# set log system severity level.
 *
 * Set log system severity level.
 *
 * @name dmLogSetLevel
 * @param [type:LogSeverity] severity
 */
void dmLogSetLevel(LogSeverity severity);


/*# get log system severity level.
 *
 * Get log system severity level.
 *
 * @name dmLogGetLevel
 * @return [type:LogSeverity] severity
 */
LogSeverity dmLogGetLevel();


#endif // NDEBUG

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace dmLog
{
    void RegisterLogListener(FLogListener listener);
    void UnregisterLogListener(FLogListener listener);
    void Setlevel(LogSeverity severity);
} //namespace dmLog

#endif //__cplusplus

#endif // DMSDK_LOG_H
