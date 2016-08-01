#include "res_skeleton.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmResource::Result AcquireResources(dmResource::HFactory factory, SkeletonResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::RESULT_OK;
        // uint32_t bone_count = resource->m_Skeleton->m_Bones.m_Count;
        // resource->m_BindPose.SetCapacity(bone_count);
        // resource->m_BindPose.SetSize(bone_count);
        // for (uint32_t i = 0; i < bone_count; ++i)
        // {
        //     SpineBone* bind_bone = &resource->m_BindPose[i];
        //     dmGameSystemDDF::Bone* bone = &resource->m_Skeleton->m_Bones[i];
        //     bind_bone->m_LocalToParent = dmTransform::Transform(Vector3(bone->m_Position), bone->m_Rotation, bone->m_Scale);
        //     if (i > 0)
        //     {
        //         bind_bone->m_LocalToModel = dmTransform::Mul(resource->m_BindPose[bone->m_Parent].m_LocalToModel, bind_bone->m_LocalToParent);
        //         if (!bone->m_InheritScale)
        //         {
        //             bind_bone->m_LocalToModel.SetScale(bind_bone->m_LocalToParent.GetScale());
        //         }
        //     }
        //     else
        //     {
        //         bind_bone->m_LocalToModel = bind_bone->m_LocalToParent;
        //     }
        //     bind_bone->m_ModelToLocal = dmTransform::Inv(bind_bone->m_LocalToModel);
        //     bind_bone->m_ParentIndex = bone->m_Parent;
        //     bind_bone->m_Length = bone->m_Length;
        // }

        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, SkeletonResource* resource)
    {
        if (resource->m_Skeleton != 0x0)
            dmDDF::FreeMessage(resource->m_Skeleton);
    }

    dmResource::Result ResSkeletonPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::Skeleton* skeleton;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGameSystemDDF_Skeleton_DESCRIPTOR, (void**) &skeleton);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params.m_PreloadData = skeleton;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSkeletonCreate(const dmResource::ResourceCreateParams& params)
    {
        SkeletonResource* ss_resource = new SkeletonResource();
        ss_resource->m_Skeleton = (dmGameSystemDDF::Skeleton*) params.m_PreloadData;
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

    dmResource::Result ResSkeletonDestroy(const dmResource::ResourceDestroyParams& params)
    {
        SkeletonResource* ss_resource = (SkeletonResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSkeletonRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGameSystemDDF::Skeleton* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGameSystemDDF_Skeleton_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        SkeletonResource* ss_resource = (SkeletonResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        ss_resource->m_Skeleton = spine_scene;
        return AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
    }
}
