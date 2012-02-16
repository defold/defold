package com.dynamo.cr.sceneed.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class AddChildOperation extends AbstractSelectOperation {

    final private Node parent;
    final private Node child;

    public AddChildOperation(String title, Node parent, Node child, IPresenterContext presenterContext) {
        super(title, child, presenterContext);
        this.parent = parent;
        this.child = child;
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.parent.addChild(this.child);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.parent.addChild(this.child);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.parent.removeChild(this.child);
        return Status.OK_STATUS;
    }

}
