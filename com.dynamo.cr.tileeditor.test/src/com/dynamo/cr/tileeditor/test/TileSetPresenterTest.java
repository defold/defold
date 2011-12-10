package com.dynamo.cr.tileeditor.test;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.IOException;
import java.util.Arrays;

import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.test.AbstractPresenterTest;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;

public class TileSetPresenterTest extends AbstractPresenterTest {

    private TileSetNodePresenter presenter;

    @Override
    @Before
    public void setup() {
        super.setup();
        this.presenter = new TileSetNodePresenter();
    }

    @Test
    public void testAddCollisionGroup() {
        // Mocking
        TileSetNode tileSet = new TileSetNode();
        CollisionGroupNode collisionGroup = new CollisionGroupNode();
        tileSet.addCollisionGroup(collisionGroup);

        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collisionGroup));
        this.presenter.onAddCollisionGroup(getPresenterContext());
        verifyExecution();

        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(tileSet));
        this.presenter.onAddCollisionGroup(getPresenterContext());
        verifyExecution();
    }

    @Test
    public void testRemoveCollisionGroup() {
        TileSetNode tileSet = new TileSetNode();
        CollisionGroupNode collisionGroup = new CollisionGroupNode();
        tileSet.addCollisionGroup(collisionGroup);

        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collisionGroup));
        this.presenter.onRemoveCollisionGroup(getPresenterContext());
        verifyExecution();
    }

    @Test
    public void testSelectCollisionGroup() {
        TileSetNode tileSet = new TileSetNode();
        CollisionGroupNode collisionGroup = new CollisionGroupNode();
        tileSet.addCollisionGroup(collisionGroup);

        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collisionGroup));
        this.presenter.onSelectCollisionGroup(getPresenterContext(), -1);
        verifyRefresh();
        verifySelection();
        verifyNoExecution();

        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(tileSet));
        this.presenter.onSelectCollisionGroup(getPresenterContext(), -1);
        verifyNoSelection();
        verifyNoExecution();

        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(tileSet));
        this.presenter.onSelectCollisionGroup(getPresenterContext(), 0);
        verifyRefresh();
        verifySelection();
        verifyNoExecution();

        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collisionGroup));
        this.presenter.onSelectCollisionGroup(getPresenterContext(), 0);
        verifyNoSelection();
        verifyNoExecution();
    }

    /**
     * Part of Use Case 1.1.4 - Add a Collision Group
     *
     * @throws IOException
     */
    @Test
    public void testPainting() throws Exception {
        // Mocking
        TileSetNode tileSet = mock(TileSetNode.class);
        CollisionGroupNode collisionGroup = mock(CollisionGroupNode.class);
        when(collisionGroup.getName()).thenReturn("default");
        when(collisionGroup.getTileSetNode()).thenReturn(tileSet);
        when(tileSet.getTileCollisionGroups()).thenReturn(Arrays.asList("", "", "", ""));
        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collisionGroup));

        // Simulate painting
        this.presenter.onBeginPaintTile(getPresenterContext());
        this.presenter.onPaintTile(getPresenterContext(), 1);
        verifyRefresh();
        this.presenter.onPaintTile(getPresenterContext(), 1);
        this.presenter.onPaintTile(getPresenterContext(), 2);
        verifyRefresh();
        this.presenter.onPaintTile(getPresenterContext(), 2);
        this.presenter.onEndPaintTile(getPresenterContext());
        verifyExecution();
    }

}
