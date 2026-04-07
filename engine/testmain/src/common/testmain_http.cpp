#include <stdlib.h>
#include <string.h>
#include <string>

#define CPPHTTPLIB_NO_EXCEPTIONS
#include "../cpp-httplib/httplib.h"

namespace
{

static bool ParseHttpUrl(const char* url, std::string* host, int* port, std::string* path)
{
    if (url == 0 || host == 0 || port == 0 || path == 0)
    {
        return false;
    }

    const char* p = url;
    if (strncmp(p, "http://", 7) != 0)
    {
        return false;
    }
    p += 7;

    const char* host_begin = p;
    while (*p != '\0' && *p != ':' && *p != '/')
    {
        ++p;
    }

    if (p == host_begin)
    {
        return false;
    }

    host->assign(host_begin, p - host_begin);
    *port = 80;

    if (*p == ':')
    {
        ++p;
        char* end = 0;
        long parsed_port = strtol(p, &end, 10);
        if (end == p || parsed_port <= 0 || parsed_port > 65535)
        {
            return false;
        }
        *port = (int)parsed_port;
        p = end;
    }

    if (*p == '\0')
    {
        path->assign("/");
    }
    else if (*p == '/')
    {
        path->assign(p);
    }
    else
    {
        return false;
    }

    return true;
}

} // namespace

int TestHttpGet(const char* url, uint32_t* length, const char** content)
{
#if defined(_WIN32)
    return -1000;
#else
    if (length == 0)
    {
        return -1;
    }

    *length = 0;
    if (content != 0)
    {
        *content = 0;
    }

    std::string host;
    std::string path;
    int port = 0;
    if (!ParseHttpUrl(url, &host, &port, &path))
    {
        return -1;
    }

    httplib::Client client(host, port);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);

    httplib::Result result = client.Get(path);
    if (!result || result.error() != httplib::Error::Success)
    {
        return -1;
    }

    *length = (uint32_t)result->body.size();

    if (content != 0)
    {
        char* bytes = (char*)malloc(result->body.size() + 1);
        if (bytes == 0)
        {
            return -1;
        }

        if (!result->body.empty())
        {
            memcpy(bytes, result->body.data(), result->body.size());
        }
        bytes[result->body.size()] = '\0';
        *content = bytes;
    }

    return result->status;
#endif
}
