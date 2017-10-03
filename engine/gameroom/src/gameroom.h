#ifndef GAMEROOM_PRIVATE_H
#define GAMEROOM_PRIVATE_H

#include <FBG_Platform.h>
#include <dlib/array.h>

namespace dmFBGameroom
{
    bool CheckGameroomInit();
    dmArray<fbgMessageHandle>* GetFacebookMessages();
    dmArray<fbgMessageHandle>* GetIAPMessages();
}

#endif // GAMEROOM_PRIVATE_H
