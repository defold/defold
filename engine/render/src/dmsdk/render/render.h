// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_RENDER_H
#define DMSDK_RENDER_H

#include <stdint.h>
#include <render/material_ddf.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/graphics/graphics.h>

namespace dmIntersection
{
    struct Frustum;
}

/*# Render API documentation
 *
 * Api for render specific data
 *
 * @document
 * @name Render
 * @namespace dmRender
 * @language C++
 */

/*# Render extension context name
 * Name used when registering the render context with the engine context registry.
 * @constant
 * @name RENDER_CONTEXT_NAME
 */
#define RENDER_CONTEXT_NAME "render"

namespace dmRender
{
    /*#
     * The render context
     * @typedef
     * @name HRenderContext
     */
    typedef struct RenderContext* HRenderContext;

    /*# persistent render item handle
     * Identifies a renderer-owned item until DeleteRenderItem() invalidates it.
     * Zero is invalid. Use the handle only with the context that created it.
     * Create and update items before the draw that consumes them, outside render
     * dispatch callbacks. Item setters return false for invalid or deleted handles.
     * @typedef
     * @name HRenderItem
     */
    typedef uint64_t HRenderItem;

    /*# persistent render dispatch handle
     * Identifies a registration created by RegisterRenderListDispatch(). Use it
     * only with its creating context, and never after UnregisterRenderListDispatch().
     * This handle is separate from the frame-local HRenderListDispatch used in
     * RenderListEntry. Zero is invalid.
     * @typedef
     * @name HRenderItemDispatch
     */
    typedef uint32_t HRenderItemDispatch;

    /*#
     * Material instance handle
     * @typedef
     * @name HMaterial
     */
    typedef struct Material* HMaterial;

    /*#
     * Font map handle
     * @typedef
     * @name HFont
     */
    typedef struct FontMap* HFontMap;

    // Old typedef, used internally. We want to migrate towards HFont
    typedef struct FontMap* HFontMap;


    /*#
     * Shader constant handle
     * @typedef
     * @name HConstant
     */
    typedef struct Constant* HConstant;

    /*#
     * Shader constant buffer handle. Holds name and values for a constant.
     * @typedef
     * @name HNamedConstantBuffer
     */
    typedef struct NamedConstantBuffer* HNamedConstantBuffer;

    /*#
    * Sampler handle
    * @typedef
    * @name HSampler
    */
    typedef struct Sampler* HSampler;

    /*#
    * Light prototype handle. Used to create light instances.
    * @typedef
    * @name HLightPrototype
    */
    typedef uint32_t HLightPrototype;

    /*#
     * @enum
     * @name Result
     * @member RESULT_OK
     * @member RESULT_INVALID_CONTEXT
     * @member RESULT_OUT_OF_RESOURCES
     * @member RESULT_BUFFER_IS_FULL
     * @member RESULT_INVALID_PARAMETER
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_INVALID_CONTEXT = -1,
        RESULT_OUT_OF_RESOURCES = -2,
        RESULT_BUFFER_IS_FULL = -3,
        RESULT_INVALID_PARAMETER = -4,
        RESULT_TYPE_MISMATCH = -5,
    };

    /*#
     * Get the vertex space (local or world)
     * @name dmRender::GetMaterialVertexSpace
     * @param material [type: dmRender::HMaterial] the material
     * @return vertex_space [type: dmRenderDDF::MaterialDesc::VertexSpace] the vertex space
     */
    dmRenderDDF::MaterialDesc::VertexSpace GetMaterialVertexSpace(HMaterial material);

    /*#
     * Struct holding stencil operation setup
     * @struct
     * @name StencilTestParams
     * @member m_Func [type: dmGraphics::CompareFunc] the compare function
     * @member m_OpSFail [type: dmGraphics::StencilOp] the stencil fail operation
     * @member m_OpDPFail [type: dmGraphics::StencilOp] the depth pass fail operation
     * @member m_OpDPPass [type: dmGraphics::StencilOp] the depth pass pass operation
     * @member m_Ref [type: uint8_t]
     * @member m_RefMask [type: uint8_t]
     * @member m_BufferMask [type: uint8_t]
     * @member m_ColorBufferMask [type: uint8_t:4]
     * @member m_ClearBuffer [type: uint8_t:1]
     */
    struct StencilTestParams
    {
        StencilTestParams();
        void Init();

        struct
        {
            dmGraphics::CompareFunc m_Func;
            dmGraphics::StencilOp   m_OpSFail;
            dmGraphics::StencilOp   m_OpDPFail;
            dmGraphics::StencilOp   m_OpDPPass;
        } m_Front;

        struct
        {
            dmGraphics::CompareFunc m_Func;
            dmGraphics::StencilOp   m_OpSFail;
            dmGraphics::StencilOp   m_OpDPFail;
            dmGraphics::StencilOp   m_OpDPPass;
        } m_Back;

