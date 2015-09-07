package com.dynamo.cr.guied.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Point3d;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.guied.core.BoxNode;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneLoader;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.LayoutNode;
import com.dynamo.cr.guied.core.TemplateNode;
import com.dynamo.cr.guied.operations.AddGuiNodeOperation;
import com.dynamo.cr.guied.operations.AddLayoutOperation;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;


public class TemplateTest extends AbstractNodeTest {

    private GuiSceneNode sceneNode;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
        this.sceneNode = registerAndLoadRoot(GuiSceneNode.class, "gui", new GuiSceneLoader());
    }

    private void addNode(GuiNode node, Node parent) throws Exception {
        execute(new AddGuiNodeOperation(parent, node, getPresenterContext()));
    }

    private BoxNode addBox(String id, Node parent) throws Exception {
        BoxNode box = new BoxNode();
        box.setId(id);
        addNode(box, parent);
        return (BoxNode)parent.getChildren().get(parent.getChildren().size()-1);
    }

    private BoxNode addBox(String id) throws Exception {
        return addBox(id, this.sceneNode.getNodesNode());
    }

    private TemplateNode addTemplate(String id, String templateScenePath, Node parent) throws Exception {
        TemplateNode t = new TemplateNode();
        t.setId(id);
        addNode(t, parent);
        t.setTemplate(templateScenePath);
        return (TemplateNode)parent.getChildren().get(parent.getChildren().size()-1);
    }

    private TemplateNode addTemplate(String id, String templateScenePath) throws Exception {
        return addTemplate(id, templateScenePath, this.sceneNode.getNodesNode());
    }

    private LayoutNode addLayout(String layout) throws Exception {
        List<Node> nodes = new ArrayList<Node>();
        LayoutNode layoutNode = new LayoutNode(layout);
        nodes.add(layoutNode);
        execute(new AddLayoutOperation(this.sceneNode, nodes, getPresenterContext()));
        return (LayoutNode)this.sceneNode.getLayoutsNode().getChildren().get(this.sceneNode.getLayoutsNode().getChildren().size()-1);
    }

    protected <T extends Node> void saveLoadCompare(INodeLoader<T> loader, Message.Builder builder, String path) throws Exception {
        IFile file = getFile(path);
        Reader reader = new InputStreamReader(file.getContents());
        TextFormat.merge(reader, builder);
        reader.close();
        Message directMessage = builder.build();

        T node = loader.load(this.getLoaderContext(), file.getContents());
        Message createdMessage = loader.buildMessage(this.getLoaderContext(), node, new NullProgressMonitor());
        assertEquals(directMessage.toString(), createdMessage.toString());
    }

    protected void saveScene(GuiSceneNode sceneNode, String path) throws Exception {
        GuiSceneLoader loader = new GuiSceneLoader();
        Message scene = loader.buildMessage(this.getLoaderContext(), sceneNode, new NullProgressMonitor());
        registerFile(path, scene.toString());
    }

    protected GuiNode findNode(Node node, String id) {
        if(node.getChildren().isEmpty()) {
            return null;
        }
        for(Node n : node.getChildren()) {
            if(id.equals(((GuiNode)n).getId())) {
                return (GuiNode) n;
            }
            GuiNode g = findNode(n, id);
            if(g != null) {
                return g;
            }
        }
        return null;
    }

    /**
     * Test simple template
     *
     * @throws Exception
     */
    @Test
    public void testSimpleTemplate() throws Exception {
        Point3d pos = new Point3d(100, 0, 0);
        BoxNode b;
        GuiNode g;
        b = addBox("b");
        b.setTranslation(pos);
        saveScene(this.sceneNode, "/simple.gui");
        addTemplate("simple", "/simple.gui");
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b");
        assertThat(g.getTranslation(), equalTo(pos));
    }

    /**
     * Test simple overridden template
     *
     * @throws Exception
     */
    @Test
    public void testSimpleOverriddenTemplate() throws Exception {
        Point3d pos1 = new Point3d(110, 0, 0);
        Point3d pos2 = new Point3d(120, 0, 0);
        BoxNode b;
        GuiNode g;
        b = addBox("b");
        b.setTranslation(pos1);
        saveScene(this.sceneNode, "/simple.gui");
        addTemplate("simple", "/simple.gui");
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
    }

    /**
     * Test nested templates
     *
     * @throws Exception
     */
    @Test
    public void testNestedTemplates() throws Exception {
        Point3d pos = new Point3d(100, 0, 0);
        BoxNode b;
        GuiNode g;

        // depth 1
        b = addBox("b");
        b.setTranslation(pos);
        saveScene(this.sceneNode, "/simple.gui");
        addTemplate("simple", "/simple.gui");
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b");
        assertThat(g.getTranslation(), equalTo(pos));
        saveScene(this.sceneNode, "/template.gui");

        // depth 2
        addTemplate("template", "/template.gui");
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b");
        assertThat(g.getTranslation(), equalTo(pos));
    }

    /**
     * Test overridden templates
     *
     * @throws Exception
     */
    @Test
    public void testNestedOverridenTemplates() throws Exception {
        Point3d pos1 = new Point3d(110, 0, 0);
        Point3d pos2 = new Point3d(120, 0, 0);
        Point3d pos3 = new Point3d(130, 0, 0);
        BoxNode b;
        GuiNode g;

        // setup overriding 2 depths
        b = addBox("b");
        b.setTranslation(pos1);
        saveScene(this.sceneNode, "/simple.gui");
        addTemplate("simple", "/simple.gui");
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        saveScene(this.sceneNode, "/template.gui");
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        saveScene(this.sceneNode, "/template_overridden.gui");

        // scene overriding depth 1 ("simple" template)
        addTemplate("template", "/template.gui");
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));

        // scene overriding depth 1 and 2 ("simple" and "template" templates)
        addTemplate("template_overridden", "/template_overridden.gui");
        g =  findNode(this.sceneNode.getNodesNode(), "template_overridden/simple/b");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.setTranslation(pos3);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos3));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
    }

    /**
     * Test nested overridden templates and layouts
     *
     * @throws Exception
     */
    @Test
    public void testNestedOverridenTemplateAndLayouts() throws Exception {
        Point3d pos1 = new Point3d(110, 0, 0);
        Point3d pos1Overridden = new Point3d(110, 1, 0);
        Point3d pos2 = new Point3d(120, 0, 0);
        Point3d pos2Overridden = new Point3d(120, 1, 0);
        Point3d pos3 = new Point3d(130, 0, 0);
        Point3d pos3Overridden = new Point3d(130, 1, 0);
        BoxNode b1, b2, b3;
        GuiNode g;

        this.sceneNode.loadLayouts(this.getFile("/test.display_profiles"));
        addLayout("Landscape");
        addLayout("Portrait");

        // setup overriding 2 depths with two layouts
        // Overrides - Col: Template Row: Layout
        //             simple      template    template_verridden
        // Default                             b1
        // Landscape   b2                      b2
        // Portrait    b3

        // setup template "simple" for test
        b1 = addBox("b1");
        b1.setTranslation(pos1);
        b2 = addBox("b2");
        b2.setTranslation(pos1);
        b3 = addBox("b3");
        b3.setTranslation(pos1);
        this.sceneNode.setCurrentLayout("Landscape");
        b2.setTranslation(pos1Overridden);
        assertTrue(b2.isTranslationOverridden());
        assertThat(b2.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Portrait");
        b3.setTranslation(pos1Overridden);
        assertTrue(b3.isTranslationOverridden());
        assertThat(b3.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");
        saveScene(this.sceneNode, "/simple.gui");

        // setup template "template" for test
        addTemplate("simple", "/simple.gui");
        // overriding b1
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b1");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b2");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        this.sceneNode.setCurrentLayout("Landscape");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");
        saveScene(this.sceneNode, "/template.gui");

        // setup template "template_overridden" for test
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b1");
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b2");
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        this.sceneNode.setCurrentLayout("Landscape");
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        this.sceneNode.setCurrentLayout("Default");
        saveScene(this.sceneNode, "/template_overridden.gui");

        // scene overriding depth 1 - template "template"
        addTemplate("template", "/template.gui");
        // overriding b1
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b1");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));

        // overriding b2
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b2");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        this.sceneNode.setCurrentLayout("Landscape");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");

        // overriding b3
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b3");
        this.sceneNode.setCurrentLayout("Portrait");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");

        // scene overriding depth 1 and 2 - template "template_overridden"
        addTemplate("template_overridden", "/template_overridden.gui");
        // overriding b1
        g =  findNode(this.sceneNode.getNodesNode(), "template_overridden/simple/b1");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.setTranslation(pos3);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos3));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));

        // overriding b2
        g =  findNode(this.sceneNode.getNodesNode(), "template_overridden/simple/b2");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.setTranslation(pos3);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos3));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        this.sceneNode.setCurrentLayout("Landscape");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.setTranslation(pos3Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos3Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        this.sceneNode.setCurrentLayout("Default");

        // overriding b3
        g =  findNode(this.sceneNode.getNodesNode(), "template_overridden/simple/b3");
        this.sceneNode.setCurrentLayout("Portrait");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");
    }


    /**
     * Test nested overridden templates and mixed layouts
     *
     * @throws Exception
     */
    @Test
    public void testNestedOverridenTemplateAndMixedLayouts() throws Exception {
        Point3d pos1 = new Point3d(110, 0, 0);
        Point3d pos1Overridden = new Point3d(110, 1, 0);
        Point3d pos2 = new Point3d(120, 0, 0);
        Point3d pos2Overridden = new Point3d(120, 1, 0);
        Point3d pos3 = new Point3d(130, 0, 0);
        Point3d pos3Overridden = new Point3d(130, 1, 0);
        BoxNode b1, b2, b3;
        GuiNode g;

        this.sceneNode.loadLayouts(this.getFile("/test.display_profiles"));
        addLayout("Landscape");

        // setup overriding 2 depths with two layouts. Layout "Portrait" does not exist for "simple" depth 1 template.
        // Overrides - Col: Template Row:
        //             simple      template    template_verridden
        // Default                             b1
        // Landscape   b2                      b2
        // Portrait    N/A          b3

        // setup template "simple" for test
        b1 = addBox("b1");
        b1.setTranslation(pos1);
        b2 = addBox("b2");
        b2.setTranslation(pos1);
        b3 = addBox("b3");
        b3.setTranslation(pos1);
        this.sceneNode.setCurrentLayout("Landscape");
        b2.setTranslation(pos1Overridden);
        assertTrue(b2.isTranslationOverridden());
        assertThat(b2.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");
        saveScene(this.sceneNode, "/simple.gui");
        addLayout("Portrait");
        this.sceneNode.setCurrentLayout("Portrait");
        b3.setTranslation(pos1Overridden);
        assertTrue(b3.isTranslationOverridden());
        assertThat(b3.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");

        // setup template "template" for test
        addTemplate("simple", "/simple.gui");
        // overriding b1
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b1");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b2");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        this.sceneNode.setCurrentLayout("Landscape");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");
        saveScene(this.sceneNode, "/template.gui");

        // setup template "template_overridden" for test
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b1");
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g =  findNode(this.sceneNode.getNodesNode(), "simple/b2");
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        this.sceneNode.setCurrentLayout("Landscape");
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        this.sceneNode.setCurrentLayout("Default");
        saveScene(this.sceneNode, "/template_overridden.gui");

        // scene overriding depth 1 - template "template"
        addTemplate("template", "/template.gui");
        // overriding b1
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b1");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));

        // overriding b2
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b2");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        this.sceneNode.setCurrentLayout("Landscape");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1Overridden));
        this.sceneNode.setCurrentLayout("Default");

        // overriding b3 - has no "Portrait" layout in "simple" template, depth 1
        g =  findNode(this.sceneNode.getNodesNode(), "template/simple/b3");
        this.sceneNode.setCurrentLayout("Portrait");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        this.sceneNode.setCurrentLayout("Default");

        // scene overriding depth 1 and 2 - template "template_overridden"
        addTemplate("template_overridden", "/template_overridden.gui");
        // overriding b1
        g =  findNode(this.sceneNode.getNodesNode(), "template_overridden/simple/b1");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.setTranslation(pos3);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos3));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));

        // overriding b2
        g =  findNode(this.sceneNode.getNodesNode(), "template_overridden/simple/b2");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        g.setTranslation(pos3);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos3));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2));
        this.sceneNode.setCurrentLayout("Landscape");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.setTranslation(pos3Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos3Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        this.sceneNode.setCurrentLayout("Default");

        // overriding b3 - has no "Portrait" layout in "simple" template, depth 1
        g =  findNode(this.sceneNode.getNodesNode(), "template_overridden/simple/b3");
        this.sceneNode.setCurrentLayout("Portrait");
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        g.setTranslation(pos2Overridden);
        assertTrue(g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos2Overridden));
        g.resetTranslation();
        assertTrue(!g.isTranslationOverridden());
        assertThat(g.getTranslation(), equalTo(pos1));
        this.sceneNode.setCurrentLayout("Default");
    }

}

