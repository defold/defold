#include "comp_spine_model.h"

#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/index_pool.h>
#include <dlib/math.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../resources/res_spine_model.h"
#include "../gamesys.h"
#include "../gamesys_private.h"

#include "spine_ddf.h"
#include "sprite_ddf.h"

using namespace Vectormath::Aos;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    static const dmhash_t NULL_ANIMATION = dmHashString64("");

    static const dmhash_t PROP_SKIN = dmHashString64("skin");
    static const dmhash_t PROP_ANIMATION = dmHashString64("animation");

    union SortKeySpine
    {
        struct
        {
            uint64_t m_Index : 16;  // Index is used to ensure stable sort
            uint64_t m_Z : 16; // Quantified relative z
            uint64_t m_MixedHash : 32;
        };
        uint64_t     m_Key;
    };

    struct SpineModelComponent
    {
        dmGameObject::HInstance     m_Instance;
        Point3                      m_Position;
        Quat                        m_Rotation;
        Matrix4                     m_World;
        SortKeySpine                m_SortKey;
        // Hash of the m_Resource-pointer. Hash is used to be compatible with 64-bit arch as a 32-bit value is used for sorting
        // See GenerateKeys
        uint32_t                    m_MixedHash;
        dmGameObject::HInstance     m_ListenerInstance;
        dmhash_t                    m_ListenerComponent;
        SpineModelResource*         m_Resource;
        dmArray<dmRender::Constant> m_RenderConstants;
        dmArray<Vector4>            m_PrevRenderConstants;
        /// Animated pose, every transform is local-to-model-space
        dmArray<dmTransform::Transform> m_Pose;
        /// Currently playing animation
        dmhash_t                    m_Animation;
        /// Currently used mesh
        dmGameSystemDDF::Mesh*      m_Mesh;
        dmhash_t                    m_Skin;
        /// Used to scale the time step when updating the timer
        float                       m_AnimInvDuration;
        /// Timer in local space: [0,1]
        float                       m_AnimTimer;
        uint8_t                     m_ComponentIndex;
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_Playing : 1;
    };

    struct SpineModelVertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
    };

    struct SpineModelWorld
    {
        dmArray<SpineModelComponent>    m_Components;
        dmIndexPool32                   m_ComponentIndices;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmArray<SpineModelVertex>       m_VertexBufferData;

        dmArray<uint32_t>               m_RenderSortBuffer;
        float                           m_MinZ;
        float                           m_MaxZ;
    };

    dmGameObject::CreateResult CompSpineModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpineModelContext* context = (SpineModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        SpineModelWorld* world = new SpineModelWorld();

        world->m_Components.SetCapacity(context->m_MaxSpineModelCount);
        world->m_Components.SetSize(context->m_MaxSpineModelCount);
        memset(&world->m_Components[0], 0, sizeof(SpineModelComponent) * context->m_MaxSpineModelCount);
        world->m_ComponentIndices.SetCapacity(context->m_MaxSpineModelCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxSpineModelCount);

        world->m_RenderSortBuffer.SetCapacity(context->m_MaxSpineModelCount);
        world->m_RenderSortBuffer.SetSize(context->m_MaxSpineModelCount);
        for (uint32_t i = 0; i < context->m_MaxSpineModelCount; ++i)
        {
            world->m_RenderSortBuffer[i] = i;
        }
        world->m_MinZ = 0;
        world->m_MaxZ = 0;

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
        };

        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        // Assume 4 vertices per mesh
        world->m_VertexBufferData.SetCapacity(4 * world->m_Components.Capacity());

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpineModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static bool PlayAnimation(SpineModelComponent* component, dmhash_t animation_id)
    {
        TextureSetResource* texture_set = component->m_Resource->m_Scene->m_TextureSet;
        uint32_t* anim_id = texture_set->m_AnimationIds.Get(animation_id);
        if (anim_id)
        {
            // TODO Implement animation in DEF-252
            /*
            component->m_CurrentAnimation = animation_id;
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_TextureSet->m_Animations[*anim_id];
            uint32_t frame_count = animation->m_End - animation->m_Start;
            if (animation->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG
                    || animation->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
                frame_count = dmMath::Max(1u, frame_count * 2 - 2);
            component->m_AnimInvDuration = (float)animation->m_Fps / frame_count;
            component->m_AnimTimer = 0.0f;
            component->m_Playing = animation->m_Playback != dmGameSystemDDF::PLAYBACK_NONE;
            */
        }
        return anim_id != 0;
    }

    static void ReHash(SpineModelComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        SpineModelResource* resource = component->m_Resource;
        dmGameSystemDDF::SpineModelDesc* ddf = resource->m_Model;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &resource, sizeof(resource));
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t size = constants.Size();
        // Padding in the SetConstant-struct forces us to copy the components by hand
        for (uint32_t i = 0; i < size; ++i)
        {
            dmRender::Constant& c = constants[i];
            dmHashUpdateBuffer32(&state, &c.m_NameHash, sizeof(uint64_t));
            dmHashUpdateBuffer32(&state, &c.m_Value, sizeof(Vector4));
            component->m_PrevRenderConstants[i] = c.m_Value;
        }
        component->m_MixedHash = dmHashFinal32(&state);
    }

    dmGameObject::CreateResult CompSpineModelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;

        if (world->m_ComponentIndices.Remaining() == 0)
        {
            dmLogError("Spine Model could not be created since the buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = world->m_ComponentIndices.Pop();
        SpineModelComponent* component = &world->m_Components[index];
        component->m_Instance = params.m_Instance;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Resource = (SpineModelResource*)params.m_Resource;
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_Mesh = 0x0;
        component->m_Skin = dmHashString64(component->m_Resource->m_Model->m_Skin);
        dmGameSystemDDF::MeshSet* mesh_set = &component->m_Resource->m_Scene->m_SpineScene->m_MeshSet;
        for (uint32_t i = 0; i < mesh_set->m_Meshes.m_Count; ++i)
        {
            dmGameSystemDDF::Mesh* mesh = &mesh_set->m_Meshes[i];
            if (mesh->m_Id == component->m_Skin)
            {
                component->m_Mesh = mesh;
                break;
            }
        }
        dmGameSystemDDF::Skeleton* skeleton = &component->m_Resource->m_Scene->m_SpineScene->m_Skeleton;
        uint32_t bone_count = skeleton->m_Bones.m_Count;
        component->m_Pose.SetCapacity(bone_count);
        component->m_Pose.SetSize(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            dmGameSystemDDF::Bone* bone = &skeleton->m_Bones[i];
            component->m_Pose[i] = dmTransform::Transform(Vector3(bone->m_Position), bone->m_Rotation, bone->m_Scale);
            if (i > 0)
            {
                component->m_Pose[i] = dmTransform::Mul(component->m_Pose[bone->m_Parent], component->m_Pose[i]);
            }
        }
        component->m_Animation = NULL_ANIMATION;
        ReHash(component);
        dmhash_t default_animation = dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);
        if (component->m_Animation != default_animation)
            PlayAnimation(component, default_animation);

        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpineModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        SpineModelComponent* component = (SpineModelComponent*)*params.m_UserData;
        uint32_t index = component - &world->m_Components[0];
        memset(component, 0, sizeof(SpineModelComponent));
        world->m_ComponentIndices.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    struct SortPredSpine
    {
        SortPredSpine(SpineModelWorld* world) : m_World(world) {}

        SpineModelWorld* m_World;

        inline bool operator () (const uint32_t x, const uint32_t y)
        {
            SpineModelComponent* c1 = &m_World->m_Components[x];
            SpineModelComponent* c2 = &m_World->m_Components[y];
            return c1->m_SortKey.m_Key < c2->m_SortKey.m_Key;
        }

    };

    static void GenerateKeys(SpineModelWorld* world)
    {
        dmArray<SpineModelComponent>& components = world->m_Components;
        uint32_t n = components.Size();

        float min_z = world->m_MinZ;
        float range = 1.0f / (world->m_MaxZ - world->m_MinZ);

        SpineModelComponent* first = world->m_Components.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpineModelComponent* c = &components[i];
            uint32_t index = c - first;
            if (c->m_Resource && c->m_Enabled)
            {
                float z = (c->m_World.getElem(3, 2) - min_z) * range * 65535;
                z = dmMath::Clamp(z, 0.0f, 65535.0f);
                uint16_t zf = (uint16_t) z;
                c->m_SortKey.m_Z = zf;
                c->m_SortKey.m_Index = index;
                c->m_SortKey.m_MixedHash = c->m_MixedHash;
            }
            else
            {
                c->m_SortKey.m_Key = 0xffffffffffffffffULL;
            }
        }
    }

    static void Sort(SpineModelWorld* world)
    {
        DM_PROFILE(SpineModel, "Sort");
        dmArray<uint32_t>* buffer = &world->m_RenderSortBuffer;
        SortPredSpine pred(world);
        std::sort(buffer->Begin(), buffer->End(), pred);
    }

    static void CreateVertexData(SpineModelWorld* world, dmArray<SpineModelVertex>& vertex_buffer, TextureSetResource* texture_set, uint32_t start_index, uint32_t end_index)
    {
        DM_PROFILE(SpineModel, "CreateVertexData");

        const dmArray<SpineModelComponent>& components = world->m_Components;
        const dmArray<uint32_t>& sort_buffer = world->m_RenderSortBuffer;

        for (uint32_t i = start_index; i < end_index; ++i)
        {
            const SpineModelComponent* component = &components[sort_buffer[i]];
            dmArray<SpineBone>& bind_pose = component->m_Resource->m_Scene->m_BindPose;

            dmGameSystemDDF::Mesh* mesh = component->m_Mesh ;
            if (mesh == 0x0)
                continue;

            const Matrix4& w = component->m_World;

            uint32_t index_count = mesh->m_Indices.m_Count;
            for (uint32_t ii = 0; ii < index_count; ++ii)
            {
                vertex_buffer.SetSize(vertex_buffer.Size()+1);
                SpineModelVertex& v = vertex_buffer.Back();
                uint32_t vi = mesh->m_Indices[ii];
                uint32_t e = vi*3;
                Point3 in_p(mesh->m_Positions[e++], mesh->m_Positions[e++], mesh->m_Positions[e++]);
                Point3 out_p(0.0f, 0.0f, 0.0f);
                uint32_t bi_offset = vi * 4;
                uint32_t* bone_indices = &mesh->m_BoneIndices[bi_offset];
                float* bone_weights = &mesh->m_Weights[bi_offset];
                for (uint32_t bi = 0; bi < 4; ++bi)
                {
                    uint32_t bone_index = bone_indices[bi];
                    // TODO include bind-pose-model-to-local in the pose to avoid extra mul
                    out_p += Vector3(dmTransform::Apply(dmTransform::Mul(component->m_Pose[bone_index], bind_pose[bone_index].m_ModelToLocal), in_p)) * bone_weights[bi];
                }
                *((Vector4*)&v) = w * out_p;
                e = vi*2;
                v.u = mesh->m_Texcoord0[e++];
                v.v = mesh->m_Texcoord0[e++];
            }
        }
    }

    static uint32_t RenderBatch(SpineModelWorld* world, dmRender::HRenderContext render_context, dmArray<SpineModelVertex>& vertex_buffer, uint32_t start_index)
    {
        DM_PROFILE(SpineModel, "RenderBatch");
        uint32_t n = world->m_Components.Size();

        const dmArray<SpineModelComponent>& components = world->m_Components;
        const dmArray<uint32_t>& sort_buffer = world->m_RenderSortBuffer;

        const SpineModelComponent* first = &components[sort_buffer[start_index]];
        assert(first->m_Enabled);
        TextureSetResource* texture_set = first->m_Resource->m_Scene->m_TextureSet;
        uint64_t z = first->m_SortKey.m_Z;
        uint32_t hash = first->m_MixedHash;

        uint32_t vertex_count = 0;
        uint32_t end_index = n;
        for (uint32_t i = start_index; i < n; ++i)
        {
            const SpineModelComponent* c = &components[sort_buffer[i]];
            if (!c->m_Enabled || c->m_MixedHash != hash || c->m_SortKey.m_Z != z)
            {
                end_index = i;
                break;
            }
            if (c->m_Mesh != 0x0)
                vertex_count += c->m_Mesh->m_Indices.m_Count;
        }

        if (vertex_buffer.Remaining() < vertex_count)
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());

        // Render object
        dmRender::RenderObject ro;
        ro.m_VertexDeclaration = world->m_VertexDeclaration;
        ro.m_VertexBuffer = world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vertex_buffer.Size();
        ro.m_VertexCount = vertex_count;
        ro.m_Material = first->m_Resource->m_Material;
        ro.m_Textures[0] = texture_set->m_Texture;
        // The first transform is used for the batch. Mean-value might be better?
        // NOTE: The position is already transformed, see CreateVertexData, but set for sorting.
        // See also sprite.vp
        ro.m_WorldTransform = first->m_World;
        ro.m_CalculateDepthKey = 1;

        const dmArray<dmRender::Constant>& constants = first->m_RenderConstants;
        uint32_t size = constants.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }

        dmGameSystemDDF::SpineModelDesc::BlendMode blend_mode = first->m_Resource->m_Model->m_BlendMode;
        switch (blend_mode)
        {
            case dmGameSystemDDF::SpineModelDesc::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::SpineModelDesc::BLEND_MODE_ADD:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::SpineModelDesc::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }
        ro.m_SetBlendFactors = 1;

        world->m_RenderObjects.Push(ro);
        dmRender::AddToRender(render_context, &world->m_RenderObjects[world->m_RenderObjects.Size() - 1]);

        CreateVertexData(world, vertex_buffer, texture_set, start_index, end_index);
        return end_index;
    }

    void UpdateTransforms(SpineModelWorld* world)
    {
        DM_PROFILE(SpineModel, "UpdateTransforms");

        dmArray<SpineModelComponent>& components = world->m_Components;
        uint32_t n = components.Size();
        float min_z = FLT_MAX;
        float max_z = -FLT_MAX;
        for (uint32_t i = 0; i < n; ++i)
        {
            SpineModelComponent* c = &components[i];

            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!c->m_Enabled)
                continue;

            if (c->m_Mesh != 0x0)
            {
                dmTransform::Transform world = dmGameObject::GetWorldTransform(c->m_Instance);
                dmTransform::Transform local(Vector3(c->m_Position), c->m_Rotation, 1.0f);
                if (dmGameObject::ScaleAlongZ(c->m_Instance))
                {
                    world = dmTransform::Mul(world, local);
                }
                else
                {
                    world = dmTransform::MulNoScaleZ(world, local);
                }
                Matrix4 w = dmTransform::ToMatrix4(world);
                Vector4 position = w.getCol3();
                float z = position.getZ();
                min_z = dmMath::Min(min_z, z);
                max_z = dmMath::Max(max_z, z);
                w.setCol3(position);
                c->m_World = w;
            }
        }

        if (n == 0)
        {
            // NOTE: Avoid large numbers and risk of de-normalized etc.
            // if n == 0 the actual values of min/max-z doens't matter
            min_z = 0;
            max_z = 1;
        }

        world->m_MinZ = min_z;
        world->m_MaxZ = max_z;
    }

    static void PostMessages(SpineModelWorld* world)
    {
        DM_PROFILE(SpineModel, "PostMessages");

        dmArray<SpineModelComponent>& components = world->m_Components;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpineModelComponent* component = &components[i];
            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!component->m_Enabled || !component->m_Playing)
                continue;

            // TODO handle in DEF-252
            /*
            TextureSetResource* texture_set = component->m_Resource->m_Scene->m_TextureSet;
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(component->m_CurrentAnimation);
            if (!anim_id)
            {
                component->m_Playing = 0;
                continue;
            }

            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;

            bool once = animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_FORWARD
                    || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD
                    || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG;
            // Stop once-animation and broadcast animation_done
            if (once && component->m_AnimTimer >= 1.0f)
            {
                component->m_Playing = 0;
                if (component->m_ListenerInstance != 0x0)
                {
                    dmhash_t message_id = dmGameSystemDDF::AnimationDone::m_DDFDescriptor->m_NameHash;
                    dmGameSystemDDF::AnimationDone message;
                    // Engine has 0-based indices, scripts use 1-based
                    message.m_CurrentTile = GetCurrentTile(component, animation_ddf) - animation_ddf->m_Start + 1;
                    message.m_Id = component->m_CurrentAnimation;
                    dmMessage::URL receiver;
                    receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_ListenerInstance));
                    dmMessage::URL sender;
                    sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
                    if (dmMessage::IsSocketValid(receiver.m_Socket) && dmMessage::IsSocketValid(sender.m_Socket))
                    {
                        dmGameObject::Result go_result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment);
                        if (go_result == dmGameObject::RESULT_OK)
                        {
                            receiver.m_Path = dmGameObject::GetIdentifier(component->m_ListenerInstance);
                            receiver.m_Fragment = component->m_ListenerComponent;
                            sender.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
                            uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::AnimationDone::m_DDFDescriptor;
                            uint32_t data_size = sizeof(dmGameSystemDDF::AnimationDone);
                            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size);
                            component->m_ListenerInstance = 0x0;
                            component->m_ListenerComponent = 0xff;
                            if (result != dmMessage::RESULT_OK)
                            {
                                dmLogError("Could not send animation_done to listener.");
                            }
                        }
                        else
                        {
                            dmLogError("Could not send animation_done to listener because of incomplete component.");
                        }
                    }
                    else
                    {
                        component->m_ListenerInstance = 0x0;
                        component->m_ListenerComponent = 0xff;
                    }
                }
            }
            */
        }
    }

    static void Animate(SpineModelWorld* world, float dt)
    {
        DM_PROFILE(SpineModel, "Animate");

        dmArray<SpineModelComponent>& components = world->m_Components;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpineModelComponent* component = &components[i];
            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!component->m_Enabled || !component->m_Playing)
                continue;

            // TODO handle in DEF-252
            /*
            TextureSetResource* texture_set = component->m_Resource->m_Scene->m_TextureSet;
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(component->m_CurrentAnimation);
            if (!anim_id)
            {
                continue;
            }

            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[*anim_id];

            // Animate
            component->m_AnimTimer += dt * component->m_AnimInvDuration;
            if (component->m_AnimTimer >= 1.0f)
            {
                switch (animation_ddf->m_Playback)
                {
                    case dmGameSystemDDF::PLAYBACK_ONCE_FORWARD:
                    case dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD:
                    case dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG:
                        component->m_AnimTimer = 1.0f;
                        break;
                    default:
                        component->m_AnimTimer -= floorf(component->m_AnimTimer);
                        break;
                }
            }
            */
        }
    }

    dmGameObject::UpdateResult CompSpineModelUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        /*
         * All spine models are sorted, using the m_RenderSortBuffer, with respect to the:
         *
         *     - hash value of m_Resource, i.e. equal iff the sprite is rendering with identical atlas
         *     - z-value
         *     - component index
         *  or
         *     - 0xffffffff (or corresponding 64-bit value) if not enabled
         * such that all non-enabled spine models ends up last in the array
         * and spine models with equal atlas and depth consecutively
         *
         * The z-sorting is considered a hack as we assume a camera pointing along the z-axis. We currently
         * have no access, by design as render-data currently should be invariant to camera parameters,
         * to the transformation matrices when generating render-data. The render-system and go-system should probably
         * be changed such that unique render-objects are created when necessary and on-demand instead of up-front as
         * currently. Another option could be a call-back when the actual rendering occur.
         *
         * The sorted array of indices are grouped into batches, using z and resource-hash as predicates, and every
         * batch is rendered using a single draw-call. Note that the world transform
         * is set to first sprite transform for correct batch sorting. The actual vertex transformation is performed in code
         * and standard world-transformation is removed from vertex-program.
         *
         * NOTES:
         * * When/if transparency the batching predicates must be updated in order to
         *   support per sprite correct sorting.
         */

        SpineModelContext* context = (SpineModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;

        dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 6 * sizeof(SpineModelVertex) * world->m_Components.Size(), 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        dmArray<SpineModelVertex>& vertex_buffer = world->m_VertexBufferData;
        vertex_buffer.SetSize(0);

        dmArray<SpineModelComponent>& components = world->m_Components;
        uint32_t sprite_count = world->m_Components.Size();
        for (uint32_t i = 0; i < sprite_count; ++i)
        {
            SpineModelComponent& component = components[i];
            if (!component.m_Enabled)
                continue;
            uint32_t const_count = component.m_RenderConstants.Size();
            for (uint32_t const_i = 0; const_i < const_count; ++const_i)
            {
                float diff_sq = lengthSqr(component.m_RenderConstants[const_i].m_Value - component.m_PrevRenderConstants[const_i]);
                if (diff_sq > 0)
                {
                    ReHash(&component);
                    break;
                }
            }
        }
        UpdateTransforms(world);
        GenerateKeys(world);
        Sort(world);

        world->m_RenderObjects.SetSize(0);

        const dmArray<uint32_t>& sort_buffer = world->m_RenderSortBuffer;

        Animate(world, params.m_UpdateContext->m_DT);

        uint32_t start_index = 0;
        uint32_t n = components.Size();
        while (start_index < n && components[sort_buffer[start_index]].m_Enabled)
        {
            start_index = RenderBatch(world, render_context, vertex_buffer, start_index);
        }

        void* vertex_buffer_data = 0x0;
        if (!vertex_buffer.Empty())
            vertex_buffer_data = (void*)&(vertex_buffer[0]);
        dmGraphics::SetVertexBufferData(world->m_VertexBuffer, vertex_buffer.Size() * sizeof(SpineModelVertex), vertex_buffer_data, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

        PostMessages(world);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompSpineModelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data;
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t count = constants.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                *out_constant = &c;
                return true;
            }
        }
        return false;
    }

    static void CompSpineModelSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data;
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t count = constants.Size();
        Vector4* v = 0x0;
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                v = &c.m_Value;
                break;
            }
        }
        if (v == 0x0)
        {
            if (constants.Full())
            {
                uint32_t capacity = constants.Capacity() + 4;
                constants.SetCapacity(capacity);
                component->m_PrevRenderConstants.SetCapacity(capacity);
            }
            dmRender::Constant c;
            dmRender::GetMaterialProgramConstant(component->m_Resource->m_Material, name_hash, c);
            constants.Push(c);
            component->m_PrevRenderConstants.Push(c.m_Value);
            v = &(constants[constants.Size()-1].m_Value);
        }
        if (element_index == 0x0)
            *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        else
            v->setElem(*element_index, (float)var.m_Number);
        ReHash(component);
    }

    dmGameObject::UpdateResult CompSpineModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        SpineModelComponent* component = (SpineModelComponent*)*params.m_UserData;
        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            // TODO Fix in DEF-252
            /*
            if (params.m_Message->m_Id == dmGameSystemDDF::PlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::PlayAnimation* ddf = (dmGameSystemDDF::PlayAnimation*)params.m_Message->m_Data;
                if (PlayAnimation(component, ddf->m_Id))
                {
                    component->m_ListenerInstance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(component->m_Instance), params.m_Message->m_Sender.m_Path);
                    component->m_ListenerComponent = params.m_Message->m_Sender.m_Fragment;
                }
            }
            else */if (params.m_Message->m_Id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), CompSpineModelSetConstantCallback, component);
                if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
                {
                    dmMessage::URL& receiver = params.m_Message->m_Receiver;
                    dmLogError("'%s:%s#%s' has no constant named '%s'",
                            dmMessage::GetSocketName(receiver.m_Socket),
                            (const char*)dmHashReverse64(receiver.m_Path, 0x0),
                            (const char*)dmHashReverse64(receiver.m_Fragment, 0x0),
                            (const char*)dmHashReverse64(ddf->m_NameHash, 0x0));
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
                uint32_t size = constants.Size();
                for (uint32_t i = 0; i < size; ++i)
                {
                    if (constants[i].m_NameHash == ddf->m_NameHash)
                    {
                        constants.EraseSwap(i);
                        component->m_PrevRenderConstants.EraseSwap(i);
                        ReHash(component);
                        break;
                    }
                }
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompSpineModelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        SpineModelComponent* component = (SpineModelComponent*)*params.m_UserData;
        if (component->m_Playing)
            PlayAnimation(component, component->m_Animation);
    }

    dmGameObject::PropertyResult CompSpineModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        SpineModelComponent* component = (SpineModelComponent*)*params.m_UserData;
        if (params.m_PropertyId == PROP_SKIN)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_Skin);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANIMATION)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_Animation);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, CompSpineModelGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompSpineModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        SpineModelComponent* component = (SpineModelComponent*)*params.m_UserData;
        if (params.m_PropertyId == PROP_SKIN)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_HASH)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            dmGameSystemDDF::Mesh* mesh = 0x0;
            dmGameSystemDDF::MeshSet* mesh_set = &component->m_Resource->m_Scene->m_SpineScene->m_MeshSet;
            dmhash_t skin = params.m_Value.m_Hash;
            for (uint32_t i = 0; i < mesh_set->m_Meshes.m_Count; ++i)
            {
                if (skin == mesh_set->m_Meshes[i].m_Id)
                {
                    mesh = &mesh_set->m_Meshes[i];
                    break;
                }
            }
            if (mesh == 0x0)
            {
                dmLogError("Could not find skin '%s' in the mesh set.", (const char*)dmHashReverse64(skin, 0x0));
                return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
            }
            component->m_Mesh = mesh;
            component->m_Skin = params.m_Value.m_Hash;
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompSpineModelSetConstantCallback, component);
    }
}
