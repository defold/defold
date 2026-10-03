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

#include "automation_private.h"
#include "automation_engine_access.h"

#include <math.h>
#include <gameobject/gameobject.h>
#include <gui.h>
#include <gamesys/components/comp_gui.h>
#include <dmsdk/dlib/hashtable.h>
#include <stdio.h>
#include <string.h>

namespace dmAutomation
{
    static dmHashTable64<uint32_t> g_NodeIndex;
    static const char* SceneNodeTypeToString(dmGameObject::SceneNodeType type)
    {
        switch (type)
        {
        case dmGameObject::SCENE_NODE_TYPE_COLLECTION:
            return "collectionc";
        case dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT:
            return "goc";
        case dmGameObject::SCENE_NODE_TYPE_COMPONENT:
            return "component";
        case dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT:
            return "subcomponent";
        default:
            return "element";
        }
    }

    static const char* KindFromType(dmGameObject::SceneNodeType scene_node_type, const char* type)
    {
        if (StartsWith(type, "gui_node_"))
        {
            return "gui_node";
        }
        if (StringsEqual(type, "goc") || scene_node_type == dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT)
        {
            return "game_object";
        }
        if (StringsEqual(type, "collectionc") || scene_node_type == dmGameObject::SCENE_NODE_TYPE_COLLECTION)
        {
            return "collection";
        }
        if (scene_node_type == dmGameObject::SCENE_NODE_TYPE_COMPONENT)
        {
            return "component";
        }
        if (scene_node_type == dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT)
        {
            return "subcomponent";
        }
        return "element";
    }

    static void SetAnchorFromPivot(Node* node, const char* pivot)
    {
        if (StringsEqual(pivot, "PIVOT_NW"))
        {
            node->m_AnchorX = 0.0f;
            node->m_AnchorY = 0.0f;
        }
        else if (StringsEqual(pivot, "PIVOT_N"))
        {
            node->m_AnchorX = 0.5f;
            node->m_AnchorY = 0.0f;
        }
        else if (StringsEqual(pivot, "PIVOT_NE"))
        {
            node->m_AnchorX = 1.0f;
            node->m_AnchorY = 0.0f;
        }
        else if (StringsEqual(pivot, "PIVOT_W"))
        {
            node->m_AnchorX = 0.0f;
            node->m_AnchorY = 0.5f;
        }
        else if (StringsEqual(pivot, "PIVOT_CENTER"))
        {
            node->m_AnchorX = 0.5f;
            node->m_AnchorY = 0.5f;
        }
        else if (StringsEqual(pivot, "PIVOT_E"))
        {
            node->m_AnchorX = 1.0f;
            node->m_AnchorY = 0.5f;
        }
        else if (StringsEqual(pivot, "PIVOT_SW"))
        {
            node->m_AnchorX = 0.0f;
            node->m_AnchorY = 1.0f;
        }
        else if (StringsEqual(pivot, "PIVOT_S"))
        {
            node->m_AnchorX = 0.5f;
            node->m_AnchorY = 1.0f;
        }
        else if (StringsEqual(pivot, "PIVOT_SE"))
        {
            node->m_AnchorX = 1.0f;
            node->m_AnchorY = 1.0f;
        }
    }

    static void FreeProperty(Property* property)
    {
        FreeString(&property->m_Name);
        FreeString(&property->m_Json);
        FreeString(&property->m_StringValue);
    }

    static bool SetPropertyJsonString(Property* property, const char* value)
    {
        StringBuffer json;
        StringBufferInit(&json);
        AppendJsonString(&json, value);
        free(property->m_Json);
        property->m_Json = StringBufferDetach(&json);
        return property->m_Json != 0;
    }

