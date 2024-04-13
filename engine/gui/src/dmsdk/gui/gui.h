// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_GUI_H
#define DMSDK_GUI_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/script/script.h>


/*# Defold GUI system
 *
 * @document
 * @name Gui
 * @namespace dmGui
 * @path engine/gui/src/dmsdk/gui/gui.h
 */
namespace dmGui
{
    /*#
     * A handle to a gui context
     * @name HContext
     * @type typedef
     */
     typedef struct Context* HContext;

    /*#
     * A handle to a gui scene
     * @name HScene
     * @type typedef
     */
    typedef struct Scene* HScene;

    /*#
     * A handle to a gui node
     * @name HNode
     * @type typedef
     */
    typedef uint32_t HNode;

    /*#
     * A handle to a gui script
     * @name HScript
     * @type typedef
     */
    typedef struct Script* HScript;

    /*#
     * A handle to a texture source, which can be a pointer to a resource,
     * a dmGraphics::HTexture or a dynamic texture created from a gui script.
     * @name HTextureSource
     * @type typedef
     */
    typedef uint64_t HTextureSource;

    /*#
     * Invalid node handle
     * @name INVALID_HANDLE
     * @type HNode
     */
    const HNode INVALID_HANDLE = 0;


    /*#
     * @name Playback
     * @type enum
     * @member PLAYBACK_ONCE_FORWARD
     * @member PLAYBACK_ONCE_BACKWARD
     * @member PLAYBACK_ONCE_PINGPONG
     * @member PLAYBACK_LOOP_FORWARD
     * @member PLAYBACK_LOOP_BACKWARD
     * @member PLAYBACK_LOOP_PINGPONG
     * @member PLAYBACK_NONE
     */
    enum Playback
    {
        PLAYBACK_ONCE_FORWARD  = 0,
        PLAYBACK_ONCE_BACKWARD = 1,
        PLAYBACK_ONCE_PINGPONG = 2,
        PLAYBACK_LOOP_FORWARD  = 3,
        PLAYBACK_LOOP_BACKWARD = 4,
        PLAYBACK_LOOP_PINGPONG = 5,
        PLAYBACK_NONE = 6,
        PLAYBACK_COUNT = 7,
    };

    /*#
     * @name AdjustReference
     * @type enum
     * @member ADJUST_REFERENCE_PARENT
     * @member ADJUST_REFERENCE_DISABLED
     */
    enum AdjustReference
    {
        ADJUST_REFERENCE_LEGACY   = 0,
        ADJUST_REFERENCE_PARENT   = 1,
        ADJUST_REFERENCE_DISABLED = 2
    };

    /*#
     * This enum denotes what kind of texture type the m_Texture pointer is referencing.
     * @name NodeTextureType
     * @type enum
     * @member NODE_TEXTURE_TYPE_NONE
     * @member NODE_TEXTURE_TYPE_TEXTURE
     * @member NODE_TEXTURE_TYPE_TEXTURE_SET
     * @member NODE_TEXTURE_TYPE_DYNAMIC
     */
    enum NodeTextureType
    {
        NODE_TEXTURE_TYPE_NONE,
        NODE_TEXTURE_TYPE_TEXTURE,
        NODE_TEXTURE_TYPE_TEXTURE_SET,
        NODE_TEXTURE_TYPE_DYNAMIC
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here

    /*#
     * @name Result
     * @type enum
     * @member NODE_TYPE_BOX //!< 0,
     * @member NODE_TYPE_TEXT //!< 1,
     * @member NODE_TYPE_PIE //!< 2,
     * @member NODE_TYPE_TEMPLATE //!< 3,
     * @member NODE_TYPE_PARTICLEFX //!< 5,
     * @member NODE_TYPE_CUSTOM //!< 6,
     * @member NODE_TYPE_COUNT //!< 7,
     */
    enum NodeType
    {
        NODE_TYPE_BOX  = 0,
        NODE_TYPE_TEXT = 1,
        NODE_TYPE_PIE  = 2,
        NODE_TYPE_TEMPLATE = 3,
        NODE_TYPE_SPINE = 4, // Deprecated. Used in the ddf for loading old content
        NODE_TYPE_PARTICLEFX = 5,
        NODE_TYPE_CUSTOM = 6,
        NODE_TYPE_COUNT = 7,
    };


