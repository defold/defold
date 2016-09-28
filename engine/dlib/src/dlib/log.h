#ifndef DM_LOG_H
#define DM_LOG_H

#include <stdint.h>

/**
 * @file
 * Logging functions. If DLIB_LOG_DOMAIN is defined the value of the defined is printed
 * after severity. Otherwise DEFAULT will be printed.
 *
 * Network protocol:
 * When connected a message with the following syntax is sent to the client
 * code <space> msg\n
 * eg 0 OK\n
 *
 * code > 0 indicates an error and the connections is closed by remote peer
 *
 * After connection is established log messages are streamed over the socket.
 * No other messages with semantic meaning is sent.
 */

/**
 * Log severity
 */
enum dmLogSeverity
{
    DM_LOG_SEVERITY_DEBUG       = 0,//!< DM_LOG_SEVERITY_DEBUB
    DM_LOG_SEVERITY_USER_DEBUG  = 1,//!< DM_LOG_SEVERITY_USER_DEBUB
    DM_LOG_SEVERITY_INFO        = 2,//!< DM_LOG_SEVERITY_INFO
    DM_LOG_SEVERITY_WARNING     = 3,//!< DM_LOG_SEVERITY_WARNING
    DM_LOG_SEVERITY_ERROR       = 4,//!< DM_LOG_SEVERITY_ERROR
    DM_LOG_SEVERITY_FATAL       = 5,//!< DM_LOG_SEVERITY_FATAL
};

#ifdef DM_LOG_DISABLE

#define dmLogInfo(format, args...) do {} while(0);
#define dmLogWarning(format, args...) do {} while(0);
#define dmLogError(format, args...) do {} while(0);
#define dmLogFatal(format, args...) do {} while(0);

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

#endif

struct dmLogParams
{
    dmLogParams()
    {
    }
};

/**
 * Initialize logging system. Running this function is only required in order to start the log-server.
 * The function will never fail even if the log-server can't be started. Any errors will be reported to stderr though
 * @param params log parameters
 */
void dmLogInitialize(const dmLogParams* params);


/**
 * Finalize logging system
 */
void dmLogFinalize();

/**
 * Get log server port
 * @return server port. 0 if the server isn't started.
 */
uint16_t dmLogGetPort();


/**
 * Set log level
 * @param severity Log severity
 */
void dmLogSetlevel(dmLogSeverity severity);

/**
 * Set log file. The file will be created and truncated.
 * Subsequent invocations to this function will close previous opened file.
 * If the file can't be created a message will be logged to the "console"
 * @param path log path
 */
void dmSetLogFile(const char* path);

#endif // DM_LOG_H
