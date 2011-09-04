package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
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

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.tileeditor.core.ITileSetView;
import com.dynamo.cr.tileeditor.core.TileSetModel;
import com.dynamo.cr.tileeditor.core.TileSetPresenter;
import com.dynamo.cr.tileeditor.operations.SetPropertiesOperation;
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

        assertEquals(tileSet.getImage(), this.model.getImage());
        assertEquals(tileSet.getTileWidth(), this.model.getTileWidth());
        assertEquals(tileSet.getTileHeight(), this.model.getTileHeight());
        assertEquals(tileSet.getTileMargin(), this.model.getTileMargin());
        assertEquals(tileSet.getTileSpacing(), this.model.getTileSpacing());
        assertEquals(tileSet.getCollision(), this.model.getCollision());
        assertEquals(tileSet.getMaterialTag(), this.model.getMaterialTag());

        verify(this.view).refreshProperties();
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

        IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
        @SuppressWarnings("unchecked")
        IPropertyAccessor<Object, TileSetModel> accessor = (IPropertyAccessor<Object, TileSetModel>) tmp;

        SetPropertiesOperation<String> operation = null;

        operation = new SetPropertiesOperation<String>(model, "image", accessor, this.model.getImage(), tileSetFile, this.model);
        this.model.executeOperation(operation);
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(2)).refreshProperties();

        operation = new SetPropertiesOperation<String>(model, "collision", accessor, this.model.getCollision(), tileSetFile, this.model);
        this.model.executeOperation(operation);
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(3)).refreshProperties();

        try {
            this.history.undo(this.undoContext, new NullProgressMonitor(), null);

            assertEquals(tileSetFile, this.model.getImage());
            assertEquals(prevTileSetFile, this.model.getCollision());
            verify(this.view, times(4)).refreshProperties();

            this.history.undo(this.undoContext, new NullProgressMonitor(), null);

            assertEquals(prevTileSetFile, this.model.getImage());
            verify(this.view, times(5)).refreshProperties();

            this.history.redo(this.undoContext, new NullProgressMonitor(), null);

            assertEquals(tileSetFile, this.model.getImage());
            assertEquals(prevTileSetFile, this.model.getCollision());
            verify(this.view, times(6)).refreshProperties();
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

        IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
        @SuppressWarnings("unchecked")
        IPropertyAccessor<Object, TileSetModel> accessor = (IPropertyAccessor<Object, TileSetModel>) tmp;

        SetPropertiesOperation<String> stringOperation = new SetPropertiesOperation<String>(model, "image", accessor, this.model.getImage(), tileSetFile, this.model);
        this.model.executeOperation(stringOperation);
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(1)).refreshProperties();

        SetPropertiesOperation<Integer> intOperation = new SetPropertiesOperation<Integer>(model, "tileWidth", accessor, this.model.getTileWidth(), 16, this.model);
        this.model.executeOperation(intOperation);
        assertEquals(16, this.model.getTileWidth());
        verify(this.view, times(1)).refreshProperties();

        intOperation = new SetPropertiesOperation<Integer>(model, "tileHeight", accessor, this.model.getTileHeight(), 16, this.model);
        this.model.executeOperation(intOperation);
        assertEquals(16, this.model.getTileHeight());
        verify(this.view, times(1)).refreshProperties();

        intOperation = new SetPropertiesOperation<Integer>(model, "tileSpacing", accessor, this.model.getTileSpacing(), 1, this.model);
        this.model.executeOperation(intOperation);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(1)).refreshProperties();

        assertEquals(20, this.model.getTiles().size());

        for (TileSetModel.Tile tile : this.model.getTiles()) {
            assertEquals(0, tile.getConvexHullCount());
        }

        stringOperation = new SetPropertiesOperation<String>(model, "collision", accessor, this.model.getCollision(), tileSetFile, this.model);
        this.model.executeOperation(stringOperation);
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(1)).refreshProperties();

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
