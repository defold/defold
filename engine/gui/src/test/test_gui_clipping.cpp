/**
 * The purpose of this test is to mimick StencilTest.java for the similar functionality in the editor.
 */
#include <map>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/dstrings.h>
#include <script/script.h>
#include "../gui.h"
#include "../gui_private.h"

typedef std::map<dmGui::HNode, dmGui::StencilScope> StateMap;
typedef std::map<dmGui::HNode, uint64_t> RenderOrderMap;
typedef std::map<dmGui::HNode, uint32_t> ShapeMap;

/* Conversions from intricate state structure. */

static uint8_t GetRefVal(const dmGui::StencilScope& scope) {
    return scope.m_RefVal;
}

static uint8_t GetTestMask(const dmGui::StencilScope& scope) {
    return scope.m_TestMask;
}

static uint8_t GetWriteMask(const dmGui::StencilScope& scope) {
    return scope.m_WriteMask;
}

static uint8_t BitArrayStr2Byte(const char *s)
{
    uint8_t v = 0;
    for(uint32_t i = 0; i < 8; i++)
        v |= s[7-i] == '0' ? 0 : 1<<i;
    return v;
}

#define BITS(val) (BitArrayStr2Byte(#val))

class Renderer {
    uint8_t m_StencilBuffer[8];
    ShapeMap m_NodeShapes;

public:
    Renderer() : m_NodeShapes() {
        ClearBuffer();
    }

    void AddShape(dmGui::HNode clipper, uint8_t shape) {
        m_NodeShapes[clipper] = shape;
    }

    void Clear() {
        m_NodeShapes.clear();
        ClearBuffer();
    }

    void ClearBuffer() {
        memset(m_StencilBuffer, 0x0, 8);
    }

public:
    void Render(dmGui::HScene scene, dmGui::HNode node, const dmGui::StencilScope& state) {
        if (dmGui::GetNodeClippingMode(scene, node) == dmGui::CLIPPING_MODE_STENCIL) {
            ShapeMap::iterator shape_it = m_NodeShapes.find(node);
            if (shape_it != m_NodeShapes.end()) {
                uint8_t shape = shape_it->second;
                uint8_t ref_val = GetRefVal(state);
                uint8_t test_mask = GetTestMask(state);
                uint8_t write_mask = GetWriteMask(state);
                for (int i = 0; i < 8; ++i) {
                    uint8_t fragment = ((shape >> i) & 1);
                    if (fragment != 0) {
                        uint8_t pixel = m_StencilBuffer[i];
                        bool test = (ref_val & test_mask) == (pixel & test_mask);
                        if (test) {
                            m_StencilBuffer[i] = (ref_val & write_mask) | (pixel & ~write_mask);
                        }
                    }
                }
            }
        }
    }

    uint8_t StencilTest(const dmGui::StencilScope& state, uint8_t shape) {
        uint8_t result = 0;
        uint8_t ref_val = GetRefVal(state);
        uint8_t test_mask = GetTestMask(state);
        uint8_t write_mask = GetWriteMask(state);
        for (int i = 0; i < 8; ++i) {
            uint8_t fragment = ((shape >> i) & 1);
            if (fragment != 0) {
                uint8_t pixel = m_StencilBuffer[i];
                bool test = (ref_val & test_mask) == (pixel & test_mask);
                if (test) {
                    result |= (1 << i);
                    // Make sure write mask is 0
                    m_StencilBuffer[i] = (ref_val & write_mask) | (pixel & ~write_mask);
                }
            }
        }
        return result;
    }
};

class dmGuiClippingTest : public ::testing::Test
{
public:
    dmScript::HContext m_ScriptContext;
    dmGui::HContext m_Context;
    dmGui::HScene m_Scene;
    Renderer m_Renderer;

    std::map<dmGui::HNode, dmGui::StencilScope> m_NodeToClipping;
    std::map<dmGui::HNode, uint64_t> m_NodeToRenderOrder;
    std::map<dmGui::HNode, uint64_t> m_NodeToClippingOrder;

    void Clear() {
        m_NodeToClipping.clear();
        m_NodeToRenderOrder.clear();
        m_NodeToClippingOrder.clear();
        m_Renderer.Clear();
    }

    virtual void SetUp()
    {
        dmScript::NewContextParams sc_params;
        m_ScriptContext = dmScript::NewContext(&sc_params);
        dmScript::Initialize(m_ScriptContext);

        dmGui::NewContextParams context_params;
        context_params.m_ScriptContext = m_ScriptContext;
        m_Context = dmGui::NewContext(&context_params);

        dmGui::NewSceneParams params;
        params.m_UserData = this;
        m_Scene = dmGui::NewScene(m_Context, &params);

        Clear();
    }

