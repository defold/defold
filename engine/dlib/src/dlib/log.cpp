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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "atomic.h"
#include "dlib.h"
#include "array.h"
#include "dstrings.h"
#include "log.h"
#include "profile/profile.h"
#include "socket.h"
#include "spinlock.h"
#include "message.h"
#include "thread.h"
#include "math.h"
#include "time.h"
#include "path.h"
#include "sys.h"
#include "network_constants.h"

#ifdef ANDROID
#include <android/log.h>
#endif

namespace dmLog
{

const char* LOG_OUTPUT_TRUNCATED_MESSAGE = "...\n[Output truncated]\n";
const int MAX_LOG_FILE_SIZE = 1024 * 1024 * 32;

struct dmLogConnection
{
    dmLogConnection()
    {
        m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
    }

    dmSocket::Socket m_Socket;
};

static const uint32_t DLIB_MAX_LOG_CONNECTIONS = 16;

struct dmLogServer
{
    dmLogServer(dmSocket::Socket server_socket, uint16_t port,
                 dmMessage::HSocket message_socket)
    {
        m_Connections.SetCapacity(DLIB_MAX_LOG_CONNECTIONS);
        m_ServerSocket = server_socket;
        m_Port = port;
        m_MessageSocket = message_socket;
        m_Thread = 0;
    }

    dmArray<dmLogConnection> m_Connections;
    dmSocket::Socket         m_ServerSocket;
    uint16_t                 m_Port;
    dmMessage::HSocket       m_MessageSocket;
    dmThread::Thread         m_Thread;
};

static dmLogServer* g_dmLogServer = 0;
static LogSeverity g_LogLevel = LOG_SEVERITY_USER_DEBUG;
static int g_TotalBytesLogged = 0;
static FILE* g_LogFile = 0;
static dmSpinlock::Spinlock g_ListenerLock; // Protects the array of listener functions

static const int g_MaxListeners = 32;
static FLogListener g_Listeners[g_MaxListeners];
static int32_atomic_t g_ListenersCount;

// create and bind the server socket, will reuse old port if supplied handle valid
static void dmLogInitSocket( dmSocket::Socket& server_socket )
{
    if (!dLib::IsDebugMode() || !dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP))
        return;

    dmSocket::Result r;
    dmSocket::Address address;
    uint16_t port = 0;
    char error_msg[1024] = { 0 };

    // If the env variable DM_LOG_PORT is set we will try to use
    // that as port for the log server instead of a dynamic one.
    const char* env_log_port = getenv("DM_LOG_PORT");
    if (env_log_port != 0x0)
    {
        long t_port = strtol(env_log_port, 0, 10);
        if (t_port > 0 && t_port < UINT16_MAX)
        {
            port = t_port;
        }
    }

    if (server_socket != dmSocket::INVALID_SOCKET_HANDLE)
    {
        r = dmSocket::GetName(server_socket, &address, &port); // need to reuse the port
        if (r != dmSocket::RESULT_OK)
        {
            snprintf(error_msg, sizeof(error_msg), "Unable to retrieve socket information (%d): %s", r, dmSocket::ResultToString(r));
            goto bail;
        }

        r = dmSocket::Delete(server_socket);
        server_socket = dmSocket::INVALID_SOCKET_HANDLE;
        if (r != dmSocket::RESULT_OK)
        {
            snprintf(error_msg, sizeof(error_msg), "Unable to delete old log socket (%d): %s", r, dmSocket::ResultToString(r));
            goto bail;
        }
    }
    else
    {
        r = dmSocket::GetHostByName(DM_UNIVERSAL_BIND_ADDRESS_IPV4, &address);
        if (r != dmSocket::RESULT_OK)
        {
            snprintf(error_msg, sizeof(error_msg), "Unable to get listening address for log socket (%d): %s", r, dmSocket::ResultToString(r));
            goto bail;
        }
    }

    r = dmSocket::New(address.m_family, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &server_socket);
    if (r != dmSocket::RESULT_OK)
    {
        snprintf(error_msg, sizeof(error_msg), "Unable to create log socket (%d): %s", r, dmSocket::ResultToString(r));
        goto bail;
    }

