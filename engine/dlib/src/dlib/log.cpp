#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "dlib.h"
#include "array.h"
#include "dstrings.h"
#include "log.h"
#include "socket.h"
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

const int DM_LOG_MAX_LOG_FILE_SIZE = 1024 * 1024 * 32;

struct dmLogConnection
{
    dmLogConnection()
    {
        m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
    }

    dmSocket::Socket m_Socket;
};

struct dmLogMessage
{
    enum Type
    {
        MESSAGE = 0,
        SHUTDOWN = 1,
    };

    uint8_t m_Type;
    char    m_Message[0];
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
        m_MessgeSocket = message_socket;
        m_Thread = 0;
    }

    dmArray<dmLogConnection> m_Connections;
    dmSocket::Socket         m_ServerSocket;
    uint16_t                 m_Port;
    dmMessage::HSocket       m_MessgeSocket;
    dmThread::Thread         m_Thread;
};

dmLogServer* g_dmLogServer = 0;
dmLogSeverity g_LogLevel = DM_LOG_SEVERITY_USER_DEBUG;
int g_TotalBytesLogged = 0;
FILE* g_LogFile = 0;

// create and bind the server socket, will reuse old port if supplied handle valid
static void dmLogInitSocket( dmSocket::Socket& server_socket )
{
    if (!dLib::IsDebugMode() || !dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP))
        return;

    dmSocket::Result r;
    dmSocket::Address address;
    uint16_t port = 0;
    char error_msg[1024] = { 0 };

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

static void dmLogDispatch(dmMessage::Message *message, void* user_ptr)
{
    dmLogServer* self = g_dmLogServer;

    bool* run = (bool*) user_ptr;
    dmLogMessage* log_message = (dmLogMessage*) &message->m_Data[0];
    if (log_message->m_Type == dmLogMessage::SHUTDOWN)
    {
        *run = false;
        return;
    }
    int msg_len = (int) strlen(log_message->m_Message);

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
        dmMessage::Dispatch(self->m_MessgeSocket, dmLogDispatch, (void*) &run);
    }
}

void dmLogInitialize(const dmLogParams* params)
{
    g_TotalBytesLogged = 0;

    if (!dLib::IsDebugMode() || !dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP))
        return;

    if (g_dmLogServer)
    {
        fprintf(stderr, "ERROR:DLIB: dmLog already initialized\n");
        return;
    }

    dmSocket::Socket server_socket = dmSocket::INVALID_SOCKET_HANDLE;
    dmSocket::Address address;
    uint16_t port;
    dmLogInitSocket(server_socket);
    if (server_socket == dmSocket::INVALID_SOCKET_HANDLE)
    {
        return;
    }
    dmSocket::GetName(server_socket, &address, &port);

    dmMessage::HSocket message_socket = 0;
    dmMessage::Result mr;
    dmThread::Thread thread = 0;

    mr = dmMessage::NewSocket("@log", &message_socket);
    if (mr != dmMessage::RESULT_OK)
    {
        fprintf(stderr, "ERROR:DLIB: Unable to create @log message socket\n");
        if (message_socket != 0)
            dmMessage::DeleteSocket(message_socket);
        dmSocket::Delete(server_socket);
        return;
    }

    g_dmLogServer = new dmLogServer(server_socket, port, message_socket);
    thread = dmThread::New(dmLogThread, 0x80000, 0, "log");
    g_dmLogServer->m_Thread = thread;

    /*
     * This message is parsed by editor 2 - don't remove or change without
     * corresponding changes in engine.clj
     */
    dmLogInfo("Log server started on port %u", (unsigned int) port);
}

