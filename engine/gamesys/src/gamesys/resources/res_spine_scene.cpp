#include "res_spine_scene.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmResource::Result AcquireResources(dmResource::HFactory factory, SpineSceneResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_SpineScene->m_TextureSet, (void**) &resource->m_TextureSet);
        if (result == dmResource::RESULT_OK)
        {
            dmGameSystemDDF::Skeleton* skeleton = &resource->m_SpineScene->m_Skeleton;
            uint32_t bone_count = skeleton->m_Bones.m_Count;
            resource->m_BindPose.SetCapacity(bone_count);
            resource->m_BindPose.SetSize(bone_count);
            for (uint32_t i = 0; i < bone_count; ++i)
            {
                SpineBone* bind_bone = &resource->m_BindPose[i];
                dmGameSystemDDF::Bone* bone = &skeleton->m_Bones[i];
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

    static void ReleaseResources(dmResource::HFactory factory, SpineSceneResource* resource)
    {
        if (resource->m_SpineScene != 0x0)
            dmDDF::FreeMessage(resource->m_SpineScene);
        if (resource->m_TextureSet != 0x0)
            dmResource::Release(factory, resource->m_TextureSet);
    }

    dmResource::Result ResSpineScenePreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::SpineScene* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGameSystemDDF_SpineScene_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, spine_scene->m_TextureSet);

        *params.m_PreloadData = spine_scene;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpineSceneCreate(const dmResource::ResourceCreateParams& params)
    {
        SpineSceneResource* ss_resource = new SpineSceneResource();
        ss_resource->m_SpineScene = (dmGameSystemDDF::SpineScene*) params.m_PreloadData;
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

    dmResource::Result ResSpineSceneDestroy(const dmResource::ResourceDestroyParams& params)
    {
        SpineSceneResource* ss_resource = (SpineSceneResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpineSceneRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGameSystemDDF::SpineScene* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGameSystemDDF_SpineScene_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        SpineSceneResource* ss_resource = (SpineSceneResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        ss_resource->m_SpineScene = spine_scene;
        return AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
    }
}
