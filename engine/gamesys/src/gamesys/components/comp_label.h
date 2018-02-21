#ifndef DM_GAMESYS_COMP_LABEL_H
#define DM_GAMESYS_COMP_LABEL_H

#include <stdint.h>
#include <gameobject/gameobject.h>

namespace dmRender
{
    struct TextMetrics;
}

namespace dmGameSystem
{
    struct LabelComponent;

    dmGameObject::CreateResult CompLabelNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompLabelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompLabelCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompLabelDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompLabelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    void*                       CompLabelGetComponent(const dmGameObject::ComponentGetParams& params);

    dmGameObject::UpdateResult CompLabelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompLabelRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompLabelOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompLabelOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompLabelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompLabelSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    void CompLabelGetTextMetrics(const LabelComponent* component, struct dmRender::TextMetrics& metrics);


}

#endif // DM_GAMESYS_COMP_LABEL_H
