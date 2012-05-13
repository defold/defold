package com.dynamo.cr.ddfeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.TreeViewer;

import com.dynamo.cr.ddfeditor.ProtoTreeEditor;
import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;

public class SetFieldOperation extends AbstractOperation {

    private IPath path;
    private Object oldValue;
    private Object newValue;
    private MessageNode message;
    private TreeViewer viewer;
    private ProtoTreeEditor editor;

    public SetFieldOperation(ProtoTreeEditor editor, TreeViewer viewer, MessageNode message, IPath path, Object oldValue, Object newValue) {
        super("set " + path.getName());
        this.viewer = viewer;
        this.message = message;
        this.path = path;
        this.oldValue = oldValue;
        this.newValue = newValue;
        this.editor = editor;
    }

    void update() {
        // NOTE: We use refresh here with parent in order to update label for parent as well
        // when editing messages
        viewer.refresh(path.getParent(), true);
        viewer.update(path, null);
        this.editor.fireFieldChanged(this.message, this.path);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.setField(this.path, newValue);
        update();
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.setField(this.path, newValue);
        update();
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        message.setField(this.path, oldValue);
        update();
        return Status.OK_STATUS;
    }

}