    virtual void TearDown()
    {
        dmGui::DeleteScene(m_Scene);
        dmGui::DeleteContext(m_Context, m_ScriptContext);

        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
    }

    static void RenderNodes(dmGui::HScene scene, const dmGui::RenderEntry* entries, const Vectormath::Aos::Matrix4* node_transforms, const float* node_opacities,
            const dmGui::StencilScope** stencil_scopes, uint32_t node_count, void* context)
    {
        dmGuiClippingTest* self = (dmGuiClippingTest*) context;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            dmGui::HNode node = entries[i].m_Node;
            const dmGui::StencilScope* scope = stencil_scopes[i];
            if (scope != 0x0) {
                if (self->m_NodeToClipping.find(node) == self->m_NodeToClipping.end()) {
                    self->m_NodeToClipping[node] = *scope;
                }
                self->m_Renderer.Render(self->m_Scene, node, *scope);
                if (scope->m_WriteMask != 0) {
                    self->m_NodeToClippingOrder[node] = entries[i].m_RenderKey;
                }
            }
            if (scope == 0x0 || scope->m_ColorMask != 0) {
                self->m_NodeToRenderOrder[node] = entries[i].m_RenderKey;
            }
        }
    }

    /* HELPERS */

    void SetShape(dmGui::HNode clipper, uint8_t shape) {
        m_Renderer.AddShape(clipper, shape);
    }

    dmGui::HNode AddBox(const char* id, dmGui::HNode parent = dmGui::INVALID_HANDLE) {
        dmGui::HNode node = dmGui::NewNode(m_Scene, Point3(), Vector3(), dmGui::NODE_TYPE_BOX);
        dmGui::SetNodeId(m_Scene, node, id);
        if (parent != dmGui::INVALID_HANDLE) {
            dmGui::SetNodeParent(m_Scene, node, parent);
        }
        return node;
    }

    dmGui::HNode AddClipperBox(const char* id, dmGui::HNode parent = dmGui::INVALID_HANDLE) {
        dmGui::HNode node = AddBox(id, parent);
        dmGui::SetNodeClippingMode(m_Scene, node, dmGui::CLIPPING_MODE_STENCIL);
        return node;
    }

    dmGui::HNode AddInvClipperBox(const char* id, dmGui::HNode parent = dmGui::INVALID_HANDLE) {
        dmGui::HNode node = AddClipperBox(id, parent);
        dmGui::SetNodeClippingInverted(m_Scene, node, true);
        return node;
    }

    void Render() {
        m_NodeToClipping.clear();
        m_NodeToRenderOrder.clear();
        m_Renderer.ClearBuffer();
        dmGui::RenderSceneParams params;
        params.m_RenderNodes = RenderNodes;
        dmGui::RenderScene(m_Scene, params, this);
    }

    void SetLayers(const char* layer0) {
        dmGui::AddLayer(m_Scene, layer0);
    }

    void SetLayers(const char* layer0, const char* layer1) {
        dmGui::AddLayer(m_Scene, layer0);
        dmGui::AddLayer(m_Scene, layer1);
    }

    void SetLayers(const char* layer0, const char* layer1, const char* layer2, const char* layer3) {
        dmGui::AddLayer(m_Scene, layer0);
        dmGui::AddLayer(m_Scene, layer1);
        dmGui::AddLayer(m_Scene, layer2);
        dmGui::AddLayer(m_Scene, layer3);
    }

    void SetLayer(dmGui::HNode node, const char* layer) {
        dmGui::SetNodeLayer(m_Scene, node, layer);
    }

    void GetStencilScope(dmGui::HNode node, dmGui::StencilScope& scope) {
        StateMap::iterator it = m_NodeToClipping.find(node);
        ASSERT_TRUE(it != m_NodeToClipping.end());
        scope = it->second;
    }

    void GetRenderOrder(dmGui::HNode node, uint64_t& order) {
        RenderOrderMap::iterator it = m_NodeToRenderOrder.find(node);
        ASSERT_TRUE(it != m_NodeToRenderOrder.end());
        order = it->second;
    }

    void GetClippingOrder(dmGui::HNode node, uint64_t& order) {
        RenderOrderMap::iterator it = m_NodeToClippingOrder.find(node);
        ASSERT_TRUE(it != m_NodeToClippingOrder.end());
        order = it->second;
    }

    void AssertClipping(dmGui::HNode node, dmGui::HNode child, uint8_t ref, uint8_t mask, uint8_t write_mask, uint8_t child_ref, uint8_t child_mask, uint8_t child_write_mask) {
        dmGui::StencilScope state;
        GetStencilScope(node, state);
        ASSERT_EQ((int)ref, (int)GetRefVal(state));
        ASSERT_EQ((int)mask, (int)GetTestMask(state));
        ASSERT_EQ((int)write_mask, (int)GetWriteMask(state));
        dmGui::StencilScope child_state;
        GetStencilScope(child, child_state);
        ASSERT_EQ((int)child_ref, (int)GetRefVal(child_state));
        ASSERT_EQ((int)child_mask, (int)GetTestMask(child_state));
        ASSERT_EQ((int)child_write_mask, (int)GetWriteMask(child_state));
    }

    void AssertRefVal(dmGui::HNode node, uint8_t ref) {
        dmGui::StencilScope state;
        GetStencilScope(node, state);
        ASSERT_EQ((int)ref, (int)GetRefVal(state));
    }

    void AssertShapeClippedBy(uint8_t shape, dmGui::HNode non_clipper, uint8_t expected) {
        dmGui::StencilScope state;
        GetStencilScope(non_clipper, state);
        ASSERT_EQ((int)0, (int)GetWriteMask(state));
        ASSERT_EQ((int)expected, (int)m_Renderer.StencilTest(state, shape));
    }

    void AssertRenderOrder(dmGui::HNode nodes[], uint32_t count) {
        uint64_t render_order;
        GetRenderOrder(nodes[0], render_order);
        for (uint32_t i = 1; i < count; ++i) {
            uint64_t next;
            GetRenderOrder(nodes[i], next);
            ASSERT_TRUE(render_order < next);
            render_order = next;
        }
    }

    void AssertRenderOrder(dmGui::HNode a, dmGui::HNode b) {
        dmGui::HNode nodes[] = {a, b};
        AssertRenderOrder(nodes, 2);
    }

    void AssertRenderOrder(dmGui::HNode a, dmGui::HNode b, dmGui::HNode c) {
        dmGui::HNode nodes[] = {a, b, c};
        AssertRenderOrder(nodes, 3);
    }

    void AssertRenderOrder(dmGui::HNode a, dmGui::HNode b, dmGui::HNode c, dmGui::HNode d) {
        dmGui::HNode nodes[] = {a, b, c, d};
        AssertRenderOrder(nodes, 4);
    }

    void AssertRenderOrder(dmGui::HNode a, dmGui::HNode b, dmGui::HNode c, dmGui::HNode d, dmGui::HNode e) {
        dmGui::HNode nodes[] = {a, b, c, d, e};
        AssertRenderOrder(nodes, 5);
    }

    void AssertClipperOrder(dmGui::HNode clipper, dmGui::HNode node) {
        uint64_t clipping_order;
        GetClippingOrder(clipper, clipping_order);
        uint64_t render_order;
        GetRenderOrder(node, render_order);
        ASSERT_TRUE(clipping_order < render_order);
    }
};

