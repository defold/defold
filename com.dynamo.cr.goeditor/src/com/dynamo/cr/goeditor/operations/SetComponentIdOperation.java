package com.dynamo.cr.goeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.goeditor.Component;
import com.dynamo.cr.goeditor.GameObjectModel;

public class SetComponentIdOperation extends AbstractOperation {

    private GameObjectModel model;
    private Component component;
    private String newId;
    private String oldId;

    public SetComponentIdOperation(Component component, String newId, GameObjectModel model) {
        super("Set id to " + newId);
        this.model = model;
        this.component = component;
        this.newId = newId;
        this.oldId = component.getId();
        this.component.setId(model.getUniqueId(component.getFileExtension()));
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        model.setComponentId(component, newId);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        model.setComponentId(component, newId);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        model.setComponentId(component, oldId);
        return Status.OK_STATUS;
    }

}
