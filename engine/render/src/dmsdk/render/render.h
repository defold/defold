// Copyright 2020-2022 The Defold Foundation
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
 * [file:<dmsdk/render/render.h>]
 *
 * Api for render specific data
 *
 * @document
 * @name Render
 * @namespace dmRender
 */

namespace dmRender
{
    /*#
     * The render context
     * @typedef
     * @name HRenderContext
     */
    typedef struct RenderContext* HRenderContext;

    /*#
     * Material instance handle
     * @typedef
     * @name HMaterial
     */
    typedef struct Material* HMaterial;

    /*#
     * Font map handle
     * @typedef
     * @name HFontMap
     */
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
     * The maximum number of textures the render object can hold (currently 8)
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

        static const uint32_t MAX_TEXTURE_COUNT = 8;
        HNamedConstantBuffer            m_ConstantBuffer;

        dmVMath::Matrix4                m_WorldTransform;
        dmVMath::Matrix4                m_TextureTransform;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HIndexBuffer        m_IndexBuffer;
        HMaterial                       m_Material;
        dmGraphics::HTexture            m_Textures[MAX_TEXTURE_COUNT];
        dmGraphics::PrimitiveType       m_PrimitiveType;
        dmGraphics::Type                m_IndexType;
        dmGraphics::BlendFactor         m_SourceBlendFactor;
        dmGraphics::BlendFactor         m_DestinationBlendFactor;
        dmGraphics::FaceWinding         m_FaceWinding;
        StencilTestParams               m_StencilTestParams;
        uint32_t                        m_VertexStart;
        uint32_t                        m_VertexCount;
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
     * @param m_Order [type: uint32_t] the order to sort on (used if m_MajorOrder != RENDER_ORDER_WORLD)
     * @param m_BatchKey [type: uint32_t] the batch key to sort on (note: only 48 bits are currently used by renderer)
     * @param m_TagListKey [type: uint32_t] the key to the list of material tags
     * @param m_UserData [type: uint64_t] user data (available in the render dispatch callback)
     * @param m_MinorOrder [type: uint32_t:4] used to sort within a batch
     * @param m_MajorOrder [type: uint32_t:2] If RENDER_ORDER_WORLD, then sorting is done based on the world position.
                                              Otherwise the sorting uses the m_Order value directly.
     * @param m_Dispatch [type: uint32_t:8] The dispatch function callback (dmRender::HRenderListDispatch)
     */
    struct RenderListEntry
    {
        dmVMath::Point3 m_WorldPosition;
        uint64_t m_UserData; // E.g. component type instance pointer
        uint32_t m_Order;
        uint32_t m_BatchKey;
        uint32_t m_TagListKey;
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
     * @member RENDER_ORDER_WORLD           Used by game objects
     * @member RENDER_ORDER_AFTER_WORLD     Used by gui
     */
    enum RenderOrder
    {
        RENDER_ORDER_BEFORE_WORLD = 0, // not currently used, so let's keep it non documented
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
     * Visibility dispatch function callback.
     * @struct
     * @name RenderListVisibilityParams
     * @member m_UserData [type: void*] the callback user data (registered with RenderListMakeDispatch())
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
     * Render dispatch function callback.
     * @typedef
     * @name RenderListDispatchFn
     * @param params [type: dmRender::RenderListDispatchParams] the params
     */
    typedef void (*RenderListVisibilityFn)(RenderListVisibilityParams const &params);

    /*#
     * Render dispatch function callback.
     * @struct
     * @name RenderListDispatchParams
     * @member m_Context [type: dmRender::HRenderContext] the context
     * @member m_UserData [type: void*] the callback user data (registered with RenderListMakeDispatch())
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

    /*#
     * Register a render dispatch function
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
     * Allocates an array of render entries
     * @note Do not store a pointer into this array, as they're reused next frame
     * @name RenderListAlloc
     * @param context [type: dmRender::HRenderContext] the context
     * @param entries [type: uint32_t] the number of entries to allocate
     * @return array [type: dmRender::RenderListEntry*] the render list entry array
     */
    RenderListEntry* RenderListAlloc(HRenderContext context, uint32_t entries);

    /*#
     * Adds a render object to the current render frame
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
    int32_t GetConstantLocation(HConstant constant);

    /*#
     * Sets the shader program constant location
     * @name SetConstantLocation
     * @param constant [type: dmRender::HConstant] The shader constant
     * @param location [type: int32_t] the location
    */
    void SetConstantLocation(HConstant constant, int32_t location);

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
     * Currently only dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER and dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4
     * are supported.
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
}

#endif /* DMSDK_RENDER_H */