    dmSocket::SetReuseAddress(server_socket, true);
    r = dmSocket::Bind(server_socket, address, port);
    if (r != dmSocket::RESULT_OK)
    {
        snprintf(error_msg, sizeof(error_msg), "Unable to bind to log socket (%d): %s", r, dmSocket::ResultToString(r));
        goto bail;
    }

    r = dmSocket::Listen(server_socket, 32);
    if (r != dmSocket::RESULT_OK)
    {
        snprintf(error_msg, sizeof(error_msg), "Unable to listen on log socket (%d): %s", r, dmSocket::ResultToString(r));
        goto bail;
    }

    return;

bail:
    fprintf(stderr, "ERROR:DLIB: %s\n", error_msg);
    if (server_socket != dmSocket::INVALID_SOCKET_HANDLE)
        dmSocket::Delete(server_socket);

    server_socket = dmSocket::INVALID_SOCKET_HANDLE;
}

static dmSocket::Result SendAll(dmSocket::Socket socket, const char* buffer, int length)
{
    int total_sent_bytes = 0;
    int sent_bytes = 0;

    while (total_sent_bytes < length)
    {
        dmSocket::Result r = dmSocket::Send(socket, buffer + total_sent_bytes, length - total_sent_bytes, &sent_bytes);
        if (r == dmSocket::RESULT_TRY_AGAIN)
            continue;

        if (r != dmSocket::RESULT_OK)
        {
            return r;
        }

        total_sent_bytes += sent_bytes;
    }

    return dmSocket::RESULT_OK;
}

static void dmLogUpdateNetwork()
{
    dmLogServer* self = g_dmLogServer;
    if (self->m_ServerSocket == dmSocket::INVALID_SOCKET_HANDLE)
        return;

    dmSocket::Selector selector;
    dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, self->m_ServerSocket);
    dmSocket::Result r = dmSocket::Select(&selector, 0);
    if (r == dmSocket::RESULT_OK)
    {
        // Check for new connections
        if (dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, self->m_ServerSocket))
        {
            dmSocket::Address address;
            dmSocket::Socket client_socket;
            r = dmSocket::Accept(self->m_ServerSocket, &address, &client_socket);
            if (r == dmSocket::RESULT_OK)
            {
                if (self->m_Connections.Full())
                {
                    dmLogError("Too many log connections opened");
                    const char* resp = "1 Too many log connections opened\n";
                    SendAll(client_socket, resp, strlen(resp));
                    dmSocket::Shutdown(client_socket, dmSocket::SHUTDOWNTYPE_READWRITE);
                    dmSocket::Delete(client_socket);
                }
                else
                {
                    const char* resp = "0 OK\n";
                    SendAll(client_socket, resp, strlen(resp));
                    dmSocket::SetNoDelay(client_socket, true);
                    dmLogConnection connection;
                    memset(&connection, 0, sizeof(connection));
                    connection.m_Socket = client_socket;
                    self->m_Connections.Push(connection);
                }
            }
            else if (r == dmSocket::RESULT_BADF || r == dmSocket::RESULT_CONNABORTED)
            {
                // reinitalize log socket
                dmLogInitSocket(g_dmLogServer->m_ServerSocket);
            }
        }
    }
}

#ifdef ANDROID
static android_LogPriority ToAndroidPriority(LogSeverity severity)
{
    switch (severity)
    {
        case LOG_SEVERITY_DEBUG:
            return ANDROID_LOG_DEBUG;

        case LOG_SEVERITY_USER_DEBUG:
            return ANDROID_LOG_DEBUG;

        case LOG_SEVERITY_INFO:
            return ANDROID_LOG_INFO;

        case LOG_SEVERITY_WARNING:
            return ANDROID_LOG_WARN;

        case LOG_SEVERITY_ERROR:
            return ANDROID_LOG_ERROR;

        case LOG_SEVERITY_FATAL:
            return ANDROID_LOG_FATAL;

        default:
            return ANDROID_LOG_ERROR;
    }
}
#endif

