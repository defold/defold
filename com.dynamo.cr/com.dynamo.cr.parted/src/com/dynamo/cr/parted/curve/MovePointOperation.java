package com.dynamo.cr.parted.curve;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class MovePointOperation extends AbstractOperation {

    private HermiteSpline spline;
    private double oldX;
    private double oldY;
    private double newX;
    private double newY;
    private int index;

    public MovePointOperation(String label, HermiteSpline spline, int index, double oldX, double oldY, double newX, double newY) {
        super(label);
        this.spline = spline;
        this.index = index;
        this.oldX = oldX;
        this.oldY = oldY;
        this.newX = newX;
        this.newY = newY;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.setPosition(index, newX, newY);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.setPosition(index, newX, newY);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        spline.setPosition(index, oldX, oldY);
        return Status.OK_STATUS;
    }

}
