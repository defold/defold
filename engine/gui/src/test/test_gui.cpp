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

#include <map>
#include <string>
#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/dstrings.h>
#include <dlib/align.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dmsdk/dlib/vmath.h>
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

using namespace dmVMath;

extern unsigned char BUG352_LUA[];
extern uint32_t BUG352_LUA_SIZE;

#if defined(__NX__)
    #define MOUNTFS "host:/"
#else
    #define MOUNTFS ""
#endif

// Basic
//  - Create scene
//  - Create nodes
//  - Stress tests
//
// self table
//
// reload script
//
// lua script basics
//  - New/Delete node
//
// "Namespaces"
//
// Animation
//
// Render
//
// Adjust reference

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
    out_data->m_State.m_End = 1;
    out_data->m_State.m_FPS = 30;
    out_data->m_FlipHorizontal = 1;
    return dmGui::FETCH_ANIMATION_OK;
}

class dmGuiTest : public jc_test_base_class
{
public:
    dmScript::HContext m_ScriptContext;
    dmGui::HContext m_Context;
    dmGui::HScene m_Scene;
    dmMessage::HSocket m_Socket;
    dmGui::HScript m_Script;
    dmGui::RenderSceneParams m_RenderParams;
    std::map<std::string, dmGui::HNode> m_NodeTextToNode;
    std::map<std::string, Point3> m_NodeTextToRenderedPosition;
    std::map<std::string, Vector3> m_NodeTextToRenderedSize;

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

        // Bogus font for the metric callback to be run (not actually using the default font)
        dmGui::SetDefaultFont(m_Context, (void*)0x1);
        dmGui::NewSceneParams params;
        params.m_MaxNodes = MAX_NODES;
        params.m_MaxAnimations = MAX_ANIMATIONS;
        params.m_UserData = this;

        params.m_MaxParticlefxs = MAX_PARTICLEFXS;
        params.m_MaxParticlefx = MAX_PARTICLEFX;
        params.m_ParticlefxContext = dmParticle::CreateContext(MAX_PARTICLEFX, MAX_PARTICLES);
        params.m_FetchTextureSetAnimCallback = FetchTextureSetAnimCallback;
        params.m_OnWindowResizeCallback = 0x0;
        m_Scene = dmGui::NewScene(m_Context, &params);
        dmGui::SetSceneResolution(m_Scene, 1, 1);
        m_Script = dmGui::NewScript(m_Context);
        dmGui::SetSceneScript(m_Scene, m_Script);

        m_RenderParams.m_RenderNodes = RenderNodes;
        m_RenderParams.m_NewTexture = 0;
        m_RenderParams.m_DeleteTexture = 0;
        m_RenderParams.m_SetTextureData = 0;
    }

    static void RenderNodes(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
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
        dmParticle::DestroyContext(m_Scene->m_ParticlefxContext);
        dmGui::DeleteScript(m_Script);
        dmGui::DeleteScene(m_Scene);
        dmGui::DeleteContext(m_Context, m_ScriptContext);
        dmMessage::DeleteSocket(m_Socket);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
    }

private:
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
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
        ASSERT_NE((dmGui::HNode) 0, node);
    }

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    ASSERT_EQ((dmGui::HNode) 0, node);
    ASSERT_EQ(m_Script, dmGui::GetSceneScript(m_Scene));
}

// Test that a newly re-created node has default values
TEST_F(dmGuiTest, RecreateNodes)
{
    uint32_t n = MAX_NODES + 1;
    for (uint32_t i = 0; i < n; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
        ASSERT_NE((dmGui::HNode) 0, node);
        ASSERT_EQ(dmGui::PIVOT_CENTER, dmGui::GetNodePivot(m_Scene, node));
        dmGui::SetNodePivot(m_Scene, node, dmGui::PIVOT_E);
        ASSERT_EQ(dmGui::PIVOT_E, dmGui::GetNodePivot(m_Scene, node));

        dmGui::DeleteNode(m_Scene, node, true);
    }
}

TEST_F(dmGuiTest, Name)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    ASSERT_EQ(2, width);
    ASSERT_EQ(3, height);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(3,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_BOX, 0);
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

