package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;


public class InsertPointOperation extends SetCurvesOperation {

    public InsertPointOperation(IUndoableOperation operation) {
        super(Messages.InsertPointOperation_LABEL);
        add(operation);
    }
}
