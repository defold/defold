package com.dynamo.cr.parted.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.parted.curve.CurveEditor;
import com.dynamo.cr.parted.curve.HermiteSpline;

public class SelectSplineOperation extends AbstractOperation {

    private CurveEditor curveEditor;
    private HermiteSpline oldSel;
    private HermiteSpline newSel;

    public SelectSplineOperation(String label, CurveEditor curveEditor, HermiteSpline oldSel, HermiteSpline newSel) {
        super(label);
        this.curveEditor = curveEditor;
        this.oldSel = oldSel;
        this.newSel = newSel;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        curveEditor.setSpline(newSel);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        curveEditor.setSpline(newSel);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        curveEditor.setSpline(oldSel);
        return Status.OK_STATUS;
    }

}