/* BIT-FIELD CONSISTENCY TESTS */

/**
 * Test the clipping states of the following hierarchy:
 * - a (inv)
 *   - b
 * - c
 */
TEST_F(dmGuiClippingTest, TestMinimal) {
    dmGui::HNode a = AddInvClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);
    dmGui::HNode c = AddClipperBox("c");
    dmGui::HNode c_child = AddBox("c_child", c);

    Render();

    //             n, child,   ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
    AssertClipping(a, a_child, BITS(10000000), BITS(00000000), BITS(11111111), BITS(00000000), BITS(10000000), 0x0);
    AssertClipping(b, b_child, BITS(00000001), BITS(10000000), BITS(11111111), BITS(00000001), BITS(10000011), 0x0);
    AssertClipping(c, c_child, BITS(00000010), BITS(00000000), BITS(11111111), BITS(00000010), BITS(00000011), 0x0);
}

/**
 * Test the clipping states of the following hierarchy:
 * - a
 *   - b
 *     - c
 *       - d
 *       - e
 * - f
 *   - g
 *     - h
 *   - i
 *   - j
 *   - k
 * - l
 *
 * Expected values are listed in the design doc: https://docs.google.com/a/king.com/document/d/1mzeoLx4HNV4Fbl9aEgtsCNZ4jQDw112SehPggtQAiYE/edit#
 */