TEST_F(dmGuiTest, NodeTextureType)
{
    int t1, t2;
    void* raw_tex;
    dmGui::Result r;
    dmGui::NodeTextureType node_texture_type;
    uint64_t fb_id;

    // Test NODE_TEXTURE_TYPE_TEXTURE_SET: Create and get type
    r = dmGui::AddTexture(m_Scene, dmHashString64("t1"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(0,0,0), dmGui::NODE_TYPE_BOX, 0);
    ASSERT_NE((dmGui::HNode) 0, node);

    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    raw_tex = dmGui::GetNodeTexture(m_Scene, node, &node_texture_type);
    ASSERT_EQ(raw_tex, &t1);
    ASSERT_EQ(node_texture_type, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET);

    // Test NODE_TEXTURE_TYPE_TEXTURE_SET: Playing flipbook animation
    r = dmGui::PlayNodeFlipbookAnim(m_Scene, node, "ta1", 0.0f, 1.0f, 0x0);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(dmHashString64("ta1"), fb_id);

    // Test NODE_TEXTURE_TYPE_TEXTURE: Create and get type
    r = dmGui::AddTexture(m_Scene, dmHashString64("t2"), (void*) &t2, dmGui::NODE_TEXTURE_TYPE_TEXTURE, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::SetNodeTexture(m_Scene, node, "t2");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    raw_tex = dmGui::GetNodeTexture(m_Scene, node, &node_texture_type);
    ASSERT_EQ(raw_tex, &t2);
    ASSERT_EQ(node_texture_type, dmGui::NODE_TEXTURE_TYPE_TEXTURE);

    // Test NODE_TEXTURE_TYPE_TEXTURE: Playing flipbook animation should not work!
    r = dmGui::PlayNodeFlipbookAnim(m_Scene, node, "ta2", 0.0f, 1.0f, 0x0);
    ASSERT_EQ(r, dmGui::RESULT_INVAL_ERROR);
    ASSERT_EQ(0U, dmGui::GetNodeFlipbookAnimId(m_Scene, node));

    // Test NODE_TEXTURE_TYPE_NONE: Removing known texture should reset node texture types
    dmGui::RemoveTexture(m_Scene, dmHashString64("t2"));
    dmGui::GetNodeTexture(m_Scene, node, &node_texture_type);
    ASSERT_EQ(node_texture_type, dmGui::NODE_TEXTURE_TYPE_NONE);
}

TEST_F(dmGuiTest, SizeMode)
{
    int t1;
    dmGui::Result r;

    r = dmGui::AddTexture(m_Scene, dmHashString64("t1"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    int t1;
    dmGui::Result r;

    r = dmGui::AddTexture(m_Scene, dmHashString64("t1"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    ASSERT_NE((dmGui::HNode) 0, node);

    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    uint64_t fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(0U, dmGui::GetNodeFlipbookAnimId(m_Scene, node));

    r = dmGui::PlayNodeFlipbookAnim(m_Scene, node, "ta1", 0.0f, 1.0f, 0x0);
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
    ASSERT_EQ(0U, fb_id);

    r = dmGui::PlayNodeFlipbookAnim(m_Scene, node, "ta1", 0.0f, 1.0f, 0x0);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(dmHashString64("ta1"), fb_id);

    dmGui::ClearTextures(m_Scene);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(0U, fb_id);

    r = dmGui::AddTexture(m_Scene, dmHashString64("t2"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::SetNodeTexture(m_Scene, node, "t2");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    fb_id = dmGui::GetNodeFlipbookAnimId(m_Scene, node);
    ASSERT_EQ(0U, dmGui::GetNodeFlipbookAnimId(m_Scene, node));
}

TEST_F(dmGuiTest, TextureFontLayer)
{
    int t1, t2;
    int f1, f2;

    dmhash_t test_font_path = dmHashString64("test_font_path");

    dmGui::AddTexture(m_Scene, dmHashString64("t1"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    dmGui::AddTexture(m_Scene, dmHashString64("t2"), (void*) &t2, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    dmGui::AddFont(m_Scene, dmHashString64("f1"), &f1, 0);
    dmGui::AddFont(m_Scene, dmHashString64("f2"), &f2, 0);
    dmGui::AddFont(m_Scene, dmHashString64("test_font_id"), &f2, test_font_path);
    dmGui::AddLayer(m_Scene, "l1");
    dmGui::AddLayer(m_Scene, "l2");

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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

    dmGui::AddTexture(m_Scene, dmHashString64("t2"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(&t1, m_Scene->m_Nodes[node & 0xffff].m_Node.m_Texture);

    dmGui::RemoveTexture(m_Scene, dmHashString64("t2"));
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

    dmGui::AddFont(m_Scene, dmHashString64("f2"), &f1, 0);
    ASSERT_EQ(&f1, m_Scene->m_Nodes[node & 0xffff].m_Node.m_Font);

    dmGui::RemoveFont(m_Scene, dmHashString64("f2"));
    ASSERT_EQ((void*)0, m_Scene->m_Nodes[node & 0xffff].m_Node.m_Font);

    // Font path
    dmhash_t path_hash = dmGui::GetFontPath(m_Scene, dmHashString64("test_font_id"));
    ASSERT_EQ(test_font_path, path_hash);


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

static void DynamicRenderNodes(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    uint32_t* count = (uint32_t*) context;
    for (uint32_t i = 0; i < node_count; ++i) {
        dmGui::HNode node = nodes[i].m_Node;
        dmGui::NodeTextureType node_texture_type;
        dmhash_t id = dmGui::GetNodeTextureId(scene, node);
        if ((id == dmHashString64("t1") || id == dmHashString64("t2")) && dmGui::GetNodeTexture(scene, node, &node_texture_type)) {
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

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    for (uint32_t i = 0; i < count; ++i) {\
        ASSERT_EQ((exp)[i], (act)[i]);\
    }\

TEST_F(dmGuiTest, DynamicTextureFlip)
{
    const uint32_t width = 2;
    const uint32_t height = 2;

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
    int t1;
    dmGui::Result r;

    r = dmGui::AddTexture(m_Scene, dmHashString64("t1"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    const char* id = "n";
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    int t;
    int f;

    dmGui::AddTexture(m_Scene, dmHashString64("t"), (void*) &t, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    dmGui::AddFont(m_Scene, dmHashString64("f"), &f, 0);
    dmGui::AddLayer(m_Scene, "l");

    const char* id = "n";
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    ASSERT_NE((dmGui::HNode) 0, node);
    dmGui::SetNodeId(m_Scene, node, id);

    const char* id2 = "n2";
    node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX, 0);
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

        dmGui::HNode new_node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX, 0);
        ASSERT_NE((dmGui::HNode) 0, new_node);
        node_to_pos[new_node] = (float) i;
    }
}

TEST_F(dmGuiTest, ClearNodes)
{
    for (uint32_t i = 0; i < MAX_NODES; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX, 0);
        ASSERT_NE((dmGui::HNode) 0, node);
    }

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX, 0);
    ASSERT_EQ((dmGui::HNode) 0, node);

    dmGui::ClearNodes(m_Scene);
    for (uint32_t i = 0; i < MAX_NODES; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3((float) i, 0, 0), Vector3(0, 0 ,0), dmGui::NODE_TYPE_BOX, 0);
        ASSERT_NE((dmGui::HNode) 0, node);
    }
}

TEST_F(dmGuiTest, AnimateNode)
{
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    for (uint32_t i = 0; i < MAX_ANIMATIONS + 1; ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(0,0,0), dmGui::NODE_TYPE_BOX, 0);

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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.1f, 0, 0, 0, 0);

    ASSERT_NEAR(dmGui::GetNodePosition(m_Scene, node).getX(), 0.0f, EPSILON);
    dmGui::HNode node2 = 0;

    // Animation
    for (int i = 0; i < 60; ++i)
    {
        if (i == 30)
        {
            dmGui::DeleteNode(m_Scene, node, true);
            node2 = dmGui::NewNode(m_Scene, Point3(2,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    Point3 completed_position;
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyAnimationComplete, (void*)(uintptr_t) node, (void*)&completed_position);

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
    dmGui::AnimateNodeHash(scene, node, property, Vector4(0,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyPingPongComplete2, (void*)(uintptr_t) node, 0);
}

void MyPingPongComplete2(dmGui::HScene scene,
                         dmGui::HNode node,
                         bool finished,
                         void* userdata1,
                         void* userdata2)
{
    ++PingPongCount;
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyPingPongComplete1, (void*)(uintptr_t) node, 0);
}

TEST_F(dmGuiTest, PingPong)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmhash_t property = dmGui::GetPropertyHash(dmGui::PROPERTY_POSITION);
    dmGui::AnimateNodeHash(m_Scene, node, property, Vector4(1,0,0,0), dmEasing::Curve(dmEasing::TYPE_LINEAR), dmGui::PLAYBACK_ONCE_FORWARD, 1.0f, 0, &MyPingPongComplete1, (void*)(uintptr_t) node, 0);

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
    dmGui::HNode parent = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode child = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeParent(m_Scene, child, parent, false);
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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(10, 20, 30), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(100, 200, 300), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::Result r;
    int durations[9] = { 1, 1,  1, 0, 0,  0, -1, -1, -1 };
    int delays[9]     = { 1, 0, -1, 1, 0, -1,  0,  1, -1 };
    const char* s = "function cb(self, node)\n"
                    "    assert(self.foobar == 123)\n"
                    "    gui.set_color(node, vmath.vector4(1.0,0,0,0))\n"
                    "end\n;"
                    "function init(self)\n"
                    "    self.foobar = 123\n"
                    "    self.node = gui.get_node(\"n\")\n"
                    "    gui.animate(self.node, gui.PROP_POSITION, vmath.vector4(1,0,0,0), gui.EASING_NONE, %d, %d, cb, gui.PLAYBACK_ONCE_BACKWARD)\n"
                    "end\n";
    for (int i = 0; i < DM_ARRAY_SIZE(durations); ++i)
    {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
        dmGui::SetNodeId(m_Scene, node, "n");
        char buffer[1024];

        dmSnPrintf(buffer, sizeof(buffer), s, durations[i], delays[i]);

        r = dmGui::SetScript(m_Script, LuaSourceFromStr(buffer));
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

        Vector4 color = dmGui::GetNodeProperty(m_Scene, node, dmGui::PROPERTY_COLOR);
        ASSERT_NEAR(color.getX(), 1.0f, EPSILON);

        dmGui::DeleteNode(m_Scene, node, true);
    }

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    ASSERT_EQ(m_Scene->m_NodePool.Capacity(), m_Scene->m_NodePool.Remaining());
}

TEST_F(dmGuiTest, ScriptAnimatePreserveAlpha)
{
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node1 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode node2 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node1 = dmGui::NewNode(m_Scene, Point3(1,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode node2 = dmGui::NewNode(m_Scene, Point3(2,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    message->m_UserData1 = 0;
    message->m_UserData2 = 0;
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
    message->m_UserData1 = 0;
    message->m_UserData2 = 0;
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

    char DM_ALIGNED(16) buffer[256 + sizeof(dmMessage::Message)];
    dmMessage::Message* message = (dmMessage::Message*)buffer;
    message->m_Sender = dmMessage::URL();
    message->m_Receiver = dmMessage::URL();
    message->m_Id = dmHashString64("amessage");
    message->m_Descriptor = 0;
    message->m_UserData1 = 0;
    message->m_UserData2 = 0;

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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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

     // * NOTE: We create a node2 which animation duration is set to 0.5f
     // * Internally the animation will removed an "erased-swapped". Used to test that the last animation
     // * for node1 really invalidates the first animation of node1

    dmGui::HNode node1 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode node2 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);

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
    dmGui::AddFont(m_Scene, dmHashString64("big_score"), 0, 0);
    dmGui::AddFont(m_Scene, dmHashString64("score"), 0, 0);
    dmGui::AddTexture(m_Scene, dmHashString64("left_hud"), 0, dmGui::NODE_TEXTURE_TYPE_NONE, 1, 1);
    dmGui::AddTexture(m_Scene, dmHashString64("right_hud"), 0, dmGui::NODE_TEXTURE_TYPE_NONE, 1, 1);

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr((const char*)BUG352_LUA, BUG352_LUA_SIZE));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::SetScript(m_Script, LuaSourceFromStr((const char*)BUG352_LUA, BUG352_LUA_SIZE));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    char DM_ALIGNED(16) buffer[256 + sizeof(dmMessage::Message)];
    dmMessage::Message* message = (dmMessage::Message*)buffer;
    message->m_Sender = dmMessage::URL();
    message->m_Receiver = dmMessage::URL();
    message->m_Id = dmHashString64("inc_score");
    message->m_Descriptor = 0;
    message->m_UserData1 = 0;
    message->m_UserData2 = 0;

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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(width/2.0f, height/2.0f, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, n1, n1_name);

    dmGui::RenderScene(m_Scene, m_RenderParams, this);

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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, n1, n1_name);
    dmGui::SetNodeXAnchor(m_Scene, n1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, n1, dmGui::YANCHOR_BOTTOM);

    const char* n2_name = "n2";
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(width - 10.0f, height - 10.0f, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, n2, n2_name);
    dmGui::SetNodeXAnchor(m_Scene, n2, dmGui::XANCHOR_RIGHT);
    dmGui::SetNodeYAnchor(m_Scene, n2, dmGui::YANCHOR_TOP);

    dmGui::RenderScene(m_Scene, m_RenderParams, this);

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

    dmGui::RenderScene(m_Scene, m_RenderParams, this);

    // These tests the actual position of the cursor when rendering text so we need to adjust with the ref-scaled text metrics
    float ref_factor = dmMath::Min(ref_scale.getX(), ref_scale.getY());
    Point3 pos1 = m_NodeTextToRenderedPosition["n1"];
    ASSERT_EQ(10 * ref_scale.getX(), pos1.getX() + ref_factor * TEXT_GLYPH_WIDTH);
    ASSERT_EQ(10 * ref_scale.getY(), pos1.getY() + ref_factor * 0.5f * (TEXT_MAX_DESCENT + TEXT_MAX_ASCENT));

    Point3 pos2 = m_NodeTextToRenderedPosition["n2"];
    const float EPSILON = 0.0001f;
    ASSERT_NEAR(physical_width - 10.0f * ref_scale.getX(), pos2.getX() + ref_factor * TEXT_GLYPH_WIDTH, EPSILON);
    ASSERT_NEAR(physical_height - 10.0f * ref_scale.getY(), pos2.getY() + ref_factor * 0.5f * (TEXT_MAX_DESCENT + TEXT_MAX_ASCENT), EPSILON);
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
            dmGui::HNode center_node = dmGui::NewNode(m_Scene, Point3(pos_val, pos_val, 0), Vector3(size_val, size_val, 0), types[t], 0);
            dmGui::SetNodeText(m_Scene, center_node, center_name);
            dmGui::SetNodePivot(m_Scene, center_node, dmGui::PIVOT_CENTER);
            dmGui::SetNodeAdjustMode(m_Scene, center_node, mode);

            const char* bl_name = "bottom_left";
            dmGui::HNode bl_node = dmGui::NewNode(m_Scene, Point3(pos_val, pos_val, 0), Vector3(size_val, size_val, 0), types[t], 0);
            dmGui::SetNodeText(m_Scene, bl_node, bl_name);
            dmGui::SetNodePivot(m_Scene, bl_node, dmGui::PIVOT_SW);
            dmGui::SetNodeAdjustMode(m_Scene, bl_node, mode);

            const char* tr_name = "top_right";
            dmGui::HNode tr_node = dmGui::NewNode(m_Scene, Point3(pos_val, pos_val, 0), Vector3(size_val, size_val, 0), types[t], 0);
            dmGui::SetNodeText(m_Scene, tr_node, tr_name);
            dmGui::SetNodePivot(m_Scene, tr_node, dmGui::PIVOT_NE);
            dmGui::SetNodeAdjustMode(m_Scene, tr_node, mode);

            dmGui::RenderScene(m_Scene, m_RenderParams, this);

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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
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
    message->m_UserData1 = 0;
    message->m_UserData2 = 0;
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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);

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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);

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
    dmGui::SetSceneResolution(m_Scene, physical_width, physical_height);
    dmGui::SetDefaultResolution(m_Context, physical_width, physical_height);

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

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, Vector4 v) {
    return buffer + dmSnPrintf(buffer, buffer_len, "vector4(%.3f, %.3f, %.3f, %.3f)", v.getX(), v.getY(), v.getZ(), v.getW());
}

template <> int jc_test_cmp_EQ(Vector4 a, Vector4 b, const char* exprA, const char* exprB) {
    bool equal = a.getX() == b.getX() && a.getY() == b.getY() && a.getZ() == b.getZ() && a.getW() == b.getW();
    if (equal) return 1;
    jc_test_log_failure(a, b, exprA, exprB, "==");
    return 0;
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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n1, 0x1);

    pos = Point3(20, 20, 0);
    size = Vector3(15, 15, 0);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n2, 0x2);

    pos = Point3(30, 30, 0);
    size = Vector3(10, 10, 0);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n3, 0x3);

    dmGui::SetNodeParent(m_Scene, n2, n1, false);
    dmGui::SetNodeParent(m_Scene, n3, n2, false);

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

    ASSERT_EQ( Vector4(4, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_FIT);

    dmGui::CalculateNodeTransform(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform);

    ASSERT_EQ( Vector4(2, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(2, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(2, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_ZOOM);

    dmGui::CalculateNodeTransform(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform);

    ASSERT_EQ( Vector4(4, 4, 1, 1), nn1->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 4, 1, 1), nn2->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 4, 1, 1), nn3->m_Node.m_LocalAdjustScale );
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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n1, 0x1);

    pos = Point3(20, 20, 0);
    size = Vector3(15, 15, 0);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n2, 0x2);

    pos = Point3(30, 30, 0);
    size = Vector3(10, 10, 0);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n3, 0x3);

    dmGui::SetNodeParent(m_Scene, n2, n1, false);
    dmGui::SetNodeParent(m_Scene, n3, n2, false);

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

    ASSERT_EQ( Vector4(4, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_FIT);

    dmGui::CalculateNodeTransformAndAlphaCached(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform, opacity);

    ASSERT_EQ( Vector4(2, 2, 1, 1), nn1->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(2, 2, 1, 1), nn2->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(2, 2, 1, 1), nn3->m_Node.m_LocalAdjustScale );

    //
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n2, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n3, dmGui::ADJUST_MODE_ZOOM);

    dmGui::CalculateNodeTransformAndAlphaCached(m_Scene, nn3, dmGui::CalculateNodeTransformFlags(dmGui::CALCULATE_NODE_BOUNDARY | dmGui::CALCULATE_NODE_INCLUDE_SIZE | dmGui::CALCULATE_NODE_RESET_PIVOT), transform, opacity);

    ASSERT_EQ( Vector4(4, 4, 1, 1), nn1->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 4, 1, 1), nn2->m_Node.m_LocalAdjustScale );
    ASSERT_EQ( Vector4(4, 4, 1, 1), nn3->m_Node.m_LocalAdjustScale );
}

// Helper LUT to get readable form of adjustment mode.
static const char* g_AdjustModeString[] = {
    "FIT",
    "ZOOM",
    "STRETCH"
};

// Log parent and node adjustment mode.
// Makes it easier to see when and for what setup parenting might fail.
#define _LOG_PARENTING_INFO(parent, node) \
    { \
        dmLogInfo("Parent: %s, Child: %s", g_AdjustModeString[GetNode(m_Scene, parent)->m_Node.m_AdjustMode], g_AdjustModeString[GetNode(m_Scene, node)->m_Node.m_AdjustMode]); \
    }

// Helper function to get the scene position of a node.
// Same functionality as in gui_script.cpp: dmGui::LuaGetScreenPosition.
static Vector4 _GET_NODE_SCENE_POSITION(dmGui::HScene scene, dmGui::HNode node)
{
    dmGui::InternalNode* n = dmGui::GetNode(scene, node);
    Matrix4 node_transform;
    CalculateNodeTransform(scene, n, dmGui::CalculateNodeTransformFlags(), node_transform);
    Vector4 node_screen_pos = Vector4(node_transform.getCol3().getXYZ(), 0.0f);
    return node_screen_pos;
}

// Assert that the scene position for a node stays the same during;
// - parenting
// - unparenting (to root)
#define _ASSERT_PARENTING_POSITION(parent, node) \
    { \
        Vector4 expected = _GET_NODE_SCENE_POSITION(m_Scene, node); \
        dmGui::SetNodeParent(m_Scene, node, parent, true); \
        Vector4 actual_parented = _GET_NODE_SCENE_POSITION(m_Scene, node); \
        ASSERT_EQ( expected, actual_parented); \
        dmGui::SetNodeParent(m_Scene, node, dmGui::INVALID_HANDLE, true); \
        Vector4 actual_unparented = _GET_NODE_SCENE_POSITION(m_Scene, node); \
        ASSERT_EQ( expected, actual_unparented); \
    }

// Set adjustment mode for all "box sides" in dmGuiTest::ReparentKeepTrans
#define _SET_NODE_ADJUSTS(adjust_mode) \
    dmGui::SetNodeAdjustMode(m_Scene, n_root, adjust_mode); \
    dmGui::SetNodeAdjustMode(m_Scene, n_top, adjust_mode); \
    dmGui::SetNodeAdjustMode(m_Scene, n_right, adjust_mode); \
    dmGui::SetNodeAdjustMode(m_Scene, n_bottom, adjust_mode); \
    dmGui::SetNodeAdjustMode(m_Scene, n_left, adjust_mode);


TEST_F(dmGuiTest, ReparentKeepTrans)
{
    // Test parenting with keep transform for GUI nodes.
    // Setup a root node (centered) and four nodes at each side of the window.
    //
    //    +-----top---+
    //    |           |
    //  left   root  right
    //    |           |
    //    +---bottom--+
    //
    // Goes through a combination of different window resizes and adjustment modes.
    //
    // Verifies that the scene position for all nodes are the same as before and after
    // the parenting with the keep transform flag set.
    //
    dmGui::SetPhysicalResolution(m_Context, 100, 100);
    dmGui::SetDefaultResolution(m_Context, 100, 100);
    dmGui::SetSceneResolution(m_Scene, 100, 100);

    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);

    Point3 pos_root(50, 50, 0); // center of gui
    Vector3 size(10, 10, 0);
    dmGui::HNode n_root = dmGui::NewNode(m_Scene, pos_root, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_root, 0x1);

    Point3 pos_top(50, 100, 0); // top edge of gui
    dmGui::HNode n_top = dmGui::NewNode(m_Scene, pos_top, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_top, 0x2);

    Point3 pos_right(100, 50, 0); // right edge of gui
    dmGui::HNode n_right = dmGui::NewNode(m_Scene, pos_right, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_right, 0x3);

    Point3 pos_bottom(50, 0, 0); // bottom edge of gui
    dmGui::HNode n_bottom = dmGui::NewNode(m_Scene, pos_bottom, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_bottom, 0x4);

    Point3 pos_left(0, 50, 0); // left edge of gui
    dmGui::HNode n_left = dmGui::NewNode(m_Scene, pos_left, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_left, 0x5);


    //////////////////////////////////////////////////////////////////////////////////////
    // Window resolution: 1:1
    dmGui::SetPhysicalResolution(m_Context, 100, 100);

    // Adjust mode: STRETCH
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_STRETCH);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: FIT
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_FIT);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: ZOOM
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_ZOOM);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);


    //////////////////////////////////////////////////////////////////////////////////////
    // Window resolution: 2:1
    dmGui::SetPhysicalResolution(m_Context, 200, 100);

    // Adjust mode: STRETCH
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_STRETCH);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: FIT
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_FIT);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: ZOOM
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_ZOOM);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);


    //////////////////////////////////////////////////////////////////////////////////////
    // Window resolution: 2:1
    dmGui::SetPhysicalResolution(m_Context, 100, 200);

    // Adjust mode: STRETCH
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_STRETCH);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: FIT
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_FIT);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: ZOOM
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_ZOOM);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);


    //////////////////////////////////////////////////////////////////////////////////////
    // Window resolution: 2:2
    dmGui::SetPhysicalResolution(m_Context, 200, 200);

    // Adjust mode: STRETCH
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_STRETCH);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: FIT
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_FIT);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

    // Adjust mode: ZOOM
    _SET_NODE_ADJUSTS(dmGui::ADJUST_MODE_ZOOM);
    _LOG_PARENTING_INFO(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_top);
    _ASSERT_PARENTING_POSITION(n_root, n_right);
    _ASSERT_PARENTING_POSITION(n_root, n_bottom);
    _ASSERT_PARENTING_POSITION(n_root, n_left);

}

