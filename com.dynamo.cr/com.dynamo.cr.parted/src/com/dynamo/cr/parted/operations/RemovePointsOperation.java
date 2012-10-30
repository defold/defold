package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;



public class RemovePointsOperation extends SetCurvesOperation {

    public RemovePointsOperation(IUndoableOperation internalOperation) {
        super(Messages.RemovePointsOperation_LABEL, internalOperation);
    }
}
