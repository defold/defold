package com.dynamo.cr.parted.operations;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.commands.operations.ICompositeOperation;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class SetCurvesOperation extends AbstractOperation implements ICompositeOperation {

    private List<IUndoableOperation> children = new ArrayList<IUndoableOperation>();

    public SetCurvesOperation(String label) {
        super(label);
    }

    protected boolean hasChildren() {
        return !children.isEmpty();
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        for (IUndoableOperation op : children) {
            IStatus status = op.execute(monitor, info);
            if (!status.isOK()) {
                return status;
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        for (IUndoableOperation op : children) {
            IStatus status = op.redo(monitor, info);
            if (!status.isOK()) {
                return status;
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        List<IUndoableOperation> revChildren = new ArrayList<IUndoableOperation>(this.children);
        Collections.reverse(revChildren);
        for (IUndoableOperation op : revChildren) {
            IStatus status = op.undo(monitor, info);
            if (!status.isOK()) {
                return status;
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public void add(IUndoableOperation operation) {
        children.add(operation);
    }

    @Override
    public void remove(IUndoableOperation operation) {
        children.remove(operation);
    }

}
