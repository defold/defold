#include "comp_label.h"

#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/object_pool.h>
#include <dlib/math.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../resources/res_label.h"
#include "../gamesys.h"
#include "../gamesys_private.h"
#include "comp_private.h"

#include "label_ddf.h"
#include "gamesys_ddf.h"

using namespace Vectormath::Aos;
namespace dmGameSystem
{
    struct LabelComponent
    {
        dmGameObject::HInstance     m_Instance;
        Point3                      m_Position;
        Quat                        m_Rotation;
        Vector3                     m_Scale;
        Matrix4                     m_World;
        // Hash of the components properties. Hash is used to be compatible with 64-bit arch as a 32-bit value is used for sorting
        // See GenerateKeys
        uint32_t                    m_MixedHash;
        dmGameObject::HInstance     m_ListenerInstance;
        dmhash_t                    m_ListenerComponent;
        LabelResource*              m_Resource;
        CompRenderConstants         m_RenderConstants;

        const char*                 m_Text;

        uint16_t                    m_ComponentIndex : 8;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_Padding : 6;
    };

    struct LabelWorld
    {
        dmObjectPool<LabelComponent>    m_Components;
        //dmArray<dmRender::RenderObject> m_RenderObjects;
    };

    DM_GAMESYS_PROP_VECTOR3(LABEL_PROP_SCALE, scale, false);
    DM_GAMESYS_PROP_VECTOR3(LABEL_PROP_SIZE, size, true);

    dmGameObject::CreateResult CompLabelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        LabelContext* label_context = (LabelContext*)params.m_Context;
        //dmRender::HRenderContext render_context = label_context->m_RenderContext;
        LabelWorld* world = new LabelWorld();

        world->m_Components.SetCapacity(label_context->m_MaxLabelCount);
        memset(world->m_Components.m_Objects.Begin(), 0, sizeof(LabelComponent) * label_context->m_MaxLabelCount);
        //world->m_RenderObjects.SetCapacity(label_context->m_MaxLabelCount);

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLabelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        //dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        //dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);
        //free(world->m_VertexBufferData);
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    void ReHash(LabelComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        LabelResource* resource = component->m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;
        dmHashInit32(&state, reverse);

        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmHashUpdateBuffer32(&state, &resource->m_FontMap, sizeof(resource->m_FontMap));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        dmHashUpdateBuffer32(&state, &ddf->m_Color, sizeof(ddf->m_Color));
        dmHashUpdateBuffer32(&state, &ddf->m_Outline, sizeof(ddf->m_Outline));
        dmHashUpdateBuffer32(&state, &ddf->m_Shadow, sizeof(ddf->m_Shadow));

        ReHashRenderConstants(&component->m_RenderConstants, &state);

        component->m_MixedHash = dmHashFinal32(&state);
    }

    dmGameObject::CreateResult CompLabelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Label could not be created since the label buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        LabelResource* resource = (LabelResource*)params.m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;

