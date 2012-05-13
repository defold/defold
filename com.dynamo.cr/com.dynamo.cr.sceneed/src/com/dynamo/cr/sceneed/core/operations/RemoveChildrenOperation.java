package com.dynamo.cr.sceneed.core.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;

public class RemoveChildrenOperation extends AbstractSelectOperation {

    private List<Node> children;
    private Node parent;

    public RemoveChildrenOperation(List<Node> children, IPresenterContext presenterContext) {
        super("Delete", NodeUtil.getSelectionReplacement(children), presenterContext);
        this.children = children;
        if (!children.isEmpty()) {
            this.parent = children.get(0).getParent();
        }
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        // Triggering any of these cases means the corresponding handler has invalid activation,
        // should never happen in production
        if (this.children.isEmpty()) {
            throw new ExecutionException("No items to delete.");
        }
        if (this.parent == null) {
            throw new ExecutionException("Only children can be deleted.");
        }
        for (Node child : this.children) {
            if (child.getParent() != this.parent) {
                throw new ExecutionException("Only siblings can be deleted.");
            }
        }
        for (Node child : this.children) {
            this.parent.removeChild(child);
        }
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (Node child : this.children) {
            this.parent.removeChild(child);
        }
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        int n = this.children.size();
        for (int i = n - 1; i >= 0; --i) {
            this.parent.addChild(this.children.get(i));
        }
        return Status.OK_STATUS;
    }

}
