package com.dynamo.cr.go.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;

public class RemoveComponentOperation extends AbstractOperation {

    final private GameObjectNode gameObject;
    final private ComponentNode component;
    final private IStructuredSelection oldSelection;
    final private IStructuredSelection newSelection;

    public RemoveComponentOperation(ComponentNode component) {
        super("Remove Component");
        this.gameObject = (GameObjectNode)component.getParent();
        this.component = component;
        this.oldSelection = component.getModel().getSelection();
        Node selected = NodeUtil.getSelectionReplacement(component);
        this.newSelection = new StructuredSelection(selected);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        this.gameObject.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        this.gameObject.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        this.gameObject.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
