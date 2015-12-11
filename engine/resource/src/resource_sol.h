#ifndef RESOURCE_SOL_H
#define RESOURCE_SOL_H

#include "resource.h"

namespace dmResource
{
    Result SolResourcePreload(const ResourcePreloadParams& params);
    Result SolResourceCreate(const ResourceCreateParams& params);
    Result SolResourceDestroy(const ResourceDestroyParams& params);
    Result SolResourceRecreate(const ResourceRecreateParams& params);
}

#endif
