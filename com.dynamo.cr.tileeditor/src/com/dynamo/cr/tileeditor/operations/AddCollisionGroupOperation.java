package com.dynamo.cr.tileeditor.operations;

import java.text.MessageFormat;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.core.TileSetModel;

public class AddCollisionGroupOperation extends AbstractOperation {
    TileSetModel model;
    String collisionGroup;
    String[] selectedCollisionGroups;

    public AddCollisionGroupOperation(TileSetModel model, String collisionGroup) {
        super("add " + collisionGroup);
        List<String> collisionGroups = model.getCollisionGroups();
        if (collisionGroups != null) {
            String pattern = collisionGroup + "{0}";
            int i = 1;
            while (collisionGroups.contains(collisionGroup)) {
                collisionGroup = MessageFormat.format(pattern, i++);
            }
        }
        setLabel("add " + collisionGroup);
        this.model = model;
        this.collisionGroup = collisionGroup;
        this.selectedCollisionGroups = model.getSelectedCollisionGroups();
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroup(collisionGroup);
        this.model.addCollisionGroup(this.collisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroup(collisionGroup);
        this.model.addCollisionGroup(this.collisionGroup);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setSelectedCollisionGroups(selectedCollisionGroups);
        this.model.removeCollisionGroup(this.collisionGroup);
        return Status.OK_STATUS;
    }

}
