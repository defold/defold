#include <map>
#include <stdlib.h>
#include <gtest/gtest.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <particle/particle.h>
#include <script/script.h>
#include <script/lua_source_ddf.h>
#include "../gui.h"
#include "../gui_private.h"
#include "test_gui_ddf.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

extern unsigned char BUG352_LUA[];
extern uint32_t BUG352_LUA_SIZE;


/*
 * Basic
 *  - Create scene
 *  - Create nodes
 *  - Stress tests
 *
 * self table
 *
 * reload script
 *
 * lua script basics
 *  - New/Delete node
 *
 * "Namespaces"
 *
 * Animation
 *
 * Render
 *
 * Adjust reference
 *
 * Spine
 *
 */

#define MAX_NODES 64U
#define MAX_ANIMATIONS 32U
#define MAX_PARTICLEFXS 8U
#define MAX_PARTICLEFX 64U
#define MAX_PARTICLES 1024U

void GetURLCallback(dmGui::HScene scene, dmMessage::URL* url);

uintptr_t GetUserDataCallback(dmGui::HScene scene);

dmhash_t ResolvePathCallback(dmGui::HScene scene, const char* path, uint32_t path_size);

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics);

static const float EPSILON = 0.000001f;
static const float TEXT_GLYPH_WIDTH = 1.0f;
static const float TEXT_MAX_ASCENT = 0.75f;
static const float TEXT_MAX_DESCENT = 0.25f;

static void CreateDummyMeshEntry(dmRigDDF::MeshEntry& mesh_entry, dmhash_t id, Vector4 color, Vector4 skin_color = Vector4(1.0f))
{
    mesh_entry.m_Id = id;
    mesh_entry.m_Meshes.m_Data = new dmRigDDF::Mesh[1];
    mesh_entry.m_Meshes.m_Count = 1;

    uint32_t vert_count = 3;

    // set vertice position so they match bone positions
    dmRigDDF::Mesh& mesh = mesh_entry.m_Meshes.m_Data[0];
    mesh.m_Positions.m_Data = new float[vert_count*3];
    mesh.m_Positions.m_Count = vert_count*3;
    mesh.m_Positions.m_Data[0] = 0.0f;
    mesh.m_Positions.m_Data[1] = 0.0f;
    mesh.m_Positions.m_Data[2] = 0.0f;
    mesh.m_Positions.m_Data[3] = 1.0f;
    mesh.m_Positions.m_Data[4] = 0.0f;
    mesh.m_Positions.m_Data[5] = 0.0f;
    mesh.m_Positions.m_Data[6] = 2.0f;
    mesh.m_Positions.m_Data[7] = 0.0f;
    mesh.m_Positions.m_Data[8] = 0.0f;

    // data for each vertex (tex coords and normals not used)
    mesh.m_Texcoord0.m_Data       = new float[vert_count*2];
    mesh.m_Texcoord0.m_Count      = vert_count*2;
    mesh.m_Normals.m_Data         = new float[vert_count*3];
    mesh.m_Normals.m_Count        = vert_count*3;

    mesh.m_Color.m_Data           = new float[vert_count*4];
    mesh.m_Color.m_Count          = vert_count*4;
    mesh.m_Color[0]               = color.getX();
    mesh.m_Color[1]               = color.getY();
    mesh.m_Color[2]               = color.getZ();
    mesh.m_Color[3]               = color.getW();
    mesh.m_Color[4]               = color.getX();
    mesh.m_Color[5]               = color.getY();
    mesh.m_Color[6]               = color.getZ();
    mesh.m_Color[7]               = color.getW();
    mesh.m_Color[8]               = color.getX();
    mesh.m_Color[9]               = color.getY();
    mesh.m_Color[10]              = color.getZ();
    mesh.m_Color[11]              = color.getW();

    mesh.m_SkinColor.m_Data           = new float[vert_count*4];
    mesh.m_SkinColor.m_Count          = vert_count*4;
    mesh.m_SkinColor[0]               = skin_color.getX();
    mesh.m_SkinColor[1]               = skin_color.getY();
    mesh.m_SkinColor[2]               = skin_color.getZ();
    mesh.m_SkinColor[3]               = skin_color.getW();
    mesh.m_SkinColor[4]               = skin_color.getX();
    mesh.m_SkinColor[5]               = skin_color.getY();
    mesh.m_SkinColor[6]               = skin_color.getZ();
    mesh.m_SkinColor[7]               = skin_color.getW();
    mesh.m_SkinColor[8]               = skin_color.getX();
    mesh.m_SkinColor[9]               = skin_color.getY();
    mesh.m_SkinColor[10]              = skin_color.getZ();
    mesh.m_SkinColor[11]              = skin_color.getW();

    mesh.m_Indices.m_Data         = new uint32_t[vert_count];
    mesh.m_Indices.m_Count        = vert_count;
    mesh.m_Indices.m_Data[0]      = 0;
    mesh.m_Indices.m_Data[1]      = 1;
    mesh.m_Indices.m_Data[2]      = 2;
    mesh.m_BoneIndices.m_Data     = new uint32_t[vert_count*4];
    mesh.m_BoneIndices.m_Count    = vert_count*4;
    mesh.m_BoneIndices.m_Data[0]  = 0;
    mesh.m_BoneIndices.m_Data[1]  = 1;
    mesh.m_BoneIndices.m_Data[2]  = 0;
    mesh.m_BoneIndices.m_Data[3]  = 0;
    mesh.m_BoneIndices.m_Data[4]  = 0;
    mesh.m_BoneIndices.m_Data[5]  = 1;
    mesh.m_BoneIndices.m_Data[6]  = 0;
    mesh.m_BoneIndices.m_Data[7]  = 0;
    mesh.m_BoneIndices.m_Data[8]  = 0;
    mesh.m_BoneIndices.m_Data[9]  = 1;
    mesh.m_BoneIndices.m_Data[10] = 0;
    mesh.m_BoneIndices.m_Data[11] = 0;

    mesh.m_Weights.m_Data         = new float[vert_count*4];
    mesh.m_Weights.m_Count        = vert_count*4;
    mesh.m_Weights.m_Data[0]      = 1.0f;
    mesh.m_Weights.m_Data[1]      = 0.0f;
    mesh.m_Weights.m_Data[2]      = 0.0f;
    mesh.m_Weights.m_Data[3]      = 0.0f;
    mesh.m_Weights.m_Data[4]      = 0.0f;
    mesh.m_Weights.m_Data[5]      = 1.0f;
    mesh.m_Weights.m_Data[6]      = 0.0f;
    mesh.m_Weights.m_Data[7]      = 0.0f;
    mesh.m_Weights.m_Data[8]      = 0.0f;
    mesh.m_Weights.m_Data[9]      = 1.0f;
    mesh.m_Weights.m_Data[10]     = 0.0f;
    mesh.m_Weights.m_Data[11]     = 0.0f;

    mesh.m_Visible = true;
    mesh.m_DrawOrder = 0;
}

static dmLuaDDF::LuaSource* LuaSourceFromStr(const char *str, int length = -1)
{
    static dmLuaDDF::LuaSource src;
    memset(&src, 0x00, sizeof(dmLuaDDF::LuaSource));
    src.m_Script.m_Data = (uint8_t*) str;
    src.m_Script.m_Count = (length != -1) ? length : strlen(str);
    src.m_Filename = "dummy";
    return &src;
}

void OnWindowResizeCallback(const dmGui::HScene scene, uint32_t width, uint32_t height)
{
    dmGui::SetSceneResolution(scene, width, height);
    dmGui::SetDefaultResolution(scene->m_Context, width, height);
}

dmGui::FetchTextureSetAnimResult FetchTextureSetAnimCallback(void* texture_set_ptr, dmhash_t animation, dmGui::TextureSetAnimDesc* out_data)
{
    out_data->Init();
    static float uv_quad[] = {0,1,0,0, 1,0,1,1};
    out_data->m_TexCoords = &uv_quad[0];
    out_data->m_End = 1;
    out_data->m_FPS = 30;
    out_data->m_FlipHorizontal = 1;
    return dmGui::FETCH_ANIMATION_OK;
}

bool FetchRigSceneDataCallback(void* spine_scene, dmhash_t rig_scene_id, dmGui::RigSceneDataDesc* out_data)
{
    if (!spine_scene) {
        return false;
    }

    dmGui::RigSceneDataDesc* spine_scene_ptr = (dmGui::RigSceneDataDesc*)spine_scene;
    out_data->m_BindPose = spine_scene_ptr->m_BindPose;
    out_data->m_TrackIdxToPose = spine_scene_ptr->m_TrackIdxToPose;
    out_data->m_Skeleton = spine_scene_ptr->m_Skeleton;
    out_data->m_MeshSet = spine_scene_ptr->m_MeshSet;
    out_data->m_AnimationSet = spine_scene_ptr->m_AnimationSet;

    return true;
}

class dmGuiTest : public ::testing::Test
{
public:
    dmScript::HContext m_ScriptContext;
    dmGui::HContext m_Context;
    dmGui::HScene m_Scene;
    dmMessage::HSocket m_Socket;
    dmGui::HScript m_Script;
    std::map<std::string, dmGui::HNode> m_NodeTextToNode;
    std::map<std::string, Point3> m_NodeTextToRenderedPosition;
    std::map<std::string, Vector3> m_NodeTextToRenderedSize;

    dmRig::HRigContext      m_RigContext;
    dmRig::HRigInstance     m_RigInstance;
    dmArray<dmRig::RigBone> m_BindPose;
    dmArray<uint32_t>       m_TrackIdxToPose;
    dmRigDDF::Skeleton*     m_Skeleton;
    dmRigDDF::MeshSet*      m_MeshSet;
    dmRigDDF::AnimationSet* m_AnimationSet;

    virtual void SetUp()
    {
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);

        dmMessage::NewSocket("test_m_Socket", &m_Socket);
        dmGui::NewContextParams context_params;
        context_params.m_ScriptContext = m_ScriptContext;
        context_params.m_GetURLCallback = GetURLCallback;
        context_params.m_GetUserDataCallback = GetUserDataCallback;
        context_params.m_ResolvePathCallback = ResolvePathCallback;
        context_params.m_GetTextMetricsCallback = GetTextMetricsCallback;
        context_params.m_PhysicalWidth = 1;
        context_params.m_PhysicalHeight = 1;
        context_params.m_DefaultProjectWidth = 1;
        context_params.m_DefaultProjectHeight = 1;

        m_Context = dmGui::NewContext(&context_params);
        m_Context->m_SceneTraversalCache.m_Data.SetCapacity(MAX_NODES);
        m_Context->m_SceneTraversalCache.m_Data.SetSize(MAX_NODES);

        dmRig::NewContextParams rig_params = {0};
        rig_params.m_Context = &m_RigContext;
        rig_params.m_MaxRigInstanceCount = 2;
        dmRig::NewContext(rig_params);

        // Bogus font for the metric callback to be run (not actually using the default font)
        dmGui::SetDefaultFont(m_Context, (void*)0x1);
        dmGui::NewSceneParams params;
        params.m_MaxNodes = MAX_NODES;
        params.m_MaxAnimations = MAX_ANIMATIONS;
        params.m_UserData = this;
        params.m_RigContext = m_RigContext;

        params.m_MaxParticlefxs = MAX_PARTICLEFXS;
        params.m_MaxParticlefx = MAX_PARTICLEFX;
        params.m_ParticlefxContext = dmParticle::CreateContext(MAX_PARTICLEFX, MAX_PARTICLES);
        params.m_FetchTextureSetAnimCallback = FetchTextureSetAnimCallback;
        params.m_FetchRigSceneDataCallback = FetchRigSceneDataCallback;
        params.m_OnWindowResizeCallback = OnWindowResizeCallback;
        m_Scene = dmGui::NewScene(m_Context, &params);
        dmGui::SetSceneResolution(m_Scene, 1, 1);
        m_Script = dmGui::NewScript(m_Context);
        dmGui::SetSceneScript(m_Scene, m_Script);

        SetUpSimpleSpine();
    }

    static void RenderNodes(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
            const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
    {
        dmGuiTest* self = (dmGuiTest*) context;
        // The node is defined to completely cover the local space (0,1),(0,1)
        Vector4 origin(0.f, 0.f, 0.0f, 1.0f);
        Vector4 unit(1.0f, 1.0f, 0.0f, 1.0f);
        for (uint32_t i = 0; i < node_count; ++i)
        {
            Vector4 o = node_transforms[i] * origin;
            Vector4 u = node_transforms[i] * unit;
            const char* text = dmGui::GetNodeText(scene, nodes[i].m_Node);
            if (text) {
                self->m_NodeTextToRenderedPosition[text] = Point3(o.getXYZ());
                self->m_NodeTextToRenderedSize[text] = Vector3((u - o).getXYZ());
            }
        }
    }

    virtual void TearDown()
    {
        TearDownSimpleSpine();
        dmParticle::DestroyContext(m_Scene->m_ParticlefxContext);
        dmGui::DeleteScript(m_Script);
        dmGui::DeleteScene(m_Scene);
        dmRig::DeleteContext(m_RigContext);
        dmGui::DeleteContext(m_Context, m_ScriptContext);
        dmMessage::DeleteSocket(m_Socket);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
    }

private:
    // code from test_rig.cpp
    void SetUpSimpleSpine()
    {
        m_RigInstance = 0x0;
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = m_RigContext;
        create_params.m_Instance = &m_RigInstance;

        m_Skeleton     = new dmRigDDF::Skeleton();
        m_MeshSet      = new dmRigDDF::MeshSet();
        m_AnimationSet = new dmRigDDF::AnimationSet();

        /*

            Bones:
            A:
            (0)---->(1)---->
             |
         B:  |
             v
            (2)
             |
             |
             v
            (3)
             |
             |
             v

         A: 0: Pos; (0,0), rotation: 0
            1: Pos; (1,0), rotation: 0

         B: 0: Pos; (0,0), rotation: 0
            2: Pos; (0,1), rotation: 0
            3: Pos; (0,2), rotation: 0

        ------------------------------------

            Animation (id: "valid") for Bone A:

            I:
            (0)---->(1)---->

            II:
            (0)---->(1)
                     |
                     |
                     v

            III:
            (0)
             |
             |
             v
            (1)
             |
             |
             v


        ------------------------------------

            Animation (id: "ik_anim") for IK on Bone B.

        */
        uint32_t bone_count = 5;
        m_Skeleton->m_Bones.m_Data = new dmRigDDF::Bone[bone_count];
        m_Skeleton->m_Bones.m_Count = bone_count;
        // Bone 0
        dmRigDDF::Bone& bone0 = m_Skeleton->m_Bones.m_Data[0];
        bone0.m_Parent       = 0xffff;
        bone0.m_Id           = 0;
        bone0.m_Position     = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
        bone0.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone0.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone0.m_InheritScale = false;
        bone0.m_Length       = 0.0f;

        // Bone 1
        dmRigDDF::Bone& bone1 = m_Skeleton->m_Bones.m_Data[1];
        bone1.m_Parent       = 0;
        bone1.m_Id           = 1;
        bone1.m_Position     = Vectormath::Aos::Point3(1.0f, 0.0f, 0.0f);
        bone1.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone1.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone1.m_InheritScale = false;
        bone1.m_Length       = 1.0f;

        // Bone 2
        dmRigDDF::Bone& bone2 = m_Skeleton->m_Bones.m_Data[2];
        bone2.m_Parent       = 0;
        bone2.m_Id           = 2;
        bone2.m_Position     = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
        bone2.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone2.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone2.m_InheritScale = false;
        bone2.m_Length       = 1.0f;

        // Bone 3
        dmRigDDF::Bone& bone3 = m_Skeleton->m_Bones.m_Data[3];
        bone3.m_Parent       = 2;
        bone3.m_Id           = 3;
        bone3.m_Position     = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
        bone3.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone3.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone3.m_InheritScale = false;
        bone3.m_Length       = 1.0f;

        // Bone 4
        dmRigDDF::Bone& bone4 = m_Skeleton->m_Bones.m_Data[4];
        bone4.m_Parent       = 3;
        bone4.m_Id           = 4;
        bone4.m_Position     = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
        bone4.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone4.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone4.m_InheritScale = false;
        bone4.m_Length       = 1.0f;

        m_BindPose.SetCapacity(bone_count);
        m_BindPose.SetSize(bone_count);

        m_TrackIdxToPose.SetCapacity(bone_count);
        m_TrackIdxToPose.SetSize(bone_count);
        for (int i = 0; i < bone_count; ++i)
        {
            m_TrackIdxToPose[i] = i;
        }

        // IK
        m_Skeleton->m_Iks.m_Data = new dmRigDDF::IK[1];
        m_Skeleton->m_Iks.m_Count = 1;
        dmRigDDF::IK& ik_target = m_Skeleton->m_Iks.m_Data[0];
        ik_target.m_Id       = dmHashString64("test_ik");
        ik_target.m_Parent   = 3;
        ik_target.m_Child    = 2;
        ik_target.m_Target   = 4;
        ik_target.m_Positive = true;
        ik_target.m_Mix      = 1.0f;

        // Calculate bind pose
        dmRig::CreateBindPose(*m_Skeleton, m_BindPose);

        // Bone animations
        uint32_t animation_count = 2;
        m_AnimationSet->m_Animations.m_Data = new dmRigDDF::RigAnimation[animation_count];
        m_AnimationSet->m_Animations.m_Count = animation_count;
        dmRigDDF::RigAnimation& anim0 = m_AnimationSet->m_Animations.m_Data[0];
        dmRigDDF::RigAnimation& anim1 = m_AnimationSet->m_Animations.m_Data[1];
        anim0.m_Id = dmHashString64("valid");
        anim0.m_Duration            = 2.0f;
        anim0.m_SampleRate          = 1.0f;
        anim0.m_EventTracks.m_Count = 0;
        anim0.m_MeshTracks.m_Count  = 0;
        anim0.m_IkTracks.m_Count    = 0;
        anim1.m_Id = dmHashString64("ik_anim");
        anim1.m_Duration            = 3.0f;
        anim1.m_SampleRate          = 1.0f;
        anim1.m_Tracks.m_Count      = 0;
        anim1.m_EventTracks.m_Count = 0;
        anim1.m_MeshTracks.m_Count  = 0;

        uint32_t bone_track_count = 2;
        anim0.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[bone_track_count];
        anim0.m_Tracks.m_Count = bone_track_count;
        dmRigDDF::AnimationTrack& anim_track0 = anim0.m_Tracks.m_Data[0];
        dmRigDDF::AnimationTrack& anim_track1 = anim0.m_Tracks.m_Data[1];

        anim_track0.m_BoneIndex         = 0;
        anim_track0.m_Positions.m_Count = 0;
        anim_track0.m_Scale.m_Count     = 0;

        anim_track1.m_BoneIndex         = 1;
        anim_track1.m_Positions.m_Count = 0;
        anim_track1.m_Scale.m_Count     = 0;

        uint32_t samples = 4;
        anim_track0.m_Rotations.m_Data = new float[samples*4];
        anim_track0.m_Rotations.m_Count = samples*4;
        ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::identity();
        ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::identity();
        ((Quat*)anim_track0.m_Rotations.m_Data)[2] = Quat::rotationZ((float)M_PI / 2.0f);
        ((Quat*)anim_track0.m_Rotations.m_Data)[3] = Quat::rotationZ((float)M_PI / 2.0f);

        anim_track1.m_Rotations.m_Data = new float[samples*4];
        anim_track1.m_Rotations.m_Count = samples*4;
        ((Quat*)anim_track1.m_Rotations.m_Data)[0] = Quat::identity();
        ((Quat*)anim_track1.m_Rotations.m_Data)[1] = Quat::rotationZ((float)M_PI / 2.0f);
        ((Quat*)anim_track1.m_Rotations.m_Data)[2] = Quat::identity();
        ((Quat*)anim_track1.m_Rotations.m_Data)[3] = Quat::identity();

        // IK animation
        anim1.m_IkTracks.m_Data = new dmRigDDF::IKAnimationTrack[1];
        anim1.m_IkTracks.m_Count = 1;
        dmRigDDF::IKAnimationTrack& ik_track = anim1.m_IkTracks.m_Data[0];
        ik_track.m_IkIndex = 0;
        ik_track.m_Mix.m_Data = new float[samples];
        ik_track.m_Mix.m_Count = samples;
        ik_track.m_Mix.m_Data[0] = 1.0f;
        ik_track.m_Mix.m_Data[1] = 1.0f;
        ik_track.m_Mix.m_Data[2] = 1.0f;
        ik_track.m_Mix.m_Data[3] = 1.0f;
        ik_track.m_Positive.m_Data = new bool[samples];
        ik_track.m_Positive.m_Count = samples;
        ik_track.m_Positive.m_Data[0] = 1.0f;
        ik_track.m_Positive.m_Data[1] = 1.0f;
        ik_track.m_Positive.m_Data[2] = 1.0f;
        ik_track.m_Positive.m_Data[3] = 1.0f;

        // Meshes / skins
        m_MeshSet->m_MeshEntries.m_Data = new dmRigDDF::MeshEntry[2];
        m_MeshSet->m_MeshEntries.m_Count = 2;

        CreateDummyMeshEntry(m_MeshSet->m_MeshEntries.m_Data[0], dmHashString64("test"), Vector4(0.0f));
        CreateDummyMeshEntry(m_MeshSet->m_MeshEntries.m_Data[1], dmHashString64("secondary_skin"), Vector4(1.0f));

        // ------------------

        // Data
        create_params.m_BindPose     = &m_BindPose;
        create_params.m_TrackIdxToPose = &m_TrackIdxToPose;
        create_params.m_Skeleton     = m_Skeleton;
        create_params.m_MeshSet      = m_MeshSet;
        create_params.m_AnimationSet = m_AnimationSet;

        create_params.m_MeshId           = dmHashString64((const char*)"test");
        create_params.m_DefaultAnimation = dmHashString64((const char*)"");

        if (dmRig::RESULT_OK != dmRig::InstanceCreate(create_params))
        {
            dmLogError("Could not create rig instance!");
        }
    }

    void TearDownSimpleSpine() {

        dmRig::InstanceDestroyParams destroy_params = {0};
        destroy_params.m_Context = m_RigContext;
        destroy_params.m_Instance = m_RigInstance;
        if (dmRig::RESULT_OK != dmRig::InstanceDestroy(destroy_params)) {
            dmLogError("Could not delete rig instance!");
        }

        delete [] m_AnimationSet->m_Animations.m_Data[1].m_IkTracks.m_Data[0].m_Positive.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[1].m_IkTracks.m_Data[0].m_Mix.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[1].m_IkTracks.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data[1].m_Rotations.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data[0].m_Rotations.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data;
        delete [] m_Skeleton->m_Bones.m_Data;
        delete [] m_Skeleton->m_Iks.m_Data;
        for (int i = 0; i < 2; ++i)
        {
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Normals.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_BoneIndices.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Weights.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Indices.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Color.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_SkinColor.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Texcoord0.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Positions.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data;
        }
        delete [] m_MeshSet->m_MeshEntries.m_Data;

        delete m_Skeleton;
        delete m_MeshSet;
        delete m_AnimationSet;
    }
};

