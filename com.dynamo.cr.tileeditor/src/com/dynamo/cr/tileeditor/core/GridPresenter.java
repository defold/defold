package com.dynamo.cr.tileeditor.core;

import java.io.IOException;

import com.dynamo.tile.proto.Tile.TileGrid;

public class GridPresenter implements IModelChangedListener {

    private final GridModel model;
    private final IGridView view;

    public GridPresenter(GridModel model, IGridView view) {
        this.model = model;
        this.view = view;
        this.model.addModelChangedListener(this);
    }

    public void load(TileGrid tileGrid) throws IOException {
        this.model.load(tileGrid);
    }

    @Override
    public void onModelChanged(ModelChangedEvent e) {
        if ((e.changes & GridModel.CHANGE_FLAG_PROPERTIES) != 0) {
            this.view.refreshProperties();
        }
        if ((e.changes & GridModel.CHANGE_FLAG_LAYERS) != 0) {
            this.view.refreshOutline();
        }
        if ((e.changes & GridModel.CHANGE_FLAG_CELLS) != 0) {
            this.view.refreshEditingView();
        }
    }

}
