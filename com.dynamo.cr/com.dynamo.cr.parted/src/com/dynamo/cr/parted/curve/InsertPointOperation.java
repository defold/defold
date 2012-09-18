package com.dynamo.cr.parted.curve;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class InsertPointOperation extends AbstractOperation {

    private HermiteSpline spline;
    private double x;
    private double y;
    private int index;

    public InsertPointOperation(String label, HermiteSpline spline, double x, double y) {
        super(label);
        this.spline = spline;
        this.x = x;
        this.y = y;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        index = spline.insertPoint(x, y);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        index = spline.insertPoint(x, y);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.removePoint(index);
        return Status.OK_STATUS;
    }

}
