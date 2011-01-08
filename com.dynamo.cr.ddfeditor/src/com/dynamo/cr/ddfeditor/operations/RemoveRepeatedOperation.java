package com.dynamo.cr.ddfeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.TreeViewer;

import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;

public class RemoveRepeatedOperation extends AbstractOperation {

    private IPath path;
    private Object oldValue;
    private MessageNode message;
    private TreeViewer viewer;

    public RemoveRepeatedOperation(TreeViewer viewer, MessageNode message, IPath path, Object oldValue) {
        super("set " + path.getName());
        this.viewer = viewer;
        this.message = message;
        this.path = path;
        this.oldValue = oldValue;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.removeRepeated(this.path);
        viewer.refresh(this.path.getParent(), true);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.removeRepeated(this.path);
        viewer.refresh(this.path.getParent(), true);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.setField(this.path.getParent(), oldValue);
        viewer.refresh(this.path.getParent(), true);
        return Status.OK_STATUS;
    }

}
