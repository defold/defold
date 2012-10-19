package com.dynamo.cr.parted.curve;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class AlterSplineOperation extends AbstractOperation {

    private HermiteSpline spline;
    private HermiteSpline oldSpline;
    private int index;
    private Object[] input;

    public AlterSplineOperation(String label, Object[] input, int index, HermiteSpline oldSpline, HermiteSpline spline) {
        super(label);
        this.input = input;
        this.index = index;
        this.oldSpline = oldSpline;
        this.spline = spline;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        input[index] = spline;
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        input[index] = spline;
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        input[index] = oldSpline;
        return Status.OK_STATUS;
    }

}
