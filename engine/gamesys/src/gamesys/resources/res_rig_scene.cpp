#include "res_rig_scene.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmResource::Result AcquireResources(dmResource::HFactory factory, RigSceneResource* resource, const char* filename)
    {
        dmResource::Result result;
        if((resource->m_RigScene->m_TextureSet != 0x0) && (*resource->m_RigScene->m_TextureSet != 0x0))
        {
            result = dmResource::Get(factory, resource->m_RigScene->m_TextureSet, (void**) &resource->m_TextureSet);
            if (result != dmResource::RESULT_OK)
                return result;
        }
        result = dmResource::Get(factory, resource->m_RigScene->m_Skeleton, (void**) &resource->m_SkeletonRes);
        if (result != dmResource::RESULT_OK)
            return result;
        result = dmResource::Get(factory, resource->m_RigScene->m_AnimationSet, (void**) &resource->m_AnimationSetRes);
        if (result != dmResource::RESULT_OK)
            return result;
        result = dmResource::Get(factory, resource->m_RigScene->m_MeshSet, (void**) &resource->m_MeshSetRes);
        if (result != dmResource::RESULT_OK)
            return result;

        if (result == dmResource::RESULT_OK)
        {
            uint32_t bone_count = resource->m_SkeletonRes->m_Skeleton->m_Bones.m_Count;
            resource->m_BindPose.SetCapacity(bone_count);
            resource->m_BindPose.SetSize(bone_count);
            for (uint32_t i = 0; i < bone_count; ++i)
            {
                dmRig::RigBone* bind_bone = &resource->m_BindPose[i];
                dmRigDDF::Bone* bone = &resource->m_SkeletonRes->m_Skeleton->m_Bones[i];
                bind_bone->m_LocalToParent = dmTransform::Transform(Vector3(bone->m_Position), bone->m_Rotation, bone->m_Scale);
                if (i > 0)
                {
                    bind_bone->m_LocalToModel = dmTransform::Mul(resource->m_BindPose[bone->m_Parent].m_LocalToModel, bind_bone->m_LocalToParent);
                    if (!bone->m_InheritScale)
                    {
                        bind_bone->m_LocalToModel.SetScale(bind_bone->m_LocalToParent.GetScale());
                    }
                }
                else
                {
                    bind_bone->m_LocalToModel = bind_bone->m_LocalToParent;
                }
                bind_bone->m_ModelToLocal = dmTransform::Inv(bind_bone->m_LocalToModel);
                bind_bone->m_ParentIndex = bone->m_Parent;
                bind_bone->m_Length = bone->m_Length;
            }
        }
        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, RigSceneResource* resource)
    {
        if (resource->m_RigScene != 0x0)
            dmDDF::FreeMessage(resource->m_RigScene);
        if (resource->m_TextureSet != 0x0)
            dmResource::Release(factory, resource->m_TextureSet);
        if (resource->m_SkeletonRes != 0x0)
            dmResource::Release(factory, resource->m_SkeletonRes);
        if (resource->m_AnimationSetRes != 0x0)
            dmResource::Release(factory, resource->m_AnimationSetRes);
        if (resource->m_MeshSetRes != 0x0)
            dmResource::Release(factory, resource->m_MeshSetRes);

    }

    dmResource::Result ResRigScenePreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRigDDF::RigScene* rig_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_RigScene_DESCRIPTOR, (void**) &rig_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, rig_scene->m_TextureSet);
        dmResource::PreloadHint(params.m_HintInfo, rig_scene->m_Skeleton);
        dmResource::PreloadHint(params.m_HintInfo, rig_scene->m_AnimationSet);
        dmResource::PreloadHint(params.m_HintInfo, rig_scene->m_MeshSet);

        *params.m_PreloadData = rig_scene;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRigSceneCreate(const dmResource::ResourceCreateParams& params)
    {
        RigSceneResource* ss_resource = new RigSceneResource();
        ss_resource->m_RigScene = (dmRigDDF::RigScene*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) ss_resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, ss_resource);
            delete ss_resource;
        }
        return r;
    }

    dmResource::Result ResRigSceneDestroy(const dmResource::ResourceDestroyParams& params)
    {
        RigSceneResource* ss_resource = (RigSceneResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRigSceneRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRigDDF::RigScene* rig_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_RigScene_DESCRIPTOR, (void**) &rig_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        RigSceneResource* ss_resource = (RigSceneResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        ss_resource->m_RigScene = rig_scene;
        return AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
    }
}