        uint8_t m_Ref;
        uint8_t m_RefMask;
        uint8_t m_BufferMask;
        uint8_t m_ColorBufferMask : 4;
        uint8_t m_ClearBuffer : 1;
        uint8_t m_SeparateFaceStates : 1;
        uint8_t : 2;
    };


    /*#
     * The maximum number of textures the render object can hold (16)
     * @constant
     * @name dmRender::RenderObject::MAX_TEXTURE_COUNT
     */

    /*#
     * Render objects represent an actual draw call
     * @struct
     * @name RenderObject
     * @member m_Constants [type: dmRender::HConstant[]] the shader constants
     * @member m_WorldTransform [type: dmVMath::Matrix4] the world transform (usually identity for batched objects)
     * @member m_TextureTransform [type: dmVMath::Matrix4] the texture transform
     * @member m_VertexBuffer [type: dmGraphics::HVertexBuffer] the vertex buffer
     * @member m_VertexDeclaration [type: dmGraphics::HVertexDeclaration] the vertex declaration
     * @member m_IndexBuffer [type: dmGraphics::HIndexBuffer] the index buffer
     * @member m_Material [type: dmRender::HMaterial] the material
     * @member m_Textures [type: dmGraphics::HTexture[]] the textures
     * @member m_PrimitiveType [type: dmGraphics::PrimitiveType] the primitive type
     * @member m_IndexType [type: dmGraphics::Type] the index type (16/32 bit)
     * @member m_SourceBlendFactor [type: dmGraphics::BlendFactor] the source blend factor
     * @member m_DestinationBlendFactor [type: dmGraphics::BlendFactor] the destination blend factor
     * @member m_StencilTestParams [type: dmRender::StencilTestParams] the stencil test params
     * @member m_VertexStart [type: uint32_t] the vertex start
     * @member m_VertexCount [type: uint32_t] the vertex count
     * @member m_SetBlendFactors [type: uint8_t:1] use the blend factors
     * @member m_SetStencilTest [type: uint8_t:1] use the stencil test
     */
    struct RenderObject
    {
        RenderObject();
        void Init();

        static const uint32_t MAX_TEXTURE_COUNT       = 16;
        static const uint32_t MAX_VERTEX_BUFFER_COUNT = 3;

        HNamedConstantBuffer            m_ConstantBuffer;
        dmVMath::Matrix4                m_WorldTransform;
        dmVMath::Matrix4                m_TextureTransform;
        union
        {
            dmGraphics::HVertexBuffer m_VertexBuffer;
            dmGraphics::HVertexBuffer m_VertexBuffers[MAX_VERTEX_BUFFER_COUNT];
        };

        union
        {
            dmGraphics::HVertexDeclaration  m_VertexDeclaration;
            dmGraphics::HVertexDeclaration  m_VertexDeclarations[MAX_VERTEX_BUFFER_COUNT];
        };
        dmGraphics::HIndexBuffer        m_IndexBuffer;
        HMaterial                       m_Material;
        dmGraphics::HTexture            m_Textures[MAX_TEXTURE_COUNT];
        dmGraphics::PrimitiveType       m_PrimitiveType;
        dmGraphics::Type                m_IndexType;
        dmGraphics::BlendFactor         m_SourceBlendFactor;
        dmGraphics::BlendFactor         m_DestinationBlendFactor;
        dmGraphics::BlendFactor         m_SourceBlendFactorAlpha;
        dmGraphics::BlendFactor         m_DestinationBlendFactorAlpha;
        dmGraphics::BlendEquation       m_BlendEquationColor;
        dmGraphics::BlendEquation       m_BlendEquationAlpha;
        dmGraphics::FaceWinding         m_FaceWinding;
        StencilTestParams               m_StencilTestParams;
        uint32_t                        m_VertexBufferOffsets[MAX_VERTEX_BUFFER_COUNT];
        uint32_t                        m_VertexStart;
        uint32_t                        m_VertexCount;
        uint32_t                        m_InstanceCount;
        uint8_t                         m_SetBlendFactors : 1;
        uint8_t                         m_SetStencilTest : 1;
        uint8_t                         m_SetFaceWinding : 1;
    };

