#ifndef DM_GAMESYS_COMP_SPINE_MODEL_H
#define DM_GAMESYS_COMP_SPINE_MODEL_H

#include <stdint.h>
#include <dlib/object_pool.h>
#include <gameobject/gameobject.h>
#include <rig/rig.h>

#include "../resources/res_spine_model.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmGameSystemDDF;

    // struct IKAnimation {
    //     float m_Mix;
    //     bool m_Positive;
    // };

    // struct IKTarget {
    //     float m_Mix;
    //     dmhash_t m_InstanceId;
    //     Vector3 m_Position;
    // };

    struct SpineModelComponent
    {
        // SpinePlayer                 m_Players[2];
        dmGameObject::HInstance     m_Instance;
        dmTransform::Transform      m_Transform;
        Matrix4                     m_World;
        SpineModelResource*         m_Resource;
        dmRig::HRigInstance         m_RigInstance;
        uint32_t                    m_MixedHash;
        // dmMessage::URL              m_Listener;
        dmArray<dmRender::Constant> m_RenderConstants;
        dmArray<Vector4>            m_PrevRenderConstants;
        /// Node instances corresponding to the bones
        // dmArray<dmGameObject::HInstance> m_NodeInstances;
        // User IK constraint targets
        // dmArray<IKTarget>           m_IKTargets;
        uint8_t                     m_ComponentIndex;
        /// Component enablement
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_DoRender : 1;
        /// Added to update or not
        uint8_t                     m_AddedToUpdate : 1;
    };

    struct SpineModelVertex
    {
        float x;
        float y;
        float z;
        uint16_t u;
        uint16_t v;
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    struct SpineModelWorld
    {
        dmRig::HRigContext                  m_RigContext;
        dmObjectPool<SpineModelComponent*>  m_Components;
        dmArray<dmRender::RenderObject>     m_RenderObjects;
        dmGraphics::HVertexDeclaration      m_VertexDeclaration;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmArray<SpineModelVertex>           m_VertexBufferData;
        // Temporary scratch array for instances, only used during the creation phase of components
        // dmArray<dmGameObject::HInstance>    m_ScratchInstances;
    };

    dmGameObject::CreateResult CompSpineModelNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompSpineModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompSpineModelCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompSpineModelDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompSpineModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompSpineModelUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompSpineModelRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompSpineModelOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompSpineModelOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompSpineModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompSpineModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params);
}

#endif // DM_GAMESYS_COMP_SPINE_MODEL_H