TEST_F(dmGuiClippingTest, TestSimpleHierarchy) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);
    dmGui::HNode c = AddClipperBox("c", b);
    dmGui::HNode c_child = AddBox("c_child", c);
    dmGui::HNode d = AddClipperBox("d", c);
    dmGui::HNode d_child = AddBox("d_child", d);
    dmGui::HNode e = AddClipperBox("e", c);
    dmGui::HNode e_child = AddBox("e_child", e);
    dmGui::HNode f = AddClipperBox("f");
    dmGui::HNode f_child = AddBox("f_child", f);
    dmGui::HNode g = AddClipperBox("g", f);
    dmGui::HNode g_child = AddBox("g_child", g);
    dmGui::HNode h = AddClipperBox("h", g);
    dmGui::HNode h_child = AddBox("h_child", h);
    dmGui::HNode i = AddClipperBox("i", f);
    dmGui::HNode i_child = AddBox("i_child", i);
    dmGui::HNode j = AddClipperBox("j", f);
    dmGui::HNode j_child = AddBox("j_child", j);
    dmGui::HNode k = AddClipperBox("k", f);
    dmGui::HNode k_child = AddBox("k_child", k);
    dmGui::HNode l = AddClipperBox("l");
    dmGui::HNode l_child = AddBox("l_child", l);

    Render();

    //             n, child,   ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
    AssertClipping(a, a_child, BITS(00000001), BITS(00000000), BITS(11111111), BITS(00000001), BITS(00000011), 0x0);
    AssertClipping(b, b_child, BITS(00000101), BITS(00000011), BITS(11111111), BITS(00000101), BITS(00000111), 0x0);
    AssertClipping(c, c_child, BITS(00001101), BITS(00000111), BITS(11111111), BITS(00001101), BITS(00001111), 0x0);
    AssertClipping(d, d_child, BITS(00011101), BITS(00001111), BITS(11111111), BITS(00011101), BITS(00111111), 0x0);
    AssertClipping(e, e_child, BITS(00101101), BITS(00001111), BITS(11111111), BITS(00101101), BITS(00111111), 0x0);
    AssertClipping(f, f_child, BITS(00000010), BITS(00000000), BITS(11111111), BITS(00000010), BITS(00000011), 0x0);
    AssertClipping(g, g_child, BITS(00000110), BITS(00000011), BITS(11111111), BITS(00000110), BITS(00011111), 0x0);
    AssertClipping(h, h_child, BITS(00100110), BITS(00011111), BITS(11111111), BITS(00100110), BITS(00111111), 0x0);
    AssertClipping(i, i_child, BITS(00001010), BITS(00000011), BITS(11111111), BITS(00001010), BITS(00011111), 0x0);
    AssertClipping(j, j_child, BITS(00001110), BITS(00000011), BITS(11111111), BITS(00001110), BITS(00011111), 0x0);
    AssertClipping(k, k_child, BITS(00010010), BITS(00000011), BITS(11111111), BITS(00010010), BITS(00011111), 0x0);
    AssertClipping(l, l_child, BITS(00000011), BITS(00000000), BITS(11111111), BITS(00000011), BITS(00000011), 0x0);
}

/**
 * Test the clipping states of the following hierarchy:
 * - a
 *   - b
 *   - e (inv)
 *   - c
 *   - f (inv)
 *   - d
 *   - g (inv)
 *     - h (inv)
 *
 * Expected values are listed in the design doc: https://docs.google.com/a/king.com/document/d/1mzeoLx4HNV4Fbl9aEgtsCNZ4jQDw112SehPggtQAiYE/edit#
 */
TEST_F(dmGuiClippingTest, TestSimpleInvertedHierarchy) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);
    dmGui::HNode c = AddClipperBox("c", a);
    dmGui::HNode c_child = AddBox("c_child", c);
    dmGui::HNode d = AddClipperBox("d", a);
    dmGui::HNode d_child = AddBox("d_child", d);
    dmGui::HNode e = AddInvClipperBox("e", a);
    dmGui::HNode e_child = AddBox("e_child", e);
    dmGui::HNode f = AddInvClipperBox("f", a);
    dmGui::HNode f_child = AddBox("f_child", f);
    dmGui::HNode g = AddInvClipperBox("g", a);
    dmGui::HNode g_child = AddBox("g_child", g);
    dmGui::HNode h = AddInvClipperBox("h", g);
    dmGui::HNode h_child = AddBox("h_child", h);

    Render();

    //             n, ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
    AssertClipping(a, a_child, BITS(00000001), BITS(00000000), BITS(11111111), BITS(00000001), BITS(00000001), 0x0);
    AssertClipping(b, b_child, BITS(00000011), BITS(00000001), BITS(11111111), BITS(00000011), BITS(00000111), 0x0);
    AssertClipping(c, c_child, BITS(00000101), BITS(00000001), BITS(11111111), BITS(00000101), BITS(00000111), 0x0);
    AssertClipping(d, d_child, BITS(00000111), BITS(00000001), BITS(11111111), BITS(00000111), BITS(00000111), 0x0);
    AssertClipping(e, e_child, BITS(10000001), BITS(00000001), BITS(11111111), BITS(00000001), BITS(10000001), 0x0);
    AssertClipping(f, f_child, BITS(01000001), BITS(00000001), BITS(11111111), BITS(00000001), BITS(01000001), 0x0);
    AssertClipping(g, g_child, BITS(00100001), BITS(00000001), BITS(11111111), BITS(00000001), BITS(00100001), 0x0);
    AssertClipping(h, h_child, BITS(00010001), BITS(00100001), BITS(11111111), BITS(00000001), BITS(00110001), 0x0);
}