void GetURLCallback(dmGui::HScene scene, dmMessage::URL* url)
{
    dmGuiTest* test = (dmGuiTest*)dmGui::GetSceneUserData(scene);
    url->m_Socket = test->m_Socket;
}

uintptr_t GetUserDataCallback(dmGui::HScene scene)
{
    return (uintptr_t)dmGui::GetSceneUserData(scene);
}

dmhash_t ResolvePathCallback(dmGui::HScene scene, const char* path, uint32_t path_size)
{
    return dmHashBuffer64(path, path_size);
}

void GetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics)
{
    out_metrics->m_Width = strlen(text) * TEXT_GLYPH_WIDTH;
    out_metrics->m_Height = TEXT_MAX_ASCENT + TEXT_MAX_DESCENT;
    out_metrics->m_MaxAscent = TEXT_MAX_ASCENT;
    out_metrics->m_MaxDescent = TEXT_MAX_DESCENT;
}

static bool SetScript(dmGui::HScript script, const char* source)
{
    dmGui::Result r;
    r = dmGui::SetScript(script, LuaSourceFromStr(source));
    return dmGui::RESULT_OK == r;
}

TEST_F(dmGuiTest, Basic)
{
    for (uint32_t i = 0; i < MAX_NODES; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
        ASSERT_NE((dmGui::HNode) 0, node);
    }

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_EQ((dmGui::HNode) 0, node);
    ASSERT_EQ(m_Script, dmGui::GetSceneScript(m_Scene));
}

// Test that a newly re-created node has default values
TEST_F(dmGuiTest, RecreateNodes)
{
    uint32_t n = MAX_NODES + 1;
    for (uint32_t i = 0; i < n; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
        ASSERT_NE((dmGui::HNode) 0, node);
        ASSERT_EQ(dmGui::PIVOT_CENTER, dmGui::GetNodePivot(m_Scene, node));
        dmGui::SetNodePivot(m_Scene, node, dmGui::PIVOT_E);
        ASSERT_EQ(dmGui::PIVOT_E, dmGui::GetNodePivot(m_Scene, node));

        dmGui::DeleteNode(m_Scene, node, true);
    }
}

TEST_F(dmGuiTest, Name)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);

    dmGui::HNode get_node = dmGui::GetNodeById(m_Scene, "my_node");
    ASSERT_EQ((dmGui::HNode) 0, get_node);

    dmGui::SetNodeId(m_Scene, node, "my_node");
    get_node = dmGui::GetNodeById(m_Scene, "my_node");
    ASSERT_EQ(node, get_node);

    const char* s = "function init(self)\n"
                    "    local n = gui.get_node(\"my_node\")\n"
                    "    local id = gui.get_id(n)\n"
                    "    local n2 = gui.get_node(id)\n"
                    "    assert(n == n2)\n"
                    "end\n";

    ASSERT_TRUE(SetScript(m_Script, s));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
}

TEST_F(dmGuiTest, ContextAndSceneResolution)
{
    uint32_t width, height;
    dmGui::GetPhysicalResolution(m_Context, width, height);
    ASSERT_EQ(1, width);
    ASSERT_EQ(1, height);
    dmGui::GetSceneResolution(m_Scene, width, height);
    ASSERT_EQ(1, width);
    ASSERT_EQ(1, height);
    dmGui::SetSceneResolution(m_Scene, 2, 3);
    dmGui::GetSceneResolution(m_Scene, width, height);
    ASSERT_EQ(2, width);
    ASSERT_EQ(3, height);
    dmGui::SetPhysicalResolution(m_Context, 4, 5);
    dmGui::GetPhysicalResolution(m_Context, width, height);
    ASSERT_EQ(4, width);
    ASSERT_EQ(5, height);
    dmGui::GetSceneResolution(m_Scene, width, height);
    ASSERT_EQ(4, width);
    ASSERT_EQ(5, height);
}

void SetNodeCallback(const dmGui::HScene scene, dmGui::HNode node, const void *node_desc)
{
    dmGui::SetNodePosition(scene, node, *((Point3 *)node_desc));
}

TEST_F(dmGuiTest, Layouts)
{
    // layout creation and access
    dmGui::Result r;
    const char *l1_name = "layout1";
    const char *l2_name = "layout2";
    dmGui::AllocateLayouts(m_Scene, 2, 2);
    r = dmGui::AddLayout(m_Scene, l1_name);
    ASSERT_EQ(r, dmGui::RESULT_OK);
    r = dmGui::AddLayout(m_Scene, l2_name);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmhash_t ld_hash, l1_hash, l2_hash;
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetLayoutId(m_Scene, 0, ld_hash));
    ASSERT_EQ(dmGui::DEFAULT_LAYOUT, ld_hash);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetLayoutId(m_Scene, 1, l1_hash));
    ASSERT_EQ(dmHashString64(l1_name), l1_hash);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetLayoutId(m_Scene, 2, l2_hash));
    ASSERT_EQ(dmHashString64(l2_name), l2_hash);

    uint16_t ld_index = dmGui::GetLayoutIndex(m_Scene, dmGui::DEFAULT_LAYOUT);
    ASSERT_EQ(0, ld_index);
    uint16_t l1_index = dmGui::GetLayoutIndex(m_Scene, l1_hash);
    ASSERT_EQ(1, l1_index);
    uint16_t l2_index = dmGui::GetLayoutIndex(m_Scene, l2_hash);
    ASSERT_EQ(2, l2_index);
    ASSERT_EQ(3, dmGui::GetLayoutCount(m_Scene));

    Point3 p0(0,0,0);
    Point3 p1(1,0,0);
    Point3 p2(2,0,0);
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(3,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);
    ASSERT_EQ(3, dmGui::GetNodePosition(m_Scene, node).getX());

    // set data for layout index range 0-2
    r = dmGui::SetNodeLayoutDesc(m_Scene, node, &p0, 0, 2);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    // test all layouts
    dmGui::SetLayout(m_Scene, dmGui::DEFAULT_LAYOUT, SetNodeCallback);
    ASSERT_EQ(dmGui::DEFAULT_LAYOUT, dmGui::GetLayout(m_Scene));
    ASSERT_EQ(dmGui::GetNodePosition(m_Scene, node).getX(), 0);

    dmGui::SetLayout(m_Scene, l1_hash, SetNodeCallback);
    ASSERT_EQ(l1_hash, dmGui::GetLayout(m_Scene));
    ASSERT_EQ(dmGui::GetNodePosition(m_Scene, node).getX(), 0);

    dmGui::SetLayout(m_Scene, l2_hash, SetNodeCallback);
    ASSERT_EQ(l2_hash, dmGui::GetLayout(m_Scene));
    ASSERT_EQ(dmGui::GetNodePosition(m_Scene, node).getX(), 0);

    // set data for layout independently index 0,1,2
    r = dmGui::SetNodeLayoutDesc(m_Scene, node, &p1, 1, 1);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::SetNodeLayoutDesc(m_Scene, node, &p2, 2, 2);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    // test all layouts
    dmGui::SetLayout(m_Scene, dmGui::DEFAULT_LAYOUT, SetNodeCallback);
    ASSERT_EQ(dmGui::GetNodePosition(m_Scene, node).getX(), 0);

    dmGui::SetLayout(m_Scene, l1_hash, SetNodeCallback);
    ASSERT_EQ(dmGui::GetNodePosition(m_Scene, node).getX(), 1);

    dmGui::SetLayout(m_Scene, l2_hash, SetNodeCallback);
    ASSERT_EQ(dmGui::GetNodePosition(m_Scene, node).getX(), 2);

    // test script functions
    const char* s = "function init(self)\n"
                    "    assert(hash(\"layout1\") == gui.get_layout())\n"
                    "end\n"
                    "function update(self, dt)\n"
                    "    assert(hash(\"layout2\") == gui.get_layout())\n"
                    "end\n";

    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::SetLayout(m_Scene, l1_hash, SetNodeCallback);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::SetLayout(m_Scene, l2_hash, SetNodeCallback);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}


TEST_F(dmGuiTest, SizeMode)
{
    int t1, ts1;
    dmGui::Result r;

    r = dmGui::AddTexture(m_Scene, "t1", (void*) &t1, (void*) &ts1, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);

    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    Point3 size = dmGui::GetNodeSize(m_Scene, node);
    ASSERT_EQ(10, size.getX());
    ASSERT_EQ(10, size.getY());

    dmGui::SetNodeSizeMode(m_Scene, node, dmGui::SIZE_MODE_AUTO);
    size = dmGui::GetNodeSize(m_Scene, node);
    ASSERT_EQ(r, dmGui::RESULT_OK);
    ASSERT_EQ(1, size.getX());
    ASSERT_EQ(1, size.getY());
}


TEST_F(dmGuiTest, FlipbookAnim)
{
    int t1, ts1;
    dmGui::Result r;

    r = dmGui::AddTexture(m_Scene, "t1", (void*) &t1, (void*) &ts1, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);

    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    uint64_t fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(0, dmGui::GetNodeFlipbookAnimId(m_Scene, node));

    r = dmGui::PlayNodeFlipbookAnim(m_Scene, node, "ta1", 0x0);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(dmHashString64("ta1"), fb_id);

    const float* fb_uv = dmGui::GetNodeFlipbookAnimUV(m_Scene, node);
    ASSERT_NE((const float*) 0, fb_uv);
    ASSERT_EQ(0, fb_uv[0]);
    ASSERT_EQ(1, fb_uv[1]);
    ASSERT_EQ(0, fb_uv[2]);
    ASSERT_EQ(0, fb_uv[3]);
    ASSERT_EQ(1, fb_uv[4]);
    ASSERT_EQ(0, fb_uv[5]);
    ASSERT_EQ(1, fb_uv[6]);
    ASSERT_EQ(1, fb_uv[7]);

    bool fb_flipx = false, fb_flipy = false;
    dmGui::GetNodeFlipbookAnimUVFlip(m_Scene, node, fb_flipx, fb_flipy);
    ASSERT_EQ(true, fb_flipx);
    ASSERT_EQ(false, fb_flipy);

    dmGui::CancelNodeFlipbookAnim(m_Scene, node);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(0, fb_id);

    r = dmGui::PlayNodeFlipbookAnim(m_Scene, node, "ta1", 0x0);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(dmHashString64("ta1"), fb_id);

    dmGui::ClearTextures(m_Scene);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(0, fb_id);

    r = dmGui::AddTexture(m_Scene, "t2", (void*) &t1, 0, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::SetNodeTexture(m_Scene, node, "t2");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(0, dmGui::GetNodeFlipbookAnimId(m_Scene, node));
}

TEST_F(dmGuiTest, TextureFontLayer)
{
    int t1, t2;
    int ts1, ts2;
    int f1, f2;

    dmGui::AddTexture(m_Scene, "t1", (void*) &t1, (void*) &ts1, 1, 1);
    dmGui::AddTexture(m_Scene, "t2", (void*) &t2, (void*) &ts2, 1, 1);
    dmGui::AddFont(m_Scene, "f1", &f1);
    dmGui::AddFont(m_Scene, "f2", &f2);
    dmGui::AddLayer(m_Scene, "l1");
    dmGui::AddLayer(m_Scene, "l2");

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);

    dmGui::Result r;

    // Texture
    r = dmGui::SetNodeTexture(m_Scene, node, "foo");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    r = dmGui::SetNodeTexture(m_Scene, node, "f1");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::SetNodeTexture(m_Scene, node, "t2");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::AddTexture(m_Scene, "t2", (void*) &t1, (void*) &ts1, 1, 1);
    ASSERT_EQ(&t1, m_Scene->m_Nodes[node & 0xffff].m_Node.m_Texture);
    ASSERT_EQ(&ts1, m_Scene->m_Nodes[node & 0xffff].m_Node.m_TextureSet);

    dmGui::RemoveTexture(m_Scene, "t2");
    ASSERT_EQ((void*)0, m_Scene->m_Nodes[node & 0xffff].m_Node.m_Texture);

    r = dmGui::SetNodeTexture(m_Scene, node, "t2");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    dmGui::ClearTextures(m_Scene);
    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    // Font
    r = dmGui::SetNodeFont(m_Scene, node, "foo");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    r = dmGui::SetNodeFont(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    r = dmGui::SetNodeFont(m_Scene, node, "f1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::SetNodeFont(m_Scene, node, "f2");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::AddFont(m_Scene, "f2", &f1);
    ASSERT_EQ(&f1, m_Scene->m_Nodes[node & 0xffff].m_Node.m_Font);

    dmGui::RemoveFont(m_Scene, "f2");
    ASSERT_EQ((void*)0, m_Scene->m_Nodes[node & 0xffff].m_Node.m_Font);

    dmGui::ClearFonts(m_Scene);
    r = dmGui::SetNodeFont(m_Scene, node, "f1");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    // Layer
    r = dmGui::SetNodeLayer(m_Scene, node, "foo");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    r = dmGui::SetNodeLayer(m_Scene, node, "l1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::SetNodeLayer(m_Scene, node, "l2");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::DeleteNode(m_Scene, node, true);
}

static void* DynamicNewTexture(dmGui::HScene scene, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, void* context)
{
    return malloc(16);
}

static void DynamicDeleteTexture(dmGui::HScene scene, void* texture, void* context)
{
    assert(texture);
    free(texture);
}

static void DynamicSetTextureData(dmGui::HScene scene, void* texture, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, void* context)
{
}

static void DynamicRenderNodes(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    uint32_t* count = (uint32_t*) context;
    for (uint32_t i = 0; i < node_count; ++i) {
        dmGui::HNode node = nodes[i].m_Node;
        dmhash_t id = dmGui::GetNodeTextureId(scene, node);
        if ((id == dmHashString64("t1") || id == dmHashString64("t2")) && dmGui::GetNodeTexture(scene, node)) {
            *count = *count + 1;
        }
    }
}

TEST_F(dmGuiTest, DynamicTexture)
{
    uint32_t count = 0;
    dmGui::RenderSceneParams rp;
    rp.m_RenderNodes = DynamicRenderNodes;
    rp.m_NewTexture = DynamicNewTexture;
    rp.m_DeleteTexture = DynamicDeleteTexture;
    rp.m_SetTextureData = DynamicSetTextureData;

    const int width = 2;
    const int height = 2;
    char data[width * height * 3] = { 0 };

    // Test creation/deletion in the same frame (case 2355)
    dmGui::Result r;
    r = dmGui::NewDynamicTexture(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_RGB, false, data, sizeof(data));
    ASSERT_EQ(r, dmGui::RESULT_OK);
    r = dmGui::DeleteDynamicTexture(m_Scene, dmHashString64("t1"));
    ASSERT_EQ(r, dmGui::RESULT_OK);
    dmGui::RenderScene(m_Scene, rp, &count);

    r = dmGui::NewDynamicTexture(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_RGB, false, data, sizeof(data));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::SetDynamicTextureData(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_RGB, false, data, sizeof(data));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);

    r = dmGui::SetNodeTexture(m_Scene, node, "foo");
    ASSERT_EQ(r, dmGui::RESULT_RESOURCE_NOT_FOUND);

    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::RenderScene(m_Scene, rp, &count);
    ASSERT_EQ(1U, count);

    r = dmGui::DeleteDynamicTexture(m_Scene, dmHashString64("t1"));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    // Recreate the texture again (without RenderScene)
    r = dmGui::NewDynamicTexture(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_RGB, false, data, sizeof(data));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::DeleteDynamicTexture(m_Scene, dmHashString64("t1"));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    // Set data on deleted texture
    r = dmGui::SetDynamicTextureData(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_RGB, false, data, sizeof(data));
    ASSERT_EQ(r, dmGui::RESULT_INVAL_ERROR);

    dmGui::DeleteNode(m_Scene, node, true);

    dmGui::RenderScene(m_Scene, rp, &count);
}


#define ASSERT_BUFFER(exp, act, count)\
    for (int i = 0; i < count; ++i) {\
        ASSERT_EQ((exp)[i], (act)[i]);\
    }\

TEST_F(dmGuiTest, DynamicTextureFlip)
{
    const int width = 2;
    const int height = 2;

    // Test data tuples (regular image data & and flipped counter part)
    const uint8_t data_lum[width * height * 1] = {
            255, 0,
            0, 255,
        };
    const uint8_t data_lum_flip[width * height * 1] = {
            0, 255,
            255, 0,
        };
    const uint8_t data_rgb[width * height * 3] = {
            255, 0, 0,  0, 255, 0,
            0, 0, 255,  255, 255, 255,
        };
    const uint8_t data_rgb_flip[width * height * 3] = {
            0, 0, 255,  255, 255, 255,
            255, 0, 0,  0, 255, 0,
        };
    const uint8_t data_rgba[width * height * 4] = {
            255, 0, 0, 255,  0, 255, 0, 255,
            0, 0, 255, 255,  255, 255, 255, 255,
        };
    const uint8_t data_rgba_flip[width * height * 4] = {
            0, 0, 255, 255,  255, 255, 255, 255,
            255, 0, 0, 255,  0, 255, 0, 255,
        };

    // Vars to fetch data results
    uint32_t out_width = 0;
    uint32_t out_height = 0;
    dmImage::Type out_type = dmImage::TYPE_LUMINANCE;
    const uint8_t* out_buffer = NULL;

    // Create and upload RGB image + flip
    dmGui::Result r;
    r = dmGui::NewDynamicTexture(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_RGB, true, data_rgb, sizeof(data_rgb));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    // Get buffer, verify same as input but flipped
    r = dmGui::GetDynamicTextureData(m_Scene, dmHashString64("t1"), &out_width, &out_height, &out_type, (const void**)&out_buffer);
    ASSERT_EQ(r, dmGui::RESULT_OK);
    ASSERT_EQ(width, out_width);
    ASSERT_EQ(height, out_height);
    ASSERT_EQ(dmImage::TYPE_RGB, out_type);
    ASSERT_BUFFER(data_rgb_flip, out_buffer, width*height*3);

    // Upload RGBA data and flip
    r = dmGui::SetDynamicTextureData(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_RGBA, true, data_rgba, sizeof(data_rgba));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    // Verify flipped result
    r = dmGui::GetDynamicTextureData(m_Scene, dmHashString64("t1"), &out_width, &out_height, &out_type, (const void**)&out_buffer);
    ASSERT_EQ(r, dmGui::RESULT_OK);
    ASSERT_EQ(width, out_width);
    ASSERT_EQ(height, out_height);
    ASSERT_EQ(dmImage::TYPE_RGBA, out_type);
    ASSERT_BUFFER(data_rgba_flip, out_buffer, width*height*4);

    // Upload luminance data and flip
    r = dmGui::SetDynamicTextureData(m_Scene, dmHashString64("t1"), width, height, dmImage::TYPE_LUMINANCE, true, data_lum, sizeof(data_lum));
    ASSERT_EQ(r, dmGui::RESULT_OK);

    // Verify flipped result
    r = dmGui::GetDynamicTextureData(m_Scene, dmHashString64("t1"), &out_width, &out_height, &out_type, (const void**)&out_buffer);
    ASSERT_EQ(r, dmGui::RESULT_OK);
    ASSERT_EQ(width, out_width);
    ASSERT_EQ(height, out_height);
    ASSERT_EQ(dmImage::TYPE_LUMINANCE, out_type);
    ASSERT_BUFFER(data_lum_flip, out_buffer, width*height);

    r = dmGui::DeleteDynamicTexture(m_Scene, dmHashString64("t1"));
    ASSERT_EQ(r, dmGui::RESULT_OK);
}

#undef ASSERT_BUFFER

TEST_F(dmGuiTest, ScriptFlipbookAnim)
{
    int t1, ts1;
    dmGui::Result r;

    r = dmGui::AddTexture(m_Scene, "t1", (void*) &t1, (void*) &ts1, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    const char* id = "n";
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);
    dmGui::SetNodeId(m_Scene, node, id);

    const char* s = "local ref_val = 0\n"
                    "local frame_count = 0\n"
                    "\n"
                    "function flipbook_complete(self, node)\n"
                    "   ref_val = ref_val + 1\n"
                    "end\n"
                    "\n"
                    "function init(self)\n"
                    "    local n = gui.get_node(\"n\")\n"
                    "    gui.set_texture(n, \"t1\")\n"
                    "    local id = hash(\"ta1\")\n"
                    "    gui.play_flipbook(n, id)\n"
                    "    local id2 = gui.get_flipbook(n)\n"
                    "    assert(id == id2)\n"
                    "    gui.cancel_flipbook(n)\n"
                    "    id2 = gui.get_flipbook(n)\n"
                    "    gui.play_flipbook(n, id, flipbook_complete)\n"
                    "    assert(id ~= id2)\n"
                    "end\n"
                    "\n"
                    "function update(self, dt)\n"
                    "    assert(ref_val == frame_count)\n"
                    "    frame_count = frame_count + 1\n"
                    "end\n";

    ASSERT_TRUE(SetScript(m_Script, s));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::UpdateScene(m_Scene, 1.0f / 30.0f));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::UpdateScene(m_Scene, 1.0f / 30.0f));
}

