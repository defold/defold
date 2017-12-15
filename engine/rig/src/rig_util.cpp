#include "rig.h"

namespace dmRig
{

    static bool LoadMessages(const void* buffer_skeleton, uint32_t buffer_skeleton_size, dmRigDDF::Skeleton** skeleton_ddf,
                             const void* buffer_meshset, uint32_t buffer_meshset_size, dmRigDDF::MeshSet** meshset_ddf,
                             const void* buffer_animationset, uint32_t buffer_animationset_size, dmRigDDF::AnimationSet** animationset_ddf)
    {
        dmDDF::Result r = dmDDF::LoadMessage<dmRigDDF::Skeleton>(buffer_skeleton, buffer_skeleton_size, skeleton_ddf);
        if (r != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to load skeleton ddf data.");
            return false;
        }

        r = dmDDF::LoadMessage<dmRigDDF::MeshSet>(buffer_meshset, buffer_meshset_size, meshset_ddf);
        if (r != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to load meshset ddf data.");
            return false;
        }

        r = dmDDF::LoadMessage<dmRigDDF::AnimationSet>(buffer_animationset, buffer_animationset_size, animationset_ddf);
        if (r != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to load animationset ddf data.");
            return false;
        }

        return true;
    }

    HRigInstance InstanceCreate(HRigContext context,
                                const void* buffer_skeleton, uint32_t buffer_skeleton_size,
                                const void* buffer_meshset, uint32_t buffer_meshset_size,
                                const void* buffer_animationset, uint32_t buffer_animationset_size,
                                dmhash_t mesh_id, dmhash_t default_animation)
    {
        HRigInstance rig_instance = 0x0;
        if (mesh_id == 0x0)
        {
            dmLogError("Could not create rig instance with an invalid mesh id.");
            return 0x0;
        }

        dmRigDDF::Skeleton* skeleton = 0x0;
        dmRigDDF::MeshSet* meshset = 0x0;
        dmRigDDF::AnimationSet* animationset = 0x0;

        bool r = LoadMessages(buffer_skeleton, buffer_skeleton_size, &skeleton, buffer_meshset, buffer_meshset_size, &meshset, buffer_animationset, buffer_animationset_size, &animationset);
        if (!r)
        {
            dmLogError("Could not create rig instance, loading ddf messages failed.");
            return 0x0;
        }

        dmArray<dmRig::RigBone>* bind_pose = new dmArray<dmRig::RigBone>();
        dmArray<uint32_t>* pose_idx_to_influence = new dmArray<uint32_t>();
        dmArray<uint32_t>* track_idx_to_pose = new dmArray<uint32_t>();

        dmRig::CreateBindPose(*skeleton, *bind_pose);
        dmRig::FillBoneListArrays(*meshset, *animationset, *skeleton, *pose_idx_to_influence, *track_idx_to_pose);

        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = context;
        create_params.m_Instance = &rig_instance;

        create_params.m_BindPose           = bind_pose;
        create_params.m_Skeleton           = skeleton;
        create_params.m_MeshSet            = meshset;
        create_params.m_AnimationSet       = animationset;
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

        dmDDF::FreeMessage((void*)instance->m_Skeleton);
        dmDDF::FreeMessage((void*)instance->m_MeshSet);
        dmDDF::FreeMessage((void*)instance->m_AnimationSet);

        return true;
    }

    float* GetPoseMatrices(HRigInstance instance)
    {
        assert(instance);

        const uint32_t bone_count = instance->m_Skeleton->m_Bones.m_Count;
        const uint32_t pose_matrices_size = bone_count * 4 * 4;
        float* pose_matrices = new float[pose_matrices_size];

        for (int i = 0; i < bone_count; ++i)
        {
            dmTransform::Transform bone_trans = instance->m_Pose[i];
            Vectormath::Aos::Matrix4 bone_matrix = dmTransform::ToMatrix4(bone_trans);
            pose_matrices[i*4*4+ 0] = bone_matrix.getCol0().getX();
            pose_matrices[i*4*4+ 1] = bone_matrix.getCol0().getY();
            pose_matrices[i*4*4+ 2] = bone_matrix.getCol0().getZ();
            pose_matrices[i*4*4+ 3] = bone_matrix.getCol0().getW();

            pose_matrices[i*4*4+ 4] = bone_matrix.getCol1().getX();
            pose_matrices[i*4*4+ 5] = bone_matrix.getCol1().getY();
            pose_matrices[i*4*4+ 6] = bone_matrix.getCol1().getZ();
            pose_matrices[i*4*4+ 7] = bone_matrix.getCol1().getW();

            pose_matrices[i*4*4+ 8] = bone_matrix.getCol2().getX();
            pose_matrices[i*4*4+ 9] = bone_matrix.getCol2().getY();
            pose_matrices[i*4*4+10] = bone_matrix.getCol2().getZ();
            pose_matrices[i*4*4+11] = bone_matrix.getCol2().getW();

            pose_matrices[i*4*4+12] = bone_matrix.getCol3().getX();
            pose_matrices[i*4*4+13] = bone_matrix.getCol3().getY();
            pose_matrices[i*4*4+14] = bone_matrix.getCol3().getZ();
            pose_matrices[i*4*4+15] = bone_matrix.getCol3().getW();
        }

        return pose_matrices;
    }


    void ReleasePoseMatrices(float* matrices)
    {
        assert(matrices);

        delete [] matrices;
    }

}