static void DoLogPlatform(LogSeverity severity, const char* output, int output_len)
{
#ifdef ANDROID
        __android_log_print(dmLog::ToAndroidPriority(severity), "defold", "%s", output);

// iOS
#elif defined(__MACH__) && (defined(__arm__) || defined(__arm64__))
        dmLog::__ios_log_print(severity, output);
#endif

#ifdef __EMSCRIPTEN__
        //Emscripten maps stderr to console.error and stdout to console.log.
        if (severity == LOG_SEVERITY_ERROR || severity == LOG_SEVERITY_FATAL){
            fwrite(output, 1, output_len, stderr);
        } else {
            fwrite(output, 1, output_len, stdout);
        }
#elif !defined(ANDROID)
        fwrite(output, 1, output_len, stderr);
#endif

    if (dmLog::g_LogFile && dmLog::g_TotalBytesLogged < dmLog::MAX_LOG_FILE_SIZE) {
        dmLog::g_TotalBytesLogged += output_len;
        fwrite(output, 1, output_len, dmLog::g_LogFile);
        fflush(dmLog::g_LogFile);
    }
}

// Here we put logging that needs to be thread safe
// We either push it on the logger thread, or from the main thread if threads aren't supported (e.g. html5)
static void DoLogSynchronized(LogSeverity severity, const char* domain, const char* output, int output_len)
{
    DM_SPINLOCK_SCOPED_LOCK(g_ListenerLock); // Make sure no listeners are added or removed during the scope

    for (int i = dmLog::g_ListenersCount - 1; i >= 0 ; --i)
    {
        g_Listeners[i]((LogSeverity)severity, domain, output);
    }

    dmProfile::LogText("%s", output);
}

static void dmLogDispatch(dmMessage::Message *message, void* user_ptr)
{
    dmLogServer* self = g_dmLogServer;

    bool* run = (bool*) user_ptr;
    LogMessage* log_message = (LogMessage*) &message->m_Data[0];
    if (log_message->m_Type == LogMessage::SHUTDOWN)
    {
        *run = false;
        return;
    }

    int msg_len = (int) strlen(log_message->m_Message);
    DoLogSynchronized((LogSeverity)log_message->m_Severity, log_message->m_Domain, log_message->m_Message, msg_len);

    // NOTE: Keep i as signed! See --i below after EraseSwap
    int n = (int) self->m_Connections.Size();
    for (int i = 0; i < n; ++i)
    {
        dmLogConnection* c = &self->m_Connections[i];
        dmSocket::Result r;
        int sent_bytes;
        int total_sent = 0;
        do
        {
            r = dmSocket::Send(c->m_Socket, log_message->m_Message + total_sent, msg_len - total_sent, &sent_bytes);
            if (r == dmSocket::RESULT_OK)
            {
                total_sent += sent_bytes;
            }
            else if (r == dmSocket::RESULT_TRY_AGAIN)
            {
                // Ok
            }
            else
            {
                dmSocket::Shutdown(c->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
                dmSocket::Delete(c->m_Socket);
                c->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
                self->m_Connections.EraseSwap(i);
                --n;
                --i;
                break;
            }
        } while (total_sent < msg_len);
    }
}

static void dmLogThread(void* args)
{
    dmLogServer* self = g_dmLogServer;

    volatile bool run = true;
    while (run)
    {
        // NOTE: We have support for blocking dispatch in dmMessage
        // but we have to wait for both new messages and on sockets.
        // Currently no support for that and hence the sleep here
        dmTime::Sleep(1000 * 30);
        dmLogUpdateNetwork();
        dmMessage::Dispatch(self->m_MessageSocket, dmLogDispatch, (void*) &run);
    }
}

void LogInitialize(const LogParams* params)
{
    g_TotalBytesLogged = 0;

    if (g_dmLogServer)
    {
        fprintf(stderr, "ERROR:DLIB: dmLog already initialized\n");
        return;
    }

    dmSocket::Socket server_socket = dmSocket::INVALID_SOCKET_HANDLE;
    uint16_t port = 0;

    if (dLib::IsDebugMode() && dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP))
    {
        dmLogInitSocket(server_socket);
        if (server_socket != dmSocket::INVALID_SOCKET_HANDLE)
        {
            dmSocket::Address address;
            dmSocket::GetName(server_socket, &address, &port);
        }
    }

    dmMessage::HSocket message_socket = 0;
    dmMessage::Result mr;

    mr = dmMessage::NewSocket("@log", &message_socket);
    if (mr != dmMessage::RESULT_OK)
    {
        fprintf(stderr, "ERROR:DLIB: Unable to create @log message socket\n");
        if (message_socket != 0)
            dmMessage::DeleteSocket(message_socket);
        if (server_socket != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(server_socket);
        return;
    }

    g_dmLogServer = new dmLogServer(server_socket, port, message_socket);

    g_dmLogServer->m_Thread = 0;
    if(dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP)) // e.g. Emscripten
    {
        g_dmLogServer->m_Thread = dmThread::New(dmLogThread, 0x80000, 0, "log");
    }

    dmAtomicStore32(&g_ListenersCount, 0);
    dmSpinlock::Init(&g_ListenerLock);

    /*
     * This message is parsed by editor 2 - don't remove or change without
     * corresponding changes in engine.clj
     */
    dmLogInfo("Log server started on port %u", (unsigned int) port);
}

