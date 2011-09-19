package com.dynamo.cr.goeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.goeditor.Component;
import com.dynamo.cr.goeditor.GameObjectModel;

public class RemoveComponentOperation extends AbstractOperation {

    private GameObjectModel model;
    private Component component;

    public RemoveComponentOperation(Component component, GameObjectModel model) {
        super(component.toString());
        this.model = model;
        this.component = component;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        model.removeComponent(component);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        model.addComponent(component);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        model.addComponent(component);
        return Status.OK_STATUS;
    }

}
