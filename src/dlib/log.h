#ifndef DM_LOG_H
#define DM_LOG_H

/**
 * @file
 * Logging functions. If DLIB_LOG_DOMAIN is defined the value of the defined is printed
 * after severity. Otherwise DEFAULT will be printed.
 */

/**
 * Log severity
 */
enum dmLogSeverity
{
    DM_LOG_SEVERITY_INFO    = 0,//!< DM_LOG_SEVERITY_INFO
    DM_LOG_SEVERITY_WARNING = 1,//!< DM_LOG_SEVERITY_WARNING
    DM_LOG_SEVERITY_ERROR   = 2,//!< DM_LOG_SEVERITY_ERROR
    DM_LOG_SEVERITY_FATAL   = 3,//!< DM_LOG_SEVERITY_FATAL
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

void dmLogInternal(dmLogSeverity severity, const char* domain, const char* format, ...);

#ifdef _MSC_VER
#define dmLogInfo(format, ... ) dmLogInternal(DM_LOG_SEVERITY_INFO, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogWarning(format, ... ) dmLogInternal(DM_LOG_SEVERITY_WARNING, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogError(format, ... ) dmLogInternal(DM_LOG_SEVERITY_ERROR, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#define dmLogFatal(format, ... ) dmLogInternal(DM_LOG_SEVERITY_FATAL, DLIB_LOG_DOMAIN, format, __VA_ARGS__ );
#else
#define dmLogInfo(format, args...) dmLogInternal(DM_LOG_SEVERITY_INFO, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogWarning(format, args...) dmLogInternal(DM_LOG_SEVERITY_WARNING, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogError(format, args...) dmLogInternal(DM_LOG_SEVERITY_ERROR, DLIB_LOG_DOMAIN, format, ## args);
#define dmLogFatal(format, args...) dmLogInternal(DM_LOG_SEVERITY_FATAL, DLIB_LOG_DOMAIN, format, ## args);
#endif

#endif

/**
 * Set log level
 * @param severity Log severity
 */
void dmLogSetlevel(dmLogSeverity severity);

#endif // DM_LOG_H
