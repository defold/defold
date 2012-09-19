package com.dynamo.cr.parted.curve;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class MoveTangentOperation extends AbstractOperation {

    private HermiteSpline spline;
    private double oldTX;
    private double oldTY;
    private double newTX;
    private double newTY;
    private int index;

    public MoveTangentOperation(String label, HermiteSpline spline, int index, double oldTX, double oldTY, double newTX, double newTY) {
        super(label);
        this.spline = spline;
        this.index = index;
        this.oldTX = oldTX;
        this.oldTY = oldTY;
        this.newTX = newTX;
        this.newTY = newTY;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.setTangent(index, newTX, newTY);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.setTangent(index, newTX, newTY);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.setTangent(index, oldTX, oldTY);
        return Status.OK_STATUS;
    }

}
