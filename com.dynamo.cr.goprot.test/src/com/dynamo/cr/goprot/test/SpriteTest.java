package com.dynamo.cr.goprot.test;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.goprot.sprite.AnimationNode;
import com.dynamo.cr.goprot.sprite.SpriteNode;
import com.dynamo.cr.goprot.sprite.SpritePresenter;
import com.google.inject.Module;
import com.google.inject.Singleton;

public class SpriteTest extends AbstractTest {

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(SpritePresenter.class).in(Singleton.class);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
        this.manager.registerPresenter(SpriteNode.class, this.injector.getInstance(SpritePresenter.class));
    }

    // Tests

    @Test
    public void testLoad() throws IOException {
        String ddf = "texture: '' width: 1 height: 1";
        SpritePresenter presenter = (SpritePresenter)this.manager.getPresenter(SpriteNode.class);
        presenter.onLoad(new ByteArrayInputStream(ddf.getBytes()));
        Node root = this.model.getRoot();
        assertTrue(root instanceof SpriteNode);
        verifyUpdate(root);
        verifySelection();
    }

    @Test
    public void testAddAnimation() throws Exception {
        testLoad();

        SpriteNode sprite = (SpriteNode)this.model.getRoot();
        SpritePresenter presenter = (SpritePresenter)this.manager.getPresenter(SpriteNode.class);
        presenter.onAddAnimation();
        assertEquals(1, sprite.getChildren().size());
        assertTrue(sprite.getChildren().get(0) instanceof AnimationNode);
        verifyUpdate(sprite);
        verifySelection();

        undo();
        assertEquals(0, sprite.getChildren().size());
        verifyUpdate(sprite);
        verifySelection();

        redo();
        assertEquals(1, sprite.getChildren().size());
        assertTrue(sprite.getChildren().get(0) instanceof AnimationNode);
        verifyUpdate(sprite);
        verifySelection();
    }

    @Test
    public void testRemoveAnimation() throws Exception {
        testAddAnimation();

        SpriteNode sprite = (SpriteNode)this.model.getRoot();
        AnimationNode animation = (AnimationNode)sprite.getChildren().get(0);
        SpritePresenter presenter = (SpritePresenter)this.manager.getPresenter(SpriteNode.class);

        presenter.onRemoveAnimation();
        assertEquals(0, sprite.getChildren().size());
        assertEquals(null, animation.getParent());
        verifyUpdate(sprite);
        verifySelection();

        undo();
        assertEquals(1, sprite.getChildren().size());
        assertEquals(animation, sprite.getChildren().get(0));
        assertEquals(sprite, animation.getParent());
        verifyUpdate(sprite);
        verifySelection();

        redo();
        assertEquals(0, sprite.getChildren().size());
        assertEquals(null, animation.getParent());
        verifyUpdate(sprite);
        verifySelection();
    }

    @Test
    public void testSetTileSet() throws Exception {
        testLoad();

        SpriteNode sprite = (SpriteNode)this.model.getRoot();
        String oldTileSet = sprite.getTileSet();
        String newTileSet = "test.tileset";

        setNodeProperty(sprite, "tileSet", newTileSet);
        assertEquals(newTileSet, sprite.getTileSet());
        verifyUpdate(sprite);

        undo();
        assertEquals(oldTileSet, sprite.getTileSet());
        verifyUpdate(sprite);

        redo();
        assertEquals(newTileSet, sprite.getTileSet());
        verifyUpdate(sprite);
    }

    @Override
    Module getModule() {
        return new TestModule();
    }

}