#undef _SET_NODE_ADJUSTS // Undef here, no need for it in remaining parenting tests

TEST_F(dmGuiTest, ReparentKeepTransDifferentAdjust)
{
    // Test parenting with keep transform for GUI nodes, using different
    // adjust modes for child and parent.
    //
    // Setup a root node (centered) and a child node at right side.
    //
    //    +-----------+
    //    |           |
    //    |   root  right
    //    |           |
    //    +-----------+
    //
    // Goes through a combination of different window resizes and adjustment modes combinations
    //
    // Verifies that the scene position for all nodes are the same as before and after
    // the parenting with the keep transform flag set.
    //
    dmGui::SetPhysicalResolution(m_Context, 100, 100);
    dmGui::SetDefaultResolution(m_Context, 100, 100);
    dmGui::SetSceneResolution(m_Scene, 100, 100);

    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);

    Point3 pos_root(50, 50, 0);
    Vector3 size(10, 10, 0);
    dmGui::HNode n_root = dmGui::NewNode(m_Scene, pos_root, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_root, 0x1);

    Point3 pos_child(100, 50, 0);
    dmGui::HNode n_child = dmGui::NewNode(m_Scene, pos_child, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_child, 0x2);

    dmGui::SetPhysicalResolution(m_Context, 200, 100);

    dmGui::SetNodeAdjustMode(m_Scene, n_root, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeAdjustMode(m_Scene, n_child, dmGui::ADJUST_MODE_FIT);
    _LOG_PARENTING_INFO(n_root, n_child);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetNodeAdjustMode(m_Scene, n_root, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n_child, dmGui::ADJUST_MODE_STRETCH);
    _LOG_PARENTING_INFO(n_root, n_child);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetNodeAdjustMode(m_Scene, n_root, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeAdjustMode(m_Scene, n_child, dmGui::ADJUST_MODE_ZOOM);
    _LOG_PARENTING_INFO(n_root, n_child);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetNodeAdjustMode(m_Scene, n_root, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n_child, dmGui::ADJUST_MODE_STRETCH);
    _LOG_PARENTING_INFO(n_root, n_child);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetNodeAdjustMode(m_Scene, n_root, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeAdjustMode(m_Scene, n_child, dmGui::ADJUST_MODE_ZOOM);
    _LOG_PARENTING_INFO(n_root, n_child);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetNodeAdjustMode(m_Scene, n_root, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeAdjustMode(m_Scene, n_child, dmGui::ADJUST_MODE_FIT);
    _LOG_PARENTING_INFO(n_root, n_child);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

}

TEST_F(dmGuiTest, ReparentKeepTransComplexTree)
{
    // Test parenting with keep transform for GUI nodes by parenting
    // to a tree of nodes with different adjustment modes.
    //
    // Setup a tree of nodes, each offsetted by (10,10) and the following modes:
    // STRETCH, ZOOM and FIT.
    //
    // Goes through different window resizes.
    //
    // Verifies that the scene position for all nodes are the same as before and after
    // the parenting with the keep transform flag set.
    //
    dmGui::SetPhysicalResolution(m_Context, 100, 100);
    dmGui::SetDefaultResolution(m_Context, 100, 100);
    dmGui::SetSceneResolution(m_Scene, 100, 100);

    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);

    Point3 pos_offset(10, 10, 0);
    Vector3 size(10, 10, 0);
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos_offset, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeId(m_Scene, n1, 0x1);

    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos_offset, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_ZOOM);
    dmGui::SetNodeId(m_Scene, n2, 0x2);

    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos_offset, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeAdjustMode(m_Scene, n1, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeId(m_Scene, n3, 0x3);

    Point3 pos_child(75, 75, 0);
    dmGui::HNode n_child = dmGui::NewNode(m_Scene, pos_child, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_child, 0x4);

    dmGui::SetPhysicalResolution(m_Context, 100, 100);
    _ASSERT_PARENTING_POSITION(n1, n_child);
    _ASSERT_PARENTING_POSITION(n2, n_child);
    _ASSERT_PARENTING_POSITION(n3, n_child);

    dmGui::SetPhysicalResolution(m_Context, 200, 100);
    _ASSERT_PARENTING_POSITION(n1, n_child);
    _ASSERT_PARENTING_POSITION(n2, n_child);
    _ASSERT_PARENTING_POSITION(n3, n_child);

    dmGui::SetPhysicalResolution(m_Context, 100, 200);
    _ASSERT_PARENTING_POSITION(n1, n_child);
    _ASSERT_PARENTING_POSITION(n2, n_child);
    _ASSERT_PARENTING_POSITION(n3, n_child);

    dmGui::SetPhysicalResolution(m_Context, 200, 200);
    _ASSERT_PARENTING_POSITION(n1, n_child);
    _ASSERT_PARENTING_POSITION(n2, n_child);
    _ASSERT_PARENTING_POSITION(n3, n_child);

}


TEST_F(dmGuiTest, ReparentKeepTransAnchoring)
{
    // Test parenting with keep transform for GUI nodes,
    // using different X and Y anchoring points.
    //
    // Setup a root node (centered) and a child node at right side for XAnchoring,
    // and top side YAnchoring.
    //
    //    +---(top)---+
    //    |           |
    //    |   root (right)
    //    |           |
    //    +-----------+
    //
    // Goes through a combination of different window resizes and anchoring settings.
    //
    // Verifies that the scene position for all nodes are the same as before and after
    // the parenting with the keep transform flag set.
    //
    dmGui::SetPhysicalResolution(m_Context, 100, 100);
    dmGui::SetDefaultResolution(m_Context, 100, 100);
    dmGui::SetSceneResolution(m_Scene, 100, 100);

    dmGui::SetSceneAdjustReference(m_Scene, dmGui::ADJUST_REFERENCE_PARENT);

    Point3 pos_root(50, 50, 0);
    Vector3 size(10, 10, 0);
    dmGui::HNode n_root = dmGui::NewNode(m_Scene, pos_root, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_root, 0x1);
    dmGui::SetNodeAdjustMode(m_Scene, n_root, dmGui::ADJUST_MODE_STRETCH);

    Point3 pos_child(100, 50, 0);
    dmGui::HNode n_child = dmGui::NewNode(m_Scene, pos_child, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n_child, 0x2);
    dmGui::SetNodeAdjustMode(m_Scene, n_child, dmGui::ADJUST_MODE_FIT);

    //////////////////////////////////////////////////////////////////////////////////////
    // Verify parenting with XAnchoring: LEFT
    dmGui::SetNodeXAnchor(m_Scene, n_child, dmGui::XANCHOR_LEFT);

    dmGui::SetPhysicalResolution(m_Context, 200, 100);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 100, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 200, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    //////////////////////////////////////////////////////////////////////////////////////
    // Verify parenting with XAnchoring: RIGHT
    dmGui::SetNodeXAnchor(m_Scene, n_child, dmGui::XANCHOR_RIGHT);

    dmGui::SetPhysicalResolution(m_Context, 200, 100);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 100, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 200, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    //////////////////////////////////////////////////////////////////////////////////////
    // Verify parenting with YAnchoring: TOP
    dmGui::SetNodeXAnchor(m_Scene, n_child, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, n_child, dmGui::YANCHOR_TOP);
    dmGui::SetNodePosition(m_Scene, n_child, Point3(50, 100, 0));

    dmGui::SetPhysicalResolution(m_Context, 200, 100);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 100, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 200, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    //////////////////////////////////////////////////////////////////////////////////////
    // Verify parenting with YAnchoring: BOTTOM
    dmGui::SetNodeYAnchor(m_Scene, n_child, dmGui::YANCHOR_BOTTOM);

    dmGui::SetPhysicalResolution(m_Context, 200, 100);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 100, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

    dmGui::SetPhysicalResolution(m_Context, 200, 200);
    _ASSERT_PARENTING_POSITION(n_root, n_child);

}

#undef _ASSERT_PARENTING_POSITION
#undef _LOG_PARENTING_INFO

// This render function simply flags a provided boolean when called
static void RenderEnabledNodes(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);

    // Initially enabled
    dmGui::InternalNode* node = dmGui::GetNode(m_Scene, n1);
    ASSERT_TRUE(node->m_Node.m_Enabled);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderEnabledNodes;

    // Test rendering
    bool rendered = false;
    dmGui::RenderScene(m_Scene, render_params, &rendered);
    ASSERT_TRUE(rendered);

    // Test no rendering when disabled
    dmGui::SetNodeEnabled(m_Scene, n1, false);
    rendered = false;
    dmGui::RenderScene(m_Scene, render_params, &rendered);
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

TEST_F(dmGuiTest, ScriptRecursiveEnabled)
{
    char buffer[512];
    const char* s = "function init(self)\n"
                    "    local pos = vmath.vector3(1,1,1)\n"
                    "    self.n1 = gui.new_text_node(pos, 1)\n"
                    "    self.n2 = gui.new_text_node(pos, 2)\n"
                    "    gui.set_parent(self.n2, self.n1)\n"
                    "    assert(gui.is_enabled(self.n1))\n"
                    "    gui.set_enabled(self.n1, false)\n"
                    "    assert(not gui.is_enabled(self.n1))\n"
                    "    assert(not gui.is_enabled(self.n2, true))\n" // n2 node enabled but n1 disabled
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
    ASSERT_STREQ("1", node->m_Node.m_Text); // make sure we found the right one
    ASSERT_FALSE(node->m_Node.m_Enabled);
    dmGui::InternalNode* node2 = &m_Scene->m_Nodes[1];
    ASSERT_STREQ("2", node2->m_Node.m_Text);
    ASSERT_TRUE(node2->m_Node.m_Enabled);
}

static void RenderNodesOrder(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    std::map<dmGui::HNode, uint16_t>* order = (std::map<dmGui::HNode, uint16_t>*)context;
    order->clear();
    for (uint32_t i = 0; i < node_count; ++i)
    {
        (*order)[nodes[i].m_Node] = (uint16_t)i;
    }
}

// Verify specific use cases of moving around nodes:
// - single node (nop)
//   - move to top
//   - move to self (up)
//   - move to bottom
//   - move to self (down)
// - two nodes
//   - initial order
//   - move to top
//   - move explicit to top
//   - move to bottom
//   - move explicit to bottom
// - three nodes
//   - move to top
//   - move from head to middle
//   - move from middle to tail
//   - move to bottom
//   - move from tail to middle
//   - move from middle to head

TEST_F(dmGuiTest, MoveNodes)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    std::map<dmGui::HNode, uint16_t> order;

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesOrder;

    // Edge case: single node
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to top
    dmGui::MoveNodeAbove(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to self
    dmGui::MoveNodeAbove(m_Scene, n1, n1);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to bottom
    dmGui::MoveNodeBelow(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    // Move to self
    dmGui::MoveNodeBelow(m_Scene, n1, n1);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);

    // Two nodes
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    // Move to top
    dmGui::MoveNodeAbove(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // Move explicit
    dmGui::MoveNodeAbove(m_Scene, n2, n1);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    // Move to bottom
    dmGui::MoveNodeBelow(m_Scene, n2, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // Move explicit
    dmGui::MoveNodeBelow(m_Scene, n1, n2);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);

    // Three nodes
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    // Move to top
    dmGui::MoveNodeAbove(m_Scene, n1, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(2u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    ASSERT_EQ(1u, order[n3]);
    // Move explicit from head to middle
    dmGui::MoveNodeAbove(m_Scene, n2, n3);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(2u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(0u, order[n3]);
    // Move explicit from middle to tail
    dmGui::MoveNodeAbove(m_Scene, n2, n1);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(2u, order[n2]);
    ASSERT_EQ(0u, order[n3]);
    // Move to bottom
    dmGui::MoveNodeBelow(m_Scene, n2, dmGui::INVALID_HANDLE);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(2u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    ASSERT_EQ(1u, order[n3]);
    // Move explicit from tail to middle
    dmGui::MoveNodeBelow(m_Scene, n1, n3);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    // Move explicit from middle to head
    dmGui::MoveNodeBelow(m_Scene, n1, n2);
    dmGui::RenderScene(m_Scene, render_params, &order);
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
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeId(m_Scene, n1, id1);
    const char* id2 = "n2";
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
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

static void RenderNodesCount(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
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

// Verify that the render count holds under random inserts, deletes and moves
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
        dmGui::NewNode(scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
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
            dmGui::NewNode(scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
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

        dmGui::RenderSceneParams render_params;
        render_params.m_RenderNodes = RenderNodesCount;
        dmGui::RenderScene(scene, render_params, &render_count);
        ASSERT_EQ(current_count, render_count);
        if (min_node_count > current_count)
            min_node_count = current_count;
        if (max_node_count < current_count)
            max_node_count = current_count;
    }
    printf("[STATS] current: %03d min: %03d max: %03d rel: %03d abs: %03d\n", current_count, min_node_count, max_node_count, relative_move_count, absolute_move_count);

    dmGui::DeleteScene(scene);
}

// Verify specific use cases of parenting nodes:
// - single node (nop)
//   - parent to nil
//   - parent to self
// - two nodes
//   - initial order
//   - parent first to second
//   - parent second to first
//   - unparent first
//   - parent second to first
// - three nodes
//   - initial order
//   - parent second to third

TEST_F(dmGuiTest, Parenting)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    std::map<dmGui::HNode, uint16_t> order;

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesOrder;

    // Edge case: single node
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    // parent to nil
    dmGui::SetNodeParent(m_Scene, n1, dmGui::INVALID_HANDLE, false);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    // parent to self
    dmGui::SetNodeParent(m_Scene, n1, n1, false);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);

    // Two nodes
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    // parent first to second
    dmGui::SetNodeParent(m_Scene, n1, n2, false);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // parent second to first
    dmGui::SetNodeParent(m_Scene, n2, n1, false);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // unparent first
    dmGui::SetNodeParent(m_Scene, n1, dmGui::INVALID_HANDLE, false);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
    // parent second to first
    dmGui::SetNodeParent(m_Scene, n2, n1, false);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);

    // Three nodes
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    // parent second to third
    dmGui::SetNodeParent(m_Scene, n2, n3, false);
    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(2u, order[n2]);
    ASSERT_EQ(1u, order[n3]);
}

void RenderNodesStoreTransform(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    dmVMath::Matrix4* out_transforms = (dmVMath::Matrix4*)context;
    memcpy(out_transforms, node_transforms, sizeof(dmVMath::Matrix4) * node_count);
}

#define ASSERT_MAT4(m1, m2)\
    for (uint32_t i = 0; i < 16; ++i)\
    {\
        int row = i / 4;\
        int col = i % 4;\
        ASSERT_NEAR(m1.getElem(row, col), m2.getElem(row, col), EPSILON);\
    }

// Verify that the rendered transforms are correct with VectorMath library as a reference
// n1 == dmVMath::Matrix4
TEST_F(dmGuiTest, NodeTransform)
{
    Vector3 size(1.0f, 1.0f, 1.0f);
    Vector3 pos(0.25f, 0.5f, 0.75f);
    dmVMath::Matrix4 transforms[1];
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(pos), size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodePivot(m_Scene, n1, dmGui::PIVOT_SW);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesStoreTransform;

    dmVMath::Matrix4 ref_mat;
    ref_mat = dmVMath::Matrix4::identity();
    ref_mat.setTranslation(pos);
    dmGui::RenderScene(m_Scene, render_params, transforms);
    ASSERT_MAT4(transforms[0], ref_mat);

    const float radians = 90.0f * M_PI / 180.0f;
    ref_mat *= dmVMath::Matrix4::rotation(radians * 0.50f, Vector3(0.0f, 1.0f, 0.0f));
    ref_mat *= dmVMath::Matrix4::rotation(radians * 1.00f, Vector3(0.0f, 0.0f, 1.0f));
    ref_mat *= dmVMath::Matrix4::rotation(radians * 0.25f, Vector3(1.0f, 0.0f, 0.0f));
    dmGui::SetNodeProperty(m_Scene, n1, dmGui::PROPERTY_ROTATION, Vector4(90.0f*0.25f, 90.0f*0.5f, 90.0f, 0.0f));
    dmGui::RenderScene(m_Scene, render_params, transforms);
    ASSERT_MAT4(transforms[0], ref_mat);

    ref_mat *= dmVMath::Matrix4::scale(Vector3(0.25f, 0.5f, 0.75f));
    dmGui::SetNodeProperty(m_Scene, n1, dmGui::PROPERTY_SCALE, Vector4(0.25f, 0.5f, 0.75f, 1.0f));
    dmGui::RenderScene(m_Scene, render_params, transforms);
    ASSERT_MAT4(transforms[0], ref_mat);
}

// Verify that the rendered transforms are correct for a hierarchy:
// - n1
//   - n2
//     - n3
//
// In three cases, the nodes have different pivots and positions, so that their render transforms will be identical:
// - n1 center, n2 center, n3 center
// - n1 south-west, n2 center, n3 south-west
// - n1 west, n2 east, n3 west
TEST_F(dmGuiTest, HierarchicalTransforms)
{
    // Setup
    Vector3 size(1, 1, 0);

    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX, 0);
    // parent first to second, second to third
    dmGui::SetNodeParent(m_Scene, n3, n2, false);
    dmGui::SetNodeParent(m_Scene, n2, n1, false);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesStoreTransform;

    dmVMath::Matrix4 transforms[3];

    dmGui::RenderScene(m_Scene, render_params, transforms);
    ASSERT_MAT4(transforms[0], transforms[1]);
    ASSERT_MAT4(transforms[0], transforms[2]);

    dmGui::SetNodePivot(m_Scene, n1, dmGui::PIVOT_SW);
    dmGui::SetNodePosition(m_Scene, n2, Point3(size * 0.5f));
    dmGui::SetNodePivot(m_Scene, n3, dmGui::PIVOT_SW);
    dmGui::SetNodePosition(m_Scene, n3, Point3(-size * 0.5f));
    dmGui::RenderScene(m_Scene, render_params, transforms);
    ASSERT_MAT4(transforms[0], transforms[1]);
    ASSERT_MAT4(transforms[0], transforms[2]);

    dmGui::SetNodePivot(m_Scene, n1, dmGui::PIVOT_W);
    dmGui::SetNodePivot(m_Scene, n2, dmGui::PIVOT_E);
    dmGui::SetNodePosition(m_Scene, n2, Point3(size.getX(), 0.0f, 0.0f));
    dmGui::SetNodePivot(m_Scene, n3, dmGui::PIVOT_W);
    dmGui::SetNodePosition(m_Scene, n3, Point3(-size.getY(), 0.0f, 0.0f));
    dmGui::RenderScene(m_Scene, render_params, transforms);
    ASSERT_MAT4(transforms[0], transforms[1]);
    ASSERT_MAT4(transforms[0], transforms[2]);
}

#undef ASSERT_MAT4

struct TransformColorData
{
    dmVMath::Matrix4 m_Transform;
    float m_Opacity;
};

void RenderNodesStoreOpacityAndTransform(dmGui::HScene scene, const dmGui::RenderEntry* nodes, const dmVMath::Matrix4* node_transforms, const float* node_opacities,
        const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
{
    TransformColorData* out_data = (TransformColorData*) context;
    for(uint32_t i = 0; i < node_count; i++)
    {
        out_data[i].m_Transform = node_transforms[i];
        out_data[i].m_Opacity = node_opacities[i];
    }
}

// Verify that the rendered colors are correct for a hierarchy:
// - n1
//   - n2
//   - n3
// - n4
//   - n5
//     - n6
//

TEST_F(dmGuiTest, HierarchicalColors)
{
    Vector3 size(1, 1, 0);

    dmGui::HNode node[6];
    const size_t node_count = sizeof(node)/sizeof(dmGui::HNode);

    for(uint32_t i = 0; i < node_count; ++i) {
        node[i] = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX, 0);
        dmGui::SetNodeInheritAlpha(m_Scene, node[i], true);
    }

    // test siblings
    dmGui::SetNodeParent(m_Scene, node[1], node[0], false);
    dmGui::SetNodeParent(m_Scene, node[2], node[0], false);
    dmGui::SetNodeProperty(m_Scene, node[0], dmGui::PROPERTY_COLOR, Vector4(0.5f, 0.5f, 0.5f, 0.5f));
    dmGui::SetNodeProperty(m_Scene, node[1], dmGui::PROPERTY_COLOR, Vector4(1.0f, 0.5f, 1.0f, 0.5f));
    dmGui::SetNodeProperty(m_Scene, node[2], dmGui::PROPERTY_COLOR, Vector4(1.0f, 1.0f, 1.0f, 0.25f));

    // test child tree
    dmGui::SetNodeParent(m_Scene, node[4], node[3], false);
    dmGui::SetNodeParent(m_Scene, node[5], node[4], false);
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

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesStoreOpacityAndTransform;
    dmGui::RenderScene(m_Scene, render_params, &cbres);

    // siblings
    ASSERT_EQ(0.5000f, cbres[0].m_Opacity);
    ASSERT_EQ(0.2500f, cbres[1].m_Opacity);
    ASSERT_EQ(0.1250f, cbres[2].m_Opacity);

    // tree
    ASSERT_EQ(0.5000f, cbres[3].m_Opacity);
    ASSERT_EQ(0.2500f, cbres[4].m_Opacity);
    ASSERT_EQ(0.0625f, cbres[5].m_Opacity);
}

// Test coherence of dmGui::RenderScene internal node-cache by adding, deleting nodes and altering node
// properties in two passes of rendering
//
// - n1
//   - n2
//     - n3
//       - n4
// - n5
//   - n6
//     - n7
//       - n8
//
// Render
// Change color and transform properties of n5-n8, delete n3, n4
// Render
//
TEST_F(dmGuiTest, SceneTransformCacheCoherence)
{
    Vector3 size(1, 1, 0);

    dmGui::HNode node[8];
    const size_t node_count = sizeof(node)/sizeof(dmGui::HNode);
    const size_t node_count_h = node_count/2;
    dmGui::HNode dummy_node[node_count];

    for(uint32_t i = 0; i < node_count; ++i)
    {
        dummy_node[i] = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0.0f), size, dmGui::NODE_TYPE_BOX, 0);
    }

    float a;
    a = 1.0f;
    for(uint32_t i = 0; i < node_count_h; ++i)
    {
        node[i] = dmGui::NewNode(m_Scene, Point3(1.0f, 1.0f, 1.0f), size, dmGui::NODE_TYPE_BOX, 0);
        dmGui::SetNodeInheritAlpha(m_Scene, node[i], true);
        dmGui::SetNodePivot(m_Scene, node[i], dmGui::PIVOT_SW);
        dmGui::SetNodeProperty(m_Scene, node[i], dmGui::PROPERTY_COLOR, Vector4(1, 1, 1, a));
        if(i == 0)
            a = 0.5f;
    }
    a = 0.5f;
    for(uint32_t i = node_count_h; i < node_count; ++i)
    {
        node[i] = dmGui::NewNode(m_Scene, Point3(0.5f, 0.5f, 0.5f), size, dmGui::NODE_TYPE_BOX, 0);
        dmGui::SetNodeInheritAlpha(m_Scene, node[i], true);
        dmGui::SetNodePivot(m_Scene, node[i], dmGui::PIVOT_SW);
        dmGui::SetNodeProperty(m_Scene, node[i], dmGui::PROPERTY_COLOR, Vector4(1, 1, 1, a));
        if(i == node_count_h)
            a = 0.5f;
    }
    for(uint32_t i = 1; i < node_count_h; ++i)
    {
        dmGui::SetNodeParent(m_Scene, node[i], node[i-1], false);
        dmGui::SetNodeParent(m_Scene, node[i+(node_count_h)], node[(i+(node_count_h))-1], false);
    }

    for(uint32_t i = 0; i < node_count; ++i)
    {
        DeleteNode(m_Scene, dummy_node[i], true);
    }

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesStoreOpacityAndTransform;

    TransformColorData cbres[node_count];
    memset(cbres, 0x0, sizeof(TransformColorData)*node_count);
    dmGui::RenderScene(m_Scene, render_params, &cbres);

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
    dmGui::RenderScene(m_Scene, render_params, &cbres);

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
    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,0), dmGui::NODE_TYPE_BOX, 0);
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

// Verify layer rendering order.
// Hierarchy:
// - n1 (l1)
// - n2
TEST_F(dmGuiTest, LayerRendering)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    dmGui::AddLayer(m_Scene, "l1");

    std::map<dmGui::HNode, uint16_t> order;

    // Initial case
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesOrder;

    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);

    // Reverse
    dmGui::SetNodeLayer(m_Scene, n1, "l1");

    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(1u, order[n1]);
    ASSERT_EQ(0u, order[n2]);
}

