#ifndef DM_LOG_H
#define DM_LOG_H

#include <dmsdk/dlib/log.h>

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

/**
 * iOS specific print function that wraps NSLog to be able to
 * output logging to the device/XCode log.
 *
 * Declared here to be accessible from log.cpp, defined in log_ios.mm
 * since it needs to be compiled as Objective-C.
 *
 * @param severity Log severity
 * @param str_buf String buffer to print
 */
void __ios_log_print(dmLogSeverity severity, const char* str_buf);


#endif // DM_LOG_H
