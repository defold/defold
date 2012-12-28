package com.dynamo.cr.sceneed.core.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class MoveChildrenOperation extends AbstractSelectOperation {

    final private AddChildrenOperation addChildren;
    final private RemoveChildrenOperation removeChildren;

    public MoveChildrenOperation(List<Node> originals, Node targetParent, int index, List<Node> copies, IPresenterContext presenterContext) {
        super("Move", copies, presenterContext);
        // Adjust index for removed nodes
        for (Node child : originals) {
            if (child.getParent() == targetParent && targetParent.getChildren().indexOf(child) < index) {
                --index;
            }
        }
        this.addChildren = new AddChildrenOperation("Add", targetParent, index, copies, presenterContext);
        this.removeChildren = new RemoveChildrenOperation(originals, presenterContext);
    }

    @Override
    public IStatus doExecute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = this.removeChildren.execute(monitor, info);
        if (status.isOK()) {
            status = this.addChildren.execute(monitor, info);
            if (!status.isOK()) {
                this.removeChildren.undo(monitor, info);
            }
        }
        return status;
    }

    @Override
    public IStatus doRedo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = this.removeChildren.redo(monitor, info);
        if (status.isOK()) {
            status = this.addChildren.redo(monitor, info);
            if (!status.isOK()) {
                this.removeChildren.undo(monitor, info);
            }
        }
        return status;
    }

    @Override
    public IStatus doUndo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = this.addChildren.undo(monitor, info);
        if (status.isOK()) {
            status = this.removeChildren.undo(monitor, info);
            if (!status.isOK()) {
                this.addChildren.redo(monitor, info);
            }
        }
        return status;
    }

}
