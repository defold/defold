package com.dynamo.cr.editor.core.test.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;

public class DummyOperation extends AbstractOperation implements IMergeableOperation {

    private Object newValue;
    private Type type = Type.OPEN;

    public DummyOperation(Object newValue) {
        super("");
        this.newValue = newValue;
    }

    @Override
    public boolean canMerge(IMergeableOperation operation) {
        if (operation instanceof DummyOperation) {
            DummyOperation o = (DummyOperation) operation;
            return o.newValue.getClass() == newValue.getClass();
        }
        return false;
    }

    @Override
    public void mergeWith(IMergeableOperation operation) {
        DummyOperation o = (DummyOperation) operation;
        this.newValue = o.newValue;
    }

    @Override
    public void setType(Type type) {
        this.type = type;
    }

    @Override
    public Type getType() {
        return type;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return Status.OK_STATUS;
    }

}
