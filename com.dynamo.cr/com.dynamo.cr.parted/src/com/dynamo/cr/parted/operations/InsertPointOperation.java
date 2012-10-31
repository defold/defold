package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.ISelection;

import com.dynamo.cr.parted.curve.CurvePresenter;


public class InsertPointOperation extends CurveSelectOperation {

    public InsertPointOperation(IUndoableOperation internalOperation, ISelection oldSelection, ISelection newSelection, CurvePresenter presenter) {
        super(Messages.InsertPointOperation_LABEL, internalOperation, oldSelection, newSelection, presenter);
    }
}
