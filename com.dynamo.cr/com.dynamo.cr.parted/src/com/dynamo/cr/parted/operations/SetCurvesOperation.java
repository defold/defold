package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;

public class SetCurvesOperation extends AbstractOperation implements IMergeableOperation {

    IUndoableOperation internalOperation;

    public SetCurvesOperation(String label, IUndoableOperation internalOperation) {
        super(label);
        this.internalOperation = internalOperation;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        return this.internalOperation.execute(monitor, info);
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        return this.internalOperation.redo(monitor, info);
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        return this.internalOperation.undo(monitor, info);
    }

    @Override
    public void setType(Type type) {
        if (this.internalOperation instanceof IMergeableOperation) {
            IMergeableOperation mop = (IMergeableOperation)this.internalOperation;
            mop.setType(type);
        }
    }

    @Override
    public Type getType() {
        if (this.internalOperation instanceof IMergeableOperation) {
            IMergeableOperation mop = (IMergeableOperation)this.internalOperation;
            return mop.getType();
        }
        return IMergeableOperation.Type.CLOSE;
    }

    @Override
    public boolean canMerge(IMergeableOperation operation) {
        if (this.internalOperation instanceof IMergeableOperation) {
            IMergeableOperation mop = (IMergeableOperation)this.internalOperation;
            return mop.canMerge(operation);
        }
        return false;
    }

    @Override
    public void mergeWith(IMergeableOperation operation) {
        if (this.internalOperation instanceof IMergeableOperation) {
            IMergeableOperation mop = (IMergeableOperation)this.internalOperation;
            mop.mergeWith(operation);
        }
    }

}
