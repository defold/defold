package com.dynamo.cr.ddfeditor.operations;

import java.util.List;

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
import com.dynamo.cr.sceneed.core.Node;

public class RemoveShapeNodeOperation extends AbstractOperation {

    final private CollisionObjectNode collisionObject;
    final private CollisionShapeNode shape;
    final private IStructuredSelection oldSelection;
    final private IStructuredSelection newSelection;

    public RemoveShapeNodeOperation(CollisionShapeNode shape) {
        super("Remove Shape");
        this.collisionObject = (CollisionObjectNode)shape.getParent();
        this.shape = shape;
        this.oldSelection = shape.getModel().getSelection();
        List<Node> children = this.collisionObject.getChildren();
        int index = children.indexOf(shape);
        if (index + 1 < children.size()) {
            ++index;
        } else {
            --index;
        }
        Node selected = null;
        if (index >= 0) {
            selected = children.get(index);
        } else {
            selected = this.collisionObject;
        }
        this.newSelection = new StructuredSelection(selected);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collisionObject.removeShape(this.shape);
        this.collisionObject.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collisionObject.removeShape(this.shape);
        this.collisionObject.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collisionObject.addShape(this.shape);
        this.collisionObject.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
