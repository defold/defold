#ifndef DM_GAMESYS_COMP_MESH_H
#define DM_GAMESYS_COMP_MESH_H

#include <stdint.h>
#include <dlib/object_pool.h>
#include <gameobject/gameobject.h>

#include "../resources/res_mesh.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmMeshDDF;

    struct MeshComponent
    {
        dmGameObject::HInstance        m_Instance;
        dmTransform::Transform         m_Transform;
        Matrix4                        m_World;
        MeshResource*                  m_Resource;

        // dmRig::HRigInstance         m_RigInstance;
        // uint32_t                    m_MixedHash;
        // dmMessage::URL              m_Listener;
        dmArray<dmRender::Constant>    m_RenderConstants;
        dmArray<Vector4>               m_PrevRenderConstants;
        uint16_t                       m_ComponentIndex;
        /// Component enablement
        uint8_t                        m_Enabled : 1;
        // uint8_t                     m_DoRender : 1;
        /// Added to update or not
        uint8_t                        m_AddedToUpdate : 1;

        // Rendering stuff
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
        dmGraphics::HVertexBuffer      m_VertexBuffer;
        // dmArray<dmRig::RigModelVertex> m_VertexBufferData;
    };

    struct MeshWorld
    {
        dmResource::HFactory            m_ResourceFactory;
        dmObjectPool<MeshComponent*>    m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
    };

    dmGameObject::CreateResult CompMeshNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompMeshDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompMeshCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompMeshDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompMeshAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompMeshUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompMeshRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompMeshOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompMeshOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompMeshGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompMeshSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

}

#endif // DM_GAMESYS_COMP_MESH_H
