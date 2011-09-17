package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.io.IOException;
import java.util.List;

import org.eclipse.core.runtime.IStatus;

import com.dynamo.tile.proto.Tile.TileGrid;

public class GridPresenter implements PropertyChangeListener {

    private final GridModel model;
    private final IGridView view;
    private boolean loading = false;
    private int undoRedoCounter;

    public GridPresenter(GridModel model, IGridView view) {
        this.model = model;
        this.view = view;
        this.model.addTaggedPropertyListener(this);
    }

    public void load(TileGrid tileGrid) throws IOException {
        this.loading = true;
        try {
            this.model.load(tileGrid);
        } finally {
            this.loading = false;
        }
        refresh();
        setUndoRedoCounter(0);
    }

    public void refresh() {
        this.view.setTileSetProperty(this.model.getTileSet());
        this.view.setCellWidthProperty(this.model.getCellWidth());
        this.view.setCellHeightProperty(this.model.getCellHeight());
        this.view.setLayers(this.model.getLayers());
        this.view.refreshProperties();
    }

    private void setUndoRedoCounter(int undoRedoCounter) {
        boolean prevDirty = this.undoRedoCounter != 0;
        boolean dirty = undoRedoCounter != 0;
        if (prevDirty != dirty) {
            this.view.setDirty(dirty);
        }
        this.undoRedoCounter = undoRedoCounter;
    }

    @SuppressWarnings("unchecked")
    @Override
    public void propertyChange(PropertyChangeEvent evt) {
        if (loading)
            return;

        this.view.setValid(this.model.isOk());
        this.view.refreshProperties();
        if (evt.getNewValue() instanceof IStatus) {
        }
        else {
            if (evt.getSource() instanceof GridModel) {
                if (evt.getPropertyName().equals("tileSet")) {
                    view.setTileSetProperty((String)evt.getNewValue());
                } else if (evt.getPropertyName().equals("cellWidth")) {
                    view.setCellWidthProperty((Float)evt.getNewValue());
                } else if (evt.getPropertyName().equals("cellHeight")) {
                    view.setCellHeightProperty((Float)evt.getNewValue());
                } else if (evt.getPropertyName().equals("layers")) {
                    view.setLayers((List<GridModel.Layer>)evt.getNewValue());
                }
            }
        }
    }

}
