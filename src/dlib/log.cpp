#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "log.h"

dmLogSeverity g_LogLevel = DM_LOG_SEVERITY_WARNING;

void dmLogSetlevel(dmLogSeverity severity)
{
    g_LogLevel = severity;
}

void dmLogInternal(dmLogSeverity severity, const char* domain, const char* format, ...)
{
    if (severity < g_LogLevel)
        return;

    va_list lst;
    va_start(lst, format);

    const char* severity_str = 0;
    switch (severity)
    {
        case DM_LOG_SEVERITY_INFO:
            severity_str = "INFO";
            break;
        case DM_LOG_SEVERITY_WARNING:
            severity_str = "WARNING";
            break;
        case DM_LOG_SEVERITY_ERROR:
            severity_str = "ERROR";
            break;
        case DM_LOG_SEVERITY_FATAL:
            severity_str = "FATAL";
            break;
        default:
            assert(0);
    }

    fprintf(stderr, "%s:%s: ", severity_str, domain);
    vfprintf(stderr, format, lst);
    fprintf(stderr, "\n");
    va_end(lst);
}