// Verify layer rendering order.
// Hierarchy:
// - n1 (l1)
//   - n2
// - n3 (l2)
//   - n4
// Layers:
// - l1
// - l2
//
// - initial order: n1, n2, n3, n4
// - reverse layer order: n3, n4, n1, n2
TEST_F(dmGuiTest, LayerRenderingHierarchies)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    dmGui::AddLayer(m_Scene, "l1");
    dmGui::AddLayer(m_Scene, "l2");

    std::map<dmGui::HNode, uint16_t> order;

    // Initial case
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeLayer(m_Scene, n1, "l1");
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeParent(m_Scene, n2, n1, false);
    dmGui::HNode n3 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeLayer(m_Scene, n3, "l2");
    dmGui::HNode n4 = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeParent(m_Scene, n4, n3, false);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesOrder;

    dmGui::RenderScene(m_Scene, render_params, &order);
    ASSERT_EQ(0u, order[n1]);
    ASSERT_EQ(1u, order[n2]);
    ASSERT_EQ(2u, order[n3]);
    ASSERT_EQ(3u, order[n4]);

    // Reverse
    dmGui::SetNodeLayer(m_Scene, n1, "l2");
    dmGui::SetNodeLayer(m_Scene, n3, "l1");
    dmGui::RenderScene(m_Scene, render_params, &order);
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

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesCount;

    uint32_t count;

    // Edge case: single node
    dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode parent = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode child = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeParent(m_Scene, child, parent, false);
    dmGui::RenderScene(m_Scene, render_params, &count);
    ASSERT_EQ(3u, count);

    dmGui::SetNodeEnabled(m_Scene, parent, false);
    dmGui::RenderScene(m_Scene, render_params, &count);

    ASSERT_EQ(1u, count);
}

