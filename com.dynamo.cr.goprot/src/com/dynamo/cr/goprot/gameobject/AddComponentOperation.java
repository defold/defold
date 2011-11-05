package com.dynamo.cr.goprot.gameobject;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class AddComponentOperation extends AbstractOperation {

    final private GameObjectNode gameObject;
    final private ComponentNode component;

    public AddComponentOperation(GameObjectNode gameObject, ComponentNode component) {
        super("Add Component");
        this.gameObject = gameObject;
        this.component = component;
        ComponentTypeNode componentType = (ComponentTypeNode)component.getChildren().get(0);
        this.component.setId(componentType.getTypeName().toLowerCase());
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        return Status.OK_STATUS;
    }

}
