#ifndef DM_SOCKET_H
#define DM_SOCKET_H

#include <stdint.h>

#if defined(__linux__) || defined(__MACH__)
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

/*
  ACCES,
  AFNOSUPPORT,
  AGAIN,
  BADF,
  CONNRESET,
  DESTADDRREQ,
  FAULT,
  HOSTUNREACH,
  INTR,
  INVAL,
  ISCONN,
  MFILE,
  MSGSIZE,
  NETDOWN,
  NETUNREACH,
  NFILE,
  NOBUFS,
  NOENT,
  NOMEM,
  NOTCONN,
  NOTDIR,
  NOTSOCK,
  OPNOTSUPP,
  PIPE,
  PROTONOSUPPORT,
  PROTOTYPE,
  TIMEDOUT,
*/

namespace dmSocket
{
    enum Result
    {
        RESULT_OK             = 0,

        RESULT_ACCES          = -1,
        RESULT_AFNOSUPPORT    = -2,
        RESULT_WOULDBLOCK     = -3,
        RESULT_BADF           = -4,
        RESULT_CONNRESET      = -5,
        RESULT_DESTADDRREQ    = -6,
        RESULT_FAULT          = -7,
        RESULT_HOSTUNREACH    = -8,
        RESULT_INTR           = -9,
        RESULT_INVAL          = -10,
        RESULT_ISCONN         = -11,
        RESULT_MFILE          = -12,
        RESULT_MSGSIZE        = -13,
        RESULT_NETDOWN        = -14,
        RESULT_NETUNREACH     = -15,
        //RESULT_NFILE          = -16,
        RESULT_NOBUFS         = -17,
        //RESULT_NOENT          = -18,
        //RESULT_NOMEM          = -19,
        RESULT_NOTCONN        = -20,
        //RESULT_NOTDIR         = -21,
        RESULT_NOTSOCK        = -22,
        RESULT_OPNOTSUPP      = -23,
        //RESULT_PIPE           = -24,
        RESULT_PROTONOSUPPORT = -25,
        RESULT_PROTOTYPE      = -26,
        RESULT_TIMEDOUT       = -27,

        RESULT_ADDRNOTAVAIL   = -28,
        RESULT_CONNREFUSED    = -29,
        RESULT_ADDRINUSE      = -30,

        // gethostbyname errors
        RESULT_HOST_NOT_FOUND = -100,
        RESULT_TRY_AGAIN      = -101,
        RESULT_NO_RECOVERY    = -102,
        RESULT_NO_DATA        = -103,

        RESULT_UNKNOWN        = -1000,
    };

    typedef int Socket;
    typedef uint32_t Address; // TODO: Always in network order?

#if defined(__linux__) || defined(__MACH__)
    const Socket INVALID_SOCKET = -1;
#endif

    enum Type
    {
        TYPE_STREAM = SOCK_STREAM,
        TYPE_DGRAM = SOCK_DGRAM,
    };

    enum Protocol
    {
        PROTOCOL_TCP = IPPROTO_TCP,
        PROTOCOL_UDP = IPPROTO_UDP,
    };

    enum ShutdownType
    {
#if defined(__linux__) || defined(__MACH__)
        SHUTDOWNTYPE_READ      = SHUT_RD,
        SHUTDOWNTYPE_WRITE     = SHUT_WR,
        SHUTDOWNTYPE_READWRITE = SHUT_RDWR,
#else
        SHUTDOWNTYPE_READ      = SD_RECEIVE,
        SHUTDOWNTYPE_WRITE     = SD_SEND,
        SHUTDOWNTYPE_READWRITE = SD_BOTH,
#endif
    };

    Result Setup();
    Result Shutdown();
    Result New(Type type, Protocol protocol, Socket* socket);
    Result Delete(Socket socket);
    Result Accept(Socket socket, Address* address, Socket* accept_socket);
    Result Bind(Socket socket, Address address, int port);
    Result Connect(Socket socket, Address address, int port);
    Result Listen(Socket socket, int backlog);
    Result Shutdown(Socket socket, ShutdownType how);

    Result Send(Socket socket, const void* buffer, int length, int* sent_bytes);     // TODO: No flags here
    //ssize_t SendMessage(Socket socket, const struct msghdr *buffer, int flags);
    //ssize_t SentTo(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr,
    //socklen_t dest_len);

    Result Receive(Socket socket, void* buffer, int length, int* received_bytes); // TODO: No flags here

    Result SetBlocking(Socket socket, bool blocking);

    //ssize_t recvfrom(int socket, void *restrict buffer, size_t length, int flags,
    //struct sockaddr *restrict address, socklen_t *restrict address_len);
    //ssize_t recvmsg(int socket, struct msghdr *message, int flags);

    Address AddressFromIPString(const char* address);
    Result GetHostByName(const char* name, Address* address);

    const char* ResultToString(Result result);

#if 0

// int fcntl(int fildes, int cmd, ...);
#endif

}

#endif // DM_SOCKET_H
