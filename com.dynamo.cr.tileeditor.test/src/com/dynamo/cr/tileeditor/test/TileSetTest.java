package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.tileeditor.core.ITileSetView;
import com.dynamo.cr.tileeditor.core.TileSetModel;
import com.dynamo.cr.tileeditor.core.TileSetPresenter;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetTest {
    ITileSetView view;
    TileSetModel model;
    TileSetPresenter presenter;
    IOperationHistory history;
    IUndoContext undoContext;

    @Before
    public void setup() {
        System.setProperty("java.awt.headless", "true");
        this.view = mock(ITileSetView.class);
        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();
        this.model = new TileSetModel(this.history, this.undoContext);
        this.presenter = new TileSetPresenter(this.model, this.view);
    }

    private TileSet loadNewFile() {
        // new file
        TileSet tileSet = TileSet.newBuilder()
                .setImage("")
                .setTileWidth(0)
                .setTileHeight(0)
                .setTileSpacing(0)
                .setTileMargin(0)
                .setCollision("")
                .setMaterialTag("tile")
                .build();
        this.presenter.load(tileSet);
        return tileSet;
    }

    /**
     * Test load
     */
    @Test
    public void testNewFile() {
        TileSet tileSet = loadNewFile();

        verify(this.view).setImage(eq(tileSet.getImage()));
        assertEquals(tileSet.getImage(), this.model.getImage());
        verify(this.view).setTileWidth(eq(tileSet.getTileWidth()));
        assertEquals(tileSet.getTileWidth(), this.model.getTileWidth());
        verify(this.view).setTileHeight(eq(tileSet.getTileHeight()));
        assertEquals(tileSet.getTileHeight(), this.model.getTileHeight());
        verify(this.view).setTileMargin(eq(tileSet.getTileMargin()));
        assertEquals(tileSet.getTileMargin(), this.model.getTileMargin());
        verify(this.view).setTileSpacing(eq(tileSet.getTileSpacing()));
        assertEquals(tileSet.getTileSpacing(), this.model.getTileSpacing());
        verify(this.view).setCollision(eq(tileSet.getCollision()));
        assertEquals(tileSet.getCollision(), this.model.getCollision());
        verify(this.view).setMaterialTag(eq(tileSet.getMaterialTag()));
        assertEquals(tileSet.getMaterialTag(), this.model.getMaterialTag());
    }

    /**
     * Test undo/redo
     */
    @Test
    public void testUndoRedo() {
        TileSet tileSet = loadNewFile();

        String prevTileSetFile = tileSet.getImage();
        String tileSetFile = "test/mario_tileset.png";

        assertEquals(prevTileSetFile, this.model.getImage());
        assertEquals(prevTileSetFile, this.model.getCollision());

        this.presenter.setImage(tileSetFile);
        assertEquals(tileSetFile, this.model.getImage());

        this.presenter.setCollision(tileSetFile);
        assertEquals(tileSetFile, this.model.getCollision());

        try {
            this.history.undo(this.undoContext, new NullProgressMonitor(), null);

            assertEquals(tileSetFile, this.model.getImage());
            assertEquals(prevTileSetFile, this.model.getCollision());
            verify(this.view, times(2)).setCollision(prevTileSetFile);

            this.history.undo(this.undoContext, new NullProgressMonitor(), null);

            assertEquals(prevTileSetFile, this.model.getImage());
            verify(this.view, times(2)).setImage(prevTileSetFile);

            this.history.redo(this.undoContext, new NullProgressMonitor(), null);

            assertEquals(tileSetFile, this.model.getImage());
            assertEquals(prevTileSetFile, this.model.getCollision());
            verify(this.view, times(2)).setImage(tileSetFile);
        } catch (ExecutionException e) {
            // fail
            assertTrue(false);
        }

    }

    /**
     * Use Case 1.1
     */
    @Test
    public void testSuperMarioTileSet() {
        testNewFile();

        String tileSetFile = "test/mario_tileset.png";
        // set properties
        this.presenter.setImage(tileSetFile);
        assertEquals(tileSetFile, this.model.getImage());
        this.presenter.setTileWidth(16);
        assertEquals(16, this.model.getTileWidth());
        this.presenter.setTileHeight(16);
        assertEquals(16, this.model.getTileHeight());
        this.presenter.setTileMargin(0);
        assertEquals(0, this.model.getTileMargin());
        this.presenter.setTileSpacing(1);
        assertEquals(1, this.model.getTileSpacing());

        assertEquals(20, this.model.getTiles().size());

        for (TileSetModel.Tile tile : this.model.getTiles()) {
            assertEquals(0, tile.getConvexHullCount());
        }
        this.presenter.setCollision(tileSetFile);
        assertEquals(tileSetFile, this.model.getCollision());
        for (int i = 0; i < 10; ++i) {
            assertEquals(8, this.model.getTiles().get(i).getConvexHullCount());
        }
        for (int i = 10; i < 14; ++i) {
            assertEquals(4, this.model.getTiles().get(i).getConvexHullCount());
        }
        for (int i = 14; i < 20; ++i) {
            assertEquals(8, this.model.getTiles().get(i).getConvexHullCount());
        }

        this.presenter.setTileCollisionGroup(1, "obstruction");
        assertEquals("obstruction", this.model.getTiles().get(1).getCollisionGroup());

        this.presenter.setTileCollisionGroup(2, "hazard");
        assertEquals("hazard", this.model.getTiles().get(2).getCollisionGroup());
    }
}