TEST_F(dmGuiTest, ScriptTextureFontLayer)
{
    int t, ts;
    int f;

    dmGui::AddTexture(m_Scene, "t", (void*) &t, (void*) &ts, 1, 1);
    dmGui::AddFont(m_Scene, "f", &f);
    dmGui::AddLayer(m_Scene, "l");

    const char* id = "n";
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);
    dmGui::SetNodeId(m_Scene, node, id);

    const char* s = "function init(self)\n"
                    "    local n = gui.get_node(\"n\")\n"
                    "    gui.set_texture(n, \"t\")\n"
                    "    local t = gui.get_texture(n)\n"
                    "    gui.set_texture(n, t)\n"
                    "    local t2 = gui.get_texture(n)\n"
                    "    assert(t == t2)\n"
                    "    gui.set_font(n, \"f\")\n"
                    "    local f = gui.get_font(n)\n"
                    "    gui.set_font(n, f)\n"
                    "    local f2 = gui.get_font(n)\n"
                    "    assert(f == f2)\n"
                    "    gui.set_layer(n, \"l\")\n"
                    "    local l = gui.get_layer(n)\n"
                    "    gui.set_layer(n, l)\n"
                    "    local l2 = gui.get_layer(n)\n"
                    "    assert(l == l2)\n"
                    "end\n";

    ASSERT_TRUE(SetScript(m_Script, s));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
}

TEST_F(dmGuiTest, ScriptDynamicTexture)
{
    const char* id = "n";
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);
    dmGui::SetNodeId(m_Scene, node, id);

    const char* s = "function init(self)\n"
                    "    local r = gui.new_texture('t', 2, 2, 'rgb', string.rep('\\0', 2 * 2 * 3))\n"
                    "    assert(r == true)\n"
                    "    local r = gui.set_texture_data('t', 2, 2, 'rgb', string.rep('\\0', 2 * 2 * 3))\n"
                    "    assert(r == true)\n"
                    "    local n = gui.get_node('n')\n"
                    "    gui.set_texture(n, 't')\n"
                    "    gui.delete_texture('t')\n"

                    "    local r = gui.new_texture(hash('t'), 2, 2, 'rgb', string.rep('\\0', 2 * 2 * 3))\n"
                    "    assert(r == true)\n"
                    "    local r = gui.set_texture_data(hash('t'), 2, 2, 'rgb', string.rep('\\0', 2 * 2 * 3))\n"
                    "    assert(r == true)\n"
                    "    local n = gui.get_node('n')\n"
                    "    gui.set_texture(n, hash('t'))\n"
                    "    gui.delete_texture(hash('t'))\n"
                    "end\n";

    ASSERT_TRUE(SetScript(m_Script, s));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::UpdateScene(m_Scene, 1.0f / 60.0f));

    dmGui::RenderSceneParams rp;
    rp.m_RenderNodes = DynamicRenderNodes;
    rp.m_NewTexture = DynamicNewTexture;
    rp.m_DeleteTexture = DynamicDeleteTexture;
    rp.m_SetTextureData = DynamicSetTextureData;
    dmGui::RenderScene(m_Scene, rp, this);
}

TEST_F(dmGuiTest, ScriptIndex)
{
    const char* id = "n";
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);
    dmGui::SetNodeId(m_Scene, node, id);

    const char* id2 = "n2";
    node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);
    dmGui::SetNodeId(m_Scene, node, id2);

    const char* s = "function init(self)\n"
                    "    local n = gui.get_node(\"n\")\n"
                    "    assert(gui.get_index(n) == 0)\n"
                    "    local n2 = gui.get_node(\"n2\")\n"
                    "    assert(gui.get_index(n2) == 1)\n"
                    "end\n";

    ASSERT_TRUE(SetScript(m_Script, s));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
}

TEST_F(dmGuiTest, NewDeleteNode)
{
    std::map<dmGui::HNode, float> node_to_pos;

    for (uint32_t i = 0; i < MAX_NODES; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX);
        ASSERT_NE((dmGui::HNode) 0, node);
        node_to_pos[node] = (float) i;
    }

    for (uint32_t i = 0; i < 1000; ++i)
    {
        ASSERT_EQ(MAX_NODES, node_to_pos.size());

        std::map<dmGui::HNode, float>::iterator iter;
        for (iter = node_to_pos.begin(); iter != node_to_pos.end(); ++iter)
        {
            dmGui::HNode node = iter->first;
            ASSERT_EQ(iter->second, dmGui::GetNodePosition(m_Scene, node).getX());
        }
        int index = rand() % MAX_NODES;
        iter = node_to_pos.begin();
        for (int j = 0; j < index; ++j)
            ++iter;
        dmGui::HNode node_to_remove = iter->first;
        node_to_pos.erase(iter);
        dmGui::DeleteNode(m_Scene, node_to_remove, true);

        dmGui::HNode new_node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX);
        ASSERT_NE((dmGui::HNode) 0, new_node);
        node_to_pos[new_node] = (float) i;
    }
}

TEST_F(dmGuiTest, ClearNodes)
{
    for (uint32_t i = 0; i < MAX_NODES; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX);
        ASSERT_NE((dmGui::HNode) 0, node);
    }

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX);
    ASSERT_EQ((dmGui::HNode) 0, node);

    dmGui::ClearNodes(m_Scene);
    for (uint32_t i = 0; i < MAX_NODES; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX);
        ASSERT_NE((dmGui::HNode) 0, node);
    }
}

TEST_F(dmGuiTest, AnimateNode)
{
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    for (uint32_t i = 0; i < MAX_ANIMATIONS + 1; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
        dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0.5f, 0, 0, 0);

        ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

        // Delay
        for (int i = 0; i < 30; ++i)
            dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);

        ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

        // Animation
        for (int i = 0; i < 60; ++i)
        {
            dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        }

        ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f, EPSILON);
        dmGui::DeleteNode(m_Scene, node, true);
    }
}

TEST_F(dmGuiTest, CustomEasingAnimation)
{
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);

    dmVMath::FloatVector vector(64);
    dmEasing::Curve curve(dmEasing::TYPE_FLOAT_VECTOR);
    curve.vector = &vector;

    // fill as linear curve
    for (int i = 0; i < 64; ++i) {
        float t = i / 63.0f;
        vector.values[i] = t;
    }

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), curve, dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0.0f, 0, 0, 0);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), (float)i / 60.0f, EPSILON);
        dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    }

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f, EPSILON);
    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, Playback)
{
    const float duration = 4 / 60.0f;
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(0,0,0), dmGui::NODE_TYPE_BOX);

    dmGui::SetNodePosition(m_Scene, node, Point3(0,0,0));
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_BACKWARD, duration, 0, 0, 0, 0);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 3.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f / 4.0f, EPSILON);

    dmGui::SetNodePosition(m_Scene, node, Point3(0,0,0));
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_LOOP_FORWARD, duration, 0, 0, 0, 0);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 3.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 4.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f / 4.0f, EPSILON);

    dmGui::SetNodePosition(m_Scene, node, Point3(0,0,0));
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_LOOP_BACKWARD, duration, 0, 0, 0, 0);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 3.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 3.0f / 4.0f, EPSILON);

    dmGui::SetNodePosition(m_Scene, node, Point3(0,0,0));
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_LOOP_PINGPONG, duration, 0, 0, 0, 0);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 4.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f / 4.0f, EPSILON);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 4.0f / 4.0f, EPSILON);


    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, AnimateNode2)
{
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.1f, 0, 0, 0, 0);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Animation
    for (int i = 0; i < 200; ++i)
    {
        dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    }

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f, EPSILON);
    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, AnimateNodeDelayUnderFlow)
{
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 2.0f / 60.0f, 1.0f / 60.0f, 0, 0, 0);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    dmGui::UpdateScene(m_Scene, 0.5f * (1.0f / 60.0f));
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    dmGui::UpdateScene(m_Scene, 1.0f * (1.0f / 60.0f));
    // With underflow compensation: -(0.5 / 60.) + dt = 0.5 / 60
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.25f, EPSILON);

    // Animation done
    dmGui::UpdateScene(m_Scene, 1.5f * (1.0f / 60.0f));
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f, EPSILON);

    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, AnimateNodeDelete)
{
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.1f, 0, 0, 0, 0);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);
    dmGui::HNode node2 = 0;

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        if (i == 30)
        {
            dmGui::DeleteNode(m_Scene, node, true);
            node2 = dmGui::NewNode(m_Scene, Point3(2,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
        }

        dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    }

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node2).getX(), 2.0f, EPSILON);
    dmGui::DeleteNode(m_Scene, node2, true);
}

uint32_t MyAnimationCompleteCount = 0;
void MyAnimationComplete(dmGui::HScene scene,
                         dmGui::HNode node,
                         bool finished,
                         void* userdata1,
                         void* userdata2)
{
    MyAnimationCompleteCount++;
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(scene, node, property, Vector4(2,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, 0, 0, 0);
    // Check that we reached target position
    *(Point3*)userdata2 = dmGui::GetNodePosition(scene, node);
}

TEST_F(dmGuiTest, AnimateComplete)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    Point3 completed_position;
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyAnimationComplete, (void*) node, (void*)&completed_position);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Animation
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i)
    {
        dmGui::UpdateScene(m_Scene, dt);
    }
    Point3 position = dmGui::GetNodePosition(m_Scene, node);
    ASSERT_NEAR(position.getX(), 1.0f, EPSILON);
    ASSERT_EQ(1.0f, completed_position.getX());

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    }
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f, EPSILON);

    dmGui::DeleteNode(m_Scene, node, true);
}

void MyPingPongComplete2(dmGui::HScene scene,
                         dmGui::HNode node,
                         bool finished,
                         void* userdata1,
                         void* userdata2);

uint32_t PingPongCount = 0;
void MyPingPongComplete1(dmGui::HScene scene,
                        dmGui::HNode node,
                        bool finished,
                        void* userdata1,
                        void* userdata2)
{
    ++PingPongCount;
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(scene, node, property, Vector4(0,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyPingPongComplete2, (void*) node, 0);
}

void MyPingPongComplete2(dmGui::HScene scene,
                         dmGui::HNode node,
                         bool finished,
                         void* userdata1,
                         void* userdata2)
{
    ++PingPongCount;
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyPingPongComplete1, (void*) node, 0);
}

TEST_F(dmGuiTest, PingPong)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyPingPongComplete1, (void*) node, 0);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    for (int j = 0; j < 10; ++j)
    {
        // Animation
        for (int i = 0; i < 60; ++i)
        {
            dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        }
    }

    ASSERT_EQ(10U, PingPongCount);
    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, AnimateNodeOfDisabledParent)
{
    dmGui::HNode parent = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode child = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeParent(m_Scene, child, parent);
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(m_Scene, child, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0.0f, 0, 0, 0);

    dmGui::SetNodeEnabled(m_Scene, parent, false);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, child).getX(), 0.0f, EPSILON);

    // Delay
    for (int i = 0; i < 30; ++i)
        dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, child).getX(), 0.0f, EPSILON);

    dmGui::DeleteNode(m_Scene, child, true);
    dmGui::DeleteNode(m_Scene, parent, true);
}

TEST_F(dmGuiTest, Reset)
{
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(10, 20, 30), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(100, 200, 300), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    // Set reset point only for the first node
    dmGui::SetNodeResetPoint(m_Scene, n1);
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(m_Scene, n1, property, Vector4(1, 0, 0, 0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0.0f, 0, 0, 0);
    dmGui::AnimateNodeHash(m_Scene, n2, property, Vector4(101, 0, 0, 0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0.0f, 0, 0, 0);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);

    dmGui::ResetNodes(m_Scene);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, n1).getX(), 10.0f, EPSILON);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, n2).getX(), 100.0f + 1.0f / 60.0f, EPSILON);

    dmGui::DeleteNode(m_Scene, n1, true);
    dmGui::DeleteNode(m_Scene, n2, true);
}

TEST_F(dmGuiTest, ScriptAnimate)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.node = gui.get_node(\"n\")\n"
                    "    gui.animate(self.node, gui.PROP_POSITION, vmath.vector4(1,0,0,0), gui.EASING_NONE, 1, 0.5)\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(self.node)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Delay
    for (int i = 0; i < 30; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f, EPSILON);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    ASSERT_EQ(m_Scene->m_NodePool.Capacity(), m_Scene->m_NodePool.Remaining());
}

TEST_F(dmGuiTest, ScriptPlayback)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.node = gui.get_node(\"n\")\n"
                    "    gui.animate(self.node, gui.PROP_POSITION, vmath.vector4(1,0,0,0), gui.EASING_NONE, 1, 0, nil, gui.PLAYBACK_ONCE_BACKWARD)\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(self.node)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    ASSERT_EQ(m_Scene->m_NodePool.Capacity(), m_Scene->m_NodePool.Remaining());
}

TEST_F(dmGuiTest, ScriptAnimatePreserveAlpha)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.node = gui.get_node(\"n\")\n"
                    "    gui.set_color(self.node, vmath.vector4(0,0,0,0.5))\n"
                    "    gui.animate(self.node, gui.PROP_COLOR, vmath.vector3(1,0,0), gui.EASING_NONE, 0.01)\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(self.node)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    Vector4 color = dmGui::GetNodeProperty(m_Scene, node, dmGui::PROPERTY_COLOR);
    ASSERT_NEAR(color.getX(), 1.0f, EPSILON);
    ASSERT_NEAR(color.getW(), 0.5f, EPSILON);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptAnimateComponent)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.node = gui.get_node(\"n\")\n"
                    "    gui.set_color(self.node, vmath.vector4(0.1,0.2,0.3,0.4))\n"
                    "    gui.animate(self.node, \"color.z\", 0.9, gui.EASING_NONE, 0.01)\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(self.node)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    Vector4 color = dmGui::GetNodeProperty(m_Scene, node, dmGui::PROPERTY_COLOR);
    ASSERT_NEAR(color.getX(), 0.1f, EPSILON);
    ASSERT_NEAR(color.getY(), 0.2f, EPSILON);
    ASSERT_NEAR(color.getZ(), 0.9f, EPSILON);
    ASSERT_NEAR(color.getW(), 0.4f, EPSILON);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptAnimateComplete)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function cb(self, node)\n"
                    "    assert(self.foobar == 123)\n"
                    "    gui.animate(node, gui.PROP_POSITION, vmath.vector4(2,0,0,0), gui.EASING_NONE, 0.5, 0)\n"
                    "end\n;"
                    "function init(self)\n"
                    "    self.foobar = 123\n"
                    "    gui.animate(gui.get_node(\"n\"), gui.PROP_POSITION, vmath.vector4(1,0,0,0), gui.EASING_NONE, 1, 0, cb)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 1.0f, EPSILON);

    // Animation
    for (int i = 0; i < 30; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 2.0f, EPSILON);

    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, ScriptAnimateCompleteDelete)
{
    dmGui::HNode node1 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node2 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node1, "n1");
    dmGui::SetNodeId(m_Scene, node2, "n2");
    const char* s = "function cb(self, node)\n"
                    "    gui.delete_node(node)\n"
                    "end\n;"
                    "function init(self)\n"
                    "    gui.animate(gui.get_node(\"n1\"), gui.PROP_POSITION, vmath.vector4(1,0,0,0), gui.EASING_NONE, 1, 0, cb)\n"
                    "    gui.animate(gui.get_node(\"n2\"), gui.PROP_POSITION, vmath.vector4(1,0,0,0), gui.EASING_NONE, 1, 0, cb)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    uint32_t node_count = dmGui::GetNodeCount(m_Scene);
    ASSERT_EQ(2U, node_count);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node1).getX(), 0.0f, EPSILON);
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node2).getX(), 0.0f, EPSILON);
    // Animation
    for (int i = 0; i < 60; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }

    node_count = dmGui::GetNodeCount(m_Scene);
    ASSERT_EQ(0U, node_count);
}

TEST_F(dmGuiTest, ScriptAnimateCancel1)
{
    // Immediate cancel
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.node = gui.get_node(\"n\")\n"
                    "    gui.animate(self.node, gui.PROP_COLOR, vmath.vector4(1,0,0,0), gui.EASING_NONE, 0.2)\n"
                    "    gui.cancel_animation(self.node, gui.PROP_COLOR)\n"
                    "end\n"
                    "function update(self, dt)\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(gui.get_node(\"n\"))\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }

    ASSERT_NEAR(dmGui::GetNodeProperty(m_Scene, node, dmGui::PROPERTY_COLOR).getX(), 1.0f, EPSILON);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}


TEST_F(dmGuiTest, ScriptAnimateCancel2)
{
    // Cancel after 50% has elapsed
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.node = gui.get_node(\"n\")\n"
                    "    gui.animate(self.node, gui.PROP_POSITION, vmath.vector4(10,0,0,0), gui.EASING_NONE, 1)\n"
                    "    self.nframes = 0\n"
                    "end\n"
                    "function update(self, dt)\n"
                    "    self.nframes = self.nframes + 1\n"
                    "    if self.nframes > 30 then\n"
                    "        gui.cancel_animation(self.node, gui.PROP_POSITION)\n"
                    "    end\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(gui.get_node(\"n\"))\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        ASSERT_EQ(dmGui::RESULT_OK, r);
    }

    // We can't use epsilon here because of precision errors when the animation is canceled, so half precision (= twice the error)
    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 5.0f, 2*EPSILON);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptOutOfNodes)
{
    const char* s = "function init(self)\n"
                    "    for i=1,10000 do\n"
                    "        gui.new_box_node(vmath.vector3(0,0,0), vmath.vector3(1,1,1))\n"
                    "    end\n"
                    "end\n"
                    "function update(self)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, r);
}

TEST_F(dmGuiTest, ScriptGetNode)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function update(self) local n = gui.get_node(\"n\")\n print(n)\n end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, ScriptGetMissingNode)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function update(self) local n = gui.get_node(\"x\")\n print(n)\n end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, r);

    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, ScriptGetDeletedNode)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function update(self) local n = gui.get_node(\"n\")\n print(n)\n end";
    dmGui::DeleteNode(m_Scene, node, true);

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, r);
}

