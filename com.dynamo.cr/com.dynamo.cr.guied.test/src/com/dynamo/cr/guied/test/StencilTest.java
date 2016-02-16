package com.dynamo.cr.guied.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.guied.core.BoxNode;
import com.dynamo.cr.guied.core.ClippingNode;
import com.dynamo.cr.guied.core.ClippingNode.ClippingState;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneLoader;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.LayerNode;
import com.dynamo.cr.guied.operations.AddGuiNodeOperation;
import com.dynamo.cr.guied.operations.AddLayersOperation;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.MoveChildrenOperation;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.gui.proto.Gui.NodeDesc.ClippingMode;

public class StencilTest extends AbstractNodeTest {

    private static class Renderer {
        int[] stencilBuffer = new int[8];
        Map<ClippingNode, Integer> nodeShapes = new HashMap<ClippingNode, Integer>();

        public void addShape(ClippingNode clipper, int shape) {
            nodeShapes.put(clipper, shape);
        }

        private void render(GuiNode n) {
            if (n instanceof ClippingNode) {
                ClippingNode clipper = (ClippingNode)n;
                ClippingState state = clipper.getClippingState();
                if (nodeShapes.containsKey(clipper)) {
                    int shape = nodeShapes.get(clipper);
                    if (state != null) {
                        if (state.stencilClearBuffer) {
                            this.stencilBuffer = new int[8];
                        }
                        for (int i = 0; i < 8; ++i) {
                            int fragment = ((shape >> i) & 0b1);
                            if (fragment != 0) {
                                int pixel = this.stencilBuffer[i];
                                boolean test = (state.stencilRefVal & state.stencilMask) == (pixel & state.stencilMask);
                                if (test) {
                                    this.stencilBuffer[i] = (state.stencilRefVal & state.stencilWriteMask) | (pixel & ~state.stencilWriteMask);
                                }
                            }
                        }
                    }
                }
            }
        }

        public void collectNodes(Node parent, List<GuiNode> outNodes) {
            for (Node n : parent.getChildren()) {
                if (n instanceof GuiNode) {
                    outNodes.add((GuiNode)n);
                    collectNodes(n, outNodes);
                }
            }
        }

        public void render(GuiSceneNode scene) {
            Clipping.updateClippingStates(scene);
            scene.updateRenderOrder();
            List<GuiNode> allNodes = new ArrayList<GuiNode>();
            collectNodes(scene.getNodesNode(), allNodes);
            Collections.sort(allNodes, new Comparator<GuiNode>() {
                @Override
                public int compare(GuiNode o1, GuiNode o2) {
                    return Long.compare(o1.getRenderKey(), o2.getRenderKey());
                }
            });
            for (GuiNode n : allNodes) {
                render(n);
            }
        }

        public int stencilTest(ClippingState state, int shape) {
            int result = 0;
            for (int i = 0; i < 8; ++i) {
                int fragment = ((shape >> i) & 0b1);
                if (fragment != 0) {
                    int pixel = this.stencilBuffer[i];
                    boolean test = (state.stencilRefVal & state.stencilMask) == (pixel & state.stencilMask);
                    if (test) {
                        result |= (1 << i);
                    }
                }
            }
            return result;
        }
    }

