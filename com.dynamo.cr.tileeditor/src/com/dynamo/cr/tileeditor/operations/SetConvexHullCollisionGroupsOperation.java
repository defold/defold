package com.dynamo.cr.tileeditor.operations;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.core.TileSetModel;

public class SetConvexHullCollisionGroupsOperation extends AbstractOperation {
    TileSetModel model;
    List<Integer> indices;
    List<String> oldCollisionGroups;
    String collisionGroup;

    public SetConvexHullCollisionGroupsOperation(TileSetModel model, String[] oldCollisionGroups, String collisionGroup) {
        super(collisionGroup.equals("")?"clear collision group":"set collision group to " + collisionGroup);
        this.model = model;
        this.indices = new ArrayList<Integer>();
        this.oldCollisionGroups = new ArrayList<String>();
        for (int i = 0; i < oldCollisionGroups.length; ++i) {
            if (oldCollisionGroups[i] != null) {
                this.indices.add(i);
                this.oldCollisionGroups.add(oldCollisionGroups[i]);
            }
        }
        this.collisionGroup = collisionGroup;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        int n = this.indices.size();
        for (int i = 0; i < n; ++i) {
            int index = this.indices.get(i);
            this.model.getConvexHulls().get(index).setCollisionGroup(this.collisionGroup);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        int n = this.indices.size();
        for (int i = 0; i < n; ++i) {
            int index = this.indices.get(i);
            this.model.getConvexHulls().get(index).setCollisionGroup(this.collisionGroup);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        int n = this.indices.size();
        for (int i = 0; i < n; ++i) {
            int index = this.indices.get(i);
            this.model.getConvexHulls().get(index).setCollisionGroup(this.oldCollisionGroups.get(i));
        }
        return Status.OK_STATUS;
    }

}
