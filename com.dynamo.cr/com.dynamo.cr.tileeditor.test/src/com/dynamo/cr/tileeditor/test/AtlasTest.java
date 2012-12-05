package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertThat;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.swt.widgets.Display;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.tileeditor.atlas.AtlasMap;
import com.dynamo.cr.tileeditor.atlas.AtlasMap.Tile;
import com.dynamo.cr.tileeditor.operations.AddImagesNodeOperation;
import com.dynamo.cr.tileeditor.scene.AtlasAnimationNode;
import com.dynamo.cr.tileeditor.scene.AtlasImageNode;
import com.dynamo.cr.tileeditor.scene.AtlasLoader;
import com.dynamo.cr.tileeditor.scene.AtlasNode;
import com.dynamo.cr.tileeditor.scene.Messages;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class AtlasTest extends AbstractNodeTest {

    private AtlasLoader loader;
    private AtlasNode node;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        super.setup();

        this.loader = new AtlasLoader();
        this.node = registerAndLoadRoot(AtlasNode.class, "atlas", this.loader);
    }

    AtlasNode load(String content) throws CoreException, IOException {
        // We load, save and then load again in order to test save
        // with every test
        String path = "/test.atlas";
        String path2 = "/test2.atlas";
        registerFile(path, content);
        AtlasNode atlas = new AtlasLoader().load(getLoaderContext(), getFile(path).getContents());
        atlas.setModel(getModel());

        Message msg = new AtlasLoader().buildMessage(getLoaderContext(), atlas, new NullProgressMonitor());
        String content2 = TextFormat.printToString(msg);

        registerFile(path2, content2);
        AtlasNode atlas2 = new AtlasLoader().load(getLoaderContext(), getFile(path2).getContents());
        atlas2.setModel(getModel());
        atlas2.updateStatus();

        return atlas2;
    }

    @Test
    public void testLoad() throws Exception {
        AtlasNode node = load("images: { image: \"/2x5_16_1.png\" }");
        assertThat(node.getChildren().size(), is(1));
        assertNodeStatus(node, IStatus.OK, null);
    }

    @Test
    public void testLoadAnimation() throws Exception {
        AtlasNode node = load("animations: { images: { image: \"/2x5_16_1.png\" } }");
        assertThat(node.getChildren().size(), is(1));
        assertThat(node.getChildren().get(0).getChildren().size(), is(1));
        assertNodeStatus(node, IStatus.OK, null);
    }

    @Test
    public void testOnlyDistinctImages1() throws Exception {
        AtlasNode node = load("images: { image: \"/16x16_1.png\" } images: { image: \"/16x16_1.png\" }");
        assertThat(node.getChildren().size(), is(2));
        assertNodeStatus(node, IStatus.OK, null);

        AtlasMap atlasMap = node.getAtlasMap();
        List<Tile> tiles = atlasMap.getTiles();
        assertThat(tiles.size(), is(1));
    }

    @Test
    public void testOnlyDistinctImages2() throws Exception {
        AtlasNode node = load("animations: { images: { image: \"/16x16_1.png\" } images: { image: \"/16x16_1.png\" } }");
        assertThat(node.getChildren().size(), is(1));
        assertThat(node.getChildren().get(0).getChildren().size(), is(2));
        assertNodeStatus(node, IStatus.OK, null);

        AtlasMap atlasMap = node.getAtlasMap();
        List<Tile> tiles = atlasMap.getTiles();
        assertThat(tiles.size(), is(1));
    }

    @Test
    public void testImageNotFound() throws Exception {
        AtlasNode node = load("images: { image: \"/does_not_exists.png\" }");
        assertThat(node.getChildren().size(), is(1));
        assertNodeStatus(node, IStatus.ERROR, Messages.Atlas_UNABLE_TO_LOAD_IMAGE);
    }

    void addImage(Node parent, String image) throws ExecutionException {
        List<Node> children = new ArrayList<Node>();
        children.add(new AtlasImageNode(image));
        execute(new AddImagesNodeOperation(parent, children, getPresenterContext()));
    }

    @Test
    public void testDirty1() throws Exception {
        int version = node.getVersion();
        addImage(node, "/2x5_16_1.png");
        assertThat(node.getVersion(), is(not(version)));
        assertNodeStatus(node, IStatus.OK, null);

        undo();
        assertThat(node.getVersion(), is(not(version)));
    }

    @Test
    public void testDirty2() throws Exception {
        AtlasAnimationNode animation = new AtlasAnimationNode();
        node.addChild(animation);

        int version = node.getVersion();
        addImage(animation, "/2x5_16_1.png");
        assertThat(node.getVersion(), is(not(version)));
        assertNodeStatus(node, IStatus.OK, null);

        undo();
        assertThat(node.getVersion(), is(not(version)));
    }

}