/**
 * The following hierarchy notes which bit indices are assigned to which nodes:
 * - a (inv, 7)
 *   - b (inv, 6)
 *     - c (0)
 *       - d (inv, 1)
 *     - e (inv, 5)
 * - h (inv, 4)
 *   - i (inv, 3)
 * - j (inv, 2)
 *
 */
TEST_F(dmGuiClippingTest, TestRefIdsAndBitCollision) {
    dmGui::HNode a = AddInvClipperBox("a");
    dmGui::HNode b = AddInvClipperBox("b", a);
    dmGui::HNode c = AddClipperBox("c", b);
    dmGui::HNode d = AddInvClipperBox("d", c);
    dmGui::HNode e = AddInvClipperBox("e", b);
    dmGui::HNode h = AddInvClipperBox("h");
    dmGui::HNode i = AddInvClipperBox("i", h);
    dmGui::HNode j = AddInvClipperBox("j");

    Render();

    AssertRefVal(a, BITS(10000000));
    AssertRefVal(b, BITS(01000000));
    AssertRefVal(c, BITS(00000001));
    AssertRefVal(d, BITS(00000011));
    AssertRefVal(e, BITS(00100000));
    AssertRefVal(h, BITS(00010000));
    AssertRefVal(i, BITS(00001000));
    AssertRefVal(j, BITS(00000100));
}

/**
 * Test that previous nodes do not corrupt an inverted hierarchy.
 *
 * -a
 *   - b
 * - c (inv)
 *   - d (inv)
 *
 * (b) must not interfere with the test for (d).
 */
TEST_F(dmGuiClippingTest, TestInvertedConsistency) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode c = AddInvClipperBox("c");
    dmGui::HNode d = AddInvClipperBox("d", c);

    Render();

    StateMap::iterator b_it = m_NodeToClipping.find(b);
    StateMap::iterator d_it = m_NodeToClipping.find(d);
    ASSERT_TRUE(b_it != m_NodeToClipping.end());
    ASSERT_TRUE(d_it != m_NodeToClipping.end());

    uint8_t d_ref_val = GetRefVal(d_it->second);
    uint8_t d_test_mask = GetTestMask(d_it->second);
    uint8_t b_ref_val = GetRefVal(b_it->second);

    int lhs = d_ref_val & d_test_mask;
    int rhs = b_ref_val & d_test_mask;
    ASSERT_EQ(lhs, rhs);
}

/* STENCIL BUFFER TESTS */

/**
 * Hierarchy:
 *
 * - a
 *   - b
 *
 * Shapes:
 *
 * a [XXXXX   ]
 * b [   XXX  ]
 */
TEST_F(dmGuiClippingTest, TestRender_NonInv_NonInv) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);

    SetShape(a, BITS(11111000));
    SetShape(b, BITS(00011100));

    Render();

    AssertShapeClippedBy(BITS(11111111), a_child, BITS(11111000));
    AssertShapeClippedBy(BITS(11111111), b_child, BITS(00011000));
}

/**
 * Hierarchy:
 *
 * - a
 *   - b (inv)
 *
 * Shapes:
 *
 * a [XXXXX   ]
 * b [   XX   ]
 * @throws Exception
 */
TEST_F(dmGuiClippingTest, TestRender_NonInv_Inv) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddInvClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);

    SetShape(a, BITS(11111000));
    SetShape(b, BITS(00011000));

    Render();

    AssertShapeClippedBy(BITS(11111111), a_child, BITS(11111000));
    AssertShapeClippedBy(BITS(11111111), b_child, BITS(11100000));
}

/**
 * Hierarchy:
 *
 * - a (inv)
 *   - b
 *
 * Shapes:
 *
 * a [XXXXX   ]
 * b [   XXXX ]
 * @throws Exception
 */
TEST_F(dmGuiClippingTest, TestRender_Inv_NonInv) {
    dmGui::HNode a = AddInvClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);

    SetShape(a, BITS(11111000));
    SetShape(b, BITS(00011110));

    Render();

    AssertShapeClippedBy(BITS(11111111), a_child, BITS(00000111));
    AssertShapeClippedBy(BITS(11111111), b_child, BITS(00000110));
}

/**
 * Hierarchy:
 *
 * - a (inv)
 *   - b (inv)
 *
 * Shapes:
 *
 * a [XXXXX   ]
 * b [    XXX ]
 */