    static bool MakeProperty(Property* property, const dmGameObject::SceneNodeProperty* source)
    {
        memset(property, 0, sizeof(*property));
        if (!SetHashString(&property->m_Name, source->m_NameHash))
        {
            return false;
        }

        switch (source->m_Type)
        {
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH:
            if (!SetHashString(&property->m_StringValue, source->m_Value.m_Hash))
            {
                return false;
            }
            SetPropertyJsonString(property, property->m_StringValue);
            break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_NUMBER:
        {
            StringBuffer json;
            StringBufferInit(&json);
            AppendNumber(&json, source->m_Value.m_Number);
            property->m_Json = StringBufferDetach(&json);
            break;
        }
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN:
            property->m_BoolValue = source->m_Value.m_Bool;
            property->m_HasBool = true;
            SetString(&property->m_Json, source->m_Value.m_Bool ? "true" : "false");
            SetString(&property->m_StringValue, source->m_Value.m_Bool ? "true" : "false");
            break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_URL:
            SetString(&property->m_StringValue, source->m_Value.m_URL);
            SetPropertyJsonString(property, property->m_StringValue);
            break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_TEXT:
            SetString(&property->m_StringValue, source->m_Value.m_Text ? source->m_Value.m_Text : "");
            SetPropertyJsonString(property, property->m_StringValue);
            break;
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3:
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4:
        case dmGameObject::SCENE_NODE_PROPERTY_TYPE_QUAT:
        {
            property->m_HasVector = true;
            property->m_VectorCount = source->m_Type == dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3 ? 3 : 4;

            StringBuffer json;
            StringBufferInit(&json);
            StringBufferAppendChar(&json, '[');
            for (uint32_t i = 0; i < property->m_VectorCount; ++i)
            {
                property->m_Vector[i] = source->m_Value.m_V4[i];
                if (i > 0)
                {
                    StringBufferAppendChar(&json, ',');
                }
                AppendNumber(&json, property->m_Vector[i]);
            }
            StringBufferAppendChar(&json, ']');
            property->m_Json = StringBufferDetach(&json);
            break;
        }
        default:
            SetString(&property->m_Json, "null");
            break;
        }

        return property->m_Json != 0;
    }

    static void InitNode(Node* node)
    {
        memset(node, 0, sizeof(*node));
        node->m_Visible = true;
        node->m_Enabled = true;
        node->m_AnchorX = 0.5f;
        node->m_AnchorY = 0.5f;
    }

    static void FreeNode(Node* node)
    {
        FreeString(&node->m_Id);
        FreeString(&node->m_InstanceId);
        FreeString(&node->m_LogicalId);
        FreeString(&node->m_Name);
        FreeString(&node->m_Type);
        FreeString(&node->m_Kind);
        FreeString(&node->m_Path);
        FreeString(&node->m_Parent);
        FreeString(&node->m_Text);
        FreeString(&node->m_Url);
        FreeString(&node->m_Resource);
        FreeString(&node->m_AutomationId);
        FreeString(&node->m_LocalizationKey);
        FreeString(&node->m_Role);
        for (uint32_t i = 0; i < node->m_Properties.Size(); ++i)
        {
            FreeProperty(&node->m_Properties.Begin()[i]);
        }
        ArrayFree(&node->m_Properties);
        ArrayFree(&node->m_Children);
    }