TEST_F(dmGuiTest, ScriptEqNode)
{
    dmGui::HNode node1 = dmGui::NewNode(m_Scene, Point3(1,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node2 = dmGui::NewNode(m_Scene, Point3(2,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node1, "n");
    dmGui::SetNodeId(m_Scene, node2, "m");

    const char* s = "function update(self)\n"
                    "local n1 = gui.get_node(\"n\")\n "
                    "local n2 = gui.get_node(\"n\")\n "
                    "local m = gui.get_node(\"m\")\n "
                    "assert(n1 == n2)\n"
                    "assert(m ~= n1)\n"
                    "assert(m ~= n2)\n"
                    "assert(m ~= 1)\n"
                    "assert(1 ~= m)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::DeleteNode(m_Scene, node1, true);
    dmGui::DeleteNode(m_Scene, node2, true);
}

TEST_F(dmGuiTest, ScriptNewNode)
{
    const char* s = "function init(self)\n"
                    "    self.n1 = gui.new_box_node(vmath.vector3(0,0,0), vmath.vector3(1,1,1))"
                    "    self.n2 = gui.new_text_node(vmath.vector3(0,0,0), \"My Node\")"
                    "end\n"
                    "function update(self)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptNewNodeVec4)
{
    const char* s = "function init(self)\n"
                    "    self.n1 = gui.new_box_node(vmath.vector4(0,0,0,0), vmath.vector3(1,1,1))"
                    "    self.n2 = gui.new_text_node(vmath.vector4(0,0,0,0), \"My Node\")"
                    "end\n"
                    "function update(self)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptGetSet)
{
    const char* s = "function init(self)\n"
                    "    self.n1 = gui.new_box_node(vmath.vector4(0,0,0,0), vmath.vector3(1,1,1))\n"
                    "    local p = gui.get_position(self.n1)\n"
                    "    assert(string.find(tostring(p), \"vector3\") ~= nil)\n"
                    "    gui.set_position(self.n1, p)\n"
                    "    local s = gui.get_scale(self.n1)\n"
                    "    assert(string.find(tostring(s), \"vector3\") ~= nil)\n"
                    "    gui.set_scale(self.n1, s)\n"
                    "    local r = gui.get_rotation(self.n1)\n"
                    "    assert(string.find(tostring(r), \"vector3\") ~= nil)\n"
                    "    gui.set_rotation(self.n1, r)\n"
                    "    local c = gui.get_color(self.n1)\n"
                    "    assert(string.find(tostring(c), \"vector4\") ~= nil)\n"
                    "    gui.set_color(self.n1, c)\n"
                    "    gui.set_color(self.n1, vmath.vector4(0, 0, 0, 1))\n"
                    "    gui.set_color(self.n1, vmath.vector3(0, 0, 0))\n"
                    "    c = gui.get_color(self.n1)\n"
                    "    assert(c.w == 1)\n"
                   "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptInput)
{
    const char* s = "function update(self)\n"
                    "   assert(g_value == 123)\n"
                    "end\n"
                    "function on_input(self, action_id, action)\n"
                    "   if(action_id == hash(\"SPACE\")) then\n"
                    "       g_value = 123\n"
                    "   end\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::InputAction input_action;
    input_action.m_ActionId = dmHashString64("SPACE");
    bool consumed;
    r = dmGui::DispatchInput(m_Scene, &input_action, 1, &consumed);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    ASSERT_FALSE(consumed);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptInputConsume)
{
    const char* s = "function update(self)\n"
                    "   assert(g_value == 123)\n"
                    "end\n"
                    "function on_input(self, action_id, action)\n"
                    "   if(action_id == hash(\"SPACE\")) then\n"
                    "       g_value = 123\n"
                    "   end\n"
                    "   return true\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::InputAction input_action;
    input_action.m_ActionId = dmHashString64("SPACE");
    bool consumed;
    r = dmGui::DispatchInput(m_Scene, &input_action, 1, &consumed);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    ASSERT_TRUE(consumed);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptInputMouseMovement)
{
    // No mouse
    const char* s = "function on_input(self, action_id, action)\n"
                    "   assert(action.x == nil)\n"
                    "   assert(action.y == nil)\n"
                    "   assert(action.dx == nil)\n"
                    "   assert(action.dy == nil)\n"
                    "end\n";
    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::InputAction input_action;
    input_action.m_ActionId = dmHashString64("SPACE");
    bool consumed;
    r = dmGui::DispatchInput(m_Scene, &input_action, 1, &consumed);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    // Mouse movement
    s = "function on_input(self, action_id, action)\n"
        "   assert(action_id == nil)\n"
        "   assert(action.value == nil)\n"
        "   assert(action.pressed == nil)\n"
        "   assert(action.released == nil)\n"
        "   assert(action.repeated == nil)\n"
        "   assert(action.x == 1.0)\n"
        "   assert(action.y == 2.0)\n"
        "   assert(action.dx == 3.0)\n"
        "   assert(action.dy == 4.0)\n"
        "end\n";
    input_action.m_ActionId = 0;
    input_action.m_PositionSet = true;
    input_action.m_X = 1.0f;
    input_action.m_Y = 2.0f;
    input_action.m_DX = 3.0f;
    input_action.m_DY = 4.0f;

    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::DispatchInput(m_Scene, &input_action, 1, &consumed);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

struct TestMessage
{
    dmhash_t m_ComponentId;
    dmhash_t m_MessageId;
};

static void Dispatch1(dmMessage::Message* message, void* user_ptr)
{
    TestMessage* test_message = (TestMessage*) user_ptr;
    test_message->m_ComponentId = message->m_Receiver.m_Fragment;
    test_message->m_MessageId = message->m_Id;
}

TEST_F(dmGuiTest, PostMessage1)
{
    const char* s = "function init(self)\n"
                    "   msg.post(\"#component\", \"my_named_message\")\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    TestMessage test_message;
    dmMessage::Dispatch(m_Socket, &Dispatch1, &test_message);

    ASSERT_EQ(dmHashString64("component"), test_message.m_ComponentId);
    ASSERT_EQ(dmHashString64("my_named_message"), test_message.m_MessageId);
}

TEST_F(dmGuiTest, MissingSetSceneInDispatchInputBug)
{
    const char* s = "function update(self)\n"
                    "end\n"
                    "function on_input(self, action_id, action)\n"
                    "   msg.post(\"#component\", \"my_named_message\")\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::InputAction input_action;
    input_action.m_ActionId = dmHashString64("SPACE");
    bool consumed;
    r = dmGui::DispatchInput(m_Scene, &input_action, 1, &consumed);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

static void Dispatch2(dmMessage::Message* message, void* user_ptr)
{
    assert(message->m_Receiver.m_Fragment == dmHashString64("component"));
    assert((dmDDF::Descriptor*)message->m_Descriptor == dmTestGuiDDF::AMessage::m_DDFDescriptor);

    dmTestGuiDDF::AMessage* amessage = (dmTestGuiDDF::AMessage*) message->m_Data;
    dmTestGuiDDF::AMessage* amessage_out = (dmTestGuiDDF::AMessage*) user_ptr;

    *amessage_out = *amessage;
}

TEST_F(dmGuiTest, PostMessage2)
{
    const char* s = "function init(self)\n"
                    "   msg.post(\"#component\", \"a_message\", { a = 123, b = 456 })\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmTestGuiDDF::AMessage amessage;
    dmMessage::Dispatch(m_Socket, &Dispatch2, &amessage);

    ASSERT_EQ(123, amessage.m_A);
    ASSERT_EQ(456, amessage.m_B);
}

static void Dispatch3(dmMessage::Message* message, void* user_ptr)
{
    dmGui::Result r = dmGui::DispatchMessage((dmGui::HScene)user_ptr, message);
    assert(r == dmGui::RESULT_OK);
}

TEST_F(dmGuiTest, PostMessage3)
{
    const char* s1 = "function init(self)\n"
                     "    msg.post(\"#component\", \"test_message\", { a = 123 })\n"
                     "end\n";

    const char* s2 = "function update(self, dt)\n"
                     "    assert(self.a == 123)\n"
                     "end\n"
                     "\n"
                     "function on_message(self, message_id, message, sender)\n"
                     "    if message_id == hash(\"test_message\") then\n"
                     "        self.a = message.a\n"
                     "    end\n"
                     "    local test_url = msg.url(\"\")\n"
                     "    assert(sender.socket == test_url.socket, \"invalid socket\")\n"
                     "    assert(sender.path == test_url.path, \"invalid path\")\n"
                     "    assert(sender.fragment == test_url.fragment, \"invalid fragment\")\n"
                     "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s1));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::NewSceneParams params;
    params.m_UserData = (void*)this;
    dmGui::HScene scene2 = dmGui::NewScene(m_Context, &params);
    ASSERT_NE((void*)scene2, (void*)0x0);
    dmGui::HScript script2 = dmGui::NewScript(m_Context);
    r = dmGui::SetSceneScript(scene2, script2);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::SetScript(script2, LuaSourceFromStr(s2));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    uint32_t message_count = dmMessage::Dispatch(m_Socket, &Dispatch3, scene2);
    ASSERT_EQ(1u, message_count);

    r = dmGui::UpdateScene(scene2, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::DeleteScript(script2);
    dmGui::DeleteScene(scene2);
}

TEST_F(dmGuiTest, PostMessageMissingField)
{
    const char* s = "function init(self)\n"
                    "   msg.post(\"a_message\", { a = 123 })\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, r);
}

TEST_F(dmGuiTest, PostMessageToGuiDDF)
{
    const char* s = "local a = 0\n"
                    "function update(self)\n"
                    "   assert(a == 123)\n"
                    "end\n"
                    "function on_message(self, message_id, message)\n"
                    "   assert(message_id == hash(\"amessage\"))\n"
                    "   a = message.a\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    char buf[sizeof(dmMessage::Message) + sizeof(dmTestGuiDDF::AMessage)];
    dmMessage::Message* message = (dmMessage::Message*)buf;
    message->m_Sender = dmMessage::URL();
    message->m_Receiver = dmMessage::URL();
    message->m_Id = dmHashString64("amessage");
    message->m_Descriptor = (uintptr_t)dmTestGuiDDF::AMessage::m_DDFDescriptor;
    message->m_DataSize = sizeof(dmTestGuiDDF::AMessage);
    dmTestGuiDDF::AMessage* amessage = (dmTestGuiDDF::AMessage*)message->m_Data;
    amessage->m_A = 123;
    r = dmGui::DispatchMessage(m_Scene, message);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, PostMessageToGuiEmptyLuaTable)
{
    const char* s = "local a = 0\n"
                    "function update(self)\n"
                    "   assert(a == 1)\n"
                    "end\n"
                    "function on_message(self, message_id, message)\n"
                    "   assert(message_id == hash(\"amessage\"))\n"
                    "   a = 1\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    char buffer[256 + sizeof(dmMessage::Message)];
    dmMessage::Message* message = (dmMessage::Message*)buffer;
    message->m_Sender = dmMessage::URL();
    message->m_Receiver = dmMessage::URL();
    message->m_Id = dmHashString64("amessage");
    message->m_Descriptor = 0;

    message->m_DataSize = 0;

    r = dmGui::DispatchMessage(m_Scene, message);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, PostMessageToGuiLuaTable)
{
    const char* s = "local a = 0\n"
                    "function update(self)\n"
                    "   assert(a == 456)\n"
                    "end\n"
                    "function on_message(self, message_id, message)\n"
                    "   assert(message_id == hash(\"amessage\"))\n"
                    "   a = message.a\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    char buffer[256 + sizeof(dmMessage::Message)];
    dmMessage::Message* message = (dmMessage::Message*)buffer;
    message->m_Sender = dmMessage::URL();
    message->m_Receiver = dmMessage::URL();
    message->m_Id = dmHashString64("amessage");
    message->m_Descriptor = 0;

    lua_State* L = lua_open();
    lua_newtable(L);
    lua_pushstring(L, "a");
    lua_pushinteger(L, 456);
    lua_settable(L, -3);
    message->m_DataSize = dmScript::CheckTable(L, (char*)message->m_Data, 256, -1);
    ASSERT_GT(message->m_DataSize, 0U);
    ASSERT_LE(message->m_DataSize, 256u);

    r = dmGui::DispatchMessage(m_Scene, message);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    lua_close(L);
}

TEST_F(dmGuiTest, SaveNode)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.n = gui.get_node(\"n\")\n"
                    "end\n"
                    "function update(self)\n"
                    "    assert(self.n, \"Node could not be saved!\")\n"
                    "end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, UseDeletedNode)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self) self.n = gui.get_node(\"n\")\n end function update(self) print(self.n)\n end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::DeleteNode(m_Scene, node, true);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, r);
}

TEST_F(dmGuiTest, NodeProperties)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    self.n = gui.get_node(\"n\")\n"
                    "    gui.set_position(self.n, vmath.vector4(1,2,3,0))\n"
                    "    gui.set_text(self.n, \"test\")\n"
                    "    gui.set_text(self.n, \"flipper\")\n"
                    "end\n"
                    "function update(self) "
                    "    local pos = gui.get_position(self.n)\n"
                    "    assert(pos.x == 1)\n"
                    "    assert(pos.y == 2)\n"
                    "    assert(pos.z == 3)\n"
                    "    assert(gui.get_text(self.n) == \"flipper\")\n"
                    "end";
    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, ReplaceAnimation)
{
    /*
     * NOTE: We create a node2 which animation duration is set to 0.5f
     * Internally the animation will removed an "erased-swapped". Used to test that the last animation
     * for node1 really invalidates the first animation of node1
     */
    dmGui::HNode node1 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node2 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);

    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(m_Scene, node2, property, Vector4(123,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 0.5f, 0, 0, 0, 0);
    dmGui::AnimateNodeHash(m_Scene, node1, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, 0, 0, 0);
    dmGui::AnimateNodeHash(m_Scene, node1, property, Vector4(10,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, 0, 0, 0);

    for (int i = 0; i < 60; ++i)
    {
        dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    }

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node1).getX(), 10.0f, EPSILON);

    dmGui::DeleteNode(m_Scene, node1, true);
    dmGui::DeleteNode(m_Scene, node2, true);
}

TEST_F(dmGuiTest, SyntaxError)
{
    const char* s = "function_ foo(self)";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_SYNTAX_ERROR, r);
}

TEST_F(dmGuiTest, MissingUpdate)
{
    const char* s = "function init(self) end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, MissingInit)
{
    const char* s = "function update(self) end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, NoScript)
{
    dmGui::Result r;
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, Self)
{
    const char* s = "function init(self) self.x = 1122 end\n function update(self) assert(self.x==1122) end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, Reload)
{
    const char* s1 = "function init(self)\n"
                     "    self.x = 1122\n"
                     "end\n"
                     "function update(self)\n"
                     "    assert(self.x==1122)\n"
                     "    self.x = self.x + 1\n"
                     "end";
    const char* s2 = "function update(self)\n"
                     "    assert(self.x==1124)\n"
                     "end\n"
                     "function on_reload(self)\n"
                     "    self.x = self.x + 1\n"
                     "end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s1));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    // assert should fail due to + 1
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, r);

    // Reload
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s2));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    // Should fail since on_reload has not been called
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_SCRIPT_ERROR, r);

    r = dmGui::ReloadScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, ScriptNamespace)
{
    // Test that "local" per file works, default lua behavior
    // The test demonstrates how to create file local variables by using the local keyword at top scope
    const char* s1 = "local x = 123\n local function f() return x end\n function update(self) assert(f()==123)\n end\n";
    const char* s2 = "local x = 456\n local function f() return x end\n function update(self) assert(f()==456)\n end\n";

    dmGui::NewSceneParams params;
    dmGui::HScene scene2 = dmGui::NewScene(m_Context, &params);

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s1));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s2));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(scene2, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::DeleteScene(scene2);
}

TEST_F(dmGuiTest, DeltaTime)
{
    const char* s = "function update(self, dt)\n"
                    "assert (dt == 1122)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1122);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

TEST_F(dmGuiTest, Bug352)
{
    dmGui::AddFont(m_Scene, "big_score", 0);
    dmGui::AddFont(m_Scene, "score", 0);
    dmGui::AddTexture(m_Scene, "left_hud", 0,0, 1, 1);
    dmGui::AddTexture(m_Scene, "right_hud", 0,0, 1, 1);

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr((const char*)BUG352_LUA, BUG352_LUA_SIZE));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::SetScript(m_Script, LuaSourceFromStr((const char*)BUG352_LUA, BUG352_LUA_SIZE));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    char buffer[256 + sizeof(dmMessage::Message)];
    dmMessage::Message* message = (dmMessage::Message*)buffer;
    message->m_Sender = dmMessage::URL();
    message->m_Receiver = dmMessage::URL();
    message->m_Id = dmHashString64("inc_score");
    message->m_Descriptor = 0;

    lua_State* L = lua_open();
    lua_newtable(L);
    lua_pushstring(L, "score");
    lua_pushinteger(L, 123);
    lua_settable(L, -3);

    message->m_DataSize = dmScript::CheckTable(L, (char*)message->m_Data, 256, -1);
    ASSERT_GT(message->m_DataSize, 0U);
    ASSERT_LE(message->m_DataSize, 256u);

    for (int i = 0; i < 100; ++i)
    {
        dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
        dmGui::DispatchMessage(m_Scene, message);
    }

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    lua_close(L);
}

TEST_F(dmGuiTest, Scaling)
{
    uint32_t width = 1024;
    uint32_t height = 768;

    uint32_t physical_width = 640;
    uint32_t physical_height = 480;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    const char* n1_name = "n1";
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(width/2.0f, height/2.0f, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, n1, n1_name);

    dmGui::RenderScene(m_Scene, &RenderNodes, this);

    Point3 center = m_NodeTextToRenderedPosition[n1_name] + m_NodeTextToRenderedSize[n1_name] * 0.5f;
    ASSERT_EQ(physical_width/2, center.getX());
    ASSERT_EQ(physical_height/2, center.getY());
}

TEST_F(dmGuiTest, Anchoring)
{
    uint32_t width = 1024;
    uint32_t height = 768;

    uint32_t physical_width = 640;
    uint32_t physical_height = 320;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    Vector4 ref_scale = dmGui::CalculateReferenceScale(m_Scene, 0);

    const char* n1_name = "n1";
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, n1, n1_name);
    dmGui::SetNodeXAnchor(m_Scene, n1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, n1, dmGui::YANCHOR_BOTTOM);

    const char* n2_name = "n2";
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(width - 10.0f, height - 10.0f, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, n2, n2_name);
    dmGui::SetNodeXAnchor(m_Scene, n2, dmGui::XANCHOR_RIGHT);
    dmGui::SetNodeYAnchor(m_Scene, n2, dmGui::YANCHOR_TOP);

    dmGui::RenderScene(m_Scene, &RenderNodes, this);

    Point3 pos1 = m_NodeTextToRenderedPosition[n1_name] + m_NodeTextToRenderedSize[n1_name] * 0.5f;
    const float EPSILON = 0.0001f;
    ASSERT_NEAR(10 * ref_scale.getX(), pos1.getX(), EPSILON);
    ASSERT_NEAR(10 * ref_scale.getY(), pos1.getY(), EPSILON);

    Point3 pos2 = m_NodeTextToRenderedPosition[n2_name] + m_NodeTextToRenderedSize[n2_name] * 0.5f;
    ASSERT_NEAR(physical_width - 10 * ref_scale.getX(), pos2.getX(), EPSILON);
    ASSERT_NEAR(physical_height - 10 * ref_scale.getY(), pos2.getY(), EPSILON);
}

TEST_F(dmGuiTest, ScriptAnchoring)
{
    uint32_t width = 1024;
    uint32_t height = 768;

    uint32_t physical_width = 640;
    uint32_t physical_height = 320;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    Vector4 ref_scale = dmGui::CalculateReferenceScale(m_Scene, 0);

    const char* s = "function init(self)\n"
                    "    assert (1024 == gui.get_width())\n"
                    "    assert (768 == gui.get_height())\n"
                    "    self.n1 = gui.new_text_node(vmath.vector3(10, 10, 0), \"n1\")"
                    "    gui.set_xanchor(self.n1, gui.ANCHOR_LEFT)\n"
                    "    assert(gui.get_xanchor(self.n1) == gui.ANCHOR_LEFT)\n"
                    "    gui.set_yanchor(self.n1, gui.ANCHOR_BOTTOM)\n"
                    "    assert(gui.get_yanchor(self.n1) == gui.ANCHOR_BOTTOM)\n"
                    "    self.n2 = gui.new_text_node(vmath.vector3(gui.get_width() - 10, gui.get_height()-10, 0), \"n2\")"
                    "    gui.set_xanchor(self.n2, gui.ANCHOR_RIGHT)\n"
                    "    assert(gui.get_xanchor(self.n2) == gui.ANCHOR_RIGHT)\n"
                    "    gui.set_yanchor(self.n2, gui.ANCHOR_TOP)\n"
                    "    assert(gui.get_yanchor(self.n2) == gui.ANCHOR_TOP)\n"
                    "end\n"
                    "function update(self)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::RenderScene(m_Scene, &RenderNodes, this);

    // These tests the actual position of the cursor when rendering text so we need to adjust with the ref-scaled text metrics
    float ref_factor = dmMath::Min(ref_scale.getX(), ref_scale.getY());
    Point3 pos1 = m_NodeTextToRenderedPosition["n1"];
    ASSERT_EQ(10 * ref_scale.getX(), pos1.getX() + ref_factor * TEXT_GLYPH_WIDTH);
    ASSERT_EQ(10 * ref_scale.getY(), pos1.getY() + ref_factor * 0.5f * (TEXT_MAX_DESCENT + TEXT_MAX_ASCENT));

    Point3 pos2 = m_NodeTextToRenderedPosition["n2"];
    ASSERT_EQ(physical_width - 10 * ref_scale.getX(), pos2.getX() + ref_factor * TEXT_GLYPH_WIDTH);
    ASSERT_EQ(physical_height - 10 * ref_scale.getY(), pos2.getY() + ref_factor * 0.5f * (TEXT_MAX_DESCENT + TEXT_MAX_ASCENT));
}

TEST_F(dmGuiTest, ScriptPivot)
{
    const char* s = "function init(self)\n"
                    "    local n1 = gui.new_text_node(vmath.vector3(10, 10, 0), \"n1\")"
                    "    assert(gui.get_pivot(n1) == gui.PIVOT_CENTER)\n"
                    "    gui.set_pivot(n1, gui.PIVOT_N)\n"
                    "    assert(gui.get_pivot(n1) == gui.PIVOT_N)\n"
                    "end\n";

    ASSERT_TRUE(SetScript(m_Script, s));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
}

TEST_F(dmGuiTest, AdjustMode)
{
    uint32_t width = 640;
    uint32_t height = 320;

    uint32_t physical_width = 1280;
    uint32_t physical_height = 320;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    // Verify that if we haven't specifically set adjust reference we should get
    // the legacy/old behaviour -> scene resolution should be adjust reference.
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_LEGACY, dmGui::GetSceneAdjustReference(m_Scene));

    Vector4 ref_scale = dmGui::CalculateReferenceScale(m_Scene, 0);
    float min_ref_scale = dmMath::Min(ref_scale.getX(), ref_scale.getY());
    float max_ref_scale = dmMath::Max(ref_scale.getX(), ref_scale.getY());

    dmGui::AdjustMode modes[] = {dmGui::ADJUST_MODE_FIT, dmGui::ADJUST_MODE_ZOOM, dmGui::ADJUST_MODE_STRETCH};
    Vector3 adjust_scales[] = {
            Vector3(min_ref_scale, min_ref_scale, 1.0f),
            Vector3(max_ref_scale, max_ref_scale, 1.0f),
            ref_scale.getXYZ()
    };

    dmGui::NodeType types[4] = {dmGui::NODE_TYPE_BOX, dmGui::NODE_TYPE_PIE, dmGui::NODE_TYPE_TEMPLATE, dmGui::NODE_TYPE_TEXT};
    float size_vals[4] = { 10.0f, 10.0f, 10.0f, 1.0f }; // Since the text nodes' transform isn't calculated with CALCULATE_NODE_BOUNDARY, the size will be 1

    for( uint32_t t = 0; t < sizeof(types)/sizeof(types[0]); ++t )
    {
        for (uint32_t i = 0; i < 3; ++i)
        {
            dmGui::AdjustMode mode = modes[i];
            Vector3 adjust_scale = adjust_scales[i];

            const float pos_val = 15.0f;
            const float size_val = size_vals[t];

            const char* center_name = "center";
            dmGui::HNode center_node = dmGui::NewNode(m_Scene, Point3(pos_val, pos_val, 0), Vector3(size_val, size_val, 0), types[t]);
            dmGui::SetNodeText(m_Scene, center_node, center_name);
            dmGui::SetNodePivot(m_Scene, center_node, dmGui::PIVOT_CENTER);
            dmGui::SetNodeAdjustMode(m_Scene, center_node, mode);

            const char* bl_name = "bottom_left";
            dmGui::HNode bl_node = dmGui::NewNode(m_Scene, Point3(pos_val, pos_val, 0), Vector3(size_val, size_val, 0), types[t]);
            dmGui::SetNodeText(m_Scene, bl_node, bl_name);
            dmGui::SetNodePivot(m_Scene, bl_node, dmGui::PIVOT_SW);
            dmGui::SetNodeAdjustMode(m_Scene, bl_node, mode);

            const char* tr_name = "top_right";
            dmGui::HNode tr_node = dmGui::NewNode(m_Scene, Point3(pos_val, pos_val, 0), Vector3(size_val, size_val, 0), types[t]);
            dmGui::SetNodeText(m_Scene, tr_node, tr_name);
            dmGui::SetNodePivot(m_Scene, tr_node, dmGui::PIVOT_NE);
            dmGui::SetNodeAdjustMode(m_Scene, tr_node, mode);

            dmGui::RenderScene(m_Scene, &RenderNodes, this);

            Vector3 offset((physical_width - width * adjust_scale.getX()) * 0.5f, (physical_height - height * adjust_scale.getY()) * 0.5f, 0.0f);

            Point3 center_p = m_NodeTextToRenderedPosition[center_name] + m_NodeTextToRenderedSize[center_name] * 0.5f;
            ASSERT_EQ(offset.getX() + pos_val * adjust_scale.getX(), center_p.getX());
            ASSERT_EQ(offset.getY() + pos_val * adjust_scale.getY(), center_p.getY());

            Point3 bl_p = m_NodeTextToRenderedPosition[bl_name];
            ASSERT_EQ(offset.getX() + pos_val * adjust_scale.getX(), bl_p.getX());
            ASSERT_EQ(offset.getY() + pos_val * adjust_scale.getY(), bl_p.getY());

            Point3 tr_p = m_NodeTextToRenderedPosition[tr_name] + m_NodeTextToRenderedSize[center_name];
            ASSERT_EQ(offset.getX() + pos_val * adjust_scale.getX(), tr_p.getX());
            ASSERT_EQ(offset.getY() + pos_val * adjust_scale.getY(), tr_p.getY());

            dmGui::DeleteNode(m_Scene, center_node, true);
            dmGui::DeleteNode(m_Scene, bl_node, true);
            dmGui::DeleteNode(m_Scene, tr_node, true);
        }
    }
}

TEST_F(dmGuiTest, ScriptErroneousReturnValues)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, node, "n");
    const char* s = "function init(self)\n"
                    "    return true\n"
                    "end\n"
                    "function final(self)\n"
                    "    return true\n"
                    "end\n"
                    "function update(self, dt)\n"
                    "    return true\n"
                    "end\n"
                    "function on_message(self, message_id, message, sender)\n"
                    "    return true\n"
                    "end\n"
                    "function on_input(self, action_id, action)\n"
                    "    return 1\n"
                    "end\n"
                    "function on_reload(self)\n"
                    "    return true\n"
                    "end";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_NE(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_NE(dmGui::RESULT_OK, r);
    char buffer[sizeof(dmMessage::Message) + sizeof(dmTestGuiDDF::AMessage)];
    dmMessage::Message* message = (dmMessage::Message*)buffer;
    message->m_Sender = dmMessage::URL();
    message->m_Receiver = dmMessage::URL();
    message->m_Id = 1;
    message->m_DataSize = 0;
    message->m_Descriptor = (uintptr_t)dmTestGuiDDF::AMessage::m_DDFDescriptor;
    message->m_Next = 0;
    dmTestGuiDDF::AMessage* data = (dmTestGuiDDF::AMessage*)message->m_Data;
    data->m_A = 0;
    data->m_B = 0;
    r = dmGui::DispatchMessage(m_Scene, message);
    ASSERT_NE(dmGui::RESULT_OK, r);
    dmGui::InputAction action;
    action.m_ActionId = 1;
    action.m_Value = 1.0f;
    bool consumed;
    r = dmGui::DispatchInput(m_Scene, &action, 1, &consumed);
    ASSERT_NE(dmGui::RESULT_OK, r);
    r = dmGui::FinalScene(m_Scene);
    ASSERT_NE(dmGui::RESULT_OK, r);
    dmGui::DeleteNode(m_Scene, node, true);
}

