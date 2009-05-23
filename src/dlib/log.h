#ifndef LOG_H
#define LOG_H

enum LogSeverity
{
    LOG_SEVERITY_INFO    = 0,
    LOG_SEVERITY_WARNING = 1,
    LOG_SEVERITY_ERROR   = 2,
    LOG_SEVERITY_FATAL   = 3,
};

#ifdef LOG_DISABLE

#define LogInfo(format, args...) do {} while(0);
#define LogWarning(format, args...) do {} while(0);
#define LogError(format, args...) do {} while(0);
#define LogFatal(format, args...) do {} while(0);

#else

void LogInternal(LogSeverity severity, const char* format, ...);

#ifdef _MSC_VER
#define LogInfo(format, args ) LogInternal(LOG_SEVERITY_INFO, format, ## args );
#define LogWarning(format, args ) LogInternal(LOG_SEVERITY_WARNING, format, ## args );
#define LogError(format, args ) LogInternal(LOG_SEVERITY_ERROR, format, ## args );
#define LogFatal(format, args ) LogInternal(LOG_SEVERITY_FATAL, format, ## args );
#else
#define LogInfo(format, args...) LogInternal(LOG_SEVERITY_INFO, format, ## args);
#define LogWarning(format, args...) LogInternal(LOG_SEVERITY_WARNING, format, ## args);
#define LogError(format, args...) LogInternal(LOG_SEVERITY_ERROR, format, ## args);
#define LogFatal(format, args...) LogInternal(LOG_SEVERITY_FATAL, format, ## args);
#endif

#endif

void LogSetlevel(LogSeverity severity);

#endif // LOG_H