    /*#
     * @name Result
     * @type enum
     * @member RESULT_OK //!< 0
     * @member RESULT_SYNTAX_ERROR //!< -1
     * @member RESULT_SCRIPT_ERROR //!< -2
     * @member RESULT_OUT_OF_RESOURCES //!< -4
     * @member RESULT_RESOURCE_NOT_FOUND //!< -5
     * @member RESULT_TEXTURE_ALREADY_EXISTS //!< -6
     * @member RESULT_INVAL_ERROR //!< -7
     * @member RESULT_INF_RECURSION //!< -8
     * @member RESULT_DATA_ERROR //!< -9
     * @member RESULT_WRONG_TYPE //!< -10
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_SYNTAX_ERROR = -1,
        RESULT_SCRIPT_ERROR = -2,
        RESULT_OUT_OF_RESOURCES = -4,
        RESULT_RESOURCE_NOT_FOUND = -5,
        RESULT_TEXTURE_ALREADY_EXISTS = -6,
        RESULT_INVAL_ERROR = -7,
        RESULT_INF_RECURSION = -8,
        RESULT_DATA_ERROR = -9,
        RESULT_WRONG_TYPE = -10,
    };

    /*#
     * @name NewNode
     * @param scene [type:dmGui::HScene] the gui scene
     * @param position [type:dmVMath::Point3] the position
     * @param size [type:dmVMath::Vector3] the size
     * @param node_type [type:dmGui::NodeType] the node type
     * @param custom_type [type:uint32_t] If node_type == dmGui::NODE_TYPE_CUSTOM, then this is used to create a custom node data for the registered custom type
     * @return node [type: dmGui::HNode] the created node
     */
    HNode NewNode(HScene scene, const dmVMath::Point3& position, const dmVMath::Vector3& size, NodeType node_type, uint32_t custom_type);

    /*#
     * Defer delete a node
     * @name DeleteNode
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the node to delete
     */
    void DeleteNode(HScene scene, HNode node);

    /*#
     * Set the id of a node.
     * @note The id must be unique
     * @name SetNodeId
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the gui node
     * @param id [type:dmhash_t] the id
     */
    void SetNodeId(HScene scene, HNode node, dmhash_t id);

    /*#
     * Get the id of a node.
     * @name GetNodeId
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the gui node
     * @return id [type:dmhash_t] the id of the node
     */
    dmhash_t GetNodeId(HScene scene, HNode node);

    /*#
     * Set the parent of a gui node
     * @name SetNodeParent
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the gui node
     * @param parent [type:dmGui::HNode] the new parent. May be null
     * @param keep_scene_transform [type:bool] true to keep the world position
     * @return result [type:dmGui::Result] dmGui::RESULT_OK is successful
     */
    Result SetNodeParent(HScene scene, HNode node, HNode parent, bool keep_scene_transform);

    /*#
     * Get the parent of a gui node
     * @name GetNodeParent
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the gui node
     * @return parent [type:dmGui::HNode] the parent, or INVALID_HANDLE is unsuccessful
     */
    HNode GetNodeParent(HScene scene, HNode node);

    /*#
     * Get first child node
     * @name GetFirstChildNode
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] Gets the first child node. If 0, gets the first top level node.
     * @return child [type:dmGui::HNode] The first child node
     */
    HNode GetFirstChildNode(HScene scene, HNode node);

    /*#
     * Get next sibling
     * @name GetNextNode
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the current sibling
     * @return sibling [type:dmGui::HNode] the next sibling, or INVALID_HANDLE if no more siblings
     */
    HNode GetNextNode(HScene scene, HNode node);

    /*#
     * Query if the node is a bone
     * @name GetNodeIsBone
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the gui node
     * @return result [type:bool] true if the node is a bone
     */
    bool GetNodeIsBone(HScene scene, HNode node);

    /*#
     * Set the bone state of the node
     * @name SetNodeIsBone
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:dmGui::HNode] the gui node
     * @param is_bone [type:bool] true if the node is ot be used as a bone
     */
    void SetNodeIsBone(HScene scene, HNode node, bool is_bone);

    /*#
     * @name Property
     * @type enum
     * @member PROPERTY_POSITION    //!< 0
     * @member PROPERTY_ROTATION    //!< 1
     * @member PROPERTY_SCALE       //!< 2
     * @member PROPERTY_COLOR       //!< 3
     * @member PROPERTY_SIZE        //!< 4
     * @member PROPERTY_OUTLINE     //!< 5
     * @member PROPERTY_SHADOW      //!< 6
     * @member PROPERTY_SLICE9      //!< 7
     * @member PROPERTY_PIE_PARAMS  //!< 8
     * @member PROPERTY_TEXT_PARAMS //!< 9
     * @member PROPERTY_COUNT       //!< 10
     */
    enum Property
    {
        PROPERTY_POSITION   = 0,
        PROPERTY_ROTATION   = 1,
        PROPERTY_SCALE      = 2,
        PROPERTY_COLOR      = 3,
        PROPERTY_SIZE       = 4,
        PROPERTY_OUTLINE    = 5,
        PROPERTY_SHADOW     = 6,
        PROPERTY_SLICE9     = 7,
        PROPERTY_PIE_PARAMS = 8,
        PROPERTY_TEXT_PARAMS= 9,
        PROPERTY_EULER      = 10,
        PROPERTY_PREV_EULER = 11, // Used to automatically detect if the euler property has been animated