    /*#
     * Represents a renderable object (e.g. a single sprite)
     * The renderer will (each frame) collect all entries with the current material tag, then batch these objects together.
     * Batching is done based on the batch key and Z value (or order for GUI nodes)
     * The caller will also register a callback function where the batched entries will be returned.
     * Each callback then represents a draw call, and will register a RenderObject
     * @name RenderListEntry
     * @param m_WorldPosition [type: dmVMath::Point3] the world position of the object
     * @param m_UserData [type: uint64_t] user data (available in the render dispatch callback)
     * @param m_Order [type: uint32_t] the order to sort on (used if m_MajorOrder != RENDER_ORDER_WORLD)
     * @param m_BatchKey [type: uint32_t] the batch key to sort on (note: only 48 bits are currently used by renderer)
     * @param m_TagListKey [type: uint32_t] the key to the list of material tags
     * @param m_FrustumHash [type: uint32_t] Last combined frustum cull key (note: engine internal use only!)
     * @param m_MinorOrder [type: uint32_t:4] used to sort within a batch
     * @param m_MajorOrder [type: uint32_t:2] If RENDER_ORDER_WORLD, then sorting is done based on the world position.
                                              Otherwise the sorting uses the m_Order value directly.
     * @param m_Dispatch [type: uint32_t:8] The dispatch function callback (dmRender::HRenderListDispatch)
     * @param m_Visibility [type: uint32_t:1] Visibility flag. Used for frustrum culling. See enum Visibility
     */
    struct RenderListEntry
    {
        dmVMath::Point3 m_WorldPosition;
        uint64_t m_UserData; // E.g. component type instance pointer
        uint32_t m_Order;
        uint32_t m_BatchKey;
        uint32_t m_TagListKey;
        uint32_t m_FrustumHash;
        uint32_t m_MinorOrder : 4;
        uint32_t m_MajorOrder : 2;
        uint32_t m_Dispatch   : 8;
        uint32_t m_Visibility : 1; // See enum Visibility
        uint32_t              : 17;
    };

    /*#
     * Render batch callback states
     * @enum
     * @name RenderListOperation
     * @member RENDER_LIST_OPERATION_BEGIN
     * @member RENDER_LIST_OPERATION_BATCH
     * @member RENDER_LIST_OPERATION_END
     */
    enum RenderListOperation
    {
        RENDER_LIST_OPERATION_BEGIN,
        RENDER_LIST_OPERATION_BATCH,
        RENDER_LIST_OPERATION_END
    };

    /*#
     * Render order
     * @enum
     * @name RenderOrder
     * @member RENDER_ORDER_BEFORE_WORLD    Drawn before world-sorted items
     * @member RENDER_ORDER_WORLD           Used by game objects
     * @member RENDER_ORDER_AFTER_WORLD     Used by gui
     */
    enum RenderOrder
    {
        RENDER_ORDER_BEFORE_WORLD = 0,
        RENDER_ORDER_WORLD        = 1,
        RENDER_ORDER_AFTER_WORLD  = 2,
    };

    /*#
     * Visibility status
     * @enum
     * @name Visibility
     * @member VISIBILITY_NONE
     * @member VISIBILITY_FULL
     */
    enum Visibility
    {
        VISIBILITY_NONE = 0,
        VISIBILITY_FULL = 1, // Not sure if we ever need partial ?
    };

    /*#
     * Frustum planes to use in a frustum
     * @enum
     * @name FrustumPlanes
     * @member FRUSTUM_PLANES_SIDES
     * @member FRUSTUM_PLANES_ALL
     */
    enum FrustumPlanes
    {
        FRUSTUM_PLANES_SIDES = 4,
        FRUSTUM_PLANES_ALL   = 6
    };

    /*#
     * Frustum options used when setting up a draw call
     * @struct
     * @name FrustumOptions
     * @member m_FrustumMatrix [type: matrix4] the frustum matrix
     * @member m_SkipNearFarPlanes [type: bool] should the frustum culling use the near and far planes
     */
    struct FrustumOptions
    {
        dmVMath::Matrix4 m_Matrix;
        FrustumPlanes    m_NumPlanes;
    };

    /*#
     * Visibility dispatch function callback.
     * @struct
     * @name RenderListVisibilityParams
     * @member m_UserData [type: void*] the callback user data supplied to RegisterRenderListDispatch() or RenderListMakeDispatch()
     * @member m_Entries [type: dmRender::RenderListEntry] the render entry array
     * @member m_NumEntries [type: uint32_t] the number of render entries in the array
     */
    struct RenderListVisibilityParams
    {
        const dmIntersection::Frustum*  m_Frustum;
        void*                           m_UserData;
        RenderListEntry*                m_Entries;
        uint32_t                        m_NumEntries;
    };

    /*#
     * Render visibility function callback.
     * @typedef
     * @name RenderListVisibilityFn
     * @param params [type: dmRender::RenderListVisibilityParams] the params
     */
    typedef void (*RenderListVisibilityFn)(RenderListVisibilityParams const &params);