TEST_F(dmGuiTest, Picking)
{
    uint32_t physical_width = 640;
    uint32_t physical_height = 320;
    float ref_scale = 0.5f;
    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetDefaultResolution(m_Context, (uint32_t) (physical_width * ref_scale), (uint32_t) (physical_height * ref_scale));
    dmGui::SetSceneResolution(m_Scene, (uint32_t) (physical_width * ref_scale), (uint32_t) (physical_height * ref_scale));

    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);

    // Account for some loss in precision
    Vector3 tmin(EPSILON, EPSILON, 0);
    Vector3 tmax = size - tmin;
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, tmin.getX(), tmin.getY()));
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, tmin.getX(), tmax.getY()));
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, tmax.getX(), tmax.getY()));
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, tmax.getX(), tmin.getY()));
    ASSERT_FALSE(dmGui::PickNode(m_Scene, n1, ceil(size.getX() + 0.5f), size.getY()));

    dmGui::SetNodeProperty(m_Scene, n1, dmGui::PROPERTY_ROTATION, Vector4(0, 45, 0, 0));
    Vector3 ext(pos);
    ext.setX(ext.getX() * cosf((float) (M_PI * 0.25)));
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, pos.getX() + floor(ext.getX()), pos.getY()));
    ASSERT_FALSE(dmGui::PickNode(m_Scene, n1, pos.getX() + ceil(ext.getX()), pos.getY()));

    dmGui::SetNodeProperty(m_Scene, n1, dmGui::PROPERTY_ROTATION, Vector4(0, 90, 0, 0));
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, pos.getX(), pos.getY()));
    ASSERT_FALSE(dmGui::PickNode(m_Scene, n1, pos.getX() + 1.0f, pos.getY()));
}

TEST_F(dmGuiTest, PickingDisabledAdjust)
{
    uint32_t physical_width = 640;
    uint32_t physical_height = 320;
    float ref_scale = 0.5f;
    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetDefaultResolution(m_Context, (uint32_t) (physical_width * ref_scale), (uint32_t) (physical_height * ref_scale));
    dmGui::SetSceneResolution(m_Scene, (uint32_t) (physical_width * ref_scale), (uint32_t) (physical_height * ref_scale));
    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_DISABLED);

    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);

    // Account for some loss in precision
    Vector3 tmin(EPSILON, EPSILON, 0);
    Vector3 tmax = size - tmin;
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, pos.getX(), pos.getY()));
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, tmin.getX(), tmin.getY()));

    // If the physical window is doubled (as in our case), the visual gui elements are at
    // 50% of their original positions/sizes since we have disabled adjustments.
    ASSERT_FALSE(dmGui::PickNode(m_Scene, n1, tmin.getX(), tmax.getY()));
    ASSERT_TRUE(dmGui::PickNode(m_Scene, n1, tmin.getX()*ref_scale, tmax.getY()*ref_scale));
}

TEST_F(dmGuiTest, ScriptPicking)
{
    uint32_t physical_width = 640;
    uint32_t physical_height = 320;
    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);

    char buffer[1024];

    const char* s = "function init(self)\n"
                    "    local id = \"node_1\"\n"
                    "    local size = vmath.vector3(string.len(id) * %.2f, %.2f + %.2f, 0)\n"
                    "    local epsilon = %.6f\n"
                    "    local min = vmath.vector3(epsilon, epsilon, 0)\n"
                    "    local max = size - min\n"
                    "    local position = size * 0.5\n"
                    "    local n1 = gui.new_text_node(position, id)\n"
                    "    assert(gui.pick_node(n1, min.x, min.y))\n"
                    "    assert(gui.pick_node(n1, min.x, max.y))\n"
                    "    assert(gui.pick_node(n1, max.x, min.y))\n"
                    "    assert(gui.pick_node(n1, max.x, max.y))\n"
                    "    assert(not gui.pick_node(n1, size.x + 1, size.y))\n"
                    "end\n";

    sprintf(buffer, s, TEXT_GLYPH_WIDTH, TEXT_MAX_ASCENT, TEXT_MAX_DESCENT, EPSILON);
    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(buffer));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

::testing::AssertionResult IsEqual(const Vector4& v1, const Vector4& v2) {
    bool equal = v1.getX() == v2.getX() &&
                v1.getY() == v2.getY() &&
                v1.getZ() == v2.getZ() &&
                v1.getW() == v2.getW();
  if(equal)
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure() << "(" << v1.getX() << ", " << v1.getY() << ", " << v1.getZ() << ", " << v1.getW() << ") != (" << v2.getX() << ", " << v2.getY() << ", " << v2.getZ() << ", " << v2.getW() << ")";
}

TEST_F(dmGuiTest, CalculateNodeTransform)
{
    // Tests for a bug where the picking forces an update of the local transform
    // Root cause was children were updated before parents, making the adjust scale wrong
    uint32_t physical_width = 200;
    uint32_t physical_height = 100;
    float ref_scale_width = 0.25f;
    float ref_scale_height = 0.5f;
    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetDefaultResolution(m_Context, (uint32_t) (physical_width * ref_scale_width), (uint32_t) (physical_height * ref_scale_height));
    dmGui::SetSceneResolution(m_Scene, (uint32_t) (physical_width * ref_scale_width), (uint32_t) (physical_height * ref_scale_height));

    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_PARENT, dmGui::GetSceneAdjustReference(m_Scene));

    Point3 pos(10, 10, 0);
    Vector3 size(20, 20, 0);
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n1, 0x1);

    pos = Point3(20, 20, 0);
    size = Vector3(15, 15, 0);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n2, 0x2);

    pos = Point3(30, 30, 0);
    size = Vector3(10, 10, 0);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n3, 0x3);

    dmGui::SetNodeParent(m_Scene, n2, n1);
    dmGui::SetNodeParent(m_Scene, n3, n2);

    dmGui::InternalNode* nn1 = dmGui::GetNode(m_Scene, n1);
    dmGui::InternalNode* nn2 = dmGui::GetNode(m_Scene, n2);
    dmGui::InternalNode* nn3 = dmGui::GetNode(m_Scene, n3);

    Matrix4 transform;
    (void)transform;

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_STRETCH);

    dmGui::CalculateNodeTransform(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform);

    ASSERT_TRUE( IsEqual( Vector4(4, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale ) );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_FIT);

    dmGui::CalculateNodeTransform(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform);

    ASSERT_TRUE( IsEqual( Vector4(2, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(2, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(2, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale ) );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_ZOOM);

    dmGui::CalculateNodeTransform(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform);

    ASSERT_TRUE( IsEqual( Vector4(4, 4, 4, 1), nn1->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 4, 4, 1), nn2->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 4, 4, 1), nn3->m_Node.m_LocalAdjustScale ) );
}

TEST_F(dmGuiTest, CalculateNodeTransformCached)
{
    // Tests for the same bug as CalculateNodeTransform does, just in the sibling function CalculateNodeTransformAndAlphaCached
    uint32_t physical_width = 200;
    uint32_t physical_height = 100;
    float ref_scale_width = 0.25f;
    float ref_scale_height = 0.5f;
    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetDefaultResolution(m_Context, (uint32_t) (physical_width * ref_scale_width), (uint32_t) (physical_height * ref_scale_height));
    dmGui::SetSceneResolution(m_Scene, (uint32_t) (physical_width * ref_scale_width), (uint32_t) (physical_height * ref_scale_height));

    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_PARENT, dmGui::GetSceneAdjustReference(m_Scene));

    Point3 pos(10, 10, 0);
    Vector3 size(20, 20, 0);
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n1, 0x1);

    pos = Point3(20, 20, 0);
    size = Vector3(15, 15, 0);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n2, 0x2);

    pos = Point3(30, 30, 0);
    size = Vector3(10, 10, 0);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n3, 0x3);

    dmGui::SetNodeParent(m_Scene, n2, n1);
    dmGui::SetNodeParent(m_Scene, n3, n2);

    dmGui::InternalNode* nn1 = dmGui::GetNode(m_Scene, n1);
    dmGui::InternalNode* nn2 = dmGui::GetNode(m_Scene, n2);
    dmGui::InternalNode* nn3 = dmGui::GetNode(m_Scene, n3);

    Matrix4 transform;
    float opacity;
    (void)transform;
    (void)opacity;

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_STRETCH);

    dmGui::CalculateNodeTransformAndAlphaCached(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform, opacity);

    ASSERT_TRUE( IsEqual( Vector4(4, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale ) );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_FIT);

    dmGui::CalculateNodeTransformAndAlphaCached(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform, opacity);

    ASSERT_TRUE( IsEqual( Vector4(2, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(2, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(2, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale ) );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_ZOOM);

    dmGui::CalculateNodeTransformAndAlphaCached(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform, opacity);

    ASSERT_TRUE( IsEqual( Vector4(4, 4, 4, 1), nn1->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 4, 4, 1), nn2->m_Node.m_LocalAdjustScale ) );
    ASSERT_TRUE( IsEqual( Vector4(4, 4, 4, 1), nn3->m_Node.m_LocalAdjustScale ) );
}

// This render function simply flags a provided boolean when called
static void RenderEnabledNodes(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    if (node_count > 0)
    {
        bool* rendered = (bool*)context;
        *rendered = true;
    }
}

TEST_F(dmGuiTest, EnableDisable)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);

    // Initially enabled
    dmGui::InternalNode* node = dmGui::GetNode(m_Scene, n1);
    ASSERT_TRUE(node->m_Node.m_Enabled);

    // Test rendering
    bool rendered = false;
    dmGui::RenderScene(m_Scene, RenderEnabledNodes, &rendered);
    ASSERT_TRUE(rendered);

    // Test no rendering when disabled
    dmGui::SetNodeEnabled(m_Scene, n1, false);
    rendered = false;
    dmGui::RenderScene(m_Scene, RenderEnabledNodes, &rendered);
    ASSERT_FALSE(rendered);

    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_COLOR);
    dmGui::AnimateNodeHash(m_Scene, n1, property, Vector4(0.0f, 0.0f, 0.0f, 0.0f), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0.0f, 0x0, 0x0, 0x0);
    ASSERT_EQ(4U, m_Scene->m_Animations.Size());

    // Test no animation evaluation
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(0.0f, m_Scene->m_Animations[0].m_Elapsed);

    // Test animation evaluation when enabled
    dmGui::SetNodeEnabled(m_Scene, n1, true);
    dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_LT(0.0f, m_Scene->m_Animations[0].m_Elapsed);
}

TEST_F(dmGuiTest, ScriptEnableDisable)
{
    char buffer[512];
    const char* s = "function init(self)\n"
                    "    local id = \"node_1\"\n"
                    "    local size = vmath.vector3(string.len(id) * %.2f, %.2f + %.2f, 0)\n"
                    "    local position = size * 0.5\n"
                    "    self.n1 = gui.new_text_node(position, id)\n"
                    "    assert(gui.is_enabled(self.n1))\n"
                    "    gui.set_enabled(self.n1, false)\n"
                    "    assert(not gui.is_enabled(self.n1))\n"
                    "end\n";
    sprintf(buffer, s, TEXT_GLYPH_WIDTH, TEXT_MAX_ASCENT, TEXT_MAX_DESCENT);
    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(buffer));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    // Run init function
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    // Retrieve node
    dmGui::InternalNode* node = &m_Scene->m_Nodes[0];
    ASSERT_STREQ("node_1", node->m_Node.m_Text); // make sure we found the right one
    ASSERT_FALSE(node->m_Node.m_Enabled);
}

static void RenderNodesOrder(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    std::map<dmGui::HNode, uint16_t>* order = (std::map<dmGui::HNode, uint16_t>*)context;
    order->clear();
    for (uint32_t i = 0; i < node_count; ++i)
    {
        (*order)[nodes[i].m_Node] = (uint16_t)i;
    }
}

/**
 * Verify specific use cases of moving around nodes:
 * - single node (nop)
 *   - move to top
 *   - move to self (up)
 *   - move to bottom
 *   - move to self (down)
 * - two nodes
 *   - initial order
 *   - move to top
 *   - move explicit to top
 *   - move to bottom
 *   - move explicit to bottom
 * - three nodes
 *   - move to top
 *   - move from head to middle
 *   - move from middle to tail
 *   - move to bottom
 *   - move from tail to middle
 *   - move from middle to head
 */
TEST_F(dmGuiTest, MoveNodes)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    std::map<dmGui::HNode, uint16_t> order;

    // Edge case: single node
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to top
    dmGui::MoveNodeAbove(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to self
    dmGui::MoveNodeAbove(m_Scene, n1, n1);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to bottom
    dmGui::MoveNodeBelow(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to self
    dmGui::MoveNodeBelow(m_Scene, n1, n1);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);

    // Two nodes
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    // Move to top
    dmGui::MoveNodeAbove(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // Move explicit
    dmGui::MoveNodeAbove(m_Scene, n2, n1);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    // Move to bottom
    dmGui::MoveNodeBelow(m_Scene, n2, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // Move explicit
    dmGui::MoveNodeBelow(m_Scene, n1, n2);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);

    // Three nodes
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    // Move to top
    dmGui::MoveNodeAbove(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(2u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    ASSERT_EQ(1u, order[n3]);
    // Move explicit from head to middle
    dmGui::MoveNodeAbove(m_Scene, n2, n3);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(2u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(0u, order[n3]);
    // Move explicit from middle to tail
    dmGui::MoveNodeAbove(m_Scene, n2, n1);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(2u, order[n2]);
    ASSERT_EQ(0u, order[n3]);
    // Move to bottom
    dmGui::MoveNodeBelow(m_Scene, n2, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(2u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    ASSERT_EQ(1u, order[n3]);
    // Move explicit from tail to middle
    dmGui::MoveNodeBelow(m_Scene, n1, n3);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    // Move explicit from middle to head
    dmGui::MoveNodeBelow(m_Scene, n1, n2);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
}

TEST_F(dmGuiTest, MoveNodesScript)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    const char* id1 = "n1";
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n1, id1);
    const char* id2 = "n2";
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeId(m_Scene, n2, id2);
    const char* s = "function init(self)\n"
                    "    local n1 = gui.get_node(\"n1\")\n"
                    "    local n2 = gui.get_node(\"n2\")\n"
                    "    assert(gui.get_index(n1) == 0)\n"
                    "    assert(gui.get_index(n2) == 1)\n"
                    "    gui.move_above(n1, n2)\n"
                    "    assert(gui.get_index(n1) == 1)\n"
                    "    assert(gui.get_index(n2) == 0)\n"
                    "    gui.move_below(n1, n2)\n"
                    "    assert(gui.get_index(n1) == 0)\n"
                    "    assert(gui.get_index(n2) == 1)\n"
                    "end\n";
    ASSERT_TRUE(SetScript(m_Script, s));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
}

static void RenderNodesCount(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    uint32_t* count = (uint32_t*)context;
    *count = node_count;
}

static dmGui::HNode PickNode(dmGui::HScene scene, uint32_t* seed)
{
    const uint32_t max_it = 10;
    for (uint32_t i = 0; i < max_it; ++i)
    {
        uint32_t index = dmMath::Rand(seed) % scene->m_Nodes.Size();
        if (scene->m_Nodes[index].m_Index != dmGui::INVALID_INDEX)
        {
            return dmGui::GetNodeHandle(&scene->m_Nodes[index]);
        }
    }
    return dmGui::INVALID_HANDLE;
}

/**
 * Verify that the render count holds under random inserts, deletes and moves
 */
TEST_F(dmGuiTest, MoveNodesLoad)
{
    const uint32_t node_count = 100;
    const uint32_t iterations = 500;

    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = node_count * 2;
    params.m_MaxAnimations = MAX_ANIMATIONS;
    params.m_UserData = this;
    dmGui::HScene scene = dmGui::NewScene(m_Context, &params);

    for (uint32_t i = 0; i < node_count; ++i)
    {
        dmGui::NewNode(scene, pos, size, dmGui::NODE_TYPE_BOX);
    }
    uint32_t current_count = node_count;
    uint32_t render_count = 0;

    enum OpType {OP_ADD, OP_DELETE, OP_MOVE_ABOVE, OP_MOVE_BELOW, OP_TYPE_COUNT};

    uint32_t seed = 0;

    uint32_t min_node_count = node_count;
    uint32_t max_node_count = 0;
    uint32_t relative_move_count = 0;
    uint32_t absolute_move_count = 0;
    OpType op_type = OP_ADD;
    uint32_t op_count = 0;
    for (uint32_t i = 0; i < iterations; ++i)
    {
        if (op_count == 0)
        {
            op_type = (OpType)(dmMath::Rand(&seed) % OP_TYPE_COUNT);
            op_count = dmMath::Rand(&seed) % 10 + 1;
            if (op_type == OP_ADD || op_type == OP_DELETE)
            {
                int32_t diff = (int32_t)current_count - (int32_t)node_count;
                float t = dmMath::Min(1.0f, dmMath::Max(-1.0f, diff / (0.5f * node_count)));
                if (dmMath::Rand11(&seed) > t*t*t)
                {
                    op_type = OP_ADD;
                }
                else
                {
                    op_type = OP_DELETE;
                }
            }
        }
        --op_count;
        switch (op_type)
        {
        case OP_ADD:
            dmGui::NewNode(scene, pos, size, dmGui::NODE_TYPE_BOX);
            ++current_count;
            break;
        case OP_DELETE:
            {
                dmGui::HNode node = PickNode(scene, &seed);
                if (node != dmGui::INVALID_HANDLE)
                {
                    dmGui::DeleteNode(scene, node, true);
                    --current_count;
                }
            }
            break;
        case OP_MOVE_ABOVE:
        case OP_MOVE_BELOW:
            {
                dmGui::HNode source = PickNode(scene, &seed);
                if (source != dmGui::INVALID_HANDLE)
                {
                    dmGui::HNode target = dmGui::INVALID_HANDLE;
                    if (dmMath::Rand01(&seed) < 0.8f)
                        target = PickNode(scene, &seed);
                    if (op_type == OP_MOVE_ABOVE)
                    {
                        dmGui::MoveNodeAbove(scene, source, target);
                    }
                    else
                    {
                        dmGui::MoveNodeBelow(scene, source, target);
                    }
                    if (target != dmGui::INVALID_HANDLE)
                    {
                        ++relative_move_count;
                    }
                    else
                    {
                        ++absolute_move_count;
                    }
                }
            }
            break;
        default:
            break;
        }
        dmGui::RenderScene(scene, RenderNodesCount, &render_count);
        ASSERT_EQ(current_count, render_count);
        if (min_node_count > current_count)
            min_node_count = current_count;
        if (max_node_count < current_count)
            max_node_count = current_count;
    }
    printf("[STATS] current: %03d min: %03d max: %03d rel: %03d abs: %03d\n", current_count, min_node_count, max_node_count, relative_move_count, absolute_move_count);

    dmGui::DeleteScene(scene);
}

/**
 * Verify specific use cases of parenting nodes:
 * - single node (nop)
 *   - parent to nil
 *   - parent to self
 * - two nodes
 *   - initial order
 *   - parent first to second
 *   - parent second to first
 *   - unparent first
 *   - parent second to first
 * - three nodes
 *   - initial order
 *   - parent second to third
 */
TEST_F(dmGuiTest, Parenting)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    std::map<dmGui::HNode, uint16_t> order;

    // Edge case: single node
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    // parent to nil
    dmGui::SetNodeParent(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    // parent to self
    dmGui::SetNodeParent(m_Scene, n1, n1);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);

    // Two nodes
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    // parent first to second
    dmGui::SetNodeParent(m_Scene, n1, n2);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // parent second to first
    dmGui::SetNodeParent(m_Scene, n2, n1);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // unparent first
    dmGui::SetNodeParent(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // parent second to first
    dmGui::SetNodeParent(m_Scene, n2, n1);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);

    // Three nodes
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    // parent second to third
    dmGui::SetNodeParent(m_Scene, n2, n3);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(2u, order[n2]);
    ASSERT_EQ(1u, order[n3]);
}

void RenderNodesStoreTransform(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    Vectormath::Aos::Matrix4* out_transforms = (Vectormath::Aos::Matrix4*)context;
    memcpy(out_transforms, node_transforms, sizeof(Vectormath::Aos::Matrix4) * node_count);
}

#define ASSERT_MAT4(m1, m2)\
    for (uint32_t i = 0; i < 16; ++i)\
    {\
        int row = i / 4;\
        int col = i % 4;\
        ASSERT_NEAR(m1.getElem(row, col), m2.getElem(row, col), EPSILON);\
    }

/**
 * Verify that the rendered transforms are correct with VectorMath library as a reference
 * n1 == Vectormath::Aos::Matrix4
 */
TEST_F(dmGuiTest, NodeTransform)
{
    Vector3 size(1.0f, 1.0f, 1.0f);
    Vector3 pos(0.25f, 0.5f, 0.75f);
    Vectormath::Aos::Matrix4 transforms[1];
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(pos), size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodePivot(m_Scene, n1, dmGui::PIVOT_SW);

    Vectormath::Aos::Matrix4 ref_mat;
    ref_mat = Vectormath::Aos::Matrix4::identity();
    ref_mat.setTranslation(pos);
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, transforms);
    ASSERT_MAT4(transforms[0], ref_mat);

    const float radians = 90.0f * M_PI / 180.0f;
    ref_mat *= Vectormath::Aos::Matrix4::rotation(radians * 0.50f, Vector3(0.0f, 1.0f, 0.0f));
    ref_mat *= Vectormath::Aos::Matrix4::rotation(radians * 1.00f, Vector3(0.0f, 0.0f, 1.0f));
    ref_mat *= Vectormath::Aos::Matrix4::rotation(radians * 0.25f, Vector3(1.0f, 0.0f, 0.0f));
    dmGui::SetNodeProperty(m_Scene, n1, dmGui::PROPERTY_ROTATION, Vector4(90.0f*0.25f, 90.0f*0.5f, 90.0f, 0.0f));
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, transforms);
    ASSERT_MAT4(transforms[0], ref_mat);

    ref_mat *= Vectormath::Aos::Matrix4::scale(Vector3(0.25f, 0.5f, 0.75f));
    dmGui::SetNodeProperty(m_Scene, n1, dmGui::PROPERTY_SCALE, Vector4(0.25f, 0.5f, 0.75f, 1.0f));
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, transforms);
    ASSERT_MAT4(transforms[0], ref_mat);
}