TEST_F(dmGuiClippingTest, TestRender_Inv_Inv) {
    dmGui::HNode a = AddInvClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddInvClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);

    SetShape(a, BITS(11111000));
    SetShape(b, BITS(00001110));

    Render();

    AssertShapeClippedBy(BITS(11111111), a_child, BITS(00000111));
    AssertShapeClippedBy(BITS(11111111), b_child, BITS(00000001));
}

/**
 * Hierarchy:
 *
 * - a (inv)
 *   - b (inv)
 *
 * Shapes:
 *
 * a [ XX     ]
 * b [     XX ]
 * @throws Exception
 */
TEST_F(dmGuiClippingTest, TestRender_Inv_Inv_Separate) {
    dmGui::HNode a = AddInvClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddInvClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);

    SetShape(a, BITS(01100000));
    SetShape(b, BITS(00000110));

    Render();

    AssertShapeClippedBy(BITS(11111111), a_child, BITS(10011111));
    AssertShapeClippedBy(BITS(11111111), b_child, BITS(10011001));
}

/**
 * Hierarchy:
 *
 * - a
 *   - b
 * - c (inv)
 *   - d (inv)
 *
 * Shapes:
 *
 * a [XXX     ]
 * b [XX      ]
 * c [  XX    ]
 * d [   XX   ]
 * @throws Exception
 */
TEST_F(dmGuiClippingTest, TestRender_InvConsistency) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode a_child = AddBox("a_child", a);
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode b_child = AddBox("b_child", b);
    dmGui::HNode c = AddInvClipperBox("c");
    dmGui::HNode c_child = AddBox("c_child", c);
    dmGui::HNode d = AddInvClipperBox("d", c);
    dmGui::HNode d_child = AddBox("d_child", d);

    SetShape(a, BITS(11100000));
    SetShape(b, BITS(11000000));
    SetShape(c, BITS(00110000));
    SetShape(d, BITS(00011000));

    Render();

    //             n, child,   ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
    AssertClipping(a, a_child, BITS(00000001), BITS(00000000), BITS(11111111), BITS(00000001), BITS(00000001), 0x0);
    AssertClipping(b, b_child, BITS(00000011), BITS(00000001), BITS(11111111), BITS(00000011), BITS(00000011), 0x0);
    AssertClipping(c, c_child, BITS(10000000), BITS(00000000), BITS(11111111), BITS(00000000), BITS(10000000), 0x0);
    AssertClipping(d, d_child, BITS(01000000), BITS(10000000), BITS(11111111), BITS(00000000), BITS(11000000), 0x0);

    AssertRenderOrder(a, b, c, d);

    AssertShapeClippedBy(BITS(11111111), c_child, BITS(11001111));
    AssertShapeClippedBy(BITS(11111111), d_child, BITS(11000111));
}

/* RENDER ORDER TESTS */

