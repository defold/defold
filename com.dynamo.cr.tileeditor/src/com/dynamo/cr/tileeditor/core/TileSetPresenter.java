package com.dynamo.cr.tileeditor.core;

import java.io.OutputStream;

import org.eclipse.core.runtime.IProgressMonitor;

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

    public void save(OutputStream outputStream, IProgressMonitor monitor) {
        this.model.save(outputStream, monitor);
    }

    public TileSetModel getModel() {
        return this.model;
    }

    public void setTileCollisionGroup(int index, String collisionGroup) {
        this.model.setTileCollisionGroup(index, collisionGroup);
    }

    @Override
    public void onModelChanged(ModelChangedEvent e) {
        if ((e.changes & TileSetModel.CHANGE_FLAG_PROPERTIES) != 0) {
            this.view.refreshProperties();
        }
        if ((e.changes & TileSetModel.CHANGE_FLAG_COLLISION_GROUPS) != 0) {
            this.view.refreshOutline();
        }
        if ((e.changes & TileSetModel.CHANGE_FLAG_TILES) != 0) {
            this.view.refreshEditingView();
        }
    }

    public void addCollisionGroup(String collisionGroup) {
        this.model.addCollisionGroup(collisionGroup);
    }

    public void renameCollisionGroup(String collisionGroup, String newCollisionGroup) {
        this.model.renameCollisionGroup(collisionGroup, newCollisionGroup);
    }

}
