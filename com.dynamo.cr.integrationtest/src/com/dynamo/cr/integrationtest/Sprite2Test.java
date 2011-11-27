package com.dynamo.cr.integrationtest;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import org.junit.Test;

import com.dynamo.cr.tileeditor.scene.Sprite2Node;

public class Sprite2Test extends AbstractNodeTest {

    @Test
    public void testLoadDefault() throws Exception {
        Sprite2Node sprite = (Sprite2Node)this.loaderContext.loadNodeFromTemplate(Sprite2Node.class);
        assertThat(sprite.getTileSet(), is(""));
        assertThat(sprite.getDefaultAnimation(), is(""));
    }
}