static void CloseLogFile()
{
    if (g_LogFile) {
        fclose(g_LogFile);
        g_LogFile = 0;
    }
}

void LogFinalize()
{
    if (!g_dmLogServer)
    {
        CloseLogFile();
        return;
    }
    dmLogServer* self = g_dmLogServer;

    LogMessage msg;
    msg.m_Type = LogMessage::SHUTDOWN;
    dmMessage::URL receiver;
    receiver.m_Socket = self->m_MessageSocket;
    receiver.m_Path = 0;
    receiver.m_Fragment = 0;
    dmMessage::Post(0, &receiver, 0, 0, 0, &msg, sizeof(msg), 0);

    if (self->m_Thread)
        dmThread::Join(self->m_Thread);

    uint32_t n = self->m_Connections.Size();
    for (uint32_t i = 0; i < n; ++i)
    {
        dmLogConnection* c = &self->m_Connections[i];
        dmSocket::Shutdown(c->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
        dmSocket::Delete(c->m_Socket);
        c->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
    }

    if (self->m_ServerSocket != dmSocket::INVALID_SOCKET_HANDLE)
    {
        dmSocket::Delete(self->m_ServerSocket);
        self->m_ServerSocket = dmSocket::INVALID_SOCKET_HANDLE;
    }

    if (self->m_MessageSocket != 0)
    {
        dmMessage::DeleteSocket(self->m_MessageSocket);
    }

    delete self;
    g_dmLogServer = 0;
    CloseLogFile();
}

uint16_t GetPort()
{
    if (!g_dmLogServer)
        return 0;

    return g_dmLogServer->m_Port;
}

bool SetLogFile(const char* path)
{
    if (g_LogFile) {
        fclose(g_LogFile);
        g_LogFile = 0;
    }
    g_LogFile = fopen(path, "wb");
    if (g_LogFile) {
        dmLogInfo("Writing log to: %s", path);
    } else {
        dmLogFatal("Failed to open log-file '%s'", path);
        return false;
    }
    return true;
}

} //namespace dmLog

void dmLogRegisterListener(FLogListener listener)
{
    DM_SPINLOCK_SCOPED_LOCK(dmLog::g_ListenerLock);
    if (dmLog::g_ListenersCount >= dmLog::g_MaxListeners) {
        dmLogWarning("Max dmLog listeners reached (%d)", dmLog::g_MaxListeners);
    } else {
        dmLog::g_Listeners[dmLog::g_ListenersCount++] = listener;
    }
}

void dmLogUnregisterListener(FLogListener listener)
{
    DM_SPINLOCK_SCOPED_LOCK(dmLog::g_ListenerLock);
    for (int i = 0; i < dmLog::g_ListenersCount; ++i)
    {
        if (dmLog::g_Listeners[i] == listener)
        {
            dmLog::g_Listeners[i] = dmLog::g_Listeners[dmLog::g_ListenersCount - 1];
            dmLog::g_ListenersCount--;
            return;
        }
    }
    dmLogWarning("dmLog listener not found");
}

void dmLogSetLevel(LogSeverity severity)
{
    dmLog::g_LogLevel = severity;
}

LogSeverity dmLogGetLevel()
{
    return dmLog::g_LogLevel;
}

