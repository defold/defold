package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.viewers.ISelection;

import com.dynamo.cr.parted.curve.CurvePresenter;


public class CurveSelectOperation extends SetCurvesOperation {

    private ISelection oldSelection;
    private ISelection newSelection;
    private CurvePresenter presenter;

    public CurveSelectOperation(String label, IUndoableOperation internalOperation, ISelection oldSelection, ISelection newSelection, CurvePresenter presenter) {
        super(label, internalOperation);
        this.oldSelection = oldSelection;
        this.newSelection = newSelection;
        this.presenter = presenter;
    }

    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = super.execute(monitor, info);
        this.presenter.setSelection(newSelection);
        return status;
    };

    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = super.undo(monitor, info);
        this.presenter.setSelection(oldSelection);
        return status;
    };

    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = super.redo(monitor, info);
        this.presenter.setSelection(newSelection);
        return status;
    };
}
