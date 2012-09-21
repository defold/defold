package com.dynamo.cr.parted.curve;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class RemovePointOperation extends AbstractOperation {

    private HermiteSpline spline;
    private int index;
    private SplinePoint prevPoint;

    public RemovePointOperation(String label, HermiteSpline spline, int index) {
        super(label);
        this.spline = spline;
        this.index = index;
        this.prevPoint = spline.getPoint(index);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.removePoint(index);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.removePoint(index);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.insertPoint(prevPoint.x, prevPoint.y);
        spline.setTangent(index, prevPoint.tx, prevPoint.ty);
        return Status.OK_STATUS;
    }

}
