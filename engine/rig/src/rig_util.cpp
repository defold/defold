#include "rig.h"

namespace dmRig
{

    HRigInstance InstanceCreate(HRigContext context, dmRigDDF::Skeleton* skeleton, const dmRigDDF::MeshSet* mesh_set, const dmRigDDF::AnimationSet* animation_set, dmhash_t mesh_id, dmhash_t default_animation)
    {
        HRigInstance rig_instance = 0x0;
        if (mesh_id == 0x0)
        {
            dmLogError("Could not create rig instance with an invalid mesh id.");
            return 0x0;
        }

        dmArray<dmRig::RigBone>* bind_pose = new dmArray<dmRig::RigBone>();
        dmArray<uint32_t>* pose_idx_to_influence = new dmArray<uint32_t>();
        dmArray<uint32_t>* track_idx_to_pose = new dmArray<uint32_t>();

        dmRig::CreateBindPose(*skeleton, *bind_pose);
        dmRig::FillBoneListArrays(*mesh_set, *animation_set, *skeleton, *pose_idx_to_influence, *track_idx_to_pose);

        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = context;
        create_params.m_Instance = &rig_instance;

        create_params.m_BindPose           = bind_pose;
        create_params.m_Skeleton           = skeleton;
        create_params.m_MeshSet            = mesh_set;
        create_params.m_AnimationSet       = animation_set;
        create_params.m_PoseIdxToInfluence = pose_idx_to_influence;
        create_params.m_TrackIdxToPose     = track_idx_to_pose;
        create_params.m_MeshId             = mesh_id;
        create_params.m_DefaultAnimation   = default_animation;

        dmRig::Result res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by spine model: %d.", res);
            delete bind_pose;
            delete pose_idx_to_influence;
            delete track_idx_to_pose;
            return 0x0;
        }

        return rig_instance;
    }

    bool InstanceDestroy(HRigContext context, HRigInstance instance)
    {
        assert(context);
        assert(instance);

        delete instance->m_PoseIdxToInfluence;
        delete instance->m_TrackIdxToPose;
        delete instance->m_BindPose;

        InstanceDestroyParams destroy_params;
        destroy_params.m_Context = context;
        destroy_params.m_Instance = instance;
        Result r = InstanceDestroy(destroy_params);
        if (r != RESULT_OK)
        {
            return false;
        }

        return true;
    }

}
