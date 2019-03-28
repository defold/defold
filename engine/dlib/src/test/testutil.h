#pragma once

#include <dlib/configfile.h>
#include <dlib/windefines.h>
#include <gtest/gtest.h>

namespace dmTestUtil
{

void GetSocketsFromConfig(dmConfigFile::HConfig config, int* socket, int* ssl_socket, int* test_sslsocket);

}
