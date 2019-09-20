#ifndef DM_GAMESYS_SCRIPT_SOUND_H
#define DM_GAMESYS_SCRIPT_SOUND_H

#include <dlib/configfile.h>

namespace dmGameSystem
{
    void ScriptSoundRegister(const ScriptLibContext& context);
    void ScriptSoundOnWindowFocus(bool focus);
}

#endif // DM_GAMESYS_SCRIPT_SOUND_H
