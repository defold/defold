package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;


public class InsertPointOperation extends SetCurvesOperation {

    public InsertPointOperation(IUndoableOperation internalOperation) {
        super(Messages.InsertPointOperation_LABEL, internalOperation);
    }
}