TEST_F(dmGuiTest, TurnOffNodeVisibility)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesCount;

    uint32_t count;

    // Edge case: single node
    dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode parent = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode child = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeParent(m_Scene, child, parent, false);
    dmGui::RenderScene(m_Scene, render_params, &count);
    ASSERT_EQ(3u, count);

    dmGui::SetNodeVisible(m_Scene, parent, false);
    dmGui::RenderScene(m_Scene, render_params, &count);

    ASSERT_EQ(2u, count);
}

TEST_F(dmGuiTest, DeleteTree)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size * 0.5f);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesCount;

    uint32_t count;

    dmGui::HNode parent = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode child = dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);

    dmGui::SetNodeParent(m_Scene, child, parent, false);
    dmGui::RenderScene(m_Scene, render_params, &count);
    ASSERT_EQ(2u, count);

    dmGui::DeleteNode(m_Scene, parent, true);
    dmGui::RenderScene(m_Scene, render_params, &count);
    ASSERT_EQ(0u, count);
    ASSERT_EQ(m_Scene->m_NodePool.Remaining(), m_Scene->m_NodePool.Capacity());
}

TEST_F(dmGuiTest, PhysResUpdatesTransform)
{
    // Setup
    Vector3 size(10, 10, 0);
    Point3 pos(size);

    dmGui::RenderSceneParams render_params;
    render_params.m_RenderNodes = RenderNodesStoreTransform;

    dmGui::NewNode(m_Scene, pos, size, dmGui::NODE_TYPE_BOX, 0);

    Matrix4 transform;
    dmGui::RenderScene(m_Scene, render_params, &transform);

    Matrix4 next_transform;
    dmGui::RenderScene(m_Scene, render_params, &next_transform);

    Vector4 p = transform.getCol3();
    Vector4 next_p = next_transform.getCol3();
    ASSERT_LT(lengthSqr(p - next_p), EPSILON);

    dmGui::SetPhysicalResolution(m_Context, 10, 10);
    dmGui::SetSceneResolution(m_Scene, 1, 1);
    dmGui::RenderScene(m_Scene, render_params, &next_transform);

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
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_FIT);

    dmGui::SetNodeParent(m_Scene, node_level1, node_level0, false);

    // update
    dmGui::RenderScene(m_Scene, m_RenderParams, this);


    //        before resize:
    //        a----------------------+
    //        |                      |
    //        |  b---------------+   |
    //        |  |[c]  <-80->    |   |
    //        |10+---------------+   |
    //        | 10                   |
    //        +----------------------+
    //
    //        after resize:
    //        a---------------------------------------------+
    //        |                                             |
    //        |    b----------------------------------+     |
    //        |    |[c]           <-160->             |     |
    //        | 20 +----------------------------------+     |
    //        |   10                                        |
    //        +---------------------------------------------+
    //
    //
    //        a: window (ADJUST_REFERENCE_PARENT)
    //        b: node_level_0 (ADJUST_MOD_STRETCH)
    //        c: node_level_1 (parent: b, ADJUST_MODE_FIT)
    //
    //        => node c should not resize or offset inside b

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
    dmGui::HNode node_levelB = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_levelB, "node_levelB");
    dmGui::SetNodePivot(m_Scene, node_levelB, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_levelB, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_levelB, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_levelB, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_levelC = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_levelC, "node_levelC");
    dmGui::SetNodePivot(m_Scene, node_levelC, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_levelC, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_levelC, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_levelC, dmGui::ADJUST_MODE_FIT);

    dmGui::SetNodeParent(m_Scene, node_levelC, node_levelB, false);

    // update
    dmGui::RenderScene(m_Scene, m_RenderParams, this);


    //        before resize:
    //        A----------------------+
    //        |                      |
    //        |  B---------------+   |
    //        |  |[C]  <-80->    |   |
    //        |10+---------------+   |
    //        | 10                   |
    //        +----------------------+
    //
    //        after resize:
    //        A---------------------------------------------+
    //        |                                             |
    //        |  B---------------+                          |
    //        |  |[C]  <-80->    |                          |
    //        |10+---------------+                          |
    //        |   10                                        |
    //        +---------------------------------------------+
    //
    //
    //        A: window (ADJUST_REFERENCE_DISABLED)
    //        B: node_levelB (ADJUST_MOD_STRETCH)
    //        C: node_levelC (parent: B, ADJUST_MODE_FIT)
    //
    //        => neither node B nor C should resize, since we have disabled automatic adjustments

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
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_STRETCH);
    dmGui::SetNodeParent(m_Scene, node_level1, node_level0, false);

    dmGui::HNode node_level2 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level2, "node_level2");
    dmGui::SetNodePivot(m_Scene, node_level2, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level2, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level2, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeParent(m_Scene, node_level2, node_level1, false);

    // update
    dmGui::RenderScene(m_Scene, m_RenderParams, this);

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
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(50.0f, 25.0f, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_CENTER);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(50, 50, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_CENTER);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeParent(m_Scene, node_level1, node_level0, false);

    dmGui::HNode node_level2 = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level2, "node_level2");
    dmGui::SetNodePivot(m_Scene, node_level2, dmGui::PIVOT_CENTER);
    dmGui::SetNodeXAnchor(m_Scene, node_level2, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, node_level2, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, node_level2, dmGui::ADJUST_MODE_FIT);
    dmGui::SetNodeParent(m_Scene, node_level2, node_level1, false);

    // update
    dmGui::RenderScene(m_Scene, m_RenderParams, this);

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

    dmGui::HNode root_node = dmGui::NewNode(m_Scene, Point3(0.0f, 0.0f, 0), Vector3(1024, 768, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, root_node, "root_node");
    dmGui::SetNodePivot(m_Scene, root_node, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, root_node, dmGui::XANCHOR_NONE);
    dmGui::SetNodeYAnchor(m_Scene, root_node, dmGui::YANCHOR_NONE);
    dmGui::SetNodeAdjustMode(m_Scene, root_node, dmGui::ADJUST_MODE_STRETCH);

    const char* n1_name = "n1";
    dmGui::HNode n1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, n1, n1_name);
    dmGui::SetNodeXAnchor(m_Scene, n1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, n1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeParent(m_Scene, n1, root_node, false);

    const char* n2_name = "n2";
    dmGui::HNode n2 = dmGui::NewNode(m_Scene, Point3(width - 10.0f, height - 10.0f, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, n2, n2_name);
    dmGui::SetNodeXAnchor(m_Scene, n2, dmGui::XANCHOR_RIGHT);
    dmGui::SetNodeYAnchor(m_Scene, n2, dmGui::YANCHOR_TOP);
    dmGui::SetNodeParent(m_Scene, n2, root_node, false);

    dmGui::RenderScene(m_Scene, m_RenderParams, this);

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
    dmGui::HNode node_level0 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(80, 30, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level0, "node_level0");
    dmGui::SetNodePivot(m_Scene, node_level0, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level0, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level0, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level0, dmGui::ADJUST_MODE_STRETCH);

    dmGui::HNode node_level1 = dmGui::NewNode(m_Scene, Point3(10, 10, 0), Vector3(10, 10, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::SetNodeText(m_Scene, node_level1, "node_level1");
    dmGui::SetNodePivot(m_Scene, node_level1, dmGui::PIVOT_SW);
    dmGui::SetNodeXAnchor(m_Scene, node_level1, dmGui::XANCHOR_LEFT);
    dmGui::SetNodeYAnchor(m_Scene, node_level1, dmGui::YANCHOR_BOTTOM);
    dmGui::SetNodeAdjustMode(m_Scene, node_level1, dmGui::ADJUST_MODE_FIT);

    dmGui::SetNodeParent(m_Scene, node_level1, node_level0, false);

    // We scale node_level0 (root node) to half of its height.
    // => This means that the child (node_level1) will also be half in height,
    //    still obeying the adjust mode related scaling of the parent.
    dmGui::SetNodeProperty(m_Scene, node_level0, dmGui::PROPERTY_SCALE, Vector4(1.0f, 0.5f, 1.0f, 1.0f));

    // update
    dmGui::RenderScene(m_Scene, m_RenderParams, this);

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

bool LoadParticlefxPrototype(const char* filename, dmParticle::HPrototype* prototype)
{
    char path[64];
    dmSnPrintf(path, 64, MOUNTFS "build/src/test/%s", filename);
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
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE, 0);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT, 0);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));

    ASSERT_EQ(1U, dmGui::GetParticlefxCount(m_Scene));
    dmGui::DeleteNode(m_Scene, node_pfx, false);
    dmGui::DeleteNode(m_Scene, node_box, false);
    dmGui::DeleteNode(m_Scene, node_pie, false);
    dmGui::DeleteNode(m_Scene, node_text, false);
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
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE, 0);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT, 0);

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
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(10,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));

    // should only succeed when trying to play particlefx on correct node type
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));
    dmGui::InternalNode* n = dmGui::GetNode(m_Scene, node_pfx);
    Vector3 pos = dmParticle::GetPosition(m_Scene->m_ParticlefxContext, n->m_Node.m_ParticleInstance);
    ASSERT_EQ(10, pos.getX());

    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