    private GuiSceneNode sceneNode;
    private Renderer renderer;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.sceneNode = registerAndLoadRoot(GuiSceneNode.class, "gui", new GuiSceneLoader());
        this.renderer = new Renderer();
    }

    // Helpers

    private void addNode(GuiNode node, Node parent) throws Exception {
        execute(new AddGuiNodeOperation(parent, node, getPresenterContext()));
        verifySelection();
    }

    private BoxNode addBox(String id, Node parent, boolean clipping, boolean inverted) throws Exception {
        BoxNode box = new BoxNode();
        box.setId(id);
        if (clipping) {
            box.setClippingMode(ClippingMode.CLIPPING_MODE_STENCIL);
        }
        box.setClippingInverted(inverted);
        addNode(box, parent);
        return (BoxNode)parent.getChildren().get(parent.getChildren().size()-1);
    }

    private BoxNode addBox(String id) throws Exception {
        return addBox(id, this.sceneNode.getNodesNode(), false, false);
    }

    private BoxNode addBox(String id, GuiNode parent) throws Exception {
        return addBox(id, parent, false, false);
    }

    private BoxNode addClipperBox(String id, Node parent) throws Exception {
        return addBox(id, parent, true, false);
    }

    private BoxNode addClipperBox(String id) throws Exception {
        return addBox(id, this.sceneNode.getNodesNode(), true, false);
    }

    private BoxNode addInvClipperBox(String id, Node parent) throws Exception {
        return addBox(id, parent, true, true);
    }

    private BoxNode addInvClipperBox(String id) throws Exception {
        return addBox(id, this.sceneNode.getNodesNode(), true, true);
    }

    private void updateClippingStates() {
        Clipping.updateClippingStates(this.sceneNode);
    }

    private void updateRenderOrder() {
        updateClippingStates();
        this.sceneNode.updateRenderOrder();
    }

    private void setShape(ClippingNode clipper, int shape) {
        this.renderer.addShape(clipper, shape);
    }

    private void render() {
        this.renderer.render(this.sceneNode);
    }

    private void setLayers(String ... layers) throws Exception {
        List<Node> nodes = new ArrayList<Node>();
        for (int i = 0; i < layers.length; ++i) {
            LayerNode layer = new LayerNode(layers[i]);
            nodes.add(layer);
        }
        execute(new AddLayersOperation(this.sceneNode, nodes, getPresenterContext()));
        verifySelection();
    }

    private void assertRenderOrder(GuiNode ... guiNodes) {
        long renderKey = guiNodes[0].getRenderKey();
        for (int i = 1; i < guiNodes.length; ++i) {
            long next = guiNodes[i].getRenderKey();
            assertTrue(renderKey < next);
            renderKey = next;
        }
    }

    private void assertClipperOrder(ClippingNode clipper, GuiNode subNode) {
        assertTrue(clipper.getClippingKey() < subNode.getRenderKey());
    }

    private void assertClipping(ClippingNode node, int ref, int mask, int writeMask, int childRef, int childMask, int childWriteMask) {
        ClippingState state = node.getClippingState();
        assertThat(state.stencilRefVal, equalTo(ref));
        assertThat(state.stencilMask, equalTo(mask));
        assertThat(state.stencilWriteMask, equalTo(writeMask));
        ClippingState childState = node.getChildClippingState();
        assertThat(childState.stencilRefVal, equalTo(childRef));
        assertThat(childState.stencilMask, equalTo(childMask));
        assertThat(childState.stencilWriteMask, equalTo(childWriteMask));
    }

    private void assertRefVal(ClippingNode node, int ref) {
        ClippingState state = node.getClippingState();
        assertThat(state.stencilRefVal, equalTo(ref));
    }

    private void assertShapeClippedBy(int shape, ClippingNode clipper, int expected) {
        assertThat(this.renderer.stencilTest(clipper.getChildClippingState(), shape), is(expected));
    }

    /* BIT-FIELD CONSISTENCY TESTS */

    /**
     * Test the clipping states of the following hierarchy:
     * - a (inv)
     *   - b
     * - c
     *
     * @throws Exception
     */
    @Test
    public void testMinimal() throws Exception {
        BoxNode a = addInvClipperBox("a");
        BoxNode b = addClipperBox("b", a);
        BoxNode c = addClipperBox("c");

        updateClippingStates();

        //             n, ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
        assertClipping(a, 0b10000000, 0b00000000, 0b11111111, 0b00000000, 0b10000000, 0x0);
        assertClipping(b, 0b00000001, 0b10000000, 0b11111111, 0b00000001, 0b10000011, 0x0);
        assertClipping(c, 0b00000010, 0b00000000, 0b11111111, 0b00000010, 0b00000011, 0x0);
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
     * Expected values are listed in the design doc: ***REMOVED***/1mzeoLx4HNV4Fbl9aEgtsCNZ4jQDw112SehPggtQAiYE/edit#
     *
     * @throws Exception
     */
    @Test
    public void testSimpleHierarchy() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addClipperBox("b", a);
        BoxNode c = addClipperBox("c", b);
        BoxNode d = addClipperBox("d", c);
        BoxNode e = addClipperBox("e", c);
        BoxNode f = addClipperBox("f");
        BoxNode g = addClipperBox("g", f);
        BoxNode h = addClipperBox("h", g);
        BoxNode i = addClipperBox("i", f);
        BoxNode j = addClipperBox("j", f);
        BoxNode k = addClipperBox("k", f);
        BoxNode l = addClipperBox("l");

        updateClippingStates();

        //             n, ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
        assertClipping(a, 0b00000001, 0b00000000, 0b11111111, 0b00000001, 0b00000011, 0x0);
        assertClipping(b, 0b00000101, 0b00000011, 0b11111111, 0b00000101, 0b00000111, 0x0);
        assertClipping(c, 0b00001101, 0b00000111, 0b11111111, 0b00001101, 0b00001111, 0x0);
        assertClipping(d, 0b00011101, 0b00001111, 0b11111111, 0b00011101, 0b00111111, 0x0);
        assertClipping(e, 0b00101101, 0b00001111, 0b11111111, 0b00101101, 0b00111111, 0x0);
        assertClipping(f, 0b00000010, 0b00000000, 0b11111111, 0b00000010, 0b00000011, 0x0);
        assertClipping(g, 0b00000110, 0b00000011, 0b11111111, 0b00000110, 0b00011111, 0x0);
        assertClipping(h, 0b00100110, 0b00011111, 0b11111111, 0b00100110, 0b00111111, 0x0);
        assertClipping(i, 0b00001010, 0b00000011, 0b11111111, 0b00001010, 0b00011111, 0x0);
        assertClipping(j, 0b00001110, 0b00000011, 0b11111111, 0b00001110, 0b00011111, 0x0);
        assertClipping(k, 0b00010010, 0b00000011, 0b11111111, 0b00010010, 0b00011111, 0x0);
        assertClipping(l, 0b00000011, 0b00000000, 0b11111111, 0b00000011, 0b00000011, 0x0);
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
     * Expected values are listed in the design doc: ***REMOVED***/1mzeoLx4HNV4Fbl9aEgtsCNZ4jQDw112SehPggtQAiYE/edit#
     *
     * @throws Exception
     */
    @Test
    public void testSimpleInvertedHierarchy() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addClipperBox("b", a);
        BoxNode c = addClipperBox("c", a);
        BoxNode d = addClipperBox("d", a);
        BoxNode e = addInvClipperBox("e", a);
        BoxNode f = addInvClipperBox("f", a);
        BoxNode g = addInvClipperBox("g", a);
        BoxNode h = addInvClipperBox("h", g);

        updateClippingStates();

        //             n, ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
        assertClipping(a, 0b00000001, 0b00000000, 0b11111111, 0b00000001, 0b00000001, 0x0);
        assertClipping(b, 0b00000011, 0b00000001, 0b11111111, 0b00000011, 0b00000111, 0x0);
        assertClipping(c, 0b00000101, 0b00000001, 0b11111111, 0b00000101, 0b00000111, 0x0);
        assertClipping(d, 0b00000111, 0b00000001, 0b11111111, 0b00000111, 0b00000111, 0x0);
        assertClipping(e, 0b10000001, 0b00000001, 0b11111111, 0b00000001, 0b10000001, 0x0);
        assertClipping(f, 0b01000001, 0b00000001, 0b11111111, 0b00000001, 0b01000001, 0x0);
        assertClipping(g, 0b00100001, 0b00000001, 0b11111111, 0b00000001, 0b00100001, 0x0);
        assertClipping(h, 0b00010001, 0b00100001, 0b11111111, 0b00000001, 0b00110001, 0x0);
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
    @Test
    public void testRefIdsAndBitCollision() throws Exception {
        BoxNode a = addInvClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);
        BoxNode c = addClipperBox("c", b);
        BoxNode d = addInvClipperBox("d", c);
        BoxNode e = addInvClipperBox("e", b);
        BoxNode h = addInvClipperBox("h");
        BoxNode i = addInvClipperBox("i", h);
        BoxNode j = addInvClipperBox("j");

        updateClippingStates();

        assertRefVal(a, 0b10000000);
        assertRefVal(b, 0b01000000);
        assertRefVal(c, 0b00000001);
        assertRefVal(d, 0b00000011);
        assertRefVal(e, 0b00100000);
        assertRefVal(h, 0b00010000);
        assertRefVal(i, 0b00001000);
        assertRefVal(j, 0b00000100);
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
    @Test
    public void testInvertedConsistency() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addClipperBox("b", a);
        BoxNode c = addInvClipperBox("c");
        BoxNode d = addInvClipperBox("d", c);

        updateClippingStates();

        ClippingState prevState = b.getClippingState();

        ClippingState invState = d.getChildClippingState();

        boolean result = (invState.stencilRefVal & invState.stencilMask) == (prevState.stencilRefVal & invState.stencilMask);

        assertThat(result, is(true));
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
     * @throws Exception
     */
    @Test
    public void testRender_NonInv_NonInv() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addClipperBox("b", a);

        setShape(a, 0b11111000);
        setShape(b, 0b00011100);

        render();

        assertShapeClippedBy(0b11111111, a, 0b11111000);
        assertShapeClippedBy(0b11111111, b, 0b00011000);
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
    @Test
    public void testRender_NonInv_Inv() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);

        setShape(a, 0b11111000);
        setShape(b, 0b00011000);

        render();

        assertShapeClippedBy(0b11111111, a, 0b11111000);
        assertShapeClippedBy(0b11111111, b, 0b11100000);
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
    @Test
    public void testRender_Inv_NonInv() throws Exception {
        BoxNode a = addInvClipperBox("a");
        BoxNode b = addClipperBox("b", a);

        setShape(a, 0b11111000);
        setShape(b, 0b00011110);

        render();

        assertShapeClippedBy(0b11111111, a, 0b00000111);
        assertShapeClippedBy(0b11111111, b, 0b00000110);
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
     * @throws Exception
     */
    @Test
    public void testRender_Inv_Inv() throws Exception {
        BoxNode a = addInvClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);

        setShape(a, 0b11111000);
        setShape(b, 0b00001110);

        render();

        assertShapeClippedBy(0b11111111, a, 0b00000111);
        assertShapeClippedBy(0b11111111, b, 0b00000001);
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
    @Test
    public void testRender_Inv_Inv_Separate() throws Exception {
        BoxNode a = addInvClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);

        setShape(a, 0b01100000);
        setShape(b, 0b00000110);

        render();

        assertShapeClippedBy(0b11111111, a, 0b10011111);
        assertShapeClippedBy(0b11111111, b, 0b10011001);
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
    @Test
    public void testRender_InvConsistency() throws Exception {
        BoxNode a = addClipperBox("a");
        addBox("a_child", a);
        BoxNode b = addClipperBox("b", a);
        addBox("b_child", b);
        BoxNode c = addInvClipperBox("c");
        addBox("c_child", c);
        BoxNode d = addInvClipperBox("d", c);
        addBox("d_child", d);

        setShape(a, 0b11111100);
        setShape(b, 0b11100000);
        setShape(c, 0b00110000);
        setShape(d, 0b00011000);

        render();

        //             n, ref,        mask,       write-mask, child-ref,  child-mask, child-write-mask
        assertClipping(a, 0b00000001, 0b00000000, 0b11111111, 0b00000001, 0b00000001, 0x0);
        assertClipping(b, 0b00000011, 0b00000001, 0b11111111, 0b00000011, 0b00000011, 0x0);
        assertClipping(c, 0b10000000, 0b00000000, 0b11111111, 0b00000000, 0b10000000, 0x0);
        assertClipping(d, 0b01000000, 0b10000000, 0b11111111, 0b00000000, 0b11000000, 0x0);

        assertRenderOrder(a, b, c, d);

        assertShapeClippedBy(0b11111111, c, 0b11001111);
        assertShapeClippedBy(0b11111111, d, 0b11000111);
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
    @Test
    public void testRenderOrder_InheritLayer() throws Exception {
        BoxNode a = addBox("a");
        BoxNode b = addBox("b", a);

        setLayers("layer1");
        a.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(a, b);
    }

    /**
     * Render order with hierarchy and layers:
     *
     * - a (layer2)
     *   - b (layer1)
     *
     * Expected order: b, a
     */
    @Test
    public void testRenderOrder_Layers() throws Exception {
        BoxNode a = addBox("a");
        BoxNode b = addBox("b", a);

        setLayers("layer1", "layer2");
        a.setLayer("layer2");
        b.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(b, a);
    }

    /**
     * Render order for the following hierarchy:
     * - a (clipper)
     *   - b
     * - c
     *
     * Expected order: a, b, c
     */
    @Test
    public void testRenderOrder_ClipperStraight() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addBox("b", a);
        BoxNode c = addBox("c");

        updateRenderOrder();

        assertRenderOrder(a, b, c);

        assertClipperOrder(a, a);
        assertClipperOrder(a, b);
    }

    /**
     * Render order for the following hierarchy:
     * - a (clipper)
     *   - b (layer1)
     * - c
     *
     * Expected order: a, b, c
     */
    @Test
    public void testRenderOrder_OneLayer() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addBox("b", a);
        BoxNode c = addBox("c");

        setLayers("layer1");

        b.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(a, b, c);

        assertClipperOrder(a, a);
        assertClipperOrder(a, b);
    }

    /**
     * Render order for the following hierarchy:
     * - a (clipper, layer1)
     *   - b
     * - c
     *
     * Expected order: a, b, c
     */
    @Test
    public void testRenderOrder_OneClipperLayer() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addBox("b", a);
        BoxNode c = addBox("c");

        setLayers("layer1");

        a.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(a, b, c);

        assertClipperOrder(a, a);
        assertClipperOrder(a, b);
    }

    /**
     * Render order for the following hierarchy:
     * - a (clipper, layer2)
     *   - b (layer1)
     * - c
     *
     * Expected order: b, a, c
     */
    @Test
    public void testRenderOrder_BothLayers() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addBox("b", a);
        BoxNode c = addBox("c");

        setLayers("layer1", "layer2");

        a.setLayer("layer2");
        b.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(b, a, c);

        assertClipperOrder(a, a);
        assertClipperOrder(a, b);
    }

    /**
     * Render order for the following hierarchy:
     * - c
     * - a (clipper, layer2)
     *   - b (layer1)
     *
     * Expected order: c, b, a
     */
    @Test
    public void testRenderOrder_BothLayersAfter() throws Exception {
        BoxNode c = addBox("c");
        BoxNode a = addClipperBox("a");
        BoxNode b = addBox("b", a);

        setLayers("layer1", "layer2");

        a.setLayer("layer2");
        b.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(c, b, a);

        assertClipperOrder(a, a);
        assertClipperOrder(a, b);
    }

    /**
     * Render order for the following hierarchy:
     * - a (inv-clipper, layer2)
     *   - b (layer1)
     * - c
     *
     * Expected order: b, a, c
     */
    @Test
    public void testRenderOrder_BothLayersInvClipper() throws Exception {
        BoxNode a = addInvClipperBox("a");
        BoxNode b = addBox("b", a);
        BoxNode c = addBox("c");

        setLayers("layer1", "layer2");

        a.setLayer("layer2");
        b.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(b, a, c);

        assertClipperOrder(a, a);
        assertClipperOrder(a, b);
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
    @Test
    public void testRenderOrder_BothLayersSub() throws Exception {
        BoxNode z = addClipperBox("z");
        BoxNode a = addClipperBox("a", z);
        BoxNode b = addBox("b", a);
        BoxNode c = addBox("c", z);

        setLayers("layer1", "layer2");

        a.setLayer("layer2");
        b.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(z, b, a, c);

        assertClipperOrder(z, a);
        assertClipperOrder(z, b);
        assertClipperOrder(z, c);
        assertClipperOrder(a, a);
        assertClipperOrder(a, b);
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
    @Test
    public void testRenderOrder_Complex() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addClipperBox("b", a);
        BoxNode c = addBox("c", b);
        BoxNode d = addBox("d", a);
        BoxNode e = addBox("e");

        setLayers("layer1", "layer2", "layer3", "layer4");

        a.setLayer("layer2");
        b.setLayer("layer4");
        c.setLayer("layer3");
        d.setLayer("layer1");

        updateRenderOrder();

        assertRenderOrder(c, b, d, a, e);

        // Verify clipping order (color-less stencil rendering)
        assertClipperOrder(a, e);
        assertClipperOrder(a, d);
        assertClipperOrder(b, d);
        assertClipperOrder(b, c);
    }

    /**
     * Render order for the following hierarchy:
     * - a (layer2)
     *   - b (clipper, layer1)
     *     - c (layer2)
     *
     * Expected order: b, c, a
     */
    @Test
    public void testRenderOrder_Complex2() throws Exception {
        BoxNode a = addBox("a");
        BoxNode b = addClipperBox("b", a);
        BoxNode c = addBox("c", b);

        setLayers("layer1", "layer2");

        a.setLayer("layer2");
        b.setLayer("layer1");
        c.setLayer("layer2");

        updateRenderOrder();

        assertRenderOrder(b, c, a);
    }

    /**
     * Render order for the following hierarchy:
     * - a
     *   - b
     *   - c (clipper)
     *   - d
     *
     * Expected order: a, b, c, d
     */
    @Test
    public void testRenderOrder_NonClipperSiblings() throws Exception {
        BoxNode a = addBox("a");
        BoxNode b = addBox("b", a);
        BoxNode c = addClipperBox("c", a);
        BoxNode d = addBox("d", a);

        updateRenderOrder();

        assertRenderOrder(a, b, c, d);
    }

    /**
     * Render order for the following hierarchy:
     * - a (clipper)
     *   - b
     *   - c (clipper)
     *   - d
     *
     * Expected order: a, b, c, d
     */
    @Test
    public void testRenderOrder_NonClipperSiblingsUnderClipper() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addBox("b", a);
        BoxNode c = addClipperBox("c", a);
        BoxNode d = addBox("d", a);

        updateRenderOrder();

        assertRenderOrder(a, b, c, d);
    }

    /**
     * Render order for the following hierarchy:
     * - a (clipper)
     *   - b (inv-clipper)
     *     - c (clipper)
     *   - d (inv-clipper)
     *
     * Expected order: a, b, c, d
     */
    @Test
    public void testRenderOrder_InvertedSiblings() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);
        BoxNode c = addClipperBox("c", b);
        BoxNode d = addInvClipperBox("d", a);

        updateRenderOrder();

        assertRenderOrder(a, b, c, d);

        assertTrue(a.getClippingKey() < b.getClippingKey());
        assertTrue(b.getClippingKey() < c.getClippingKey());
        assertTrue(c.getClippingKey() < d.getClippingKey());
    }

    /**
     * Render order for the following hierarchy:
     * - a (clipper)
     *   - b (inv)
     *     - c (inv)
     *       - d (inv)
     *         - e (
     *   - d (inv-clipper)
     *
     * Expected order: a, b, c, d
     */
    @Test
    public void testRenderOrder_InvertedSiblingsComplex() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);
        BoxNode c = addClipperBox("c", b);
        BoxNode d = addInvClipperBox("d", a);

        updateRenderOrder();

        assertRenderOrder(a, b, c, d);
    }

    /* OVERFLOW TESTS */

    /**
     * Verify that the number of root nodes is infinite and clears the stencil buffer at overflow.
     *
     * - a (2 bits)
     * - b (2 bits)
     *   - c (1 bit)
     *     - d (1 bit)
     *       - e (1 bit)
     *         - f (1 bit)
     *           - g (1 bit)
     *             - h (1 bit)
     *               - i (1 bit)
     *
     * (i) will not fit, which should trigger b to be reevaluated after a clear, assigning 1 bit to b
     */
    @Test
    public void testRootClear() throws Exception {
        addClipperBox("a");
        BoxNode b = addClipperBox("b");
        BoxNode last = b;
        for (int i = 0; i < 7; ++i) {
            last = addClipperBox("box" + i, last);
        }
        assertTrue(b.getClippingState().stencilClearBuffer);
        assertThat(b.getClippingState().stencilRefVal, is(1));
    }

    /**
     * Same as above, but with inverteds.
     *
     * - a (inv, 1 bit)
     * - b (inv, 1 bit)
     * - c (inv, 1 bit)
     * - d (inv, 1 bit)
     * - e (inv, 1 bit)
     * - f (inv, 1 bit)
     * - g (inv, 1 bit)
     * - h (inv, 1 bit)
     * - i (inv, 1 bit)
     */
    @Test
    public void testRootClearInverteds() throws Exception {
        BoxNode last = null;
        for (int i = 0; i < 9; ++i) {
            last = addInvClipperBox("box" + i);
        }
        assertTrue(last.getStatus().isOK());
    }

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
    @Test
    public void testOverflow() throws Exception {
        Node last = this.sceneNode.getNodesNode();
        Node secondLast = null;
        for (int i = 0; i < 9; ++i) {
            secondLast = last;
            last = addClipperBox("box" + i, last);
        }
        assertTrue(!last.getStatus().isOK());

        // Recover from the error by moving second last to the root
        execute(new MoveChildrenOperation(Collections.singletonList(secondLast), this.sceneNode.getNodesNode(), 1, Collections.singletonList(secondLast), getPresenterContext()));
        assertTrue(last.getStatus().isOK());
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
    @Test
    public void testOverflowInverteds() throws Exception {
        BoxNode a = addInvClipperBox("a");
        BoxNode last = null;
        for (int i = 0; i < 8; ++i) {
            last = addInvClipperBox("box" + i, a);
        }
        assertTrue(!last.getStatus().isOK());

        // recover from the error
        setNodeProperty(last, "clippingMode", ClippingMode.CLIPPING_MODE_NONE);
        assertTrue(last.getStatus().isOK());
    }

    /**
     * Verify that an overflow is handled with a clear.
     *
     * *NOTE* This is an editor specific test.
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
     * (g) will not fit, but should trigger a clear between (a) and (i), thus making it fit.
     * This is a feature of the editor, assuming that nodes will be enabled/disabled at run-time.
     * Otherwise the user will see a warning.
     */
    @SuppressWarnings("unused")
    @Test
    public void testOverflowClearStart() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);
        BoxNode c = addInvClipperBox("c", b);
        BoxNode d = addClipperBox("d", c);
        BoxNode e = addInvClipperBox("e", c);
        BoxNode f = addClipperBox("f", e);
        BoxNode g = addInvClipperBox("g", f);
        BoxNode h = addInvClipperBox("h", b);
        BoxNode i = addClipperBox("i");
        BoxNode j = addInvClipperBox("j", i);
        BoxNode k = addInvClipperBox("k", j);

        assertTrue(g.getStatus().isOK());
    }

    /**
     * Verify that an overflow is handled with a clear.
     *
     * *NOTE* This is an editor specific test.
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
     * (g) will not fit, but should trigger a clear between (a) and (i), thus making it fit.
     * This is a feature of the editor, assuming that nodes will be enabled/disabled at run-time.
     * Otherwise the user will see a warning.
     */
    @SuppressWarnings("unused")
    @Test
    public void testOverflowClearStart2() throws Exception {
        BoxNode a = addClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);
        BoxNode c = addInvClipperBox("c", b);
        BoxNode d = addClipperBox("d", c);
        BoxNode e = addInvClipperBox("e", c);
        BoxNode f = addClipperBox("f", e);
        BoxNode g = addClipperBox("g", f);
        BoxNode h = addInvClipperBox("h", b);
        BoxNode i = addClipperBox("i");
        BoxNode j = addInvClipperBox("j", i);
        BoxNode k = addInvClipperBox("k", j);

        assertTrue(g.getStatus().isOK());
    }

    /**
     * Verify that an overflow is handled with a clear.
     *
     * *NOTE* This is an editor specific test.
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
     * (g) will not fit, but should trigger a clear between (a) and (i), thus making it fit.
     * This is a feature of the editor, assuming that nodes will be enabled/disabled at run-time.
     * Otherwise the user will see a warning.
     */
    @SuppressWarnings("unused")
    @Test
    public void testOverflowClearEnd() throws Exception {
        BoxNode i = addClipperBox("i");
        BoxNode j = addInvClipperBox("j", i);
        BoxNode k = addInvClipperBox("k", j);
        BoxNode a = addClipperBox("a");
        BoxNode b = addInvClipperBox("b", a);
        BoxNode c = addInvClipperBox("c", b);
        BoxNode d = addClipperBox("d", c);
        BoxNode e = addInvClipperBox("e", c);
        BoxNode f = addClipperBox("f", e);
        BoxNode g = addInvClipperBox("g", f);
        BoxNode h = addInvClipperBox("h", b);

        assertTrue(g.getStatus().isOK());
    }
}
