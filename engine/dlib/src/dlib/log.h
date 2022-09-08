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

#ifndef DM_LOG_H
#define DM_LOG_H

#include <dmsdk/dlib/log.h>
#include <dlib/message.h>

namespace dmLog
{

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

struct LogMessage
{
    enum Type
    {
        MESSAGE = 0,
        SHUTDOWN = 1,
    };

    uint8_t m_Type:2;
    uint8_t m_Severity:6;
    char    m_Domain[15];
    char    m_Message[0];
};

const uint32_t MAX_STRING_SIZE = dmMessage::DM_MESSAGE_MAX_DATA_SIZE - sizeof(LogMessage);
struct LogParams
{
    LogParams()
    {
    }
};

/**
 * Initialize logging system. Running this function is only required in order to start the log-server.
 * The function will never fail even if the log-server can't be started. Any errors will be reported to stderr though
 * @param params log parameters
 */
void LogInitialize(const LogParams* params);


/**
 * Finalize logging system
 */
void LogFinalize();

/**
 * Get log server port
 * @return server port. 0 if the server isn't started.
 */
uint16_t GetPort();

/**
 * Set log file. The file will be created and truncated.
 * Subsequent invocations to this function will close previous opened file.
 * If the file can't be created a message will be logged to the "console"
 * @param path log path
 * @return true if file successfully created.
 */
bool SetLogFile(const char* path);

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
void __ios_log_print(LogSeverity severity, const char* str_buf);


} //namespace dmLog

#endif // DM_LOG_H
