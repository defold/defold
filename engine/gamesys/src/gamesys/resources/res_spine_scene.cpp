#include "res_spine_scene.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, SpineSceneResource* resource, const char* filename)
    {
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameSystemDDF_SpineScene_DESCRIPTOR, (void**) &resource->m_SpineScene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
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

    dmResource::Result ResSpineSceneCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        SpineSceneResource* ss_resource = new SpineSceneResource();
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, ss_resource, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) ss_resource;
        }
        else
        {
            ReleaseResources(factory, ss_resource);
            delete ss_resource;
        }
        return r;
    }

    dmResource::Result ResSpineSceneDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        SpineSceneResource* ss_resource = (SpineSceneResource*)resource->m_Resource;
        ReleaseResources(factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpineSceneRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        SpineSceneResource* ss_resource = (SpineSceneResource*)resource->m_Resource;
        ReleaseResources(factory, ss_resource);
        return AcquireResources(factory, buffer, buffer_size, ss_resource, filename);
    }
}
