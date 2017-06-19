#ifndef GAMEROOM_PRIVATE_H
#define GAMEROOM_PRIVATE_H

#include <extension/extension.h>

#include <FBG_Platform.h>

namespace dmFBGameroom
{
    bool CheckGameroomInit();
    fbgMessageHandle PopFacebookMessage();
    fbgMessageHandle PopIAPMessage();
}

#endif // GAMEROOM_PRIVATE_H
