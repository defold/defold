#include "testutil.h"

namespace dmTestUtil
{

void GetSocketsFromConfig(dmConfigFile::HConfig config, int* socket, int* socket_ssl, int* socket_ssl_test)
{
    if( socket != 0 )
    {
        *socket = dmConfigFile::GetInt(config, "server.socket", 7000);
    }
    if( socket_ssl != 0 )
    {
        *socket_ssl = dmConfigFile::GetInt(config, "server.socket_ssl", 7001);
    }
    if( socket_ssl_test != 0 )
    {
        *socket_ssl_test = dmConfigFile::GetInt(config, "server.socket_ssl_test", 7002);
    }
}


} // namespace