        PROPERTY_COUNT      = 12,
    };

    /*#
     * Get property value
     * @name GetNodeProperty
     * @param scene type: dmGui::HScene] scene
     * @param node type: dmGui::HNode] node
     * @param property [type: dmGui::Property] property enum
     * @return value [type: dmVMath::Vector4]
     */
    dmVMath::Vector4 GetNodeProperty(HScene scene, HNode node, Property property);

    /*#
     * Set property value
     * @name SetNodeProperty
     * @param scene type: dmGui::HScene] scene
     * @param node type: dmGui::HNode] node
     * @param property [type: dmGui::Property] property enum
     * @param value [type: dmVMath::Vector4]
     */
    void SetNodeProperty(HScene scene, HNode node, Property property, const dmVMath::Vector4& value);

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    /*#
     * @name AdjustMode
     * @type enum
     * @member ADJUST_MODE_FIT     //!< 0
     * @member ADJUST_MODE_ZOOM    //!< 1
     * @member ADJUST_MODE_STRETCH //!< 2
     */
    enum AdjustMode
    {
        ADJUST_MODE_FIT     = 0,
        ADJUST_MODE_ZOOM    = 1,
        ADJUST_MODE_STRETCH = 2,
    };

    /*#
     * Set adjust mode
     * @name SetNodeAdjustMode
     * @param scene type: dmGui::HScene] scene
     * @param node type: dmGui::HNode] node
     * @param adjust_mode [type: AdjustMode] the adjust mode
     */
    void SetNodeAdjustMode(HScene scene, HNode node, AdjustMode adjust_mode);

    /*# get node custom type
     * @name GetNodeCustomData
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:HNode] the gui node
     * @return type [type: uint32_t] the custom type. Or 0 if it is no custom type
     */
    uint32_t GetNodeCustomType(HScene scene, HNode node);

    /*# get node custom data
     * @name GetNodeCustomData
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:HNode] the gui node
     * @return data [type: void*] the custom data created per node by the gui node type extension
     */
    void* GetNodeCustomData(HScene scene, HNode node);

    /*# get node texture
     * @name GetNodeTextureId
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:HNode] the gui node
     * @return texture [type: dmhash_t] the currently assigned texture
     */
    dmhash_t GetNodeTextureId(HScene scene, HNode node);

    /*# set node texture
     * @name SetNodeTexture
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:HNode] the gui node
     * @param texture_id [type: dmhash_t] the texture id
     */
    Result SetNodeTexture(HScene scene, HNode node, dmhash_t texture_id);

    /*# set node texture
     * @name SetNodeTexture
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:HNode] the gui node
     * @param type [type:NodeTextureType] the type of texture
     * @param texture [type: void*] A pointer to a e.g. dmGameSystem::TextureSetResource*
     */
    Result SetNodeTexture(HScene scene, HNode node, NodeTextureType type, HTextureSource texture);

    // possibly use "add texture instead"

    /*#
     * Gets a resource by its resource alias.
     * @name GetResource
     * @param scene [type:dmGui::HScene] the gui scene
     * @param resource_id [type:dmhash_t] the resource alias
     * @param suffix_with_dot [type:dmhash_t] the hash of the suffix: hash(".spinescenec")
     * @return resource [type: void*] the resource if successful
     */
    void* GetResource(dmGui::HScene scene, dmhash_t resource_id, dmhash_t suffix_with_dot);

    // Scripting

    /*#
     * Pushes a dmGui::HNode to the stack
     * @name LuaPushNode
     * @param L [type:lua_State*] the Lua scene
     * @param scene [type:dmGui::HScene] the gui scene
     * @param node [type:HNode] the gui node
     */
    void LuaPushNode(lua_State* L, dmGui::HScene scene, dmGui::HNode node);

    HScene LuaCheckScene(lua_State* L);
    HNode LuaCheckNode(lua_State* L, int index);
}

#endif
