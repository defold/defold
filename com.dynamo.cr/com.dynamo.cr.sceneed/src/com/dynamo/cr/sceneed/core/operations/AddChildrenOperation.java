package com.dynamo.cr.sceneed.core.operations;

import java.util.Collections;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class AddChildrenOperation extends AbstractSelectOperation {

    final private Node parent;
    final private List<Node> children;

    public AddChildrenOperation(String title, Node parent, Node child, IPresenterContext presenterContext) {
        super(title, child, presenterContext);
        this.parent = parent;
        this.children = Collections.singletonList(child);
    }

    public AddChildrenOperation(String title, Node parent, List<Node> children, IPresenterContext presenterContext) {
        super(title, children, presenterContext);
        this.parent = parent;
        this.children = children;
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (Node child : this.children) {
            this.parent.addChild(child);
        }
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (Node child : this.children) {
            this.parent.addChild(child);
        }
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (Node child : this.children) {
            this.parent.removeChild(child);
        }
        return Status.OK_STATUS;
    }

}