    static void ApplyPropertyToNode(Node* node, const Property* property)
    {
        if (StringsEqual(property->m_Name, "id") && !IsEmpty(property->m_StringValue))
        {
            SetString(&node->m_Name, property->m_StringValue);
        }
        else if (StringsEqual(property->m_Name, "type") && !IsEmpty(property->m_StringValue))
        {
            SetString(&node->m_Type, property->m_StringValue);
        }
        else if (StringsEqual(property->m_Name, "text") && !IsEmpty(property->m_StringValue))
        {
            SetString(&node->m_Text, property->m_StringValue);
        }
        else if (StringsEqual(property->m_Name, "url") && !IsEmpty(property->m_StringValue))
        {
            SetString(&node->m_Url, property->m_StringValue);
        }
        else if (StringsEqual(property->m_Name, "resource") && !IsEmpty(property->m_StringValue))
        {
            SetString(&node->m_Resource, property->m_StringValue);
        }
        else if (StringsEqual(property->m_Name, "visible") && property->m_HasBool)
        {
            node->m_Visible = property->m_BoolValue;
        }
        else if (StringsEqual(property->m_Name, "enabled") && property->m_HasBool)
        {
            node->m_Enabled = property->m_BoolValue;
        }
        else if (StringsEqual(property->m_Name, "pivot") && !IsEmpty(property->m_StringValue))
        {
            SetAnchorFromPivot(node, property->m_StringValue);
        }

        if (property->m_HasVector && property->m_VectorCount >= 2)
        {
            if (StringsEqual(property->m_Name, "world_position") || (!node->m_HasPosition && StringsEqual(property->m_Name, "position")))
            {
                node->m_HasPosition = true;
                node->m_Position[0] = property->m_Vector[0];
                node->m_Position[1] = property->m_Vector[1];
                node->m_Position[2] = property->m_VectorCount >= 3 ? property->m_Vector[2] : 0.0f;
            }
            else if (StringsEqual(property->m_Name, "world_size") || (!node->m_HasSize && StringsEqual(property->m_Name, "size")))
            {
                node->m_HasSize = true;
                node->m_Size[0] = property->m_Vector[0];
                node->m_Size[1] = property->m_Vector[1];
                node->m_Size[2] = property->m_VectorCount >= 3 ? property->m_Vector[2] : 0.0f;
            }
        }
    }

    static bool IsUsableBounds(const Bounds* bounds)
    {
        return bounds->m_Valid &&
               IsFiniteFloat(bounds->m_X) &&
               IsFiniteFloat(bounds->m_Y) &&
               IsFiniteFloat(bounds->m_W) &&
               IsFiniteFloat(bounds->m_H) &&
               bounds->m_W >= 0.0f &&
               bounds->m_H >= 0.0f;
    }

    static bool HasClickableArea(const Bounds* bounds)
    {
        return IsUsableBounds(bounds) && bounds->m_W > 0.5f && bounds->m_H > 0.5f;
    }

    static bool ShouldUseChildBounds(const Node* node)
    {
        return !HasClickableArea(&node->m_Bounds);
    }

    static bool ChildCanContributeBounds(const Node* child)
    {
        return child->m_Visible && child->m_Enabled && HasClickableArea(&child->m_Bounds);
    }

    static void UseChildBoundsIfBetter(Snapshot* snapshot, uint32_t index)
    {
        if (!snapshot || index >= snapshot->m_Nodes.Size())
        {
            return;
        }

        Node* node = &snapshot->m_Nodes.Begin()[index];
        if (node->m_Bounds.m_Ambiguous || !ShouldUseChildBounds(node))
        {
            return;
        }

        float min_x = 0.0f;
        float min_y = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        bool has_bounds = false;

        for (uint32_t i = 0; i < node->m_Children.Size(); ++i)
        {
            uint32_t child_index = node->m_Children.Begin()[i];
            if (child_index >= snapshot->m_Nodes.Size())
            {
                continue;
            }

            const Node* child = &snapshot->m_Nodes.Begin()[child_index];
            if (child->m_Visible && child->m_Enabled && child->m_Bounds.m_Ambiguous)
            {
                node->m_Bounds.m_Valid = false;
                node->m_Bounds.m_Ambiguous = true;
                return;
            }
            if (!ChildCanContributeBounds(child))
            {
                continue;
            }

            const Bounds* bounds = &child->m_Bounds;
            float child_min_x = bounds->m_X;
            float child_min_y = bounds->m_Y;
            float child_max_x = bounds->m_X + bounds->m_W;
            float child_max_y = bounds->m_Y + bounds->m_H;

            if (!has_bounds)
            {
                min_x = child_min_x;
                min_y = child_min_y;
                max_x = child_max_x;
                max_y = child_max_y;
                has_bounds = true;
            }
            else
            {
                min_x = MinFloat(min_x, child_min_x);
                min_y = MinFloat(min_y, child_min_y);
                max_x = MaxFloat(max_x, child_max_x);
                max_y = MaxFloat(max_y, child_max_y);
            }
        }

        if (!has_bounds)
        {
            return;
        }

        node = &snapshot->m_Nodes.Begin()[index];
        node->m_Bounds.m_X = min_x;
        node->m_Bounds.m_Y = min_y;
        node->m_Bounds.m_W = MaxFloat(0.0f, max_x - min_x);
        node->m_Bounds.m_H = MaxFloat(0.0f, max_y - min_y);
        node->m_Bounds.m_CX = min_x + node->m_Bounds.m_W * 0.5f;
        node->m_Bounds.m_CY = min_y + node->m_Bounds.m_H * 0.5f;
        node->m_Bounds.m_NX = snapshot->m_WindowWidth > 0 ? node->m_Bounds.m_CX / (float)snapshot->m_WindowWidth : 0.0f;
        node->m_Bounds.m_NY = snapshot->m_WindowHeight > 0 ? node->m_Bounds.m_CY / (float)snapshot->m_WindowHeight : 0.0f;
        node->m_Bounds.m_Valid = true;
    }


