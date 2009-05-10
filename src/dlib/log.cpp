#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "log.h"

LogSeverity g_LogLevel = LOG_SEVERITY_WARNING;

void LogSetlevel(LogSeverity severity)
{
    g_LogLevel = severity;
}

void LogInternal(LogSeverity severity, const char* format, ...)
{
    if (severity < g_LogLevel)
        return;

    va_list lst;
    va_start(lst, format);

    const char* severity_str = 0;
    switch (severity)
    {
        case LOG_SEVERITY_INFO:
            severity_str = "INFO";
            break;
        case LOG_SEVERITY_WARNING:
            severity_str = "WARNING";
            break;
        case LOG_SEVERITY_ERROR:
            severity_str = "ERROR";
            break;
        case LOG_SEVERITY_FATAL:
            severity_str = "FATAL";
            break;
        default:
            assert(0);
    }

    fprintf(stderr, "%s: ", severity_str);
    vfprintf(stderr, format, lst);
    fprintf(stderr, "\n");
    va_end(lst);
}
