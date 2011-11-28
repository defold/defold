package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.tileeditor.scene.Sprite2Loader;
import com.dynamo.cr.tileeditor.scene.Sprite2Node;

public class Sprite2Test {

    private ISceneView.ILoaderContext loaderContext;
    private Sprite2Loader loader;

    @Before
    public void setup() throws CoreException, IOException {
        this.loader = new Sprite2Loader();
        this.loaderContext = mock(ISceneView.ILoaderContext.class);
    }

    @Test
    public void testLoad() throws Exception {
        String ddf = "tile_set: \"test.tileset\" default_animation: \"test\"";
        Sprite2Node sprite = this.loader.load(this.loaderContext, new ByteArrayInputStream(ddf.getBytes()));
        assertThat(sprite.getTileSet(), is("test.tileset"));
        assertThat(sprite.getDefaultAnimation(), is("test"));
    }

}