    void InitSnapshot(Snapshot* snapshot)
    {
        memset(snapshot, 0, sizeof(*snapshot));
        snapshot->m_Root = -1;
    }

    void FreeSnapshot(Snapshot* snapshot)
    {
        for (uint32_t i = 0; i < snapshot->m_Nodes.Size(); ++i)
        {
            FreeNode(&snapshot->m_Nodes.Begin()[i]);
        }
        ArrayFree(&snapshot->m_Nodes);
        InitSnapshot(snapshot);
    }

    static int32_t BuildNode(Snapshot* snapshot, dmGameObject::SceneNode* scene_node, int32_t parent_index, const char* parent_path, uint32_t sibling_index)
    {
        Node node;
        InitNode(&node);

        dmGameObject::SceneNodePropertyIterator pit = dmGameObject::TraverseIterateProperties(scene_node);
        while (dmGameObject::TraverseIteratePropertiesNext(&pit))
        {
            Property property;
            if (MakeProperty(&property, &pit.m_Property))
            {
                ApplyPropertyToNode(&node, &property);
                if (!ArrayPush(&node.m_Properties, &property))
                {
                    FreeProperty(&property);
                }
            }
        }

        if (IsEmpty(node.m_Type))
        {
            SetString(&node.m_Type, SceneNodeTypeToString(scene_node->m_Type));
        }
        SetString(&node.m_Kind, KindFromType(scene_node->m_Type, node.m_Type));

        if (IsEmpty(node.m_Name))
        {
            char buffer[64];
            dmSnPrintf(buffer, sizeof(buffer), "%s_%u", node.m_Type, sibling_index);
            SetString(&node.m_Name, buffer);
        }

        if (IsEmpty(node.m_Url) && scene_node->m_Type != dmGameObject::SCENE_NODE_TYPE_COLLECTION)
        {
            StringBuffer url;
            StringBufferInit(&url);
            if (scene_node->m_Type == dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT)
            {
                const char* socket = dmMessage::GetSocketName(dmGameObject::GetMessageSocket(scene_node->m_Collection));
                StringBufferAppend(&url, socket);
                StringBufferAppendChar(&url, ':');
                StringBufferAppend(&url, dmHashReverseSafe64(dmGameObject::GetIdentifier(scene_node->m_Instance)));
            }
            else if (parent_index >= 0)
            {
                StringBufferAppend(&url, snapshot->m_Nodes[parent_index].m_Url);
                StringBufferAppendChar(&url, scene_node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT ? '#' : '/');
                StringBufferAppend(&url, node.m_Name);
            }
            node.m_Url = StringBufferDetach(&url);
        }
        ApplyNodeAnnotation(&node);

        const char* safe_segment = IsEmpty(node.m_Name) ? node.m_Type : node.m_Name;
        char segment[128];
        dmSnPrintf(segment, sizeof(segment), "%s[%u]", safe_segment, sibling_index);

        StringBuffer path;
        StringBufferInit(&path);
        if (IsEmpty(parent_path))
        {
            StringBufferAppendChar(&path, '/');
            StringBufferAppend(&path, segment);
        }
        else
        {
            StringBufferAppend(&path, parent_path);
            StringBufferAppendChar(&path, '/');
            StringBufferAppend(&path, segment);
        }
        node.m_Path = StringBufferDetach(&path);

        char seed[256];
        uint64_t object = scene_node->m_Type == dmGameObject::SCENE_NODE_TYPE_COLLECTION ? scene_node->m_Collection : scene_node->m_Instance;
        uint64_t component = scene_node->m_Type >= dmGameObject::SCENE_NODE_TYPE_COMPONENT ? (uintptr_t)scene_node->m_ComponentPrototype : 0;
        uint64_t version = component ? EngineAccessGetComponentVersion(scene_node) : 0;
        uint64_t subelement = 0;
        uint64_t scene_version = 0;
        if (scene_node->m_Type == dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT)
        {
            if (StringsEqual(node.m_Kind, "gui_node"))
            {
                dmGui::HScene scene = dmGameSystem::CompGuiGetScene(scene_node->m_Component);
                scene_version = scene ? EngineAccessGetResourceVersion(dmGui::GetSceneScript(scene)) : 0;
                subelement = scene_node->m_Node;
            }
            else
            {
                node.m_SnapshotScoped = true;
                scene_version = snapshot->m_Sequence;
                subelement = dmHashString64(node.m_Path);
            }
        }
        dmSnPrintf(seed, sizeof(seed), "%u:%llx:%llx:%llx:%llx:%llx:%llx", scene_node->m_Type,
                   (unsigned long long)scene_node->m_Collection, (unsigned long long)object,
                   (unsigned long long)component, (unsigned long long)version,
                   (unsigned long long)scene_version, (unsigned long long)subelement);
        char id[100];
        dmSnPrintf(id, sizeof(id), "%s-e%016llx", g_AutomationBridge.m_EngineInstanceId, (unsigned long long)dmHashString64(seed));
        SetString(&node.m_Id, id);
        SetString(&node.m_LogicalId, id);
        node.m_HasInstanceIdentity = scene_node->m_Type != dmGameObject::SCENE_NODE_TYPE_COLLECTION;
        node.m_CreatedSceneSequence = snapshot->m_Sequence;
        if (node.m_HasInstanceIdentity)
        {
            char instance_id[32];
            dmSnPrintf(instance_id, sizeof(instance_id), "%016llx", (unsigned long long)scene_node->m_Instance);
            SetString(&node.m_InstanceId, instance_id);
            uint32_t* previous_index = g_NodeIndex.Get(dmHashString64(id));
            if (previous_index && *previous_index < g_AutomationBridge.m_Snapshot.m_Nodes.Size())
                node.m_CreatedSceneSequence = g_AutomationBridge.m_Snapshot.m_Nodes.Begin()[*previous_index].m_CreatedSceneSequence;
        }

        if (parent_index >= 0 && (uint32_t)parent_index < snapshot->m_Nodes.Size())
        {
            SetString(&node.m_Parent, snapshot->m_Nodes.Begin()[parent_index].m_Id);
        }

        Bounds private_bounds = {};
        EngineAccessComputeBounds(scene_node, &node, snapshot, &private_bounds);
        node.m_Bounds = private_bounds;

        uint32_t index = snapshot->m_Nodes.Size();
        if (!ArrayPush(&snapshot->m_Nodes, &node))
        {
            FreeNode(&node);
            return -1;
        }

        memset(&node, 0, sizeof(node)); // ArrayPush transferred its owned fields.
        dmGameObject::SceneNodeIterator it = dmGameObject::TraverseIterateChildren(scene_node);
        uint32_t child_index = 0;
        while (dmGameObject::TraverseIterateNext(&it))
        {
            int32_t child = BuildNode(snapshot, &it.m_Node, (int32_t)index, snapshot->m_Nodes.Begin()[index].m_Path, child_index++);
            if (child >= 0)
            {
                uint32_t child_u32 = (uint32_t)child;
                ArrayPush(&snapshot->m_Nodes.Begin()[index].m_Children, &child_u32);
            }
        }

        UseChildBoundsIfBetter(snapshot, index);

        return (int32_t)index;
    }

