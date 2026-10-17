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

#include <dlib/hash.h>
#include <dmsdk/gamesys/gui.h>
#include <dmsdk/script/script.h>
#include <gui/gui.h>

static const char TYPE_NAME[] = "DAPCustom";

static void* CreateNode(const dmGameSystem::CompGuiNodeContext*, void*, dmGui::HScene, dmGui::HNode, uint32_t)
{
    return 0;
}

static void DestroyNode(const dmGameSystem::CompGuiNodeContext*, const dmGameSystem::CustomNodeCtx*)
{
}

static int NewNode(lua_State* L)
{
    dmGui::HScene scene = dmGui::LuaCheckScene(L);
    // Do not populate reverse hashes here: this exercises startup registration.
    uint32_t type = dmHashBufferNoReverse32(TYPE_NAME, sizeof(TYPE_NAME) - 1);
    dmGui::HNode node = dmGui::NewNode(scene, dmVMath::Point3(), dmVMath::Vector3(1), dmGui::NODE_TYPE_CUSTOM, type);
    dmGui::SetNodeEnabled(scene, node, false);
    dmGui::LuaPushNode(L, scene, node);
    return 1;
}

static dmGameObject::Result CreateType(const dmGameSystem::CompGuiNodeTypeCtx* ctx, dmGameSystem::CompGuiNodeType* type)
{
    dmGameSystem::CompGuiNodeTypeSetCreateFn(type, CreateNode);
    dmGameSystem::CompGuiNodeTypeSetDestroyFn(type, DestroyNode);
    lua_State* L = dmGameSystem::GetLuaState(ctx);
    const luaL_Reg methods[] = {{"new_debugger_test_node", NewNode}, {0, 0}};
    luaL_register(L, "gui", methods);
    lua_pop(L, 1);
    return dmGameObject::RESULT_OK;
}

DM_DECLARE_COMPGUI_NODE_TYPE(DAPGuiNode, TYPE_NAME, CreateType, 0)