/**
 * Verify that the rendered transforms are correct for a hierarchy:
 * - n1
 *   - n2
 *     - n3
 *
 * In three cases, the nodes have different pivots and positions, so that their render transforms will be identical:
 * - n1 center, n2 center, n3 center
 * - n1 south-west, n2 center, n3 south-west
 * - n1 west, n2 east, n3 west
 */
TEST_F(dmGuiTest, HierarchicalTransforms)
{
    // Setup
    Vector3 size(1, 1, 0);

    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX);
    // parent first to second, second to third
    dmGui::SetNodeParent(m_Scene, n3, n2);
    dmGui::SetNodeParent(m_Scene, n2, n1);

    Vectormath::Aos::Matrix4 transforms[3];

    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, transforms);
    ASSERT_MAT4(transforms[0], transforms[1]);
    ASSERT_MAT4(transforms[0], transforms[2]);

    dmGui::SetNodePivot(m_Scene, n1, dmGui::PIVOT_SW);
    dmGui::SetNodePosition(m_Scene, n2, Point3(size * 0.5f));
    dmGui::SetNodePivot(m_Scene, n3, dmGui::PIVOT_SW);
    dmGui::SetNodePosition(m_Scene, n3, Point3(-size * 0.5f));
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, transforms);
    ASSERT_MAT4(transforms[0], transforms[1]);
    ASSERT_MAT4(transforms[0], transforms[2]);

    dmGui::SetNodePivot(m_Scene, n1, dmGui::PIVOT_W);
    dmGui::SetNodePivot(m_Scene, n2, dmGui::PIVOT_E);
    dmGui::SetNodePosition(m_Scene, n2, Point3(size.getX(), 0.0f, 0.0f));
    dmGui::SetNodePivot(m_Scene, n3, dmGui::PIVOT_W);
    dmGui::SetNodePosition(m_Scene, n3, Point3(-size.getY(), 0.0f, 0.0f));
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, transforms);
    ASSERT_MAT4(transforms[0], transforms[1]);
    ASSERT_MAT4(transforms[0], transforms[2]);
}

#undef ASSERT_MAT4

struct TransformColorData
{
    Vectormath::Aos::Matrix4 m_Transform;
    float m_Opacity;
};

void RenderNodesStoreOpacityAndTransform(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    TransformColorData* out_data = (TransformColorData*) context;
    for(uint32_t i = 0; i < node_count; i++)
    {
        out_data[i].m_Transform = node_transforms[i];
        out_data[i].m_Opacity = node_opacities[i];
    }
}

/**
 * Verify that the rendered colors are correct for a hierarchy:
 * - n1
 *   - n2
 *   - n3
 * - n4
 *   - n5
 *     - n6
 *
 */

TEST_F(dmGuiTest, HierarchicalColors)
{
    Vector3 size(1, 1, 0);

    dmGui::HNode node[6];
    const size_t node_count = sizeof(node)/sizeof(dmGui::HNode);

    for(uint32_t i = 0; i < node_count; ++i) {
        node[i] = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX);
        dmGui::SetNodeInheritAlpha(m_Scene, node[i], true);
    }

    // test siblings
    dmGui::SetNodeParent(m_Scene, node[1], node[0]);
    dmGui::SetNodeParent(m_Scene, node[2], node[0]);
    dmGui::SetNodeProperty(m_Scene, node[0], dmGui::PROPERTY_COLOR, Vector4(0.5f, 0.5f, 0.5f, 0.5f));
    dmGui::SetNodeProperty(m_Scene, node[1], dmGui::PROPERTY_COLOR, Vector4(1.0f, 0.5f, 1.0f, 0.5f));
    dmGui::SetNodeProperty(m_Scene, node[2], dmGui::PROPERTY_COLOR, Vector4(1.0f, 1.0f, 1.0f, 0.25f));

    // test child tree
    dmGui::SetNodeParent(m_Scene, node[4], node[3]);
    dmGui::SetNodeParent(m_Scene, node[5], node[4]);
    dmGui::SetNodeProperty(m_Scene, node[3], dmGui::PROPERTY_COLOR, Vector4(0.5f, 0.5f, 0.5f, 0.5f));
    dmGui::SetNodeProperty(m_Scene, node[4], dmGui::PROPERTY_COLOR, Vector4(1.0f, 0.5f, 1.0f, 0.5f));
    dmGui::SetNodeProperty(m_Scene, node[5], dmGui::PROPERTY_COLOR, Vector4(1.0f, 1.0f, 1.0f, 0.25f));

    dmGui::SetNodeProperty(m_Scene, node[3], dmGui::PROPERTY_OUTLINE, Vector4(0.3f, 0.3f, 0.3f, 0.8f));
    dmGui::SetNodeProperty(m_Scene, node[4], dmGui::PROPERTY_OUTLINE, Vector4(0.4f, 0.4f, 0.4f, 0.6f));
    dmGui::SetNodeProperty(m_Scene, node[5], dmGui::PROPERTY_OUTLINE, Vector4(0.5f, 0.5f, 0.5f, 0.4f));

    dmGui::SetNodeProperty(m_Scene, node[3], dmGui::PROPERTY_SHADOW, Vector4(0.3f, 0.3f, 0.3f, 0.6f));
    dmGui::SetNodeProperty(m_Scene, node[4], dmGui::PROPERTY_SHADOW, Vector4(0.4f, 0.4f, 0.4f, 0.4f));
    dmGui::SetNodeProperty(m_Scene, node[5], dmGui::PROPERTY_SHADOW, Vector4(0.5f, 0.5f, 0.5f, 0.2f));

    TransformColorData cbres[node_count];
    dmGui::RenderScene(m_Scene, RenderNodesStoreOpacityAndTransform, &cbres);

    // siblings
    ASSERT_EQ(0.5000f, cbres[0].m_Opacity);
    ASSERT_EQ(0.2500f, cbres[1].m_Opacity);
    ASSERT_EQ(0.1250f, cbres[2].m_Opacity);

    // tree
    ASSERT_EQ(0.5000f, cbres[3].m_Opacity);
    ASSERT_EQ(0.2500f, cbres[4].m_Opacity);
    ASSERT_EQ(0.0625f, cbres[5].m_Opacity);
}

/**
 * Test coherence of dmGui::RenderScene internal node-cache by adding, deleting nodes and altering node
 * properties in two passes of rendering
 *
 * - n1
 *   - n2
 *     - n3
 *       - n4
 * - n5
 *   - n6
 *     - n7
 *       - n8
 *
 * Render
 * Change color and transform properties of n5-n8, delete n3, n4
 * Render
 *
 */
TEST_F(dmGuiTest, SceneTransformCacheCoherence)
{
    Vector3 size(1, 1, 0);

    dmGui::HNode node[8];
    const size_t node_count = sizeof(node)/sizeof(dmGui::HNode);
    const size_t node_count_h = node_count/2;
    dmGui::HNode dummy_node[node_count];

    for(uint32_t i = 0; i < node_count; ++i)
    {
        dummy_node[i] = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX);
    }

    float a;
    a = 1.0f;
    for(uint32_t i = 0; i < node_count_h; ++i)
    {
        node[i] = dmGui::NewNode(m_Scene, Point3(1.0f, 1.0f, 1.0f), size, dmGui::NODE_TYPE_BOX);
        dmGui::SetNodeInheritAlpha(m_Scene, node[i], true);
        dmGui::SetNodePivot(m_Scene, node[i], dmGui::PIVOT_SW);
        dmGui::SetNodeProperty(m_Scene, node[i], dmGui::PROPERTY_COLOR, Vector4(1, 1, 1, a));
        if(i == 0)
            a = 0.5f;
    }
    a = 0.5f;
    for(uint32_t i = node_count_h; i < node_count; ++i)
    {
        node[i] = dmGui::NewNode(m_Scene, Point3(0.5f, 0.5f, 0.5f), size, dmGui::NODE_TYPE_BOX);
        dmGui::SetNodeInheritAlpha(m_Scene, node[i], true);
        dmGui::SetNodePivot(m_Scene, node[i], dmGui::PIVOT_SW);
        dmGui::SetNodeProperty(m_Scene, node[i], dmGui::PROPERTY_COLOR, Vector4(1, 1, 1, a));
        if(i == node_count_h)
            a = 0.5f;
    }
    for(uint32_t i = 1; i < node_count_h; ++i)
    {
        dmGui::SetNodeParent(m_Scene, node[i], node[i-1]);
        dmGui::SetNodeParent(m_Scene, node[i+(node_count_h)], node[(i+(node_count_h))-1]);
    }

    for(uint32_t i = 0; i < node_count; ++i)
    {
        DeleteNode(m_Scene, dummy_node[i], true);
    }

    TransformColorData cbres[node_count];
    memset(cbres, 0x0, sizeof(TransformColorData)*node_count);
    dmGui::RenderScene(m_Scene, RenderNodesStoreOpacityAndTransform, &cbres);

    a = 1.0f;
    for(uint32_t i = 0; i < node_count_h; ++i)
    {
        if(i > 0)
        {
            for(uint32_t e = 0; e < 3; e++)
                ASSERT_NEAR(cbres[i].m_Transform.getTranslation().getElem(e), cbres[i-1].m_Transform.getTranslation().getElem(e)+1.0f, EPSILON);
        }
        ASSERT_EQ(a, cbres[i].m_Opacity);
        a *= 0.5f;
    }
    a = 0.5f;
    for(uint32_t i = node_count_h; i < node_count; ++i)
    {
        if(i > node_count_h)
        {
            for(uint32_t e = 0; e < 3; e++)
                ASSERT_NEAR(cbres[i].m_Transform.getTranslation().getElem(e), cbres[i-1].m_Transform.getTranslation().getElem(e)+0.5f, EPSILON);
        }
        ASSERT_EQ(a, cbres[i].m_Opacity);
        a *= 0.5f;
    }

    a = 1.0f;
    for(uint32_t i = node_count_h; i < node_count; ++i)
    {
        dmGui::SetNodeProperty(m_Scene, node[i], dmGui::PROPERTY_COLOR, Vector4(1, 1, 1, a));
        dmGui::SetNodePosition(m_Scene, node[i], Point3(0.25f, 0.25f, 0.25f));
        if(i == node_count_h)
            a = 0.25f;
    }

    dmGui::DeleteNode(m_Scene, node[3], true);
    dmGui::DeleteNode(m_Scene, node[2], true);
    dmGui::RenderScene(m_Scene, RenderNodesStoreOpacityAndTransform, &cbres);

    a = 1.0f;
    for(uint32_t i = 0; i < node_count_h-2; ++i)
    {
        if(i > 0)
        {
            for(uint32_t e = 0; e < 3; e++)
                ASSERT_NEAR(cbres[i].m_Transform.getTranslation().getElem(e), cbres[i-1].m_Transform.getTranslation().getElem(e)+1.0f, EPSILON);
        }
        ASSERT_EQ(a, cbres[i].m_Opacity);
        a *= 0.5f;
    }
    a = 1.0f;
    for(uint32_t i = node_count_h-2; i < node_count-2; ++i)
    {
        if(i > node_count_h-2)
        {
            for(uint32_t e = 0; e < 3; e++)
                ASSERT_NEAR(cbres[i].m_Transform.getTranslation().getElem(e), cbres[i-1].m_Transform.getTranslation().getElem(e)+0.25f, EPSILON);
        }
        ASSERT_EQ(a, cbres[i].m_Opacity);
        a *= 0.25f;
    }
}

TEST_F(dmGuiTest, ScriptClippingFunctions)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,0), dmGui::NODE_TYPE_BOX);
    ASSERT_NE((dmGui::HNode) 0, node);
    dmGui::SetNodeId(m_Scene, node, "clipping_node");
    dmGui::HNode get_node = dmGui::GetNodeById(m_Scene, "clipping_node");
    ASSERT_EQ(node, get_node);

    const char* s = "function init(self)\n"
                    "    local n = gui.get_node(\"clipping_node\")\n"
                    "    local mode = gui.get_clipping_mode(n)\n"
                    "    assert(mode == gui.CLIPPING_MODE_NONE)\n"
                    "    gui.set_clipping_mode(n, gui.CLIPPING_MODE_STENCIL)\n"
                    "    mode = gui.get_clipping_mode(n)\n"
                    "    assert(mode == gui.CLIPPING_MODE_STENCIL)\n"
                    "    assert(gui.get_clipping_visible(n) == true)\n"
                    "    gui.set_clipping_visible(n, false)\n"
                    "    assert(gui.get_clipping_visible(n) == false)\n"
                    "    assert(gui.get_clipping_inverted(n) == false)\n"
                    "    gui.set_clipping_inverted(n, true)\n"
                    "    assert(gui.get_clipping_inverted(n) == true)\n"
                    "end\n";

    ASSERT_TRUE(SetScript(m_Script, s));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::InitScene(m_Scene));
}

/**
 * Verify layer rendering order.
 * Hierarchy:
 * - n1 (l1)
 * - n2
 */
TEST_F(dmGuiTest, LayerRendering)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    dmGui::AddLayer(m_Scene, "l1");

    std::map<dmGui::HNode, uint16_t> order;

    // Initial case
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);

    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);

    // Reverse
    dmGui::SetNodeLayer(m_Scene, n1, "l1");

    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
}

/**
 * Verify layer rendering order.
 * Hierarchy:
 * - n1 (l1)
 *   - n2
 * - n3 (l2)
 *   - n4
 * Layers:
 * - l1
 * - l2
 *
 * - initial order: n1, n2, n3, n4
 * - reverse layer order: n3, n4, n1, n2
 */
TEST_F(dmGuiTest, LayerRenderingHierarchies)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    dmGui::AddLayer(m_Scene, "l1");
    dmGui::AddLayer(m_Scene, "l2");

    std::map<dmGui::HNode, uint16_t> order;

    // Initial case
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeLayer(m_Scene, n1, "l1");
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeParent(m_Scene, n2, n1);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeLayer(m_Scene, n3, "l2");
    dmGui::HNode n4 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeParent(m_Scene, n4, n3);
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    ASSERT_EQ(3u, order[n4]);

    // Reverse
    dmGui::SetNodeLayer(m_Scene, n1, "l2");
    dmGui::SetNodeLayer(m_Scene, n3, "l1");
    dmGui::RenderScene(m_Scene, RenderNodesOrder, &order);
    ASSERT_EQ(2u, order[n1]);
    ASSERT_EQ(3u, order[n2]);
    ASSERT_EQ(0u, order[n3]);
    ASSERT_EQ(1u, order[n4]);
}

TEST_F(dmGuiTest, NoRenderOfDisabledTree)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    uint32_t count;

    // Edge case: single node
    dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::HNode parent = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::HNode child = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeParent(m_Scene, child, parent);
    dmGui::RenderScene(m_Scene, RenderNodesCount, &count);
    ASSERT_EQ(3u, count);

    dmGui::SetNodeEnabled(m_Scene, parent, false);
    dmGui::RenderScene(m_Scene, RenderNodesCount, &count);

    ASSERT_EQ(1u, count);
}

TEST_F(dmGuiTest, DeleteTree)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    uint32_t count;

    dmGui::HNode parent = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);
    dmGui::HNode child = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);

    dmGui::SetNodeParent(m_Scene, child, parent);
    dmGui::RenderScene(m_Scene, RenderNodesCount, &count);
    ASSERT_EQ(2u, count);

    dmGui::DeleteNode(m_Scene, parent, true);
    dmGui::RenderScene(m_Scene, RenderNodesCount, &count);
    ASSERT_EQ(0u, count);
    ASSERT_EQ(m_Scene->m_NodePool.Remaining(), m_Scene->m_NodePool.Capacity());
}

TEST_F(dmGuiTest, PhysResUpdatesTransform)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size);

    dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX);

    Matrix4 transform;
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, &transform);

    Matrix4 next_transform;
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, &next_transform);

    Vector4 p = transform.getCol3();
    Vector4 next_p = next_transform.getCol3();
    ASSERT_LT(lengthSqr(p - next_p), EPSILON);

    dmGui::SetPhysicalResolution(m_Context, 10, 10);
    dmGui::SetSceneResolution(m_Scene, 1, 1);
    dmGui::RenderScene(m_Scene, RenderNodesStoreTransform, &next_transform);

    next_p = next_transform.getCol3();
    ASSERT_GT(lengthSqr(p - next_p), EPSILON);
}

TEST_F(dmGuiTest, NewDeleteScene)
{
    dmGui::NewSceneParams params;
    dmGui::HScene scene2 = dmGui::NewScene(m_Context, &params);

    ASSERT_EQ(2u, m_Context->m_Scenes.Size());

    dmGui::DeleteScene(scene2);

    ASSERT_EQ(1u, m_Context->m_Scenes.Size());
}

