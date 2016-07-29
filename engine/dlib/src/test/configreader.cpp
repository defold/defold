#include "configreader.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlib/log.h>

namespace dmTestUtil
{

int GetSocketsFromFile(const char* path, int* socket, int* ssl_socket, int* test_sslsocket)
{
    FILE* f = fopen(path, "rb");

    if(!f)
    {
        dmLogError("Could not read config file %s", path);
        return 1;
    }

    char* line = 0;
    size_t len = 0;
    ssize_t read = 0;

    while( (read = getline(&line, &len, f)) != -1)
    {
        if( line[0] == '#' )
            continue; // skip all comments and empty lines
        if( line[0] == '[' )
            continue; // for now, skip all section checking

        char key[256];
        char value[2048];
        int num_items = sscanf(line, "%s = %s", key, value);
        if( num_items != 2 )
        {
            continue;
        }

        if( strcmp("socket", key) == 0 && socket != 0)
        {
            *socket = atoi(value);
        }
        else if( strcmp("sslsocket", key) == 0 && ssl_socket != 0)
        {
            *ssl_socket = atoi(value);
        }
        else if( strcmp("testsslssocket", key) == 0 && test_sslsocket != 0)
        {
            *test_sslsocket = atoi(value);
        }
    }

    if( line )
    {
        free(line);
    }
    fclose(f);

    return 0;
}

} // namespace
