package com.dynamo.cr.tileeditor.core;

import java.io.IOException;
import java.io.OutputStream;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.tileeditor.operations.AddCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.RemoveCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.RenameCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.SetConvexHullCollisionGroupOperation;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetPresenter implements IModelChangedListener {
    private final TileSetModel model;
    private final ITileSetView view;

    public TileSetPresenter(TileSetModel model, ITileSetView view) {
        this.model = model;
        this.view = view;
        this.model.addModelChangedListener(this);
    }

    public void load(TileSet tileSet) throws IOException {
        this.model.load(tileSet);
    }

    public void save(OutputStream outputStream, IProgressMonitor monitor) throws IOException {
        this.model.save(outputStream, monitor);
    }

    public TileSetModel getModel() {
        return this.model;
    }

    public void setConvexHullCollisionGroup(int index, String collisionGroup) {
        this.model.executeOperation(new SetConvexHullCollisionGroupOperation(this.model, index, collisionGroup));
    }

    public void addCollisionGroup(String collisionGroup) {
        this.model.executeOperation(new AddCollisionGroupOperation(this.model, collisionGroup));
    }

    public void removeCollisionGroup(String collisionGroup) {
        this.model.executeOperation(new RemoveCollisionGroupOperation(this.model, collisionGroup));
    }

    public void renameCollisionGroup(String collisionGroup, String newCollisionGroup) {
        this.model.executeOperation(new RenameCollisionGroupOperation(this.model, collisionGroup, newCollisionGroup));
    }

    @Override
    public void onModelChanged(ModelChangedEvent e) {
        if ((e.changes & TileSetModel.CHANGE_FLAG_PROPERTIES) != 0) {
            this.view.refreshProperties();
        }
        if ((e.changes & TileSetModel.CHANGE_FLAG_COLLISION_GROUPS) != 0) {
            this.view.refreshOutline();
        }
        if ((e.changes & TileSetModel.CHANGE_FLAG_CONVEX_HULLS) != 0) {
            this.view.refreshEditingView();
        }
    }

}
