#ifndef DM_GAMESYS_SCRIPT_RESOURCE_H
#define DM_GAMESYS_SCRIPT_RESOURCE_H

#include <stdint.h>

namespace dmBuffer
{
    struct Buffer;
}

namespace dmGameSystem
{
    void ScriptResourceRegister(const struct ScriptLibContext& context);
    void ScriptResourceFinalize(const struct ScriptLibContext& context);
}

#endif // DM_GAMESYS_SCRIPT_RESOURCE_H