        uint32_t index = world->m_Components.Alloc();
        LabelComponent* component = &world->m_Components.Get(index);
        memset(component, 0, sizeof(LabelComponent));
        component->m_Instance = params.m_Instance;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Resource = resource;
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_Scale = Vector3(ddf->m_Scale[0], ddf->m_Scale[1], ddf->m_Scale[2]);
        component->m_Text = ddf->m_Text;
        ReHash(component);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLabelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        world->m_Components.Free(index, true);
        return dmGameObject::CREATE_RESULT_OK;
    }

    /*
    static Vector3 GetSize(const SpriteComponent* sprite)
    {
        Vector3 result = Vector3(0.0f, 0.0f, 0.0f);
        if (0x0 != sprite->m_Resource && 0x0 != sprite->m_Resource->m_TextureSet)
        {
            TextureSetResource* texture_set = sprite->m_Resource->m_TextureSet;
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(sprite->m_CurrentAnimation);
            if (anim_id)
            {
                dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
                dmGameSystemDDF::TextureSetAnimation* animation = &texture_set_ddf->m_Animations[*anim_id];
                result = Vector3(animation->m_Width, animation->m_Height, 1.0f);
            }
        }
        return result;
    }

    static Vector3 ComputeScaledSize(const SpriteComponent* sprite)
    {
        Vector3 result = GetSize(sprite);
        result.setX(result.getX() * sprite->m_Scale.getX());
        result.setY(result.getY() * sprite->m_Scale.getY());
        return result;
    }
    */

    /*
    static void RenderBatch(SpriteWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Label, "RenderBatch");

        const SpriteComponent* first = (SpriteComponent*) buf[*begin].m_UserData;
        assert(first->m_Enabled);

        TextureSetResource* texture_set = first->m_Resource->m_TextureSet;

        // Ninja in-place writing of render object.
        dmRender::RenderObject& ro = *sprite_world->m_RenderObjects.End();
        sprite_world->m_RenderObjects.SetSize(sprite_world->m_RenderObjects.Size()+1);

        // Fill in vertex buffer
        SpriteVertex *vb_begin = sprite_world->m_VertexBufferWritePtr;
        sprite_world->m_VertexBufferWritePtr = CreateVertexData(sprite_world, vb_begin, texture_set, buf, begin, end);

        ro.Init();
        ro.m_VertexDeclaration = sprite_world->m_VertexDeclaration;
        ro.m_VertexBuffer = sprite_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vb_begin - sprite_world->m_VertexBufferData;
        ro.m_VertexCount = (sprite_world->m_VertexBufferWritePtr - vb_begin);
        ro.m_Material = first->m_Resource->m_Material;
        ro.m_Textures[0] = texture_set->m_Texture;

        const dmRender::Constant* constants = first->m_RenderConstants;
        uint32_t size = first->m_ConstantCount;
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }

        dmGameSystemDDF::SpriteDesc::BlendMode blend_mode = first->m_Resource->m_DDF->m_BlendMode;
        switch (blend_mode)
        {
            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }

        ro.m_SetBlendFactors = 1;

        dmRender::AddToRender(render_context, &ro);
    }*/

    static void UpdateTransforms(LabelWorld* world, bool sub_pixels)
    {
        DM_PROFILE(Label, "UpdateTransforms");

        dmArray<LabelComponent>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            LabelComponent* c = &components[i];

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            Matrix4 local = dmTransform::ToMatrix4(dmTransform::Transform(Vector3(c->m_Position), c->m_Rotation, 1.0));
            Matrix4 world = dmGameObject::GetWorldMatrix(c->m_Instance);
            Matrix4 w;

            if (dmGameObject::ScaleAlongZ(c->m_Instance))
            {
                w = world * local;
            }
            else
            {
                w = dmTransform::MulNoScaleZ(world, local);
            }

            //w = appendScale(w, ComputeScaledSize(c));
            w = w * dmTransform::appendScale(w, c->m_Scale);
            Vector4 position = w.getCol3();
            if (!sub_pixels)
            {
                position.setX((int) position.getX());
                position.setY((int) position.getY());
            }
            w.setCol3(position);
            c->m_World = w;
        }
    }

/*
    static void PostMessages(LabelWorld* world)
    {
        DM_PROFILE(Label, "PostMessages");

        dmArray<LabelComponent>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            LabelComponent* component = &components[i];
            if (!component->m_Enabled )
                continue;

            TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(component->m_CurrentAnimation);
            if (!anim_id)
            {
                component->m_Playing = 0;
                continue;
            }

            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[*anim_id];

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
                            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size, 0);
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
        }
    }*/

    dmGameObject::CreateResult CompLabelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        LabelComponent* component = &world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompLabelUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        (void)params;
        return dmGameObject::UPDATE_RESULT_OK;
    }

