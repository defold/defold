#ifndef DM_GAMESYS_RES_FRAGMENT_PROGRAM_H
#define DM_GAMESYS_RES_FRAGMENT_PROGRAM_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResFragmentProgramCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResFragmentProgramDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResFragmentProgramRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResFragmentProgramGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_FRAGMENT_PROGRAM_H
