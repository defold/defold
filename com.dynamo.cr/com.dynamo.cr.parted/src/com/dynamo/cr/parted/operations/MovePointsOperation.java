package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoableOperation;

public class MovePointsOperation extends SetCurvesOperation {

    private IOperationHistory history;

    public MovePointsOperation(IOperationHistory history) {
        super(Messages.MovePointsOperation_LABEL);
        this.history = history;
    }

    public void begin() {
        this.history.openOperation(this, IOperationHistory.EXECUTE);
    }

    public void end(boolean addToHistory) {
        boolean add = hasChildren() && addToHistory;
        if (!add) {
            try {
                super.undo(null, null);
            } catch (ExecutionException e) {
                // pass through
                // TODO fix
            }
        }
        this.history.closeOperation(true, add, IOperationHistory.EXECUTE);
    }

    @Override
    public void add(IUndoableOperation operation) {
        super.add(operation);
        this.history.operationChanged(this);
    }

    @Override
    public void remove(IUndoableOperation operation) {
        super.remove(operation);
        this.history.operationChanged(this);
    }
}
