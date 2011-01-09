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

public class SetFieldOperation extends AbstractOperation {

    private IPath path;
    private Object oldValue;
    private Object newValue;
    private MessageNode message;
    private TreeViewer viewer;
    private Object element;

    public SetFieldOperation(TreeViewer viewer, MessageNode message, IPath path, Object oldValue, Object newValue, Object element) {
        super("set " + path.getName());
        this.viewer = viewer;
        this.message = message;
        this.path = path;
        this.oldValue = oldValue;
        this.newValue = newValue;
        this.element = element;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.setField(this.path, newValue);
        // NOTE: We use refresh here with parent in order to update label for parent as well
        // when editing messages
        viewer.refresh(path.getParent(), true);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.setField(this.path, newValue);
        viewer.refresh(path.getParent(), true);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.setField(this.path, oldValue);
        viewer.refresh(path.getParent(), true);
        return Status.OK_STATUS;
    }

}