void dmLogFinalize()
{
    if (!g_dmLogServer)
        return;
    dmLogServer* self = g_dmLogServer;

    dmLogMessage msg;
    msg.m_Type = dmLogMessage::SHUTDOWN;
    dmMessage::URL receiver;
    receiver.m_Socket = self->m_MessgeSocket;
    receiver.m_Path = 0;
    receiver.m_Fragment = 0;
    dmMessage::Post(0, &receiver, 0, 0, 0, &msg, sizeof(msg), 0);
    dmThread::Join(self->m_Thread);

    uint32_t n = self->m_Connections.Size();
    for (uint32_t i = 0; i < n; ++i)
    {
        dmLogConnection* c = &self->m_Connections[i];
        dmSocket::Shutdown(c->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
        dmSocket::Delete(c->m_Socket);
    }

    if (self->m_ServerSocket != dmSocket::INVALID_SOCKET_HANDLE)
    {
        dmSocket::Delete(self->m_ServerSocket);
    }

    if (self->m_MessgeSocket != 0)
    {
        dmMessage::DeleteSocket(self->m_MessgeSocket);
    }

    delete self;
    g_dmLogServer = 0;
    if (g_LogFile) {
        fclose(g_LogFile);
        g_LogFile = 0;
    }
}

uint16_t dmLogGetPort()
{
    if (!g_dmLogServer)
        return 0;

    return g_dmLogServer->m_Port;
}

void dmLogSetlevel(dmLogSeverity severity)
{
    g_LogLevel = severity;
}

#ifdef ANDROID
static android_LogPriority ToAndroidPriority(dmLogSeverity severity)
{
    switch (severity)
    {
        case DM_LOG_SEVERITY_DEBUG:
            return ANDROID_LOG_DEBUG;

        case DM_LOG_SEVERITY_USER_DEBUG:
            return ANDROID_LOG_DEBUG;

        case DM_LOG_SEVERITY_INFO:
            return ANDROID_LOG_INFO;

        case DM_LOG_SEVERITY_WARNING:
            return ANDROID_LOG_WARN;

        case DM_LOG_SEVERITY_ERROR:
            return ANDROID_LOG_ERROR;

        case DM_LOG_SEVERITY_FATAL:
            return ANDROID_LOG_FATAL;

        default:
            return ANDROID_LOG_ERROR;
    }
}
#endif

void dmLogInternal(dmLogSeverity severity, const char* domain, const char* format, ...)
{
    if (!dLib::IsDebugMode())
        return;

    if (severity < g_LogLevel)
        return;

    va_list lst;
    va_start(lst, format);

    const char* severity_str = 0;
    switch (severity)
    {
        case DM_LOG_SEVERITY_DEBUG:
            severity_str = "DEBUG";
            break;
        case DM_LOG_SEVERITY_USER_DEBUG:
            severity_str = "DEBUG";
            break;
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
            break;
    }


    // NOTE: Must be less than DM_MESSAGE_PAGE_SIZE
    const int str_buf_size = 2048;
    char tmp_buf[sizeof(dmLogMessage) + str_buf_size];
    dmLogMessage* msg = (dmLogMessage*) &tmp_buf[0];
    char* str_buf = &tmp_buf[sizeof(dmLogMessage)];

    int n = 0;
    n += DM_SNPRINTF(str_buf + n, str_buf_size - n, "%s:%s: ", severity_str, domain);
    if (n < str_buf_size)
    {
        n += vsnprintf(str_buf + n, str_buf_size - n, format, lst);
    }

    if (n < str_buf_size)
    {
        n += DM_SNPRINTF(str_buf + n, str_buf_size - n, "\n");
    }
    str_buf[str_buf_size-1] = '\0';
    int actual_n = dmMath::Min(n, str_buf_size-1);

    g_TotalBytesLogged += actual_n;

#ifdef ANDROID
    __android_log_print(ToAndroidPriority(severity), "defold", str_buf);
#else
    fwrite(str_buf, 1, actual_n, stderr);
#endif
    va_end(lst);

    if(!dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP))
        return;

    if (g_LogFile && g_TotalBytesLogged < DM_LOG_MAX_LOG_FILE_SIZE) {
        fwrite(str_buf, 1, actual_n, g_LogFile);
        fflush(g_LogFile);
    }

    dmLogServer* self = g_dmLogServer;
    if (self)
    {
        msg->m_Type = dmLogMessage::MESSAGE;
        dmMessage::URL receiver;
        receiver.m_Socket = self->m_MessgeSocket;
        receiver.m_Path = 0;
        receiver.m_Fragment = 0;
        dmMessage::Post(0, &receiver, 0, 0, 0, msg, dmMath::Min(sizeof(dmLogMessage) + n + 1, sizeof(tmp_buf)), 0);
    }
}

void dmSetLogFile(const char* path)
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
    }
}
