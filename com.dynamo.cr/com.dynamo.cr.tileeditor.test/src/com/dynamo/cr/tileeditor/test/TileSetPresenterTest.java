package com.dynamo.cr.tileeditor.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.IOException;
import java.util.Collections;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.test.AbstractPresenterTest;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;
import com.dynamo.tile.proto.Tile.Playback;

public class TileSetPresenterTest extends AbstractPresenterTest {

    private TileSetNodePresenter presenter;

    @Override
    @Before
    public void setup() {
        super.setup();
        this.presenter = new TileSetNodePresenter();
    }

    // Collision Groups

    @Test
    public void testAddCollisionGroup() {
        // Mocking
        TileSetNode tileSet = new TileSetNode();
        CollisionGroupNode collisionGroup = new CollisionGroupNode();
        tileSet.addChild(collisionGroup);

        select(collisionGroup);
        this.presenter.onAddCollisionGroup(getPresenterContext());
        verifyExecution();

        select(tileSet);
        this.presenter.onAddCollisionGroup(getPresenterContext());
        verifyExecution();
    }

    @Test
    public void testSelectCollisionGroup() {
        TileSetNode tileSet = new TileSetNode();
        CollisionGroupNode collisionGroup = new CollisionGroupNode();
        tileSet.addChild(collisionGroup);

        select(collisionGroup);
        this.presenter.onSelectCollisionGroup(getPresenterContext(), -1);
        verifyExecution();

        select(tileSet);
        this.presenter.onSelectCollisionGroup(getPresenterContext(), -1);
        verifyNoExecution();

        select(tileSet);
        this.presenter.onSelectCollisionGroup(getPresenterContext(), 0);
        verifyExecution();

        select(collisionGroup);
        this.presenter.onSelectCollisionGroup(getPresenterContext(), 0);
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
        when(collisionGroup.getId()).thenReturn("default");
        when(collisionGroup.getTileSetNode()).thenReturn(tileSet);
        when(tileSet.getTileCollisionGroups()).thenReturn(Collections.nCopies(4, (CollisionGroupNode)null));
        select(collisionGroup);

        // Simulate painting
        this.presenter.onBeginPaintTile(getPresenterContext());
        this.presenter.onPaintTile(getPresenterContext(), 1);
        verifyRefreshRenderView();
        this.presenter.onPaintTile(getPresenterContext(), 1);
        this.presenter.onPaintTile(getPresenterContext(), 2);
        verifyRefreshRenderView();
        this.presenter.onPaintTile(getPresenterContext(), 2);
        this.presenter.onEndPaintTile(getPresenterContext());
        verifyExecution();

        // Simulate erasing
        when(tileSet.getTileCollisionGroups()).thenReturn(Collections.nCopies(4, collisionGroup));
        select(tileSet);
        this.presenter.onBeginPaintTile(getPresenterContext());
        this.presenter.onPaintTile(getPresenterContext(), 1);
        verifyRefreshRenderView();
        this.presenter.onEndPaintTile(getPresenterContext());
        verifyExecution();
    }

    // Animations

    @Test
    public void testAddAnimation() {
        // Mocking
        TileSetNode tileSet = new TileSetNode();
        AnimationNode animation = new AnimationNode();
        tileSet.addChild(animation);

        select(animation);
        this.presenter.onAddAnimation(getPresenterContext());
        verifyExecution();

        select(tileSet);
        this.presenter.onAddAnimation(getPresenterContext());
        verifyExecution();
    }

    @Test
    public void testRemoveAnimation() {
        TileSetNode tileSet = new TileSetNode();
        AnimationNode animation = new AnimationNode();
        tileSet.addChild(animation);

        select(animation);
        this.presenter.onRemoveAnimation(getPresenterContext());
        verifyExecution();
    }

    @Test
    public void testAnimationPlayback() throws Exception {

        AnimationNode animation = new AnimationNode();
        animation.setStartTile(1);
        animation.setEndTile(4);
        animation.setPlayback(Playback.PLAYBACK_ONCE_FORWARD);
        animation.setFps(30);
        select(animation);

        this.presenter.onPlayAnimation(getPresenterContext());
        verify(getPresenterContext(), times(1)).asyncExec(any(Runnable.class));

        this.presenter.onPlayAnimation(getPresenterContext());
        verify(getPresenterContext(), times(1)).asyncExec(any(Runnable.class));

        this.presenter.onStopAnimation(getPresenterContext());
        this.presenter.onPlayAnimation(getPresenterContext());
        verify(getPresenterContext(), times(2)).asyncExec(any(Runnable.class));
    }

}