    void RefreshScreenInfo(Snapshot* snapshot)
    {
        snapshot->m_DisplayWidth = g_AutomationBridge.m_DisplayWidth;
        snapshot->m_DisplayHeight = g_AutomationBridge.m_DisplayHeight;

        if (!g_AutomationBridge.m_GraphicsContext)
        {
            return;
        }

        snapshot->m_WindowWidth = dmGraphics::GetWindowWidth(g_AutomationBridge.m_GraphicsContext);
        snapshot->m_WindowHeight = dmGraphics::GetWindowHeight(g_AutomationBridge.m_GraphicsContext);
        // The public window size is the drawable framebuffer size on Defold's
        // graphics backends. GetWidth/GetHeight are the configured project
        // reference size and must not be mislabeled as backbuffer pixels.
        snapshot->m_BackbufferWidth = snapshot->m_WindowWidth;
        snapshot->m_BackbufferHeight = snapshot->m_WindowHeight;
        snapshot->m_DisplayScale = dmGraphics::GetDisplayScaleFactor(g_AutomationBridge.m_GraphicsContext);
        if (!IsFiniteFloat(snapshot->m_DisplayScale) || snapshot->m_DisplayScale <= 0.0f)
        {
            snapshot->m_DisplayScale = 1.0f;
        }
        dmGraphics::GetViewport(g_AutomationBridge.m_GraphicsContext, &snapshot->m_ViewportX, &snapshot->m_ViewportY, &snapshot->m_ViewportWidth, &snapshot->m_ViewportHeight);

        if (snapshot->m_WindowWidth == 0)
        {
            snapshot->m_WindowWidth = snapshot->m_ViewportWidth;
        }
        if (snapshot->m_WindowHeight == 0)
        {
            snapshot->m_WindowHeight = snapshot->m_ViewportHeight;
        }
    }

