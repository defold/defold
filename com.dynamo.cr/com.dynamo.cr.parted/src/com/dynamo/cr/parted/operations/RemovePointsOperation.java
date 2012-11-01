package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.ISelection;

import com.dynamo.cr.parted.curve.CurvePresenter;



public class RemovePointsOperation extends CurveSelectOperation {

    public RemovePointsOperation(IUndoableOperation internalOperation, ISelection oldSelection, ISelection newSelection, CurvePresenter presenter) {
        super(Messages.RemovePointsOperation_LABEL, internalOperation, oldSelection, newSelection, presenter);
    }
}
