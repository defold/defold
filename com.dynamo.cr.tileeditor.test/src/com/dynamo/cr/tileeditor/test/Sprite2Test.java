package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.io.IOException;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.tileeditor.scene.Messages;
import com.dynamo.cr.tileeditor.scene.Sprite2Loader;
import com.dynamo.cr.tileeditor.scene.Sprite2Node;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc;

public class Sprite2Test extends AbstractNodeTest {

    private Sprite2Loader loader;
    private Sprite2Node spriteNode;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new Sprite2Loader();

        registerFile("/test.tileset", "image: \"/2x5_16_1.png\" tile_width: 1 tile_height: 1 tile_margin: 0 tile_spacing: 0 material_tag: \"tile\"");
        registerFile("/invalid.tileset", "image: \"non_existant\" tile_width: 1 tile_height: 1 tile_margin: 0 tile_spacing: 0 material_tag: \"tile\"");

        this.spriteNode = registerAndLoadNodeType(Sprite2Node.class, "sprite2", this.loader);
        verifyUpdate();
    }

    // Helpers

    private void setProperty(String id, Object value) throws ExecutionException {
        setNodeProperty(this.spriteNode, id, value);
    }

    private void assertPropertyStatus(String id, int severity, String message) {
        assertNodePropertyStatus(this.spriteNode, id, severity, message);
    }

    // Tests

    @Test
    public void testLoad() throws Exception {
        assertThat(this.spriteNode.getTileSet(), is(""));
        assertThat(this.spriteNode.getDefaultAnimation(), is(""));
    }

    @Test
    public void testCreate() throws Exception {
        setProperty("tileSet", "/test.tileset");
        verifyUpdate();
        setProperty("defaultAnimation", "default");
        verifyUpdate();
    }

    @Test
    public void testBuildMessage() throws Exception {

        testCreate();

        Sprite2Desc ddf = (Sprite2Desc)this.loader.buildMessage(getLoaderContext(), this.spriteNode, null);

        assertThat(ddf.getTileSet(), is(this.spriteNode.getTileSet()));
        assertThat(ddf.getDefaultAnimation(), is(this.spriteNode.getDefaultAnimation()));
    }

    @Test
    public void testReloadTileSet() throws Exception {
        testCreate();

        this.spriteNode.handleReload(getFile("/test.tileset"));

        verifyUpdate();
    }

    @Test
    public void testReloadTileSetImage() throws Exception {
        testCreate();

        this.spriteNode.handleReload(getFile("/2x5_16_1.png"));

        verifyUpdate();
    }

    @Test
    public void testMessages() throws Exception {
        assertPropertyStatus("tileSet", IStatus.INFO, null); // default message

        setProperty("tileSet", "/non_existant");
        assertPropertyStatus("tileSet", IStatus.ERROR, null); // default message

        setProperty("tileSet", "/invalid.tileset");
        assertPropertyStatus("tileSet", IStatus.ERROR, Messages.Sprite2Node_tileSet_INVALID_REFERENCE);

        assertPropertyStatus("defaultAnimation", IStatus.INFO, Messages.Sprite2Node_defaultAnimation_EMPTY);
    }

}
