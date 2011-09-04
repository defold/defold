package com.dynamo.cr.tileeditor.core;

import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetPresenter implements IModelChangedListener {
    private final TileSetModel model;
    private final ITileSetView view;

    public TileSetPresenter(TileSetModel model, ITileSetView view) {
        this.model = model;
        this.view = view;
        this.model.addModelChangedListener(this);
    }

    public void load(TileSet tileSet) {
        this.model.load(tileSet);
    }

    public TileSetModel getModel() {
        return this.model;
    }

    public void setTileCollisionGroup(int index, String collisionGroup) {
        this.model.tiles.get(index).setCollisionGroup(collisionGroup);
    }

    @Override
    public void onModelChanged(ModelChangedEvent e) {
        if ((e.changes & ModelChangedEvent.CHANGE_FLAG_PROPERTIES) != 0) {
            this.view.refreshProperties();
        }
    }

}
