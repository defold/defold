package com.dynamo.cr.sceneed.gameobject;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

public class AddComponentOperation extends AbstractOperation {

    final private GameObjectNode gameObject;
    final private ComponentNode component;
    final private IStructuredSelection oldSelection;

    public AddComponentOperation(GameObjectNode gameObject, ComponentNode component) {
        super("Add Component");
        this.gameObject = gameObject;
        this.component = component;
        this.oldSelection = this.gameObject.getModel().getSelection();
        ComponentTypeNode componentType = (ComponentTypeNode)component.getChildren().get(0);
        String id = componentType.getTypeName().toLowerCase();
        id.replace(' ', '_');
        this.component.setId(id);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        this.gameObject.getModel().setSelection(new StructuredSelection(this.component));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        this.gameObject.getModel().setSelection(new StructuredSelection(this.component));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        this.gameObject.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
