#ifndef DM_GAMESYS_RES_LABEL_H
#define DM_GAMESYS_RES_LABEL_H

#include <stdint.h>

#include <dlib/hash.h>
#include <resource/resource.h>

#include "label_ddf.h"

#include <render/render.h>

namespace dmGameSystem
{
    struct LabelResource
    {
        dmGameSystemDDF::LabelDesc* m_DDF;
        dmRender::HMaterial m_Material;
        dmRender::HFontMap m_FontMap;
    };

    dmResource::Result ResLabelPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResLabelCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResLabelDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResLabelRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResLabelGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_LABEL_H
