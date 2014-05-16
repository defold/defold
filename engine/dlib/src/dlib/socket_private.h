#ifndef DM_SOCKET_PRIVATE_H
#define DM_SOCKET_PRIVATE_H

namespace dmSocket
{
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
    #define DM_SOCKET_ERRNO errno
    #define DM_SOCKET_HERRNO h_errno
#else
    #define DM_SOCKET_ERRNO WSAGetLastError()
    #define DM_SOCKET_HERRNO WSAGetLastError()
#endif

#if defined(_WIN32)
    #define DM_SOCKET_NATIVE_TO_RESULT_CASE(x) case WSAE##x: return RESULT_##x
#else
    #define DM_SOCKET_NATIVE_TO_RESULT_CASE(x) case E##x: return RESULT_##x
#endif
    static Result NativeToResult(int r)
    {
        switch (r)
        {
            DM_SOCKET_NATIVE_TO_RESULT_CASE(ACCES);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(AFNOSUPPORT);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(WOULDBLOCK);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(BADF);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNRESET);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(DESTADDRREQ);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(FAULT);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(HOSTUNREACH);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(INTR);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(INVAL);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(ISCONN);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(MFILE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(MSGSIZE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NETDOWN);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NETUNREACH);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NFILE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOBUFS);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NOENT);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NOMEM);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTCONN);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTDIR);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTSOCK);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(OPNOTSUPP);
#ifndef _WIN32
            // NOTE: EPIPE is not availble on winsock
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PIPE);
#endif
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PROTONOSUPPORT);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PROTOTYPE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(TIMEDOUT);

            DM_SOCKET_NATIVE_TO_RESULT_CASE(ADDRNOTAVAIL);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNREFUSED);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(ADDRINUSE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNABORTED);
        }

        // TODO: Add log-domain support
        dmLogError("SOCKET: Unknown result code %d\n", r);
        return RESULT_UNKNOWN;
    }
    #undef DM_SOCKET_NATIVE_TO_RESULT_CASE

    // Use this function for BSD socket compatibility
    // However, don't use it blindly as we return code ETIMEDOUT is "lost".
    // The background is that ETIMEDOUT is returned rather than EWOULDBLOCK on windows
    // on socket timeout.
    static Result NativeToResultCompat(int r)
    {
        Result res = NativeToResult(r);
        if (res == RESULT_TIMEDOUT) {
            res = RESULT_WOULDBLOCK;
        }
        return res;
    }

    #define DM_SOCKET_HNATIVE_TO_RESULT_CASE(x) case x: return RESULT_##x
    static Result HNativeToResult(int r)
    {
        switch (r)
        {
            DM_SOCKET_HNATIVE_TO_RESULT_CASE(HOST_NOT_FOUND);
            DM_SOCKET_HNATIVE_TO_RESULT_CASE(TRY_AGAIN);
            DM_SOCKET_HNATIVE_TO_RESULT_CASE(NO_RECOVERY);
            DM_SOCKET_HNATIVE_TO_RESULT_CASE(NO_DATA);
        }

        // TODO: Add log-domain support
        dmLogError("SOCKET: Unknown result code %d\n", r);
        return RESULT_UNKNOWN;
    }
    #undef DM_SOCKET_HNATIVE_TO_RESULT_CASE

}

#endif // #ifndef DM_SOCKET_PRIVATE_H

