package com.dynamo.cr.editor.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.ICompositeOperation;
import org.eclipse.core.commands.operations.IOperationApprover;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;

/**
 * Mergeable delegating operation history. See {@link IMergeableOperation}
 * for more information
 * @author chmu
 *
 */
public class MergeableDelegatingOperationHistory implements IOperationHistory {

    private IOperationHistory history;

    public MergeableDelegatingOperationHistory(IOperationHistory history) {
        this.history = history;
    }

    @Override
    public void add(IUndoableOperation operation) {
        history.add(operation);
    }

    @Override
    public void addOperationApprover(IOperationApprover approver) {
        history.addOperationApprover(approver);
    }

    @Override
    public void addOperationHistoryListener(IOperationHistoryListener listener) {
        history.addOperationHistoryListener(listener);
    }

    @Override
    public void closeOperation(boolean operationOK, boolean addToHistory,
            int mode) {
        history.closeOperation(operationOK, addToHistory, mode);
    }

    @Override
    public boolean canRedo(IUndoContext context) {
        return history.canRedo(context);
    }

    @Override
    public boolean canUndo(IUndoContext context) {
        return history.canUndo(context);
    }

    @Override
    public void dispose(IUndoContext context, boolean flushUndo,
            boolean flushRedo, boolean flushContext) {
        history.dispose(context, flushUndo, flushRedo, flushContext);
    }

    @Override
    public IStatus execute(IUndoableOperation operation,
            IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {

        for (IUndoContext context : operation.getContexts()) {
            IUndoableOperation undoOp = getUndoOperation(context);
            if (operation.hasContext(context)
               && undoOp != null
               && undoOp instanceof IMergeableOperation
               && operation instanceof IMergeableOperation) {
                IMergeableOperation mop = (IMergeableOperation) undoOp;

                Type prevType = mop.getType();
                Type type = ((IMergeableOperation) operation).getType();
                boolean mergeable = prevType != Type.CLOSE && type != Type.OPEN;
                if (mergeable && mop.canMerge((IMergeableOperation) operation)) {
                    mop.mergeWith((IMergeableOperation) operation);
                    if (type == Type.CLOSE) {
                        mop.setType(Type.CLOSE);
                    }
                    undoOp.execute(monitor, info);
                    this.history.operationChanged(undoOp);
                    return Status.OK_STATUS;
                }
            }
        }

        return history.execute(operation, monitor, info);
    }

    @Override
    public int getLimit(IUndoContext context) {
        return history.getLimit(context);
    }

    @Override
    public IUndoableOperation[] getRedoHistory(IUndoContext context) {
        return history.getRedoHistory(context);
    }

    @Override
    public IUndoableOperation getRedoOperation(IUndoContext context) {
        return history.getRedoOperation(context);
    }

    @Override
    public IUndoableOperation[] getUndoHistory(IUndoContext context) {
        return history.getUndoHistory(context);
    }

    @Override
    public void openOperation(ICompositeOperation operation, int mode) {
        history.openOperation(operation, mode);
    }

    @Override
    public void operationChanged(IUndoableOperation operation) {
        history.operationChanged(operation);
    }

    @Override
    public IUndoableOperation getUndoOperation(IUndoContext context) {
        return history.getUndoOperation(context);
    }

    @Override
    public IStatus redo(IUndoContext context, IProgressMonitor monitor,
            IAdaptable info) throws ExecutionException {
        return history.redo(context, monitor, info);
    }

    @Override
    public IStatus redoOperation(IUndoableOperation operation,
            IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return history.redoOperation(operation, monitor, info);
    }

    @Override
    public void removeOperationApprover(IOperationApprover approver) {
        history.removeOperationApprover(approver);
    }

    @Override
    public void removeOperationHistoryListener(
            IOperationHistoryListener listener) {
        history.removeOperationHistoryListener(listener);
    }

    @Override
    public void replaceOperation(IUndoableOperation operation,
            IUndoableOperation[] replacements) {
        history.replaceOperation(operation, replacements);
    }

    @Override
    public void setLimit(IUndoContext context, int limit) {
        history.setLimit(context, limit);
    }

    @Override
    public IStatus undo(IUndoContext context, IProgressMonitor monitor,
            IAdaptable info) throws ExecutionException {
        return history.undo(context, monitor, info);
    }

    @Override
    public IStatus undoOperation(IUndoableOperation operation,
            IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return history.undoOperation(operation, monitor, info);
    }

}