    void UpdateSnapshot()
    {
        Snapshot snapshot;
        InitSnapshot(&snapshot);
        snapshot.m_Sequence = g_AutomationBridge.m_Snapshot.m_Sequence + 1;
        snapshot.m_Frame = g_AutomationBridge.m_Frame;
        RefreshScreenInfo(&snapshot);

        if (g_AutomationBridge.m_Register)
        {
            dmGameObject::SceneNode root;
            memset(&root, 0, sizeof(root));
            if (dmGameObject::TraverseGetRoot(g_AutomationBridge.m_Register, &root))
            {
                snapshot.m_Root = BuildNode(&snapshot, &root, -1, "", 0);
            }
        }

        FreeSnapshot(&g_AutomationBridge.m_Snapshot);
        memcpy(&g_AutomationBridge.m_Snapshot, &snapshot, sizeof(snapshot));
        g_NodeIndex.Clear();
        uint32_t count = snapshot.m_Nodes.Size();
        if (g_NodeIndex.Capacity() < count) g_NodeIndex.SetCapacity(count * 2 + 1, count + 1);
        for (uint32_t i = 0; i < count; ++i) g_NodeIndex.Put(dmHashString64(snapshot.m_Nodes.Begin()[i].m_Id), i);
        memset(&snapshot, 0, sizeof(snapshot)); // Ownership moved to the current snapshot.
    }

    const Node* FindNodeById(const char* id)
    {
        if (IsEmpty(id)) return 0;
        const Snapshot* snapshot = &g_AutomationBridge.m_Snapshot;
        uint32_t* index = g_NodeIndex.Get(dmHashString64(id));
        if (!index || *index >= snapshot->m_Nodes.Size()) return 0;
        const Node* node = &snapshot->m_Nodes.Begin()[*index];
        return StringsEqual(node->m_Id, id) ? node : 0;
    }