/**
 * Render order with hierarchy and null layer:
 *
 * - a (layer1)
 *   - b
 *
 * Expected order: a, b (b inherits layer1)
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_InheritLayer) {
    dmGui::HNode a = AddBox("a");
    dmGui::HNode b = AddBox("b", a);

    SetLayers("layer1");
    SetLayer(a, "layer1");

    Render();

    AssertRenderOrder(a, b);
}

/**
 * Render order with hierarchy and layers:
 *
 * - a (layer2)
 *   - b (layer1)
 *
 * Expected order: b, a
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_Layers) {
    dmGui::HNode a = AddBox("a");
    dmGui::HNode b = AddBox("b", a);

    SetLayers("layer1", "layer2");
    SetLayer(a, "layer2");
    SetLayer(b, "layer1");

    Render();

    AssertRenderOrder(b, a);
}

/**
 * Render order for the following hierarchy:
 * - a (clipper)
 *   - b
 * - c
 *
 * Expected order: a, b, c
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_ClipperStraight) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddBox("b", a);
    dmGui::HNode c = AddBox("c");

    Render();

    AssertRenderOrder(a, b, c);
}

/**
 * Render order for the following hierarchy:
 * - a (clipper)
 *   - b (layer1)
 * - c
 *
 * Expected order: a, b, c
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_OneLayer) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddBox("b", a);
    dmGui::HNode c = AddBox("c");

    SetLayers("layer1");

    SetLayer(b, "layer1");

    Render();

    AssertRenderOrder(a, b, c);

    AssertClipperOrder(a, a);
    AssertClipperOrder(a, b);
}

/**
 * Render order for the following hierarchy:
 * - a (clipper, layer1)
 *   - b
 * - c
 *
 * Expected order: a, b, c
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_OneClipperLayer) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddBox("b", a);
    dmGui::HNode c = AddBox("c");

    SetLayers("layer1");

    SetLayer(a, "layer1");

    Render();

    AssertRenderOrder(a, b, c);
}

/**
 * Render order for the following hierarchy:
 * - a (clipper, layer2)
 *   - b (layer1)
 * - c
 *
 * Expected order: b, a, c
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_BothLayers) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddBox("b", a);
    dmGui::HNode c = AddBox("c");

    SetLayers("layer1", "layer2");

    SetLayer(a, "layer2");
    SetLayer(b, "layer1");

    Render();

    AssertRenderOrder(b, a, c);

    AssertClipperOrder(a, a);
    AssertClipperOrder(a, b);
}

/**
 * Render order for the following hierarchy:
 * - c
 * - a (clipper, layer2)
 *   - b (layer1)
 *
 * Expected order: c, b, a
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_BothLayersAfter) {
    dmGui::HNode c = AddBox("c");
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddBox("b", a);

    SetLayers("layer1", "layer2");

    SetLayer(a, "layer2");
    SetLayer(b, "layer1");

    Render();

    AssertRenderOrder(c, b, a);

    AssertClipperOrder(a, a);
    AssertClipperOrder(a, b);
}

/**
 * Render order for the following hierarchy:
 * - a (inv-clipper, layer2)
 *   - b (layer1)
 * - c
 *
 * Expected order: b, a, c
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_BothLayersInvClipper) {
    dmGui::HNode a = AddInvClipperBox("a");
    dmGui::HNode b = AddBox("b", a);
    dmGui::HNode c = AddBox("c");

    SetLayers("layer1", "layer2");

    SetLayer(a, "layer2");
    SetLayer(b, "layer1");

    Render();

    AssertRenderOrder(b, a, c);

    AssertClipperOrder(a, a);
    AssertClipperOrder(a, b);
}

/**
 * Render order for the following hierarchy:
 * - z (clipper)
 *   - a (clipper, layer2)
 *     - b (layer1)
 *   - c
 *
 * Expected order: z, b, a, c
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_BothLayersSub) {
    dmGui::HNode z = AddClipperBox("z");
    dmGui::HNode a = AddClipperBox("a", z);
    dmGui::HNode b = AddBox("b", a);
    dmGui::HNode c = AddBox("c", z);

    SetLayers("layer1", "layer2");

    SetLayer(a, "layer2");
    SetLayer(b, "layer1");

    Render();

    AssertRenderOrder(z, b, a, c);

    AssertClipperOrder(z, a);
    AssertClipperOrder(z, b);
    AssertClipperOrder(z, c);
    AssertClipperOrder(a, a);
    AssertClipperOrder(a, b);
}

/**
 * Render order for the following hierarchy:
 * - a (clipper, layer2)
 *   - b (clipper, layer4)
 *     - c (layer3)
 *   - d (layer1)
 * - e
 *
 * Expected order: c, b, d, a, e
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_Complex) {
    SCOPED_TRACE("Complex");

    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode c = AddBox("c", b);
    dmGui::HNode d = AddBox("d", a);
    dmGui::HNode e = AddBox("e");

    SetLayers("layer1", "layer2", "layer3", "layer4");

    SetLayer(a, "layer2");
    SetLayer(b, "layer4");
    SetLayer(c, "layer3");
    SetLayer(d, "layer1");

    Render();

    AssertRenderOrder(c, b, d, a, e);

    // Verify clipping order (color-less stencil rendering)
    AssertClipperOrder(a, e);
    AssertClipperOrder(a, d);
    AssertClipperOrder(b, d);
    AssertClipperOrder(b, c);
}

/**
 * Render order for the following hierarchy:
 * - a (layer2)
 *   - b (clipper, layer1)
 *     - c (layer2)
 *
 * Expected order: b, c, a
 */
TEST_F(dmGuiClippingTest, TestRenderOrder_Complex2) {
    dmGui::HNode a = AddBox("a");
    dmGui::HNode b = AddClipperBox("b", a);
    dmGui::HNode c = AddBox("c", b);

    SetLayers("layer1", "layer2");

    SetLayer(a, "layer2");
    SetLayer(b, "layer1");
    SetLayer(c, "layer2");

    Render();

    AssertRenderOrder(b, c, a);
}

/* OVERFLOW TESTS */

/**
 * Verify that an overflow is handled with an error.
 *
 * - a (1 bits)
 *   - b (1 bits)
 *     - c (1 bit)
 *       - d (1 bit)
 *         - e (1 bit)
 *           - f (1 bit)
 *             - g (1 bit)
 *               - h (1 bit)
 *                 - i (1 bit)
 *
 * (i) will not fit, which should trigger i to be flagged with an error
 */
TEST_F(dmGuiClippingTest, TestOverflow) {
    dmGui::HNode last = dmGui::INVALID_HANDLE;
    for (int i = 0; i < 9; ++i) {
        last = AddClipperBox("x", last);
    }

    dmLogInfo("Expected warning in test");
    Render();
}

