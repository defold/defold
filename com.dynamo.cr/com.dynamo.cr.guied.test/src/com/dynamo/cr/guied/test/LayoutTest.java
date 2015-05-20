package com.dynamo.cr.guied.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Reader;
import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Point3d;

import org.apache.commons.io.IOUtils;
import org.apache.commons.io.output.ByteArrayOutputStream;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.guied.core.BoxNode;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneLoader;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.LayoutNode;
import com.dynamo.cr.guied.operations.AddGuiNodeOperation;
import com.dynamo.cr.guied.operations.AddLayoutOperation;
import com.dynamo.cr.guied.util.Layouts;
import com.dynamo.cr.guied.util.Layouts.Layout;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;


public class LayoutTest extends AbstractNodeTest {

    private GuiSceneNode sceneNode;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
        this.sceneNode = registerAndLoadRoot(GuiSceneNode.class, "gui", new GuiSceneLoader());
    }

    // Helpers
    private byte[] serializeNode(Node node) throws Exception {
        List<Node> nodes = new ArrayList<Node>(1);
        nodes.add(node);
        ByteArrayOutputStream outByte = new ByteArrayOutputStream();
        ObjectOutputStream out = new ObjectOutputStream(outByte);
        out.writeObject(nodes);
        IOUtils.closeQuietly(out);
        return outByte.toByteArray();
    }

    private Node deserializeNode(byte[] data)  throws Exception {
        ObjectInputStream in = new ObjectInputStream(new ByteArrayInputStream(data));
        @SuppressWarnings("unchecked")
        List<Node> nodes2 = (List<Node>)in.readObject();
        assertThat(nodes2.size(), is(1));
        return nodes2.get(0);
    }

    private void addNode(GuiNode node, Node parent) throws Exception {
        execute(new AddGuiNodeOperation(parent, node, getPresenterContext()));
        // verifySelection();
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

    private LayoutNode addLayout(String layout) throws Exception {
        List<Node> nodes = new ArrayList<Node>();
        LayoutNode layoutNode = new LayoutNode(layout);
        nodes.add(layoutNode);
        execute(new AddLayoutOperation(this.sceneNode, nodes, getPresenterContext()));
        return (LayoutNode)this.sceneNode.getLayoutsNode().getChildren().get(this.sceneNode.getLayoutsNode().getChildren().size()-1);
    }

    private void loadLayouts() throws Exception {
        IFile resource = this.getFile("/test.display_profiles");
        assertNotNull(resource);
        this.sceneNode.loadLayouts(resource);
    }

    private void setupLayouts() throws Exception {
        loadLayouts();
        addLayout("Landscape");
        addLayout("Portrait");
        addLayout("Portrait2");
    }

    protected void select(Node node) {
        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(node));
    }

    /**
     * Test overrides
     *
     * @throws Exception
     */
    @Test
    public void testOverrides() throws Exception {
        setupLayouts();

        Point3d pa = new Point3d(100, 0, 0);
        Point3d pb = new Point3d(110, 0, 0);
        Point3d pc = new Point3d(111, 0, 0);
        BoxNode a = addBox("a");

        // test setting translation, nothing should override in any layout
        a.setTranslation(pa);
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());

        // test overriding single layout
        this.sceneNode.setCurrentLayout("Landscape");
        a.setTranslation(pb);
        assertThat(a.getTranslation(), equalTo(pb));
        assertTrue(a.isTranslationOverridden());
        this.sceneNode.setDefaultLayout();
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());

        // test coherence of layouts not overridden to default layout
        this.sceneNode.setDefaultLayout();
        pa.setX(105);
        a.setTranslation(pa);
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        assertThat(a.getTranslation(), equalTo(pb));
        assertTrue(a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());

        // test multiple layout overriding
        this.sceneNode.setCurrentLayout("Portrait");
        a.setTranslation(pc);
        assertThat(a.getTranslation(), equalTo(pc));
        assertTrue(a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        assertThat(a.getTranslation(), equalTo(pb));
        assertTrue(a.isTranslationOverridden());
        this.sceneNode.setDefaultLayout();
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());

        // test reset of overridden values
        this.sceneNode.setCurrentLayout("Landscape");
        a.resetTranslation();
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(a.getTranslation(), equalTo(pc));
        assertTrue(a.isTranslationOverridden());
        a.resetTranslation();
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setDefaultLayout();
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
    }

    /**
     * Test override serialization
     * In the case of layouts, this covers node operations as new, cut, copy, paste..
     *
     * @throws Exception
     */
    @Test
    public void testSerialization() throws Exception {
        setupLayouts();

        Point3d pa = new Point3d(100, 0, 0);
        Point3d pb = new Point3d(110, 0, 0);
        Point3d pc = new Point3d(111, 0, 0);
        BoxNode a = addBox("a");

        // setup node with different translation in each layout
        a.setTranslation(pa);
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        a.setTranslation(pb);
        assertThat(a.getTranslation(), equalTo(pb));
        assertTrue(a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        a.setTranslation(pc);
        assertThat(a.getTranslation(), equalTo(pc));
        assertTrue(a.isTranslationOverridden());

        // serialize node a to new node b - while in layout "portrait" into same layout.
        byte[] data = serializeNode(a);
        BoxNode b = (BoxNode) deserializeNode(data);
        addNode(b, this.sceneNode.getNodesNode());
        GuiNodeStateBuilder.restoreState(b);

        // all layouts in node b should be same as in node a
        this.sceneNode.setDefaultLayout();
        assertThat(b.getTranslation(), equalTo(pa));
        assertTrue(!b.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        assertThat(b.getTranslation(), equalTo(pb));
        assertTrue(b.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(b.getTranslation(), equalTo(pc));
        assertTrue(b.isTranslationOverridden());

        // serialize node a to new node c - while in layout "portrait" into layout "landscape".
        this.sceneNode.setCurrentLayout("Landscape");
        a.setTranslation(pc);
        data = serializeNode(a);
        BoxNode c = (BoxNode) deserializeNode(data);
        addNode(c, this.sceneNode.getNodesNode());
        GuiNodeStateBuilder.restoreState(c);

        // all layouts in node b should be same as in node a, except in layout "landscape" which was copied from layout "portrait".
        this.sceneNode.setDefaultLayout();
        assertThat(c.getTranslation(), equalTo(pa));
        assertTrue(!c.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        assertThat(c.getTranslation(), equalTo(pc));
        assertTrue(c.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(c.getTranslation(), equalTo(pc));
        assertTrue(c.isTranslationOverridden());
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

    /**
     * Test load/save with layouts containing overridden nodes
     *
     * @throws Exception
     */
    @Test
    public void testLoadSave() throws Exception {
        setupLayouts();

        Point3d pa = new Point3d(100, 0, 0);
        Point3d pb = new Point3d(110, 0, 0);
        BoxNode a = addBox("a");

        // setup node to test loading and saving a layout with overridden values
        // also test load/save of non-overridden values ("portrait2" layout).
        a.setTranslation(pa);
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        a.setTranslation(pb);
        assertThat(a.getTranslation(), equalTo(pb));
        assertTrue(a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        a = null;

        // save and reload scene
        GuiSceneLoader loader = new GuiSceneLoader();
        Message scene1 = loader.buildMessage(this.getLoaderContext(), this.sceneNode, new NullProgressMonitor());
        this.sceneNode = loader.load(this.getLoaderContext(), new ByteArrayInputStream(scene1.toString().getBytes()));
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(this.sceneNode));
        this.loadLayouts();
        this.sceneNode.setDefaultLayout();
        this.getModel().setRoot(this.sceneNode);
        Message scene2 = loader.buildMessage(this.getLoaderContext(), this.sceneNode, new NullProgressMonitor());

        // compare scenes which should be identical
        assertTrue(scene1.equals(scene2));

        // and double-check by correct overrides and values
        a = (BoxNode) this.sceneNode.getNodesNode().getChildren().get(0);
        this.sceneNode.setDefaultLayout();
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Landscape");
        assertThat(a.getTranslation(), equalTo(pb));
        assertTrue(a.isTranslationOverridden());
        this.sceneNode.setCurrentLayout("Portrait");
        assertThat(a.getTranslation(), equalTo(pa));
        assertTrue(!a.isTranslationOverridden());
    }

    /**
     * Test layout methods
     *
     * @throws Exception
     */
    @Test
    public void testLayoutsMethods() throws Exception {
        setupLayouts();
        Layouts layouts = this.sceneNode.getLayouts();

        Layout l1 = Layouts.getLayout(layouts, "Landscape");
        assertTrue(l1 != null);
        assertTrue(l1.getId().compareTo("Landscape")==0);
        Layout l2 = Layouts.getLayout(layouts, "Portrait");
        assertTrue(l2 != null);
        assertTrue(l2.getId().compareTo("Portrait")==0);
        Layout l3 = Layouts.getLayout(layouts, "Portrait2");
        assertTrue(l3 != null);
        assertTrue(l3.getId().compareTo("Portrait2")==0);
        Layout l4 = Layouts.getLayout(layouts, "Portrait3");
        assertTrue(l4 != null);
        assertTrue(l4.getId().compareTo("Portrait3")==0);
        Layout l5 = Layouts.getLayout(layouts, "Landscape2");
        assertTrue(l5 != null);
        assertTrue(l5.getId().compareTo("Landscape2")==0);
        Layout l6 = Layouts.getLayout(layouts, "Landscape3");
        assertTrue(l6 != null);
        assertTrue(l6.getId().compareTo("Landscape3")==0);

        List<String> lIdList = Layouts.getLayoutIds(layouts);
        assertThat(lIdList.size(), equalTo(7));
        assertTrue(lIdList.contains(l1.getId()));
        assertTrue(lIdList.contains(l2.getId()));
        assertTrue(lIdList.contains(l3.getId()));
        assertTrue(lIdList.contains(l4.getId()));
        assertTrue(lIdList.contains(l5.getId()));
        assertTrue(lIdList.contains(l6.getId()));

        String lM = Layouts.getMatchingLayout(layouts, new ArrayList<String>(), 1280, 720, 0);
        assertThat(lM, equalTo("Landscape"));
        lM = Layouts.getMatchingLayout(layouts, new ArrayList<String>(), 720, 1280, 0);
        assertThat(lM, equalTo("Portrait"));
        lM = Layouts.getMatchingLayout(layouts, new ArrayList<String>(), 721, 1280, 0); // testing result is first found match
        assertThat(lM, equalTo("Portrait2"));
        lM = Layouts.getMatchingLayout(layouts, new ArrayList<String>(), 1280, 722, 0); // testing multiple qualifiers
        assertThat(lM, equalTo("Landscape3"));

        // cherry picking
        ArrayList<String> choices = new ArrayList<String>();
        choices.add("Landscape");
        lM = Layouts.getMatchingLayout(layouts, choices, 721, 1280, 0);
        assertThat(lM, equalTo("Landscape"));
        choices.add("Portrait");
        lM = Layouts.getMatchingLayout(layouts, choices, 721, 1280, 0);
        assertThat(lM, equalTo("Portrait"));
    }


}


