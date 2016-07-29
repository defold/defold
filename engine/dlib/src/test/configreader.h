#ifndef CONFIGREADER_H
#define CONFIGREADER_H

namespace dmTestUtil
{

// Returns 0 if successful
int GetSocketsFromFile(const char* path, int* socket, int* ssl_socket, int* test_sslsocket);

}

#endif //CONFIGREADER_H
