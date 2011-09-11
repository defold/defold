package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyFloat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.io.IOException;
import java.util.List;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.tileeditor.core.GridModel;
import com.dynamo.cr.tileeditor.core.GridPresenter;
import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.dynamo.tile.proto.Tile.TileLayer;

public class GridTest {

    private IGridView view;
    private GridModel model;
    private GridPresenter presenter;
    private IOperationHistory history;
    private IUndoContext undoContext;

    @Before
    public void setup() {
        System.setProperty("java.awt.headless", "true");
        this.view = mock(IGridView.class);
        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();
        this.model = new GridModel(this.history, this.undoContext);
        this.presenter = new GridPresenter(this.model, this.view);
    }

    private TileGrid loadEmptyFile() throws IOException {
        // new file
        TileGrid.Builder tileGridBuilder = TileGrid.newBuilder()
                .setTileSet("")
                .setCellWidth(0)
                .setCellHeight(0);
        TileLayer.Builder tileLayerBuilder = TileLayer.newBuilder()
                .setId("layer1")
                .setZ(0.0f)
                .setIsVisible(1);
        tileGridBuilder.addLayers(tileLayerBuilder);
        TileGrid tileGrid = tileGridBuilder.build();
        this.presenter.load(tileGrid);
        return tileGrid;
    }

    /**
     * Test load
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testLoad() throws IOException {
        TileGrid tileGrid = loadEmptyFile();

        assertEquals(tileGrid.getTileSet(), this.model.getTileSet());
        assertEquals(tileGrid.getCellWidth(), this.model.getCellWidth(), 0.000001f);
        assertEquals(tileGrid.getCellHeight(), this.model.getCellHeight(), 0.000001f);
        assertEquals(1, this.model.getLayers().size());
        GridModel.Layer layer = this.model.getLayers().get(0);
        assertEquals("layer1", layer.getId());
        assertEquals(0.0f, layer.getZ(), 0.000001f);
        assertTrue(layer.isVisible());

        verify(this.view, times(1)).setTileSetProperty(any(String.class));
        verify(this.view, never()).setCellWidthProperty(anyFloat());
        verify(this.view, never()).setCellHeightProperty(anyFloat());
        verify(this.view, times(1)).setLayers(any(List.class));
    }
}