// DEF-3421 Adjust mode "Stretch" is not supported for particlefx nodes, should default to "Fit" instead.
TEST_F(dmGuiTest, PlayNodeParticlefxAdjustModeStretch)
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
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(10,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    dmGui::InternalNode* n = dmGui::GetNode(m_Scene, node_pfx);

    n->m_Node.m_AdjustMode = (uint32_t) dmGui::ADJUST_MODE_STRETCH;
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));
    ASSERT_EQ(dmGui::ADJUST_MODE_FIT, (dmGui::AdjustMode)n->m_Node.m_AdjustMode);

    dmGui::FinalScene(m_Scene);
    UnloadParticlefxPrototype(prototype);
}

TEST_F(dmGuiTest, NewNodeParticlefx)
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
    dmhash_t particlefx_id_wrong = dmHashString64("this_resource_does_not_exist.particlefxc");
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    ASSERT_EQ(dmGui::RESULT_RESOURCE_NOT_FOUND, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id_wrong));

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

    dmGui::HNode node_pfx = dmGui::NewNode(_this->m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);

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
    ASSERT_EQ(1U, data->m_NumStateChanges);

    float dt = 1.2f;
    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Spawning & Postspawn
    ASSERT_EQ(3U, data->m_NumStateChanges);

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Sleeping
    ASSERT_EQ(4U, data->m_NumStateChanges);

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
    ASSERT_EQ(1U, data->m_NumStateChanges);

    float dt = 0.1f;
    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Spawning
    ASSERT_EQ(2U, data->m_NumStateChanges);

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Still spawning, should not trigger callback
    ASSERT_EQ(2U, data->m_NumStateChanges);

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
    ASSERT_EQ(3U, data->m_NumStateChanges);

    float dt = 1.2f;
    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Spawning & Postspawn
    ASSERT_EQ(9U, data->m_NumStateChanges);

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Sleeping
    ASSERT_EQ(12U, data->m_NumStateChanges);

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
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE, 0);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT, 0);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx, 0));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::StopNodeParticlefx(m_Scene, node_pfx, false));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::StopNodeParticlefx(m_Scene, node_box, false));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::StopNodeParticlefx(m_Scene, node_pie, false));
    ASSERT_EQ(dmGui::RESULT_WRONG_TYPE, dmGui::StopNodeParticlefx(m_Scene, node_text, false));

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

    // Test playing three particlefx from one node
    dmGui::HNode node_pfx_1 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx_1, particlefx_id));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx_1, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx_1, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx_1, 0));
    ASSERT_EQ(3U, dmGui::GetParticlefxCount(m_Scene));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::StopNodeParticlefx(m_Scene, node_pfx_1, false));

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0); // Sleeping
    dmGui::UpdateScene(m_Scene, dt); // Prunes sleeping particlefx

    ASSERT_EQ(0U, dmGui::GetParticlefxCount(m_Scene));

    // Test playing particlefx's from two separate nodes
    dmGui::HNode node_pfx_2 = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx_2, particlefx_id));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx_1, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx_1, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx_2, 0));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::PlayNodeParticlefx(m_Scene, node_pfx_2, 0));
    ASSERT_EQ(4U, dmGui::GetParticlefxCount(m_Scene));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::StopNodeParticlefx(m_Scene, node_pfx_1, false));

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0);
    dmGui::UpdateScene(m_Scene, dt);

    ASSERT_EQ(dmGui::GetParticlefxCount(m_Scene), 2);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::StopNodeParticlefx(m_Scene, node_pfx_2, false));

    dmParticle::Update(m_Scene->m_ParticlefxContext, dt, 0);
    dmGui::UpdateScene(m_Scene, dt);

    ASSERT_EQ(dmGui::GetParticlefxCount(m_Scene), 0);

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
    dmGui::HNode node_box = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_BOX, 0);
    dmGui::HNode node_pie = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_PIE, 0);
    dmGui::HNode node_text = dmGui::NewNode(m_Scene, Point3(0, 0, 0), Vector3(100, 50, 0), dmGui::NODE_TYPE_TEXT, 0);
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);

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
    dmGui::HNode node_pfx = dmGui::NewNode(m_Scene, Point3(0,0,0), Vector3(1,1,1), dmGui::NODE_TYPE_PARTICLEFX, 0);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeParticlefx(m_Scene, node_pfx, particlefx_id));
    dmhash_t ret_particlefx_id = 0;
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeParticlefx(m_Scene, node_pfx, ret_particlefx_id));
    ASSERT_EQ(particlefx_id, ret_particlefx_id);
    UnloadParticlefxPrototype(prototype);
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