    static void AppendBoundsJson(StringBuffer* out, const Bounds* bounds)
    {
        if (!bounds->m_Valid)
        {
            StringBufferAppend(out, "null");
            return;
        }

        StringBufferAppend(out, "{\"screen\":{\"x\":");
        AppendNumber(out, bounds->m_X);
        StringBufferAppend(out, ",\"y\":");
        AppendNumber(out, bounds->m_Y);
        StringBufferAppend(out, ",\"w\":");
        AppendNumber(out, bounds->m_W);
        StringBufferAppend(out, ",\"h\":");
        AppendNumber(out, bounds->m_H);
        StringBufferAppend(out, "},\"center\":{\"x\":");
        AppendNumber(out, bounds->m_CX);
        StringBufferAppend(out, ",\"y\":");
        AppendNumber(out, bounds->m_CY);
        StringBufferAppend(out, "},\"normalized\":{\"x\":");
        AppendNumber(out, bounds->m_NX);
        StringBufferAppend(out, ",\"y\":");
        AppendNumber(out, bounds->m_NY);
        StringBufferAppend(out, "}}");
    }

    void AppendNodeJson(StringBuffer* out, const Snapshot* snapshot, const Node* node, const IncludeOptions* include, bool recursive, bool visible_only)
    {
        StringBufferAppend(out, "{\"id\":");
        AppendJsonString(out, node->m_Id);
        StringBufferAppend(out, ",\"snapshot_id\":");
        AppendNumber(out, (double)snapshot->m_Sequence);
        StringBufferAppend(out, ",\"identity_scope\":");
        AppendJsonString(out, node->m_SnapshotScoped ? "snapshot" : "lifetime");
        StringBufferAppend(out, ",\"bounds_status\":");
        AppendJsonString(out, node->m_Bounds.m_Valid ? "available" : (node->m_Bounds.m_Ambiguous ? "ambiguous" : "unavailable"));
        StringBufferAppend(out, ",\"scene_sequence\":");
        AppendNumber(out, (double)snapshot->m_Sequence);
        StringBufferAppend(out, ",\"engine_frame\":");
        AppendNumber(out, (double)snapshot->m_Frame);
        if (node->m_HasInstanceIdentity)
        {
            StringBufferAppend(out, ",\"instance_id\":");
            AppendJsonString(out, node->m_InstanceId);
            StringBufferAppend(out, ",\"logical_id\":");
            AppendJsonString(out, node->m_LogicalId);
            StringBufferAppend(out, ",\"created_scene_sequence\":");
            AppendNumber(out, (double)node->m_CreatedSceneSequence);
        }
        StringBufferAppend(out, ",\"name\":");
        AppendJsonString(out, node->m_Name);
        StringBufferAppend(out, ",\"type\":");
        AppendJsonString(out, node->m_Type);
        StringBufferAppend(out, ",\"kind\":");
        AppendJsonString(out, node->m_Kind);
        StringBufferAppend(out, ",\"path\":");
        AppendJsonString(out, node->m_Path);
        StringBufferAppend(out, ",\"parent\":");
        if (IsEmpty(node->m_Parent))
        {
            StringBufferAppend(out, "null");
        }
        else
        {
            AppendJsonString(out, node->m_Parent);
        }
        StringBufferAppend(out, ",\"visible\":");
        StringBufferAppend(out, node->m_Visible ? "true" : "false");
        StringBufferAppend(out, ",\"enabled\":");
        StringBufferAppend(out, node->m_Enabled ? "true" : "false");

        if (!IsEmpty(node->m_Text))
        {
            StringBufferAppend(out, ",\"text\":");
            AppendJsonString(out, node->m_Text);
        }
        if (!IsEmpty(node->m_Url))
        {
            StringBufferAppend(out, ",\"url\":");
            AppendJsonString(out, node->m_Url);
        }
        if (!IsEmpty(node->m_Resource))
        {
            StringBufferAppend(out, ",\"resource\":");
            AppendJsonString(out, node->m_Resource);
        }
        if (!IsEmpty(node->m_AutomationId))
        {
            StringBufferAppend(out, ",\"automation_id\":");
            AppendJsonString(out, node->m_AutomationId);
        }
        if (!IsEmpty(node->m_LocalizationKey))
        {
            StringBufferAppend(out, ",\"localization_key\":");
            AppendJsonString(out, node->m_LocalizationKey);
        }
        if (!IsEmpty(node->m_Role))
        {
            StringBufferAppend(out, ",\"role\":");
            AppendJsonString(out, node->m_Role);
        }

        if (include->m_Bounds)
        {
            StringBufferAppend(out, ",\"bounds\":");
            AppendBoundsJson(out, &node->m_Bounds);
        }

        if (include->m_Properties)
        {
            StringBufferAppend(out, ",\"properties\":{");
            for (uint32_t i = 0; i < node->m_Properties.Size(); ++i)
            {
                if (i > 0)
                {
                    StringBufferAppendChar(out, ',');
                }
                AppendJsonString(out, node->m_Properties.Begin()[i].m_Name);
                StringBufferAppendChar(out, ':');
                StringBufferAppend(out, node->m_Properties.Begin()[i].m_Json);
            }
            StringBufferAppendChar(out, '}');
        }

        if (include->m_Children || recursive)
        {
            StringBufferAppend(out, ",\"children\":[");
            uint32_t emitted_children = 0;
            for (uint32_t i = 0; i < node->m_Children.Size(); ++i)
            {
                uint32_t child = node->m_Children.Begin()[i];
                if (child < snapshot->m_Nodes.Size())
                {
                    const Node* child_node = &snapshot->m_Nodes.Begin()[child];
                    if (visible_only && !child_node->m_Visible)
                    {
                        continue;
                    }
                    if (emitted_children > 0)
                    {
                        StringBufferAppendChar(out, ',');
                    }
                    AppendNodeJson(out, snapshot, child_node, include, recursive, visible_only);
                    ++emitted_children;
                }
            }
            StringBufferAppendChar(out, ']');
        }

        StringBufferAppendChar(out, '}');
    }