    /*#
     * Render dispatch function callback.
     * @struct
     * @name RenderListDispatchParams
     * @member m_Context [type: dmRender::HRenderContext] the context
     * @member m_UserData [type: void*] the callback user data supplied to RegisterRenderListDispatch() or RenderListMakeDispatch()
     * @member m_Operation [type: dmRender::RenderListOperation] the operation
     * @member m_Buf [type: dmRender::RenderListEntry] the render entry array
     * @member m_Begin [type: uint32_t*] the start of the render batch. contains index into the m_Buf array
     * @member m_End [type: uint32_t*] the end of the render batch. Loop while "m_Begin != m_End"
     */
    struct RenderListDispatchParams
    {
        HRenderContext m_Context;
        void* m_UserData;
        RenderListOperation m_Operation;
        RenderListEntry* m_Buf;
        uint32_t* m_Begin;
        uint32_t* m_End;
    };

    /*#
     * Render dispatch function handle.
     * @typedef
     * @name HRenderListDispatch
     */
    typedef uint8_t HRenderListDispatch;

    /*#
     * Render dispatch function callback.
     * @typedef
     * @name RenderListDispatchFn
     * @param params [type: dmRender::RenderListDispatchParams] the params
     */
    typedef void (*RenderListDispatchFn)(RenderListDispatchParams const &params);

    /*# register a persistent render dispatch
     * Callbacks and user data remain caller-owned and must stay valid until
     * unregistration. Register before submitting. Delete all associated items before
     * unregistering, after the current render list has finished drawing.
     * @name RegisterRenderListDispatch
     * @param context [type:dmRender::HRenderContext] the render context
     * @param dispatch_fn [type:dmRender::RenderListDispatchFn] the render batch callback
     * @param visibility_fn [type:dmRender::RenderListVisibilityFn] visibility callback for items without typed bounds, or 0 for always visible
     * @param user_data [type:void*] user data passed to both callbacks
     * @return dispatch [type:dmRender::HRenderItemDispatch] the registration, or 0 if the registration limit is reached
     */
    HRenderItemDispatch RegisterRenderListDispatch(HRenderContext context, RenderListDispatchFn dispatch_fn, RenderListVisibilityFn visibility_fn, void* user_data);

    /*# unregister a persistent render dispatch
     * Delete all items belonging to this registration first. Call only after all
     * draws using the current render list have finished, before the next render list
     * is submitted. The registration handle becomes invalid. This does not release
     * caller-owned callbacks or user data.
     * @name UnregisterRenderListDispatch
     * @param context [type:dmRender::HRenderContext] the render context
     * @param dispatch [type:dmRender::HRenderItemDispatch] a registration created in this context
     */
    void UnregisterRenderListDispatch(HRenderContext context, HRenderItemDispatch dispatch);

    /*# submit persistent render items for the current frame
     * Makes the registration's enabled items eligible for this frame. Call between
     * RenderListBegin() and RenderListEnd(), before render-script draws. Repeated
     * submission in one frame has no effect. Only submitted registrations consume
     * frame dispatch IDs; at most 255 are available, shared with legacy dispatches.
     * If that limit is reached, an error is logged and this submission is skipped.
     * @name SubmitRenderListDispatch
     * @param context [type:dmRender::HRenderContext] the render context
     * @param dispatch [type:dmRender::HRenderItemDispatch] a registration created in this context
     */
    void SubmitRenderListDispatch(HRenderContext context, HRenderItemDispatch dispatch);

    /*# create a persistent render item
     * The new item is disabled, with an identity transform, no material tags or bounds,
     * and world-position ordering. Its dispatch and user data are fixed
     * for its lifetime. Any data referenced by user_data must outlive the item.
     * Creating an item does not invalidate outstanding RenderListAlloc() ranges.
     * Delete the item before unregistering its dispatch.
     * @name NewRenderItem
     * @param context [type:dmRender::HRenderContext] the render context
     * @param dispatch [type:dmRender::HRenderItemDispatch] a registration created in this context
     * @param user_data [type:uint64_t] item data exposed as RenderListEntry::m_UserData in callbacks
     * @return item [type:dmRender::HRenderItem] the item handle
     * @examples
     *
     * ```cpp
     * #include <dmsdk/render/render.h>
     *
     * // context, dispatch and material are owned by the component or its world.
     * dmRender::HRenderItem item = dmRender::NewRenderItem(context, dispatch, user_data);
     * dmRender::SetMaterial(context, item, material);
     * dmRender::SetWorldTransform(context, item, world_transform);
     * dmRender::SetBoundingOBB(context, item, local_min, local_max);
     * dmRender::Enable(context, item, true);
     *
     * // In the component render callback each frame:
     * dmRender::SubmitRenderListDispatch(context, dispatch);
     *
     * // When the component is destroyed, outside dispatch callbacks:
     * dmRender::DeleteRenderItem(context, item);
     * ```
     */
    HRenderItem NewRenderItem(HRenderContext context, HRenderItemDispatch dispatch, uint64_t user_data);

