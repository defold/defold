package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;

public class MovePointsOperation extends SetCurvesOperation {

    public MovePointsOperation(IUndoableOperation internalOperation) {
        super(Messages.MovePointsOperation_LABEL, internalOperation);
    }
}