    void AppendScreenJson(StringBuffer* out, const Snapshot* snapshot)
    {
        int32_t viewport_top = (int32_t)snapshot->m_BackbufferHeight - snapshot->m_ViewportY - (int32_t)snapshot->m_ViewportHeight;
        StringBufferAppend(out, "{\"window\":{\"x\":0,\"y\":0,\"width\":");
        AppendNumber(out, snapshot->m_WindowWidth);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, snapshot->m_WindowHeight);
        StringBufferAppend(out, "},\"client\":{\"x\":0,\"y\":0,\"width\":");
        AppendNumber(out, snapshot->m_WindowWidth);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, snapshot->m_WindowHeight);
        StringBufferAppend(out, "},\"backbuffer\":{\"x\":0,\"y\":0,\"width\":");
        AppendNumber(out, snapshot->m_BackbufferWidth);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, snapshot->m_BackbufferHeight);
        StringBufferAppend(out, "},\"display\":{\"width\":");
        AppendNumber(out, snapshot->m_DisplayWidth);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, snapshot->m_DisplayHeight);
        StringBufferAppend(out, ",\"scale\":");
        AppendNumber(out, snapshot->m_DisplayScale);
        StringBufferAppend(out, "},\"project_content\":{\"x\":0,\"y\":0,\"width\":");
        AppendNumber(out, snapshot->m_DisplayWidth);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, snapshot->m_DisplayHeight);
        StringBufferAppend(out, "},\"display_pixels\":{\"x\":0,\"y\":0,\"width\":");
        AppendNumber(out, snapshot->m_WindowWidth);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, snapshot->m_WindowHeight);
        StringBufferAppend(out, ",\"scope\":\"window_drawable\"");
        StringBufferAppend(out, "},\"viewport\":{\"x\":");
        AppendNumber(out, snapshot->m_ViewportX);
        StringBufferAppend(out, ",\"y\":");
        AppendNumber(out, viewport_top);
        StringBufferAppend(out, ",\"width\":");
        AppendNumber(out, snapshot->m_ViewportWidth);
        StringBufferAppend(out, ",\"height\":");
        AppendNumber(out, snapshot->m_ViewportHeight);
        StringBufferAppend(out, "},\"display_scale\":");
        AppendNumber(out, snapshot->m_DisplayScale);
        StringBufferAppend(out, ",\"coordinates\":{\"origin\":\"top-left\",\"units\":\"pixels\",\"input_space\":\"window\",\"spaces\":[\"window\",\"client\",\"backbuffer\",\"viewport\",\"normalized_viewport\"]}}");
    }


}
