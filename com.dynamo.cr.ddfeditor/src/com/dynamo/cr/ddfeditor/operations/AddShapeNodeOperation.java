package com.dynamo.cr.ddfeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.CollisionShapeNode;

public class AddShapeNodeOperation extends AbstractOperation {

    final private CollisionObjectNode collisionObjectNode;
    final private IStructuredSelection oldSelection;
    private CollisionShapeNode shapeNode;

    public AddShapeNodeOperation(CollisionObjectNode collisionObjectNode, CollisionShapeNode shapeNode) {
        super("Add Shape");
        this.collisionObjectNode = collisionObjectNode;
        this.shapeNode = shapeNode;
        this.oldSelection = this.collisionObjectNode.getModel().getSelection();
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collisionObjectNode.addShape(shapeNode);
        this.collisionObjectNode.getModel().setSelection(new StructuredSelection(this.shapeNode));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collisionObjectNode.addShape(this.shapeNode);
        this.collisionObjectNode.getModel().setSelection(new StructuredSelection(this.shapeNode));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collisionObjectNode.removeShape(this.shapeNode);
        this.collisionObjectNode.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
