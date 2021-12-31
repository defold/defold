// Copyright 2020 The Defold Foundation
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

#ifndef DMSDK_GAMESYSTEM_GUI_H
#define DMSDK_GAMESYSTEM_GUI_H

#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gui/gui.h>
#include <gamesys/gui_ddf.h>

namespace dmBuffer
{
    struct StreamDeclaration;
}

/*# SDK Gui Component API documentation
 *
 * Built-in scripting functions.
 *
 * @document
 * @name GameSystem Gui
 * @namespace dmGameSystem
 * @path engine/gamesys/src/dmsdk/gamesys/gui.h
 */

namespace dmGameSystem
{
    /*#
     * Gui component node type create/destroy context
     * @struct CompGuiNodeTypeCtx
    */
    struct CompGuiNodeTypeCtx;

    /*#
     * Gui component node type
     * @struct GuiNodeType
    */
    struct CompGuiNodeType;

    /*#
     * Register a new component type
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    //dmGameObject::Result RegisterCompGuiNodeType(dmGameObject::HRegister regist, const CompGuiNodeType& type);


    /*#
     * Register a new component type
     * @name RegisterCompGuiNodeType
     * @param regist [type:dmGameObject::HRegister] Gameobject register
     * @param name [type:const char*] The custom type name
     * @param context [type:void*] A user defined context
     * @param gui_node_type [type:CompGuiNodeType**] (out) Returns the pointer to the gui node type if successful.
     * @return RESULT_OK on success
     */
    //dmGameObject::Result RegisterCompGuiNodeType(dmGameObject::HRegister regist, const char* name, void* context, CompGuiNodeType** gui_node_type);

    /*#
     * Deregisters a gui component node type
     * @name RegisterCompGuiNodeType
     */
    //dmGameObject::Result UnregisterCompGuiNodeType(dmGameObject::HRegister regist, const CompGuiNodeType* gui_node_type);

    /*#
     * Retrieves a registered component type given its resource type.
     * @name FindCompGuiNodeType
     * @param regist [type:dmGameObject::HRegister] Gameobject register
     * @param name [type:const char*] The custom type name
     * @return the registered component type or 0x0 if not found
     */
    //CompGuiNodeType* FindCompGuiNodeType(dmGameObject::HRegister regist, const char* name);

    /*#
     * @name GuiNodeTypeDestroyFunction
     * @type typedef
     */
    typedef dmGameObject::Result (*GuiNodeTypeCreateFunction)(const struct CompGuiNodeTypeCtx* ctx, CompGuiNodeType* type);

    /*#
     * @name GuiNodeTypeDestroyFunction
     * @type typedef
     */
    typedef dmGameObject::Result (*GuiNodeTypeDestroyFunction)(const struct CompGuiNodeTypeCtx* ctx, CompGuiNodeType* type);

    struct CompGuiNodeType;

    struct CompGuiNodeTypeDescriptor
    {
        CompGuiNodeTypeDescriptor*  m_Next;
        GuiNodeTypeCreateFunction   m_CreateFn;
        GuiNodeTypeDestroyFunction  m_DestroyFn;
        CompGuiNodeType*            m_NodeType;
        const char*                 m_Name;
        uint32_t                    m_NameHash;
    };

    /**
     * Register a new component type (Internal)
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    dmGameObject::Result RegisterCompGuiNodeTypeDescriptor(CompGuiNodeTypeDescriptor* desc, const char* name, GuiNodeTypeCreateFunction create_fn, GuiNodeTypeDestroyFunction destroy_fn);

    /**
    * Component type desc bytesize declaration. Internal
    */
    const size_t s_CompGuiNodeTypeDescBufferSize = 128;

    #define DM_COMPGUI_PASTE_SYMREG(x, y) x ## y
    #define DM_COMPGUI_PASTE_SYMREG2(x, y) DM_COMPGUI_PASTE_SYMREG(x,y)

    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_COMPGUI_TYPE(symbol, desc, name, type_create_fn, type_destroy_fn) extern "C" void __attribute__((constructor)) symbol () { \
            dmGameSystem::RegisterCompGuiNodeTypeDescriptor((struct dmGameSystem::CompGuiNodeTypeDescriptor*) &desc, name, type_create_fn, type_destroy_fn); \
        }
    #else
        #define DM_REGISTER_COMPGUI_TYPE(symbol, desc, name, type_create_fn, type_destroy_fn) extern "C" void symbol () { \
            dmGameSystem::RegisterCompGuiNodeTypeDescriptor((struct dmGameSystem::CompGuiNodeTypeDescriptor*) &desc, name, type_create_fn, type_destroy_fn); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif


    /*# Registers a new gui node type to the Gui component
     * @name DM_DECLARE_COMPGUI_TYPE
     * @type macro
     * @param symbol [type:C++ symbol name] The unique C++ symbol name
     * @param name [type:const char*] The name of the node type
     * @param type_create_fn [type:GuiNodeTypeCreateFunction] the create function
     * @param type_destroy_fn [type:GuiNodeTypeDestroyFunction] the destroy function. May be 0
     */
    #define DM_DECLARE_COMPGUI_NODE_TYPE(symbol, name, type_create_fn, type_destroy_fn) \
        uint8_t DM_ALIGNED(16) DM_COMPGUI_PASTE_SYMREG2(symbol, __LINE__)[dmGameSystem::s_CompGuiNodeTypeDescBufferSize]; \
        DM_REGISTER_COMPGUI_TYPE(symbol, DM_COMPGUI_PASTE_SYMREG2(symbol, __LINE__), name, type_create_fn, type_destroy_fn);

    struct CompGuiCreateContext
    {
        void* m_GetResourceContext;
        void* (*m_GetResourceFn)(void* get_resource_context, dmhash_t name_hash);
    };

    typedef void* (*CompGuiCreateNodeFn)(const CompGuiCreateContext* createctx, void* typecontext, dmGui::HNode node, uint32_t custom_type);
    typedef void  (*CompGuiDestroyNodeFn)(const CompGuiCreateContext* createctx, void* typecontext, void* node_data);
    typedef void  (*CompGuiSetPropertyFn)(const CompGuiCreateContext* createctx, void* typecontext, void* node_data, dmhash_t name_hash, const dmGuiDDF::PropertyVariant* variant);
    typedef void  (*CompGuiGetVerticesFn)(void* typecontext, void* node_data, uint32_t decl_size, dmBuffer::StreamDeclaration* decl, uint32_t struct_size, dmArray<uint8_t>& vertices);


    void CompGuiNodeTypeSetCreateNodeFn(CompGuiNodeType* type, CompGuiCreateNodeFn fn);
    void CompGuiNodeTypeSetDestroyNodeFn(CompGuiNodeType* type, CompGuiDestroyNodeFn fn);
    void CompGuiNodeTypeSetSetPropertyFn(CompGuiNodeType* type, CompGuiSetPropertyFn fn);
    /*#
     * Get the vertices in local space
     */
    void CompGuiNodeTypeSetGetVerticesFn(CompGuiNodeType* type, CompGuiGetVerticesFn fn);

}

#endif // DMSDK_GAMESYSTEM_GUI_H