TEST_F(dmGuiTest, AdjustReference)
{
    uint32_t width = 100;
    uint32_t height = 50;

    uint32_t physical_width = 200;
    uint32_t physical_height = 50;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    ASSERT_EQ(dmGui::ADJUST_REFERENCE_LEGACY, dmGui::GetSceneAdjustReference(m_Scene));
    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_PARENT, dmGui::GetSceneAdjustReference(m_Scene));

    // create nodes
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_FIT);

    dmGui::SetNodeParent(m_Scene, node_level1, node_level0);

    // update
    dmGui::RenderScene(m_Scene, &RenderNodes, this);


    /*
        before resize:
        a----------------------+
        |                      |
        |  b---------------+   |
        |  |[c]  <-80->    |   |
        |10+---------------+   |
        | 10                   |
        +----------------------+

        after resize:
        a---------------------------------------------+
        |                                             |
        |    b----------------------------------+     |
        |    |[c]           <-160->             |     |
        | 20 +----------------------------------+     |
        |   10                                        |
        +---------------------------------------------+


        a: window (ADJUST_REFERENCE_PARENT)
        b: node_level_0 (ADJUST_MOD_STRETCH)
        c: node_level_1 (parent: b, ADJUST_MODE_FIT)

        => node c should not resize or offset inside b

     */


    Point3 node_level0_p = m_NodeTextToRenderedPosition["node_level0"];
    Point3 node_level1_p = m_NodeTextToRenderedPosition["node_level1"];
    Vector3 node_level0_s = m_NodeTextToRenderedSize["node_level0"];
    Vector3 node_level1_s = m_NodeTextToRenderedSize["node_level1"];

    ASSERT_EQ( 20, node_level0_p.getX());
    ASSERT_EQ( 10, node_level0_p.getY());

    ASSERT_EQ(160, node_level0_s.getX());
    ASSERT_EQ( 30, node_level0_s.getY());

    ASSERT_EQ( 10, node_level1_s.getX());
    ASSERT_EQ( 10, node_level1_s.getY());

    ASSERT_EQ( 40, node_level1_p.getX());
    ASSERT_EQ( 20, node_level1_p.getY());

    // clean up
    dmGui::DeleteNode(m_Scene, node_level1, true);
    dmGui::DeleteNode(m_Scene, node_level0, true);

}

TEST_F(dmGuiTest, AdjustReferenceDisabled)
{
    uint32_t width = 100;
    uint32_t height = 50;

    uint32_t physical_width = 200;
    uint32_t physical_height = 50;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    ASSERT_EQ(dmGui::ADJUST_REFERENCE_LEGACY, dmGui::GetSceneAdjustReference(m_Scene));
    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_DISABLED);
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_DISABLED, dmGui::GetSceneAdjustReference(m_Scene));

    // create nodes
    dmGui::HNode node_levelB = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_levelB, "node_levelB");
    dmGui::SetNodePivot(m_Scene, node_levelB, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_levelB, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_levelB, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_levelB, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_levelC = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_levelC, "node_levelC");
    dmGui::SetNodePivot(m_Scene, node_levelC, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_levelC, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_levelC, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_levelC, dmGui::ADJUST_MODE_FIT);

    dmGui::SetNodeParent(m_Scene, node_levelC, node_levelB);

    // update
    dmGui::RenderScene(m_Scene, &RenderNodes, this);


    /*
        before resize:
        A----------------------+
        |                      |
        |  B---------------+   |
        |  |[C]  <-80->    |   |
        |10+---------------+   |
        | 10                   |
        +----------------------+

        after resize:
        A---------------------------------------------+
        |                                             |
        |  B---------------+                          |
        |  |[C]  <-80->    |                          |
        |10+---------------+                          |
        |   10                                        |
        +---------------------------------------------+


        A: window (ADJUST_REFERENCE_DISABLED)
        B: node_levelB (ADJUST_MOD_STRETCH)
        C: node_levelC (parent: B, ADJUST_MODE_FIT)

        => neither node B nor C should resize, since we have disabled automatic adjustments

     */


    Point3 node_levelB_p = m_NodeTextToRenderedPosition["node_levelB"];
    Point3 node_levelC_p = m_NodeTextToRenderedPosition["node_levelC"];
    Vector3 node_levelB_s = m_NodeTextToRenderedSize["node_levelB"];
    Vector3 node_levelC_s = m_NodeTextToRenderedSize["node_levelC"];

    ASSERT_EQ( 10, node_levelB_p.getX());
    ASSERT_EQ( 10, node_levelB_p.getY());

    ASSERT_EQ( 80, node_levelB_s.getX());
    ASSERT_EQ( 30, node_levelB_s.getY());

    ASSERT_EQ( 10, node_levelC_s.getX());
    ASSERT_EQ( 10, node_levelC_s.getY());

    ASSERT_EQ( 20, node_levelC_p.getX());
    ASSERT_EQ( 20, node_levelC_p.getY());

    // clean up
    dmGui::DeleteNode(m_Scene, node_levelC, true);
    dmGui::DeleteNode(m_Scene, node_levelB, true);

}

TEST_F(dmGuiTest, AdjustReferenceMultiLevel)
{
    uint32_t width = 100;
    uint32_t height = 50;

    uint32_t physical_width = 200;
    uint32_t physical_height = 50;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    ASSERT_EQ(dmGui::ADJUST_REFERENCE_LEGACY, dmGui::GetSceneAdjustReference(m_Scene));
    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_PARENT, dmGui::GetSceneAdjustReference(m_Scene));

    // create nodes
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeParent(m_Scene, node_level1, node_level0);

    dmGui::HNode node_level2 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level2, "node_level2");
    dmGui::SetNodePivot(m_Scene, node_level2, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level2, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level2, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeParent(m_Scene, node_level2, node_level1);

    // update
    dmGui::RenderScene(m_Scene, &RenderNodes, this);

    Point3 node_level0_p = m_NodeTextToRenderedPosition["node_level0"];
    Point3 node_level1_p = m_NodeTextToRenderedPosition["node_level1"];
    Point3 node_level2_p = m_NodeTextToRenderedPosition["node_level2"];
    Vector3 node_level0_s = m_NodeTextToRenderedSize["node_level0"];
    Vector3 node_level1_s = m_NodeTextToRenderedSize["node_level1"];
    Vector3 node_level2_s = m_NodeTextToRenderedSize["node_level2"];

    ASSERT_EQ(  0, node_level0_p.getX());
    ASSERT_EQ(  0, node_level0_p.getY());

    ASSERT_EQ(200, node_level0_s.getX());
    ASSERT_EQ( 50, node_level0_s.getY());

    ASSERT_EQ( 20, node_level1_p.getX());
    ASSERT_EQ( 10, node_level1_p.getY());

    ASSERT_EQ(160, node_level1_s.getX());
    ASSERT_EQ( 30, node_level1_s.getY());

    ASSERT_EQ( 10, node_level2_s.getX());
    ASSERT_EQ( 10, node_level2_s.getY());

    ASSERT_EQ( 40, node_level2_p.getX());
    ASSERT_EQ( 20, node_level2_p.getY());

    // clean up
    dmGui::DeleteNode(m_Scene, node_level2, true);
    dmGui::DeleteNode(m_Scene, node_level1, true);
    dmGui::DeleteNode(m_Scene, node_level0, true);

}

TEST_F(dmGuiTest, AdjustReferenceOffset)
{
    uint32_t width = 100;
    uint32_t height = 50;

    uint32_t physical_width = 10;
    uint32_t physical_height = 50;

    dmGui::SetDefaultResolution(m_Context, width, height);
    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    ASSERT_EQ(dmGui::ADJUST_REFERENCE_LEGACY, dmGui::GetSceneAdjustReference(m_Scene));
    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_PARENT, dmGui::GetSceneAdjustReference(m_Scene));

    // create nodes
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(50.0f, 25.0f, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_CENTER);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(50, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_CENTER);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeParent(m_Scene, node_level1, node_level0);

    dmGui::HNode node_level2 = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level2, "node_level2");
    dmGui::SetNodePivot(m_Scene, node_level2, dmGui::PIVOT_CENTER);
    dmGui::SetNodeXAnchor(m_Scene, node_level2, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, node_level2, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, node_level2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeParent(m_Scene, node_level2, node_level1);

    // update
    dmGui::RenderScene(m_Scene, &RenderNodes, this);

    // get render positions and sizes
    Point3 node_level0_p = m_NodeTextToRenderedPosition["node_level0"];
    Point3 node_level1_p = m_NodeTextToRenderedPosition["node_level1"];
    Point3 node_level2_p = m_NodeTextToRenderedPosition["node_level2"];
    Vector3 node_level0_s = m_NodeTextToRenderedSize["node_level0"];
    Vector3 node_level1_s = m_NodeTextToRenderedSize["node_level1"];
    Vector3 node_level2_s = m_NodeTextToRenderedSize["node_level2"];

    Vector4 screen_origo = Vector4(5.0f, 25.0f, 0.0f, 0.0f);

    // validate
    ASSERT_EQ( screen_origo.getX(), node_level0_p.getX() + node_level0_s.getX() / 2.0f);
    ASSERT_EQ( screen_origo.getY(), node_level0_p.getY() + node_level0_s.getY() / 2.0f);

    ASSERT_EQ( 10.0f, node_level0_s.getX());
    ASSERT_EQ( 50.0f, node_level0_s.getY());

    ASSERT_EQ( screen_origo.getX(), node_level1_p.getX() + node_level1_s.getX() / 2.0f);
    ASSERT_EQ( screen_origo.getY(), node_level1_p.getY() + node_level1_s.getY() / 2.0f);

    ASSERT_EQ( 5.0f, node_level1_s.getX());
    ASSERT_EQ( 5.0f, node_level1_s.getY());

    ASSERT_EQ( screen_origo.getX(), node_level2_p.getX() + node_level2_s.getX() / 2.0f);
    ASSERT_EQ( screen_origo.getY(), node_level2_p.getY() + node_level2_s.getY() / 2.0f);

    ASSERT_EQ( 1.0f, node_level2_s.getX());
    ASSERT_EQ( 1.0f, node_level2_s.getY());

    // clean up
    dmGui::DeleteNode(m_Scene, node_level2, true);
    dmGui::DeleteNode(m_Scene, node_level1, true);
    dmGui::DeleteNode(m_Scene, node_level0, true);
    dmGui::ClearFonts(m_Scene);

}

TEST_F(dmGuiTest, AdjustReferenceAnchoring)
{
    uint32_t width = 1024;
    uint32_t height = 768;

    uint32_t physical_width = 640;
    uint32_t physical_height = 320;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);
    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);

    Vector4 ref_scale = dmGui::CalculateReferenceScale(m_Scene, 0);

    dmGui::HNode root_node = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0), Vector3(1024, 768, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, root_node, "root_node");
    dmGui::SetNodePivot(m_Scene, root_node, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, root_node, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, root_node, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, root_node, dmGui::ADJUST_MODE_STRETCH);

    const char* n1_name = "n1";
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, n1, n1_name);
    dmGui::SetNodeXAnchor(m_Scene, n1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, n1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeParent(m_Scene, n1, root_node);

    const char* n2_name = "n2";
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(width - 10.0f, height - 10.0f, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, n2, n2_name);
    dmGui::SetNodeXAnchor(m_Scene, n2, dmGui::XANCHOR_RIGHT);
    dmGui::SetNodeYAnchor(m_Scene, n2, dmGui::YANCHOR_TOP);
    dmGui::SetNodeParent(m_Scene, n2, root_node);

    dmGui::RenderScene(m_Scene, &RenderNodes, this);

    Point3 pos1 = m_NodeTextToRenderedPosition[n1_name] + m_NodeTextToRenderedSize[n1_name] * 0.5f;
    const float EPSILON = 0.0001f;
    ASSERT_NEAR(10 * ref_scale.getX(), pos1.getX(), EPSILON);
    ASSERT_NEAR(10 * ref_scale.getY(), pos1.getY(), EPSILON);

    Point3 pos2 = m_NodeTextToRenderedPosition[n2_name] + m_NodeTextToRenderedSize[n2_name] * 0.5f;
    ASSERT_NEAR(physical_width - 10 * ref_scale.getX(), pos2.getX(), EPSILON);
    ASSERT_NEAR(physical_height - 10 * ref_scale.getY(), pos2.getY(), EPSILON);
}

TEST_F(dmGuiTest, AdjustReferenceScaled)
{
    uint32_t width = 100;
    uint32_t height = 50;

    uint32_t physical_width = 200;
    uint32_t physical_height = 50;

    dmGui::SetPhysicalResolution(m_Context, physical_width, physical_height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    ASSERT_EQ(dmGui::ADJUST_REFERENCE_LEGACY, dmGui::GetSceneAdjustReference(m_Scene));
    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);
    ASSERT_EQ(dmGui::ADJUST_REFERENCE_PARENT, dmGui::GetSceneAdjustReference(m_Scene));

    // create nodes
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_FIT);

    dmGui::SetNodeParent(m_Scene, node_level1, node_level0);

    // We scale node_level0 (root node) to half of its height.
    // => This means that the child (node_level1) will also be half in height,
    //    still obeying the adjust mode related scaling of the parent.
    dmGui::SetNodeProperty(m_Scene, node_level0, dmGui::PROPERTY_SCALE, Vector4(1.0f, 0.5f, 1.0f, 1.0f));

    // update
    dmGui::RenderScene(m_Scene, &RenderNodes, this);

    Point3 node_level0_p = m_NodeTextToRenderedPosition["node_level0"];
    Point3 node_level1_p = m_NodeTextToRenderedPosition["node_level1"];
    Vector3 node_level0_s = m_NodeTextToRenderedSize["node_level0"];
    Vector3 node_level1_s = m_NodeTextToRenderedSize["node_level1"];

    ASSERT_EQ( 20, node_level0_p.getX());
    ASSERT_EQ( 10, node_level0_p.getY());

    ASSERT_EQ(160, node_level0_s.getX());
    ASSERT_EQ( 15, node_level0_s.getY());

    ASSERT_EQ( 40, node_level1_p.getX());
    ASSERT_EQ( 15, node_level1_p.getY());

    ASSERT_EQ( 10, node_level1_s.getX());
    ASSERT_EQ(  5, node_level1_s.getY());

    // clean up
    dmGui::DeleteNode(m_Scene, node_level1, true);
    dmGui::DeleteNode(m_Scene, node_level0, true);

}

static void CreateSpineDummyData(dmGui::RigSceneDataDesc* dummy_data, uint32_t num_dummy_mesh_entries = 0)
{
    dmRigDDF::Skeleton* skeleton          = new dmRigDDF::Skeleton();
    dmRigDDF::MeshSet* mesh_set           = new dmRigDDF::MeshSet();
    dmRigDDF::AnimationSet* animation_set = new dmRigDDF::AnimationSet();

    uint32_t bone_count = 2;
    skeleton->m_Bones.m_Data = new dmRigDDF::Bone[bone_count];
    skeleton->m_Bones.m_Count = bone_count;

    // Bone 0
    dmRigDDF::Bone& bone0 = skeleton->m_Bones.m_Data[0];
    bone0.m_Parent       = 0xffff;
    bone0.m_Id           = 0;
    bone0.m_Position     = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    bone0.m_Rotation     = Vectormath::Aos::Quat::identity();
    bone0.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
    bone0.m_InheritScale = false;
    bone0.m_Length       = 0.0f;

    // Bone 1
    dmRigDDF::Bone& bone1 = skeleton->m_Bones.m_Data[1];
    bone1.m_Parent       = 0;
    bone1.m_Id           = 1;
    bone1.m_Position     = Vectormath::Aos::Point3(1.0f, 0.0f, 0.0f);
    bone1.m_Rotation     = Vectormath::Aos::Quat::identity();
    bone1.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
    bone1.m_InheritScale = false;
    bone1.m_Length       = 1.0f;

    dummy_data->m_BindPose = new dmArray<dmRig::RigBone>();
    dummy_data->m_Skeleton = skeleton;
    dummy_data->m_MeshSet = mesh_set;
    dummy_data->m_AnimationSet = animation_set;

    dmRig::CreateBindPose(*skeleton, *dummy_data->m_BindPose);

    if(num_dummy_mesh_entries > 0)
    {
        mesh_set->m_MeshEntries.m_Data = new dmRigDDF::MeshEntry[num_dummy_mesh_entries];
        mesh_set->m_MeshEntries.m_Count = num_dummy_mesh_entries;
    }

    char buf[64];
    for (int i = 0; i < num_dummy_mesh_entries; ++i)
    {
        sprintf(buf, "skin%i", i);
        CreateDummyMeshEntry(mesh_set->m_MeshEntries.m_Data[i], dmHashString64(buf), Vector4(float(i)/float(num_dummy_mesh_entries)));
    }
}

static void DeleteSpineDummyData(dmGui::RigSceneDataDesc* dummy_data, uint32_t num_dummy_mesh_entries = 0)
{
    if (num_dummy_mesh_entries > 0)
    {
        for (int i = 0; i < num_dummy_mesh_entries; ++i)
        {
            dmRig::MeshEntry& mesh_entry = dummy_data->m_MeshSet->m_MeshEntries.m_Data[i];
            dmRig::Mesh& mesh = mesh_entry.m_Meshes.m_Data[0];
            delete [] mesh.m_Normals.m_Data;
            delete [] mesh.m_BoneIndices.m_Data;
            delete [] mesh.m_Weights.m_Data;
            delete [] mesh.m_Indices.m_Data;
            delete [] mesh.m_Color.m_Data;
            delete [] mesh.m_SkinColor.m_Data;
            delete [] mesh.m_Texcoord0.m_Data;
            delete [] mesh.m_Positions.m_Data;
            delete [] mesh_entry.m_Meshes.m_Data;
        }
        delete [] dummy_data->m_MeshSet->m_MeshEntries.m_Data;
    }

    delete dummy_data->m_BindPose;
    delete [] dummy_data->m_Skeleton->m_Bones.m_Data;
    delete dummy_data->m_Skeleton;
    delete dummy_data->m_MeshSet;
    delete dummy_data->m_AnimationSet;
    delete dummy_data;
}

TEST_F(dmGuiTest, SpineNodeNoData)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    // create nodes
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    dmGui::SetNodePivot(m_Scene, node, dmGui::PIVOT_CENTER);
    ASSERT_NE(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, "test_spine", 0, 0, true));
}

TEST_F(dmGuiTest, SpineNode)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
    CreateSpineDummyData(dummy_data);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)dummy_data));

    // create nodes
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    dmGui::SetNodePivot(m_Scene, node, dmGui::PIVOT_CENTER);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"dummy"), dmHashString64((const char*)""), true));
    DeleteSpineDummyData(dummy_data);
}

TEST_F(dmGuiTest, SpineNodeSetSkin)
{
    uint32_t width = 100;
    uint32_t height = 50;
    uint32_t num_dummy_mesh_entries = 2;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
    CreateSpineDummyData(dummy_data, num_dummy_mesh_entries);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)dummy_data));

    // create nodes
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    dmGui::SetNodePivot(m_Scene, node, dmGui::PIVOT_CENTER);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"dummy-skin"), dmHashString64((const char*)""), true));

    // set skin
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineSkin(m_Scene, node, dmHashString64("skin1")));
    // verify
    ASSERT_EQ(dmHashString64("skin1"), dmGui::GetNodeSpineSkin(m_Scene, node));

    DeleteSpineDummyData(dummy_data, num_dummy_mesh_entries);
}

TEST_F(dmGuiTest, SpineNodeGetSkin)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc rig_scene_desc;
    rig_scene_desc.m_BindPose = &m_BindPose;
    rig_scene_desc.m_Skeleton = m_Skeleton;
    rig_scene_desc.m_MeshSet = m_MeshSet;
    rig_scene_desc.m_AnimationSet = m_AnimationSet;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)&rig_scene_desc));

    // create nodes
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    dmGui::SetNodePivot(m_Scene, node, dmGui::PIVOT_CENTER);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"skin1"), dmHashString64((const char*)""), true));

    // get skin
    ASSERT_EQ(dmHashString64("skin1"), dmGui::GetNodeSpineSkin(m_Scene, node));
}

uint32_t SpineAnimationCompleteCount = 0;
void SpineAnimationComplete(dmGui::HScene scene,
                         dmGui::HNode node,
                         bool finished,
                         void* userdata1,
                         void* userdata2)
{
    SpineAnimationCompleteCount++;
}
TEST_F(dmGuiTest, SpineNodeCompleteCallback)
{
    SpineAnimationCompleteCount = 0;
    uint32_t width = 100;
    uint32_t height = 50;
    float dt = 1.500001;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc rig_scene_desc;
    rig_scene_desc.m_BindPose = &m_BindPose;
    rig_scene_desc.m_Skeleton = m_Skeleton;
    rig_scene_desc.m_MeshSet = m_MeshSet;
    rig_scene_desc.m_AnimationSet = m_AnimationSet;
    rig_scene_desc.m_TrackIdxToPose = &m_TrackIdxToPose;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)&rig_scene_desc));

    // create node
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"skin1"), dmHashString64((const char*)""), true));

    // duration is 3.0 seconds
    // play animation with cb
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeSpineAnim(m_Scene, node, dmHashString64("valid"), dmGui::PLAYBACK_ONCE_FORWARD, 0.0, 0.0, 1.0, &SpineAnimationComplete, (void*)m_Scene, 0));
    ASSERT_EQ(dmRig::RESULT_UPDATED_POSE, dmRig::Update(m_RigContext, dt));
    ASSERT_EQ(dmRig::RESULT_UPDATED_POSE, dmRig::Update(m_RigContext, dt));
    ASSERT_EQ(SpineAnimationCompleteCount, 1);

    // play animation without cb
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeSpineAnim(m_Scene, node, dmHashString64("valid"), dmGui::PLAYBACK_ONCE_FORWARD, 0.0, 0.0, 1.0, 0, 0, 0));
    ASSERT_EQ(dmRig::RESULT_UPDATED_POSE, dmRig::Update(m_RigContext, dt));
    ASSERT_EQ(dmRig::RESULT_UPDATED_POSE, dmRig::Update(m_RigContext, dt));
    ASSERT_EQ(SpineAnimationCompleteCount, 1);

    // play animation with cb once more
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeSpineAnim(m_Scene, node, dmHashString64("valid"), dmGui::PLAYBACK_ONCE_FORWARD, 0.0, 0.0, 1.0, &SpineAnimationComplete, (void*)m_Scene, 0));
    ASSERT_EQ(dmRig::RESULT_UPDATED_POSE, dmRig::Update(m_RigContext, dt));
    ASSERT_EQ(dmRig::RESULT_UPDATED_POSE, dmRig::Update(m_RigContext, dt));
    ASSERT_EQ(SpineAnimationCompleteCount, 2);
}

