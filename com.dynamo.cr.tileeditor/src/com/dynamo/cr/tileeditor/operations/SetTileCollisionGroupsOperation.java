package com.dynamo.cr.tileeditor.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class SetTileCollisionGroupsOperation extends AbstractOperation {
    TileSetNode tileSet;
    List<String> oldTileCollisionGroups;
    List<String> newTileCollisionGroups;

    public SetTileCollisionGroupsOperation(TileSetNode tileSet, List<String> oldTileCollisionGroups, List<String> newTileCollisionGroups, String collisionGroup) {
        super(collisionGroup.equals("")?"Clear Collision Group":"Set Collision Group");
        this.tileSet = tileSet;
        this.oldTileCollisionGroups = oldTileCollisionGroups;
        this.newTileCollisionGroups = newTileCollisionGroups;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.setTileCollisionGroups(this.newTileCollisionGroups);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.setTileCollisionGroups(this.oldTileCollisionGroups);
        return Status.OK_STATUS;
    }

}