    /*# delete a persistent render item
     * Disables the item, invalidates its handle and releases its slot for reuse.
     * Call before subsequent draws and outside dispatch callbacks. User data is not
     * released.
     * @name DeleteRenderItem
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool DeleteRenderItem(HRenderContext context, HRenderItem item);

    /*# enable or disable a render item
     * Changes draw eligibility without releasing the handle or its data. Repeating
     * the current enabled state has no effect. The dispatch must also be submitted
     * for the item to participate in the current frame.
     * @name Enable
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param enabled [type:bool] true to enable the item, or false to disable it
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool Enable(HRenderContext context, HRenderItem item, bool enabled);

    /*# set the render item world transform
     * Copies the local-to-world affine transform. Its translation supplies the sort
     * position, and it transforms local OBB bounds. World-space sphere and AABB
     * bounds are unaffected. Every call invalidates cached visibility; also call
     * when callback-owned visibility data changes without a transform change.
     * @name SetWorldTransform
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param world_transform [type:dmVMath::Matrix4] the affine local-to-world transform to copy
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool SetWorldTransform(HRenderContext context, HRenderItem item, const dmVMath::Matrix4& world_transform);

    /*# set world-space sphere bounds
     * Replaces the previous volume. The radius is a non-negative distance, not a
     * squared radius. SetWorldTransform() does not move or scale this sphere.
     * Visibility is invalidated only when the volume type or values change.
     * @name SetBoundingSphere
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param world_center [type:dmVMath::Point3] the world-space sphere center
     * @param radius [type:float] the non-negative sphere radius in world units
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool SetBoundingSphere(HRenderContext context, HRenderItem item, const dmVMath::Point3& world_center, float radius);

    /*# set world-space axis-aligned bounds
     * Replaces the previous volume. If any minimum coordinate exceeds its maximum,
     * removes typed bounds and restores the registration's visibility callback, or
     * always-visible behavior when no callback is registered. Equal corners define
     * a valid point. SetWorldTransform() does not transform this box.
     * Visibility is invalidated only when the volume type or values change.
     * @name SetBoundingAABB
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param world_min [type:dmVMath::Vector3] the world-space minimum corner
     * @param world_max [type:dmVMath::Vector3] the world-space maximum corner
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool SetBoundingAABB(HRenderContext context, HRenderItem item, const dmVMath::Vector3& world_min, const dmVMath::Vector3& world_max);

    /*# set local bounds for an oriented box
     * Replaces the previous volume. Each minimum coordinate must be no greater than
     * its maximum coordinate. The item's world transform supplies position,
     * orientation and scale, including reflection and shear. Visibility is
     * invalidated only when the volume type or values change.
     * @name SetBoundingOBB
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param local_min [type:dmVMath::Vector3] the local-space minimum corner
     * @param local_max [type:dmVMath::Vector3] the local-space maximum corner
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool SetBoundingOBB(HRenderContext context, HRenderItem item, const dmVMath::Vector3& local_min, const dmVMath::Vector3& local_max);

    /*# set the render item batch key
     * Sets the caller's batch compatibility key. Equal keys allow batching only when
     * the remaining state is compatible. The renderer also separates batches by
     * dispatch and ordering.
     * @name SetBatchKey
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param key [type:uint32_t] the batch compatibility key
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool SetBatchKey(HRenderContext context, HRenderItem item, uint32_t key);

    /*# set explicit render item ordering
     * Overrides the default world-position ordering. World order sorts by position.
     * Before-world and after-world order use the supplied order value; only its low
     * 24 bits are retained. Material changes and render-script material overrides
     * preserve this ordering.
     * @name SetOrder
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param render_order [type:dmRender::RenderOrder] the major rendering order
     * @param order [type:uint32_t] the 24-bit order value, ignored for world-position sorting
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool SetOrder(HRenderContext context, HRenderItem item, RenderOrder render_order, uint32_t order);

    /*# set the render item material
     * Copies the material's predicate tags. Call again after changing material tags
     * to refresh cached metadata. A null material clears the tags. The material is
     * not retained. Ordering is unchanged; use SetOrder() to configure it.
     * @name SetMaterial
     * @param context [type:dmRender::HRenderContext] the render context
     * @param item [type:dmRender::HRenderItem] an item created in this context
     * @param material [type:dmRender::HMaterial] the material supplying predicate tags, or 0 to clear them
     * @return result [type:bool] true on success, or false if the item handle is invalid or deleted
     */
    bool SetMaterial(HRenderContext context, HRenderItem item, HMaterial material);