// Deprecated. Try to move back to the C api
namespace dmLog {
    void RegisterLogListener(FLogListener listener)     { dmLogRegisterListener(listener); }
    void UnregisterLogListener(FLogListener listener)   { dmLogUnregisterListener(listener); }
    void Setlevel(LogSeverity severity)                 { dmLogSetLevel(severity); }
}


void LogInternal(LogSeverity severity, const char* domain, const char* format, ...)
{
    if (!dmLog::g_dmLogServer) // if unit tests forgot to create the log system
    {
        return;
    }

    if (severity < dmLog::g_LogLevel)
    {
        return;
    }

    // In release mode, if there are no custom listeners, we'll return here
    bool is_debug_mode = dLib::IsDebugMode();
    if (!is_debug_mode && (dmAtomicGet32(&dmLog::g_ListenersCount) == 0))
    {
        return;
    }

    va_list lst;
    va_start(lst, format);

    const char* severity_str = 0;
    switch (severity)
    {
        case LOG_SEVERITY_DEBUG:        severity_str = "DEBUG"; break;
        case LOG_SEVERITY_USER_DEBUG:   severity_str = "DEBUG"; break;
        case LOG_SEVERITY_INFO:         severity_str = "INFO"; break;
        case LOG_SEVERITY_WARNING:      severity_str = "WARNING"; break;
        case LOG_SEVERITY_ERROR:        severity_str = "ERROR"; break;
        case LOG_SEVERITY_FATAL:        severity_str = "FATAL"; break;
        default:
            assert(0);
            break;
    }

    char tmp_buf[sizeof(dmLog::LogMessage) + dmLog::MAX_STRING_SIZE];
    dmLog::LogMessage* msg = (dmLog::LogMessage*) &tmp_buf[0];
    char* str_buf = &tmp_buf[sizeof(dmLog::LogMessage)];

    int n = 0;
    n += dmSnPrintf(str_buf + n, dmLog::MAX_STRING_SIZE - n, "%s:%s: ", severity_str, domain);
    if (n < dmLog::MAX_STRING_SIZE)
    {
        n += vsnprintf(str_buf + n, dmLog::MAX_STRING_SIZE - n, format, lst);
    }

    if (n < dmLog::MAX_STRING_SIZE)
    {
        n += dmSnPrintf(str_buf + n, dmLog::MAX_STRING_SIZE - n, "\n");
    }

    if (n >= dmLog::MAX_STRING_SIZE)
    {
        strcpy(&str_buf[dmLog::MAX_STRING_SIZE - (strlen(dmLog::LOG_OUTPUT_TRUNCATED_MESSAGE) + 1)], dmLog::LOG_OUTPUT_TRUNCATED_MESSAGE);
    }

    str_buf[dmLog::MAX_STRING_SIZE-1] = '\0';
    int actual_n = dmMath::Min(n, (int)(dmLog::MAX_STRING_SIZE-1));

    va_end(lst);

    // Could potentially be moved to the thread, but these are already thread safe, and
    // I think it's good to output info directly to the native platform as soon as possible.
    if (is_debug_mode)
    {
        dmLog::DoLogPlatform(severity, str_buf, actual_n);
    }

    if (!dmLog::g_dmLogServer->m_Thread) // e.g. Emscripten
    {
        dmLog::DoLogSynchronized(severity, domain, str_buf, actual_n);
    }

    if(!dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP)) // e.g. Emscripten
        return;

    if (dmThread::GetCurrentThread() == dmLog::g_dmLogServer->m_Thread)
    {
        // Due to the recursive nature, we're not allowed make new dmLogXxx calls from the dispatch thread
        // However, we may call the print functions
        return;
    }

    dmLog::dmLogServer* self = dmLog::g_dmLogServer;
    if (self)
    {
        msg->m_Type = dmLog::LogMessage::MESSAGE;
        msg->m_Severity = severity;
        dmStrlCpy(msg->m_Domain, domain, sizeof(msg->m_Domain));
        dmMessage::URL receiver;
        receiver.m_Socket = self->m_MessageSocket;
        receiver.m_Path = 0;
        receiver.m_Fragment = 0;
        dmMessage::Post(0, &receiver, 0, 0, 0, msg, dmMath::Min(sizeof(dmLog::LogMessage) + actual_n + 1, sizeof(tmp_buf)), 0);
    }
}
