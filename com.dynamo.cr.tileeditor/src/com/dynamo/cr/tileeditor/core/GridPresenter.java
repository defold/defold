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

    public GridPresenter(GridModel model, IGridView view) {
        this.model = model;
        this.view = view;
        this.model.addTaggedPropertyListener(this);
    }

    public void load(TileGrid tileGrid) throws IOException {
        this.model.load(tileGrid);
    }

    @SuppressWarnings("unchecked")
    @Override
    public void propertyChange(PropertyChangeEvent evt) {
        // TODO: Control event storm?
        if (!(evt.getNewValue() instanceof IStatus)) {
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