TEST_F(dmGuiTest, ScriptGetSetAlpha)
{
    const char* s = "function init(self)\n"
                    "    self.n1 = gui.new_box_node(vmath.vector4(0,0,0,0), vmath.vector3(1,1,1))\n"
                    "    local alpha = 0\n"
                    "    gui.set_alpha(self.n1, alpha)\n"
                    "    local a = gui.get_alpha(self.n1)\n"
                    "    assert(a == alpha)\n"
                   "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
    r = dmGui::UpdateScene(m_Scene, 1.0f / 60.0f);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}


TEST_F(dmGuiTest, CloneNodeAndAnim)
{
    int t1;
    dmGui::Result r;

    r = dmGui::AddTexture(m_Scene, dmHashString64("t1"), (void*) &t1, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, 1, 1);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(5,5,0), Vector3(10,10,0), dmGui::NODE_TYPE_BOX, 0);
    ASSERT_NE((dmGui::HNode) 0, node);

    r = dmGui::SetNodeTexture(m_Scene, node, "t1");
    ASSERT_EQ(r, dmGui::RESULT_OK);

    r = dmGui::PlayNodeFlipbookAnim(m_Scene, node, "ta1", 0.5f, 2.0f, 0x0);
    ASSERT_EQ(r, dmGui::RESULT_OK);

    // clone the node
    dmGui::HNode clone;
    dmGui::CloneNode(m_Scene, node, &clone);
    ASSERT_NE((dmGui::HNode) 0, node);

    // same texture on the node?
    ASSERT_EQ(dmGui::GetNodeTextureId(m_Scene, node), dmGui::GetNodeTextureId(m_Scene, clone));

    // same playback rate?
    ASSERT_EQ(dmGui::GetNodeFlipbookPlaybackRate(m_Scene, node), dmGui::GetNodeFlipbookPlaybackRate(m_Scene, clone));

    // same cursor?
    ASSERT_EQ(dmGui::GetNodeFlipbookCursor(m_Scene, node), dmGui::GetNodeFlipbookCursor(m_Scene, clone));

    // same animation?
    ASSERT_EQ(dmGui::GetNodeFlipbookAnimId(m_Scene, node), dmGui::GetNodeFlipbookAnimId(m_Scene, clone));

    // cleanup
    dmGui::RemoveTexture(m_Scene, dmHashString64("t1"));
}

