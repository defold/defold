package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

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

    private IFile tileSetFile;
    private IFile imgFile;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        this.loader = new Sprite2Loader();
        super.setup();

        when(getModel().getExtension(Sprite2Node.class)).thenReturn("sprite2");

        this.tileSetFile = mock(IFile.class);
        when(this.tileSetFile.exists()).thenReturn(true);
        when(this.tileSetFile.getContents()).thenAnswer(new Answer<InputStream>() {
            @Override
            public InputStream answer(InvocationOnMock invocation)
                    throws Throwable {
                String tileSetDdf = "image: \"test.png\" tile_width: 1 tile_height: 1 tile_margin: 0 tile_spacing: 0 material_tag: \"tile\"";
                return new ByteArrayInputStream(tileSetDdf.getBytes());
            }
        });
        when(getModel().getFile("test.tileset")).thenReturn(this.tileSetFile);
        this.imgFile = mock(IFile.class);
        when(this.imgFile.exists()).thenReturn(true);
        when(this.imgFile.getContents()).thenAnswer(new Answer<InputStream>() {
            @Override
            public InputStream answer(InvocationOnMock invocation)
                    throws Throwable {
                return new ByteArrayInputStream(new byte[] {0});
            }
        });
        when(getContentRoot().getFile(new Path("test.png"))).thenReturn(this.imgFile);

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

        IFile newTileSetFile = mock(IFile.class);
        when(newTileSetFile.exists()).thenReturn(false);
        when(getModel().getFile("test2.tileset")).thenReturn(newTileSetFile);

        setNodeProperty(this.spriteNode, "tileSet", "test2.tileset");
        setNodeProperty(this.spriteNode, "defaultAnimation", "test2");

        Sprite2Desc ddf = (Sprite2Desc)this.loader.buildMessage(getLoaderContext(), this.spriteNode, null);

        assertThat(ddf.getTileSet(), is("test2.tileset"));
        assertThat(ddf.getDefaultAnimation(), is("test2"));
    }

    @Test
    public void testReloadTileSet() throws Exception {
        this.spriteNode.handleReload(this.tileSetFile);

        verifyUpdate();
    }

    @Test
    public void testReloadTileSetImage() throws Exception {
        this.spriteNode.handleReload(this.imgFile);

        verifyUpdate();
    }

}
