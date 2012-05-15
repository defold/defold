#include <dlib/socket.h>
#include "engine.h"

#include <dlib/memprofile.h>

int main(int argc, char *argv[])
{
    dmDDF::RegisterAllTypes();
    dmMemProfile::Initialize();
    dmSocket::Initialize();

    int exit_code = dmEngine::Launch(argc, argv, 0, 0, 0);

    dmSocket::Finalize();
    dmMemProfile::Finalize();
    return exit_code;
}