// Check consistancy of get_screen_position/set_screen_position functions 
TEST_F(dmGuiTest, SetGetScreenPosition)
{
    const char* s = "function init(self)\n"
                    "    self.box_node = gui.new_box_node(vmath.vector3(90, 80 ,0), vmath.vector3(180, 60, 0))\n"
                    "    gui.set_id(self.box_node, \"box\")\n"

                    "    self.internal_node = gui.new_box_node(vmath.vector3(20, 30, 0), vmath.vector3(109, 120, 0))\n"
                    "    gui.set_id(self.internal_node, \"internal_node\")\n"
                    "    gui.set_pivot(self.internal_node, gui.PIVOT_NE)\n"
                    "    gui.set_parent(self.internal_node, self.box_node, true)\n"
                    "end\n"
                    "function final(self)\n"
                    "    gui.delete_node(self.box_node)\n"
                    "    gui.delete_node(self.internal_node)\n"
                    "end\n";

    dmGui::Result r;
    r = dmGui::SetScript(m_Script, LuaSourceFromStr(s));
    ASSERT_EQ(dmGui::RESULT_OK, r);

    r = dmGui::InitScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);

    dmGui::HNode internal_node = dmGui::GetNodeById(m_Scene, "internal_node");
    Vector4 before_set = _GET_NODE_SCENE_POSITION(m_Scene, internal_node);
    ASSERT_EQ(before_set, Vector4(20, 30, 0, 0));

    Point3 local_pos = dmGui::ScreenToLocalPosition(m_Scene, internal_node, Point3(10, 10, 0));
    ASSERT_EQ(local_pos.getX(), -80);
    ASSERT_EQ(local_pos.getY(), -70);

    dmGui::SetScreenPosition(m_Scene, internal_node, Point3(before_set.getXYZ()));
    Vector4 after_set = _GET_NODE_SCENE_POSITION(m_Scene, internal_node);
    ASSERT_EQ( before_set, after_set);

    r = dmGui::FinalScene(m_Scene);
    ASSERT_EQ(dmGui::RESULT_OK, r);
}


int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