/**
 * Verify that an overflow is handled with an error.
 *
 * - a (inv, 1 bits)
 *   - b (inv, 1 bits)
 *   - c (inv, 1 bit)
 *   - d (inv, 1 bit)
 *   - e (inv, 1 bit)
 *   - f (inv, 1 bit)
 *   - g (inv, 1 bit)
 *   - h (inv, 1 bit)
 *   - i (inv, 1 bit)
 *
 * (i) will not fit, which should trigger i to be flagged with an error
 */
TEST_F(dmGuiClippingTest, TestOverflowInverteds) {
    dmGui::HNode a = AddInvClipperBox("a");
    for (int i = 0; i < 8; ++i) {
        AddInvClipperBox("x", a);
    }

    dmLogInfo("Expected warning in test");
    Render();
}

/**
 * Verify an overflow.
 *
 * - a (2 bit)
 *   - b (inv, 1 bits)
 *     - c (inv, 1 bit)
 *       - d (2 bit)
 *       - e (inv, 1 bit)
 *         - f (2 bit)
 *           - g (inv, 1 bit)
 *     - h (inv, 1 bit)
 * - i (2 bit)
 *   - j (inv, 1 bit)
 *     - k (inv, 1 bit)
 *
 * (g) will not fit
 */
TEST_F(dmGuiClippingTest, TestOverflowClearStart) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddInvClipperBox("b", a);
    dmGui::HNode c = AddInvClipperBox("c", b);
    dmGui::HNode d = AddClipperBox("d", c);
    dmGui::HNode e = AddInvClipperBox("e", c);
    dmGui::HNode f = AddClipperBox("f", e);
    dmGui::HNode g = AddInvClipperBox("g", f);
    dmGui::HNode h = AddInvClipperBox("h", b);
    dmGui::HNode i = AddClipperBox("i");
    dmGui::HNode j = AddInvClipperBox("j", i);
    dmGui::HNode k = AddInvClipperBox("k", j);
    (void)d;
    (void)g;
    (void)h;
    (void)k;

    dmLogInfo("Expected warning in test");
    Render();
}

/**
 * Verify an overflow.
 *
 * - a (2 bit)
 *   - b (inv, 1 bits)
 *     - c (inv, 1 bit)
 *       - d (2 bit)
 *       - e (inv, 1 bit)
 *         - f (2 bit)
 *           - g (1 bit)
 *     - h (inv, 1 bit)
 * - i (2 bit)
 *   - j (inv, 1 bit)
 *     - k (inv, 1 bit)
 *
 * (g) will not fit.
 */
TEST_F(dmGuiClippingTest, TestOverflowClearStart2) {
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddInvClipperBox("b", a);
    dmGui::HNode c = AddInvClipperBox("c", b);
    dmGui::HNode d = AddClipperBox("d", c);
    dmGui::HNode e = AddInvClipperBox("e", c);
    dmGui::HNode f = AddClipperBox("f", e);
    dmGui::HNode g = AddClipperBox("g", f);
    dmGui::HNode h = AddInvClipperBox("h", b);
    dmGui::HNode i = AddClipperBox("i");
    dmGui::HNode j = AddInvClipperBox("j", i);
    dmGui::HNode k = AddInvClipperBox("k", j);
    (void)d;
    (void)g;
    (void)h;
    (void)k;

    dmLogInfo("Expected warning in test");
    Render();
}

/**
 * Verify an overflow.
 *
 * - i (2 bit)
 *   - j (inv, 1 bit)
 *     - k (inv, 1 bit)
 * - a (2 bit)
 *   - b (inv, 1 bits)
 *     - c (inv, 1 bit)
 *       - d (2 bit)
 *       - e (inv, 1 bit)
 *         - f (2 bit)
 *           - g (inv, 1 bit)
 *     - h (inv, 1 bit)
 *
 * (g) will not fit
 */
TEST_F(dmGuiClippingTest, TestOverflowClearEnd) {
    dmGui::HNode i = AddClipperBox("i");
    dmGui::HNode j = AddInvClipperBox("j", i);
    dmGui::HNode k = AddInvClipperBox("k", j);
    dmGui::HNode a = AddClipperBox("a");
    dmGui::HNode b = AddInvClipperBox("b", a);
    dmGui::HNode c = AddInvClipperBox("c", b);
    dmGui::HNode d = AddClipperBox("d", c);
    dmGui::HNode e = AddInvClipperBox("e", c);
    dmGui::HNode f = AddClipperBox("f", e);
    dmGui::HNode g = AddInvClipperBox("g", f);
    dmGui::HNode h = AddInvClipperBox("h", b);
    (void)d;
    (void)g;
    (void)h;
    (void)k;

    dmLogInfo("Expected warning in test");
    Render();
}

#undef BITS

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