    /*#
     * Register a frame-local render dispatch function
     * Deprecated: use RegisterRenderListDispatch() and SubmitRenderListDispatch().
     * Emits a warning once per process. The overload without a visibility
     * callback is also deprecated.
     * @name RenderListMakeDispatch
     * @param context [type: dmRender::HRenderContext] the context
     * @param dispatch_fn [type: dmRender::RenderListDispatchFn] the render batch callback function
     * @param visibility_fn [type: dmRender::RenderListVisibilityFn] the render list visibility callback function. May be 0
     * @param user_data [type: void*] userdata to the callback
     * @return dispatch [type: dmRender::HRenderListDispatch] the render dispatch function handle
     */
    HRenderListDispatch RenderListMakeDispatch(HRenderContext context, RenderListDispatchFn dispatch_fn, RenderListVisibilityFn visibility_fn, void* user_data);

    // Deprecated, left for backwards compatibility until extensions have been updated
    HRenderListDispatch RenderListMakeDispatch(HRenderContext context, RenderListDispatchFn fn, void* user_data);

    /*#
     * Allocate frame-local render entries
     * Deprecated: use NewRenderItem() and the render item setters. Emits a warning
     * once per process. Do not retain pointers across frames; another allocation
     * can also invalidate the returned range.
     * @name RenderListAlloc
     * @param context [type: dmRender::HRenderContext] the context
     * @param entries [type: uint32_t] the number of entries to allocate
     * @return array [type: dmRender::RenderListEntry*] the render list entry array
     */
    RenderListEntry* RenderListAlloc(HRenderContext context, uint32_t entries);

    /*#
     * Submit frame-local render entries
     * Deprecated: use Enable() and SubmitRenderListDispatch(). Emits a warning
     * once per process. The range must come from a single RenderListAlloc() call.
     * @name RenderListSubmit
     * @param context [type: dmRender::HRenderContext] the context
     * @param begin [type: dmRender::RenderListEntry*] the start of the array
     * @param end [type: dmRender::RenderListEntry*] the end of the array (i.e. "while begin!=end: *begin ..."")
     */
    void RenderListSubmit(HRenderContext context, RenderListEntry* begin, RenderListEntry* end);

    /*#
     * Adds a render object to the current render frame
     * @name AddToRender
     * @param context [type: dmRender::HRenderContext] the context
     * @param ro [type: dmRender::RenderObject*] the render object
     * @return result [type: dmRender::Result] the result
     */
    Result AddToRender(HRenderContext context, RenderObject* ro);

    /*#
     * Gets the key to the material tag list
     * @name GetMaterialTagListKey
     * @param material [type: dmGraphics::HMaterial] the material
     * @return listkey [type: uint32_t] the list key
     */
    uint32_t GetMaterialTagListKey(HMaterial material);

    /*#
     * Creates a shader program constant
     * @name NewConstant
     * @param name_hash [type: dmhash_t] the name of the material constant
     * @return constant [type: dmRender::HConstant] the constant
    */
    HConstant NewConstant(dmhash_t name_hash);

    /*#
     * Deletes a shader program constant
     * @name DeleteConstant
     * @param constant [type: dmRender::HConstant] The shader constant
    */
    void DeleteConstant(HConstant constant);

    /*#
     * Gets the shader program constant values
     * @name GetConstantValues
     * @param constant [type: dmRender::HConstant] The shader constant
     * @param num_values [type: uint32_t*] (out) the array num_values
     * @return values [type: dmVMath::Vector4*] the uniform values
    */
    dmVMath::Vector4* GetConstantValues(HConstant constant, uint32_t* num_values);

    /*#
     * Sets the shader program constant values
     * @name SetConstantValues
     * @param constant [type: dmRender::HConstant] The shader constant
     * @param values [type: dmVMath::Vector4*] the array values
     * @param num_values [type: uint32_t] the array size (number of Vector4's)
     * @return result [type: dmRender::Result] the result
    */
    Result SetConstantValues(HConstant constant, dmVMath::Vector4* values, uint32_t num_values);

    /*#
     * Gets the shader program constant name
     * @name GetConstantName
     * @param constant [type: dmRender::HConstant] The shader constant
     * @return name [type: dmhash_t] the hash name
    */
    dmhash_t GetConstantName(HConstant constant);

    /*#
     * Gets the shader program constant name
     * @name GetConstantName
     * @param constant [type: dmRender::HConstant] The shader constant
     * @param name [type: dmhash_t] the hash name
    */
    void SetConstantName(HConstant constant, dmhash_t name);

    /*#
     * Gets the shader program constant location
     * @name GetConstantLocation
     * @param constant [type: dmRender::HConstant] The shader constant
     * @return location [type: int32_t] the location
    */
    dmGraphics::HUniformLocation GetConstantLocation(HConstant constant);

