package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.tileeditor.scene.Sprite2Loader;
import com.dynamo.cr.tileeditor.scene.Sprite2Node;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc;

public class Sprite2Test extends AbstractNodeTest {

    private Sprite2Loader loader;
    private Sprite2Node spriteNode;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.loader = new Sprite2Loader();
        super.setup();

        when(getModel().getExtension(Sprite2Node.class)).thenReturn("sprite2");

        String ddf = "tile_set: \"test.tileset\" default_animation: \"test\"";
        this.spriteNode = this.loader.load(getLoaderContext(), new ByteArrayInputStream(ddf.getBytes()));
        this.spriteNode.setModel(getModel());
        verifyUpdate();
    }

    @Test
    public void testLoad() throws Exception {
        assertThat(this.spriteNode.getTileSet(), is("test.tileset"));
        assertThat(this.spriteNode.getDefaultAnimation(), is("test"));
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testBuildMessage() throws Exception {
        INodeType spriteType = mock(INodeType.class);
        // Not a nice cast, but ok since this is merely a test
        when(spriteType.getLoader()).thenReturn((ISceneView.INodeLoader<Node>)(Object)new Sprite2Loader());
        when(spriteType.getExtension()).thenReturn("sprite2");

        when(getNodeTypeRegistry().getNodeType(Sprite2Node.class)).thenReturn(spriteType);

        setNodeProperty(this.spriteNode, "tileSet", "test2.tileset");
        setNodeProperty(this.spriteNode, "defaultAnimation", "test2");

        Sprite2Desc ddf = (Sprite2Desc)this.loader.buildMessage(getLoaderContext(), this.spriteNode, null);

        assertThat(ddf.getTileSet(), is("test2.tileset"));
        assertThat(ddf.getDefaultAnimation(), is("test2"));
    }

    @Test
    public void testReloadTileSet() throws Exception {
        IFile tileSetFile = mock(IFile.class);
        when(tileSetFile.exists()).thenReturn(true);
        when(getModel().getFile("test.tileset")).thenReturn(tileSetFile);

        this.spriteNode.handleReload(tileSetFile);

        verifyUpdate();
    }

}