/*
    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        SpriteWorld *world = (SpriteWorld *) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                world->m_VertexBufferWritePtr = world->m_VertexBufferData;
                world->m_RenderObjects.SetSize(0);
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(SpriteVertex) * (world->m_VertexBufferWritePtr - world->m_VertexBufferData),
                                                world->m_VertexBufferData, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                DM_COUNTER("SpriteVertexBuffer", (world->m_VertexBufferWritePtr - world->m_VertexBufferData) * sizeof(SpriteVertex));
                break;
            default:
                assert(params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH);
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
        }
    }
    */

    dmGameObject::UpdateResult CompLabelRender(const dmGameObject::ComponentsRenderParams& params)
    {
        (void)params;
        /*
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        UpdateTransforms(sprite_world, sprite_context->m_Subpixels);

        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t sprite_count = components.Size();

        if (!sprite_count)
            return dmGameObject::UPDATE_RESULT_OK;

        // Submit all sprites as entries in the render list for sorting.
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, sprite_count);
        dmRender::HRenderListDispatch sprite_dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, sprite_world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < sprite_count; ++i)
        {
            SpriteComponent& component = components[i];
            if (!component.m_Enabled || !component.m_AddedToUpdate)
                continue;

            uint32_t const_count = component.m_ConstantCount;
            for (uint32_t const_i = 0; const_i < const_count; ++const_i)
            {
                if (lengthSqr(component.m_RenderConstants[const_i].m_Value - component.m_PrevRenderConstants[const_i]) > 0)
                {
                    ReHash(&component);
                    break;
                }
            }

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(component.m_Resource->m_Material);
            write_ptr->m_Dispatch = sprite_dispatch;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        */
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompLabelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        LabelComponent* component = (LabelComponent*)user_data;
        return GetRenderConstant(&component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompLabelSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        LabelComponent* component = (LabelComponent*)user_data;
        SetRenderConstant(&component->m_RenderConstants, component->m_Resource->m_Material, name_hash, element_index, var);
        ReHash(component);
    }

    dmGameObject::UpdateResult CompLabelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        LabelComponent* component = &world->m_Components.Get(*params.m_UserData);
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
            if (params.m_Message->m_Id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash, dmGameObject::PropertyVar(ddf->m_Value), CompLabelSetConstantCallback, component);
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
                if( ClearRenderConstant(&component->m_RenderConstants, ddf->m_NameHash) )
                {
                    ReHash(component);
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetScale::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetScale* ddf = (dmGameSystemDDF::SetScale*)params.m_Message->m_Data;
                component->m_Scale = ddf->m_Scale;
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompLabelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        (void)params;
    }

    dmGameObject::PropertyResult CompLabelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        LabelWorld* world = (LabelWorld*)params.m_World;
        LabelComponent* component = &world->m_Components.Get(*params.m_UserData);
        dmhash_t get_property = params.m_PropertyId;

        if (IsReferencingProperty(LABEL_PROP_SCALE, get_property))
        {
            result = GetProperty(out_value, get_property, component->m_Scale, LABEL_PROP_SCALE);
        }
        else if (IsReferencingProperty(LABEL_PROP_SIZE, get_property))
        {
            LabelResource* resource = component->m_Resource;
            dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;
            Vector3 size = Vector3(ddf->m_Size[0], ddf->m_Size[1], ddf->m_Size[2]);
            result = GetProperty(out_value, get_property, size, LABEL_PROP_SIZE);
        }

        if (dmGameObject::PROPERTY_RESULT_NOT_FOUND == result)
        {
            result = GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, CompLabelGetConstantCallback, component);
        }

        return result;
    }

    dmGameObject::PropertyResult CompLabelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        LabelWorld* world = (LabelWorld*)params.m_World;
        LabelComponent* component = &world->m_Components.Get(*params.m_UserData);
        dmhash_t set_property = params.m_PropertyId;

        if (IsReferencingProperty(LABEL_PROP_SCALE, set_property))
        {
            result = SetProperty(set_property, params.m_Value, component->m_Scale, LABEL_PROP_SCALE);
        }
        else if (IsReferencingProperty(LABEL_PROP_SIZE, set_property))
        {
            LabelResource* resource = component->m_Resource;
            dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;
            Vector3 size = Vector3(ddf->m_Size[0], ddf->m_Size[1], ddf->m_Size[2]);
            result = SetProperty(set_property, params.m_Value, size, LABEL_PROP_SIZE);
        }

        if (dmGameObject::PROPERTY_RESULT_NOT_FOUND == result)
        {
            result = SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompLabelSetConstantCallback, component);
        }

        return result;
    }
}
