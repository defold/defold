package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;

public class SetTangentOperation extends SetCurvesOperation {

    public SetTangentOperation(IUndoableOperation internalOperation) {
        super(Messages.SetTangentOperation_LABEL, internalOperation);
    }
}