    /*#
     * Sets the shader program constant location
     * @name SetConstantLocation
     * @param constant [type: dmRender::HConstant] The shader constant
     * @param location [type: dmGraphics::HUniformLocation] the location
    */
    void SetConstantLocation(HConstant constant, dmGraphics::HUniformLocation location);

    /*#
     * Gets the type of the constant
     * @name GetConstantType
     * @param constant [type: dmRender::HConstant] The shader constant
     * @return type [type: dmRenderDDF::MaterialDesc::ConstantType] the type of the constant
    */
    dmRenderDDF::MaterialDesc::ConstantType GetConstantType(HConstant constant);

    /*#
     * Sets the type of the constant
     * @name SetConstantType
     * @param constant [type: dmRender::HConstant] The shader constant
     * @param type [type: dmRenderDDF::MaterialDesc::ConstantType] the type of the constant
    */
    void SetConstantType(HConstant constant, dmRenderDDF::MaterialDesc::ConstantType type);

    /*#
     * Allocates a named constant buffer
     * @name NewNamedConstantBuffer
     * @return buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     */
    HNamedConstantBuffer NewNamedConstantBuffer();

    /*#
     * Deletes a named constant buffer
     * @name DeleteNamedConstantBuffer
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     */
    void DeleteNamedConstantBuffer(HNamedConstantBuffer buffer);

    /*#
     * Clears a named constant buffer from any constants.
     * @name ClearNamedConstantBuffer
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     */
    void ClearNamedConstantBuffer(HNamedConstantBuffer buffer);