TEST_F(dmGuiTest, SpineNodeSetCursor)
{
    uint32_t width = 100;
    uint32_t height = 50;
    float cursor = 0.5f;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc rig_scene_desc;
    rig_scene_desc.m_BindPose = &m_BindPose;
    rig_scene_desc.m_Skeleton = m_Skeleton;
    rig_scene_desc.m_MeshSet = m_MeshSet;
    rig_scene_desc.m_AnimationSet = m_AnimationSet;
    rig_scene_desc.m_TrackIdxToPose = &m_TrackIdxToPose;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)&rig_scene_desc));

    // create node
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"skin1"), dmHashString64((const char*)""), true));

    // play animation
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeSpineAnim(m_Scene, node, dmHashString64("valid"), dmGui::PLAYBACK_LOOP_FORWARD, 0.0, 0.0, 1.0, 0,0,0));

    // set cursor
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineCursor(m_Scene, node, cursor));
    ASSERT_NEAR(cursor, dmGui::GetNodeSpineCursor(m_Scene, node), EPSILON);
}

TEST_F(dmGuiTest, SpineNodeGetCursor)
{
    uint32_t width = 100;
    uint32_t height = 50;
    float cursor_offset = 0.3f;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc rig_scene_desc;
    rig_scene_desc.m_BindPose = &m_BindPose;
    rig_scene_desc.m_Skeleton = m_Skeleton;
    rig_scene_desc.m_MeshSet = m_MeshSet;
    rig_scene_desc.m_AnimationSet = m_AnimationSet;
    rig_scene_desc.m_TrackIdxToPose = &m_TrackIdxToPose;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)&rig_scene_desc));

    // create node
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"skin1"), dmHashString64((const char*)""), true));

    // play animation with cursor initial offset
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeSpineAnim(m_Scene, node, dmHashString64("valid"), dmGui::PLAYBACK_LOOP_FORWARD, 0.0, cursor_offset, 1.0, 0,0,0));

    // get cursor
    ASSERT_EQ(cursor_offset, dmGui::GetNodeSpineCursor(m_Scene, node));
}

TEST_F(dmGuiTest, SpineNodeSetPlaybackRate)
{
    uint32_t width = 100;
    uint32_t height = 50;
    float playback_rate = 0.5f;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc rig_scene_desc;
    rig_scene_desc.m_BindPose = &m_BindPose;
    rig_scene_desc.m_Skeleton = m_Skeleton;
    rig_scene_desc.m_MeshSet = m_MeshSet;
    rig_scene_desc.m_AnimationSet = m_AnimationSet;
    rig_scene_desc.m_TrackIdxToPose = &m_TrackIdxToPose;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)&rig_scene_desc));

    // create node
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"skin1"), dmHashString64((const char*)""), true));

    // play animation
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeSpineAnim(m_Scene, node, dmHashString64("valid"), dmGui::PLAYBACK_LOOP_FORWARD, 0.0, 0.0, 1.0, 0,0,0));

    // set playback rate
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpinePlaybackRate(m_Scene, node, playback_rate));
    ASSERT_NEAR(playback_rate, dmGui::GetNodeSpinePlaybackRate(m_Scene, node), EPSILON);
}

TEST_F(dmGuiTest, SpineNodeGetPlaybackRate)
{
    uint32_t width = 100;
    uint32_t height = 50;
    float playback_rate = 0.5f;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc rig_scene_desc;
    rig_scene_desc.m_BindPose = &m_BindPose;
    rig_scene_desc.m_Skeleton = m_Skeleton;
    rig_scene_desc.m_MeshSet = m_MeshSet;
    rig_scene_desc.m_AnimationSet = m_AnimationSet;
    rig_scene_desc.m_TrackIdxToPose = &m_TrackIdxToPose;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)&rig_scene_desc));

    // create node
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"skin1"), dmHashString64((const char*)""), true));

    // play animation
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeSpineAnim(m_Scene, node, dmHashString64("valid"), dmGui::PLAYBACK_LOOP_FORWARD, 0.0, 0.0, playback_rate, 0,0,0));

    // get playback rate
    ASSERT_NEAR(playback_rate, dmGui::GetNodeSpinePlaybackRate(m_Scene, node), EPSILON);
}

TEST_F(dmGuiTest, SpineNodeGetBoneNodes)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
    CreateSpineDummyData(dummy_data);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)dummy_data));

    // create nodes
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0, 0), dmGui::NODE_TYPE_SPINE);
    dmGui::SetNodePivot(m_Scene, node, dmGui::PIVOT_CENTER);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node, dmHashString64("test_spine"), dmHashString64((const char*)"dummy"), dmHashString64((const char*)""), true));

    ASSERT_EQ(0, GetNodeSpineBone(m_Scene, node, 123));
    ASSERT_NE(0, GetNodeSpineBone(m_Scene, node, 0));
    ASSERT_NE(0, GetNodeSpineBone(m_Scene, node, 1));
    ASSERT_EQ(0, GetNodeSpineBone(m_Scene, node, 2));

    ASSERT_EQ(0, dmGui::GetNodePosition(m_Scene, GetNodeSpineBone(m_Scene, node, 0)).getX());
    ASSERT_EQ(1, dmGui::GetNodePosition(m_Scene, GetNodeSpineBone(m_Scene, node, 1)).getX());

    DeleteSpineDummyData(dummy_data);
}

bool LoadParticlefxPrototype(const char* filename, dmParticle::HPrototype* prototype)
{
    char path[64];
    DM_SNPRINTF(path, 64, "build/default/src/test/%s", filename);
    const uint32_t MAX_FILE_SIZE = 4 * 1024;
    unsigned char buffer[MAX_FILE_SIZE];
    uint32_t file_size = 0;

    FILE* f = fopen(path, "rb");
    if (f)
    {
        file_size = fread(buffer, 1, MAX_FILE_SIZE, f);
        *prototype = dmParticle::NewPrototype(buffer, file_size);
        fclose(f);
        return *prototype != 0x0;
    }
    else
    {
        dmLogWarning("Particle FX could not be loaded: %s.", path);
        return false;
    }
}

void UnloadParticlefxPrototype(dmParticle::HPrototype prototype)
{
    if (prototype)
    {
        dmParticle::DeletePrototype(prototype);
    }
}

TEST_F(dmGuiTest, KeepParticlefxOnNodeDeletion)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmParticle::HPrototype prototype;
    const char* particlefx_name = "once.particlefxc";
    LoadParticlefxPrototype(particlefx_name, &prototype);

    dmGui::Result res = dmGui::AddParticlefx(m_Scene, particlefx_name, (void*)prototype);
    ASSERT_EQ(res, dmGui::RESULT_OK);

    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));

    ASSERT_EQ(1U, dmGui::GetParticlefxCount(m_Scene));
    dmGui::DeleteNode(m_Scene, node_pfx, false);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::UpdateScene(m_Scene, 1.0f / 60.0f));
    ASSERT_EQ(1U, dmGui::GetParticlefxCount(m_Scene));
    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, PlayNodeParticlefx)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmParticle::HPrototype prototype;
    const char* particlefx_name = "once.particlefxc";
    LoadParticlefxPrototype(particlefx_name, &prototype);

    dmGui::Result res = dmGui::AddParticlefx(m_Scene, particlefx_name, (void*)prototype);
    ASSERT_EQ(res, dmGui::RESULT_OK);

    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT);

    ASSERT_EQ(dmGui::RESULT_RESOURCE_NOT_FOUND, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));

    // should only succeed when trying to play particlefx on correct node type
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::PlayNodeParticlefx(m_Scene, node_box, 0));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::PlayNodeParticlefx(m_Scene, node_pie, 0));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::PlayNodeParticlefx(m_Scene, node_text, 0));
    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, PlayNodeParticlefxInitialTransform)
{
    uint32_t width = 100;
    uint32_t height = 100;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmParticle::HPrototype prototype;
    const char* particlefx_name = "once.particlefxc";
    LoadParticlefxPrototype(particlefx_name, &prototype);

    dmGui::Result res = dmGui::AddParticlefx(m_Scene, particlefx_name, (void*)prototype);
    ASSERT_EQ(res, dmGui::RESULT_OK);

    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(10,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));

    // should only succeed when trying to play particlefx on correct node type
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));
    dmGui::InternalNode* n = dmGui::GetNode(m_Scene, node_pfx);
    Vector3 pos = dmParticle::GetPosition(m_Scene->m_ParticlefxContext, n->m_Node.m_ParticleInstance);
    ASSERT_EQ(10, pos.getX());

    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

struct EmitterStateChangedCallbackTestData
{
    bool m_CallbackWasCalled;
    uint32_t m_NumStateChanges;
};

void EmitterStateChangedCallback(uint32_t num_awake_emitters, dmhash_t emitter_id, dmParticle::EmitterState emitter_state, void* user_data)
{
    EmitterStateChangedCallbackTestData* data = (EmitterStateChangedCallbackTestData*) user_data;
    data->m_CallbackWasCalled = true;
    ++data->m_NumStateChanges;
}

static inline dmGui::HNode SetupGuiTestScene(dmGuiTest* _this, const char* particlefx_name, dmParticle::HPrototype& prototype)
{
    uint32_t width = 100;
    uint32_t height = 50;
    dmGui::SetPhysicalResolution(_this->m_Context, width, height);
    dmGui::SetSceneResolution(_this->m_Scene, width, height);

    LoadParticlefxPrototype(particlefx_name, &prototype);

    dmGui::AddParticlefx(_this->m_Scene, particlefx_name, (void*)prototype);

    dmGui::HNode node_pfx = dmGui::NewNode(_this->m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);

    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    dmGui::SetNodeParticlefx(_this->m_Scene, node_pfx, particlefx_id);

    return node_pfx;
}

//Verify emitter state change callback is called correct number of times
TEST_F(dmGuiTest, CallbackCalledCorrectNumTimes)
{
    const char* particlefx_name = "once.particlefxc";
    dmParticle::HPrototype prototype;
    dmGui::HNode node_pfx = SetupGuiTestScene(this, particlefx_name, prototype);

    EmitterStateChangedCallbackTestData* data = (EmitterStateChangedCallbackTestData*)malloc(sizeof(EmitterStateChangedCallbackTestData));
    memset(data, 0, sizeof(*data));
    dmParticle::EmitterStateChangedData particle_callback;
    particle_callback.m_StateChangedCallback = EmitterStateChangedCallback;
    particle_callback.m_UserData = data;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, &particle_callback)); // Prespawn
    
    ASSERT_TRUE(data->m_CallbackWasCalled);
    ASSERT_EQ(1, data->m_NumStateChanges);
 
    float dt = 1.2f;
    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Spawning & Postspawn
    ASSERT_EQ(3, data->m_NumStateChanges);

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Sleeping
    ASSERT_EQ(4, data->m_NumStateChanges);
    
    dmGui::DeleteNode(m_Scene, node_pfx, true);
    dmGui::UpdateScene(m_Scene, dt);

    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

// Verify emitter state change callback is only called when there a state change has occured
TEST_F(dmGuiTest, CallbackCalledSingleTimePerStateChange)
{
    const char* particlefx_name = "once.particlefxc";
    dmParticle::HPrototype prototype;
    dmGui::HNode node_pfx = SetupGuiTestScene(this, particlefx_name, prototype);

    EmitterStateChangedCallbackTestData* data = (EmitterStateChangedCallbackTestData*)malloc(sizeof(EmitterStateChangedCallbackTestData));
    memset(data, 0, sizeof(*data));
    dmParticle::EmitterStateChangedData particle_callback;
    particle_callback.m_StateChangedCallback = EmitterStateChangedCallback;
    particle_callback.m_UserData = data;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, &particle_callback)); // Prespawn
    
    ASSERT_TRUE(data->m_CallbackWasCalled);
    ASSERT_EQ(1, data->m_NumStateChanges);
 
    float dt = 0.1f;
    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Spawning
    ASSERT_EQ(2, data->m_NumStateChanges);

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Still spawning, should not trigger callback
    ASSERT_EQ(2, data->m_NumStateChanges);
    
    dmGui::DeleteNode(m_Scene, node_pfx, true);
    dmGui::UpdateScene(m_Scene, dt);

    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}



// Verify emitter state change callback is called for all emitters
TEST_F(dmGuiTest, CallbackCalledMultipleEmitters)
{
    const char* particlefx_name = "once_three_emitters.particlefxc";
    dmParticle::HPrototype prototype;
    dmGui::HNode node_pfx = SetupGuiTestScene(this, particlefx_name, prototype);

    EmitterStateChangedCallbackTestData* data = (EmitterStateChangedCallbackTestData*)malloc(sizeof(EmitterStateChangedCallbackTestData));
    memset(data, 0, sizeof(*data));
    dmParticle::EmitterStateChangedData particle_callback;
    particle_callback.m_StateChangedCallback = EmitterStateChangedCallback;
    particle_callback.m_UserData = data;

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, &particle_callback)); // Prespawn
    
    ASSERT_TRUE(data->m_CallbackWasCalled);
    ASSERT_EQ(3, data->m_NumStateChanges);
 
    float dt = 1.2f;
    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Spawning & Postspawn
    ASSERT_EQ(9, data->m_NumStateChanges);

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Sleeping
    ASSERT_EQ(12, data->m_NumStateChanges);
    
    dmGui::DeleteNode(m_Scene, node_pfx, true);
    dmGui::UpdateScene(m_Scene, dt);

    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, StopNodeParticlefx)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmParticle::HPrototype prototype;
    const char* particlefx_name = "once.particlefxc";
    LoadParticlefxPrototype(particlefx_name, &prototype);

    dmGui::Result res = dmGui::AddParticlefx(m_Scene, particlefx_name, (void*)prototype);
    ASSERT_EQ(res, dmGui::RESULT_OK);

    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::StopNodeParticlefx(m_Scene, node_pfx));
    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, StopNodeParticlefxMultiplePlaying)
{
    uint32_t width = 100;
    uint32_t height = 50;
    float dt = 1.0f;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmParticle::HPrototype prototype;
    const char* particlefx_name = "once.particlefxc";
    LoadParticlefxPrototype(particlefx_name, &prototype);

    dmGui::Result res = dmGui::AddParticlefx(m_Scene, particlefx_name, (void*)prototype);
    ASSERT_EQ(res, dmGui::RESULT_OK);

    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));

    ASSERT_EQ(dmGui::GetParticlefxCount(m_Scene), 3);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::StopNodeParticlefx(m_Scene, node_pfx));

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Sleeping
    dmGui::UpdateScene(m_Scene, dt); // Prunes sleeping particlefx

    ASSERT_EQ(dmGui::GetParticlefxCount(m_Scene), 0);

    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::StopNodeParticlefx(m_Scene, node_box));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::StopNodeParticlefx(m_Scene, node_pie));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::StopNodeParticlefx(m_Scene, node_text));
    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, SetNodeParticlefx)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmParticle::HPrototype prototype;
    const char* particlefx_name = "once.particlefxc";
    LoadParticlefxPrototype(particlefx_name, &prototype);

    //dmGui::AddParticlefx(HScene scene, const char *particlefx_name, void *particlefx_prototype)
    dmGui::Result res = dmGui::AddParticlefx(m_Scene, particlefx_name, (void*)prototype);
    ASSERT_EQ(res, dmGui::RESULT_OK);

    // create nodes
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);

    // should not be able to set particlefx to any other node than particlefx node type
    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    ASSERT_NE(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_box, particlefx_id));
    ASSERT_NE(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pie, particlefx_id));
    ASSERT_NE(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_text, particlefx_id));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, GetNodeParticlefx)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmParticle::HPrototype prototype;
    const char* particlefx_name = "once.particlefxc";
    LoadParticlefxPrototype(particlefx_name, &prototype);

    //dmGui::AddParticlefx(HScene scene, const char *particlefx_name, void *particlefx_prototype)
    dmGui::Result res = dmGui::AddParticlefx(m_Scene, particlefx_name, (void*)prototype);
    ASSERT_EQ(res, dmGui::RESULT_OK);

    dmhash_t particlefx_id = dmHashString64(particlefx_name);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    dmhash_t ret_particlefx_id = 0;
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeParticlefx(m_Scene, node_pfx, ret_particlefx_id));
    ASSERT_EQ(particlefx_id, ret_particlefx_id);
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, BoxNodeSetSpineScene)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
    CreateSpineDummyData(dummy_data);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)dummy_data));

    // create nodes
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT);
    dmGui::HNode node_spine = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_SPINE);

    // should not be able to set spine scene for any other node than spine nodes
    dmhash_t spine_scene_id = dmHashString64("test_spine");
    ASSERT_NE(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node_box, spine_scene_id, 0, 0, true));
    ASSERT_NE(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node_pie, spine_scene_id, 0, 0, true));
    ASSERT_NE(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node_text, spine_scene_id, 0, 0, true));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node_spine, spine_scene_id, 0, 0, true));

    DeleteSpineDummyData(dummy_data);
}

TEST_F(dmGuiTest, DeleteSpineNode)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
    CreateSpineDummyData(dummy_data);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)dummy_data));

    ASSERT_EQ(0, dmGui::GetNodeCount(m_Scene));
    dmGui::HNode node_spine = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_SPINE);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node_spine, "test_spine", 0, 0, true));

    // Spine node + 2 bone nodes
    ASSERT_EQ(3, dmGui::GetNodeCount(m_Scene));

    // Delete spine node will delete bone nodes also
    dmGui::DeleteNode(m_Scene, node_spine, true);
    ASSERT_EQ(0, dmGui::GetNodeCount(m_Scene));

    DeleteSpineDummyData(dummy_data);
}

TEST_F(dmGuiTest, DeleteBoneNode)
{
    uint32_t width = 100;
    uint32_t height = 50;

    dmGui::SetPhysicalResolution(m_Context, width, height);
    dmGui::SetSceneResolution(m_Scene, width, height);

    dmGui::RigSceneDataDesc* dummy_data = new dmGui::RigSceneDataDesc();
    CreateSpineDummyData(dummy_data);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddSpineScene(m_Scene, "test_spine", (void*)dummy_data));

    ASSERT_EQ(0, dmGui::GetNodeCount(m_Scene));
    dmGui::HNode node_spine = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_SPINE);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeSpineScene(m_Scene, node_spine, "test_spine", 0, 0, true));

    dmGui::HNode node_bone = dmGui::GetNodeSpineBone(m_Scene, node_spine, 1);
    ASSERT_NE(0, node_bone);

    // Spine node + 2 bone nodes
    ASSERT_EQ(3, dmGui::GetNodeCount(m_Scene));

    // Delete spine node will delete bone nodes also
    dmGui::DeleteNode(m_Scene, node_spine, true);
    ASSERT_EQ(0, dmGui::GetNodeCount(m_Scene));

    DeleteSpineDummyData(dummy_data);
}

TEST_F(dmGuiTest, InheritAlpha)
{
    const char* s = "function init(self)\n"
                    "    self.box_node = gui.new_box_node(vmath.vector3(90, 90 ,0), vmath.vector3(180, 60, 0))\n"
                    "    gui.set_id(self.box_node, \"box\")\n"
                    "    gui.set_color(self.box_node, vmath.vector4(0,0,0,0.5))\n"
                    "    gui.set_inherit_alpha(self.box_node, false)\n"

                    "    self.text_inherit_alpha = gui.new_text_node(vmath.vector3(0, 0, 0), \"Inherit alpha\")\n"
                    "    gui.set_id(self.text_inherit_alpha, \"text_inherit_alpha\")\n"
                    "    gui.set_parent(self.text_inherit_alpha, self_box_node)\n"

                    "    self.text_no_inherit_alpha = gui.new_text_node(vmath.vector3(0, 0, 0), \"No inherit alpha\")\n"
                    "    gui.set_id(self.text_no_inherit_alpha, \"text_no_inherit_alpha\")\n"
                    "    gui.set_parent(self.text_no_inherit_alpha, self_box_node)\n"
                    "    gui.set_inherit_alpha(self.text_no_inherit_alpha, true)\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(self.text_no_inherit_alpha)\n"
                    "    gui.delete_node(self.text_inherit_alpha)\n"
                    "    gui.delete_node(self.box_node)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::HNode box_node = dmGui::GetNodeById(m_Scene, "box");
    ASSERT_EQ(dmGui::GetNodeInheritAlpha(m_Scene, box_node), false);

    dmGui::HNode text_inherit_alpha = dmGui::GetNodeById(m_Scene, "text_inherit_alpha");
    ASSERT_EQ(dmGui::GetNodeInheritAlpha(m_Scene, text_inherit_alpha), false);

    dmGui::HNode text_no_inherit_alpha = dmGui::GetNodeById(m_Scene, "text_no_inherit_alpha");
    ASSERT_EQ(dmGui::GetNodeInheritAlpha(m_Scene, text_no_inherit_alpha), true);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