    /*#
     * Removes a named constant from the buffer
     * @name RemoveNamedConstant
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param name_hash [type: dmhash_t] the name of the constant
     */
    void RemoveNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash);

    /*#
     * Sets one or more named constants to the buffer
     * @name SetNamedConstant
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param name_hash [type: dmhash_t] the name of the constant
     * @param values [type: dmVMath::Vector4*] the values
     * @param num_values [type: uint32_t] the number of values
     */
    void SetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values);

    /*#
     * Sets one or more named constants to the buffer with a specified data type.
     * Currently only dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_COLOR
     * and dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4 are supported.
     * @name SetNamedConstant
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param name_hash [type: dmhash_t] the name of the constant
     * @param values [type: dmVMath::Vector4*] the values
     * @param num_values [type: uint32_t] the number of values
     * @param constant_type [type: dmRenderDDF::MaterialDesc::ConstantType] The constant type
     */
    void SetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values, dmRenderDDF::MaterialDesc::ConstantType constant_type);

    /*#
     * Sets a list of named constants to the buffer
     * @name SetNamedConstants
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param constants [type: dmRender::HConstant*] the constants
     * @param num_constants [type: uint32_t] the number of constants
     */
    void SetNamedConstants(HNamedConstantBuffer buffer, HConstant* constants, uint32_t num_constants);

    /*#
     * Sets a named constant in the buffer at a specific index
     * @name SetNamedConstantAtIndex
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param name_hash [type: dmhash_t] the name of the constant
     * @param value [type: dmVMath::Vector4] the value
     * @param value_index [type: uint32_t] the index of the value to set
     * @return result [type: Result] the result
     */
    Result SetNamedConstantAtIndex(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t num_values, uint32_t value_index, dmRenderDDF::MaterialDesc::ConstantType constant_type);

    /*#
     * Gets a named constant from the buffer
     * @name GetNamedConstant
     * @note This give access to the internal memory of the constant
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param name_hash [type: dmhash_t] the name of the constant
     * @param values [type: dmVMath::Vector4**] (out) the values. May not be null.
     * @param num_values [type: uint32_t*] (out) the number of values. May not be null.
     * @return ok [type: bool] true if constant existed.
     */
    bool GetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4** values, uint32_t* num_values);

    /*#
     * Gets a named constant from the buffer - with type information
     * @name GetNamedConstant
     * @note This give access to the internal memory of the constant
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param name_hash [type: dmhash_t] the name of the constant
     * @param values [type: dmVMath::Vector4**] (out) the values. May not be null.
     * @param num_values [type: uint32_t*] (out) the number of values. May not be null.
     * @param constant_type [type: dmRenderDDF::MaterialDesc::ConstantType*] (out) the constant type.
     * @return ok [type: bool] true if constant existed.
     */
    bool GetNamedConstant(HNamedConstantBuffer buffer, dmhash_t name_hash, dmVMath::Vector4** values, uint32_t* num_values, dmRenderDDF::MaterialDesc::ConstantType* constant_type);

    /*#
     * Gets number of constants in the buffer
     * @name GetNamedConstantCount
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @return ok [type: bool] true if constant existed.
     */
    uint32_t GetNamedConstantCount(HNamedConstantBuffer buffer);


    /*#
     * @typedef
     * @name IterateNamedConstantsFn
     * @param name_hash [type:dmhash_t]
     * @param ctx [type:void*]
     */
    typedef void (*IterateNamedConstantsFn)(dmhash_t name_hash, void* ctx);

    /*#
     * Iterates over the constants
     * @name IterateNamedConstants
     * @param buffer [type: dmRender::HNamedConstantBuffer] the constants buffer
     * @param callback [type: IterateNamedConstantsFn] the callback function
     * @param ctx [type: void*] the callback context
     */
    void IterateNamedConstants(HNamedConstantBuffer buffer, IterateNamedConstantsFn callback, void* ctx);

    /*#
     * @name GetViewMatrix
     * @param render_context [type:dmRender::HRenderContext] Render context
     * @return view_matrix [type:const dmVMath::Matrix4&]
     */
    const dmVMath::Matrix4& GetViewMatrix(HRenderContext render_context);

    /*#
     * @name NewMaterial
     * @param render_context [type:dmRender::HContext] Render context
     * @param program [type:dmGraphics::HProgram]
     * @return new_material [type:dmRender::HMaterial]
     */
    HMaterial NewMaterial(HRenderContext render_context, dmGraphics::HProgram program);

    /*#
     * @name DeleteMaterial
     * @param render_context [type:dmRender::HRenderContext] Render context
     * @param material [type:dmRender::Material]
     */
    void DeleteMaterial(HRenderContext render_context, HMaterial material);

    /*#
     * @name ClearMaterialTags
     * @param material [type:dmRender::HMaterial]
     */
    void ClearMaterialTags(HMaterial material);

    /*#
     * @name SetMaterialTags
     * @param material [type:dmRender::Material]
     * @param tag_count [type:uint32_t]
     * @param tags [type:const dmhash_t*]
     */
    void SetMaterialTags(HMaterial material, uint32_t tag_count, const dmhash_t* tags);

    /*#
     * @name SetMaterialSampler
     * @param material [type:dmRender::HMaterial]
     * @param name_hash [type:dmhash_t]
     * @param unit [type:uint32_t]
     * @param u_wrap [type:dmGraphics::TextureWrap]
     * @param v_wrap [type:dmGraphics::TextureWrap]
     * @param w_wrap [type:dmGraphics::TextureWrap]
     * @param min_filter [type:dmGraphics::TextureFilter]
     * @param mag_filter [type:dmGraphics::TextureFilter]
     * @param max_anisotropy [type:float]
     * @return is_succeed [type:bool]
     */
    bool SetMaterialSampler(HMaterial material, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureWrap w_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy);

    /*#
     * @name SetMaterialSampler
     * @param material [type:dmRender::HMaterial]
     * @param name_hash [type:dmhash_t]
     * @param unit [type:uint32_t]
     * @param u_wrap [type:dmGraphics::TextureWrap]
     * @param v_wrap [type:dmGraphics::TextureWrap]
     * @param min_filter [type:dmGraphics::TextureFilter]
     * @param mag_filter [type:dmGraphics::TextureFilter]
     * @param max_anisotropy [type:float]
     * @return is_succeed [type:bool]
     */
    bool SetMaterialSampler(HMaterial material, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy);

    /*#
     * @name GetMaterialSampler
     */
    //! TODO: not implemented
    HSampler GetMaterialSampler(HMaterial material, uint32_t unit);

    /*#
     * @name GetMaterialSamplerNameHash
     * @param material [type:dmRender::HMaterial]
     * @param unit [type:uint32_t]
     * @return name_hash [type:dmhash_t]
     */
    dmhash_t GetMaterialSamplerNameHash(HMaterial material, uint32_t unit);

    /*#
     * @name GetMaterialSamplerUnit
     * @param material [type:dmRender::HMaterial]
     * @param name_hash [type:dmhash_t]
     * @return sampler_unit [type:uint32_t]
     */
    uint32_t GetMaterialSamplerUnit(HMaterial material, dmhash_t name_hash);

    /*#
     * @name ApplyMaterialConstants
     * @param render_context [type:dmRender::HRenderContext] Render context
     * @param material [type:dmRender::Material]
     * @param render_object [type:const dmRender::RenderObject*]
     */
    void ApplyMaterialConstants(HRenderContext render_context, HMaterial material, const RenderObject* render_object);

    /*#
     * @name ApplyMaterialSampler
     * @param render_context [type:dmRender::HRenderContext]
     * @param material [type:dmRender::HMaterial]
     * @param sampler [type:dmRender::HSampler]
     * @param value_index [type:uint8_t]
     * @param texture [type:dmGraphics::HTexture]
     */
    //! TODO: not implemented
    void ApplyMaterialSampler(HRenderContext render_context, HMaterial material, HSampler sampler, uint8_t value_index, dmGraphics::HTexture texture);

}

#endif /* DMSDK_RENDER_H */
