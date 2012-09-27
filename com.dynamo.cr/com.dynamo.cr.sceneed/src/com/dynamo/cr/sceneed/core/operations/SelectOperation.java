package com.dynamo.cr.sceneed.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;

public class SelectOperation extends AbstractOperation {

    private IStructuredSelection oldSelection;
    private IStructuredSelection newSelection;
    private IPresenterContext presenterContext;

    public SelectOperation(IStructuredSelection oldSelection, IStructuredSelection newSelection,
            IPresenterContext presenterContext) {
        super("Select");

        this.oldSelection = oldSelection;
        this.newSelection = newSelection;
        this.presenterContext = presenterContext;
    }

    @Override
    public final IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        this.presenterContext.setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public final IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        this.presenterContext.setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public final IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        this.presenterContext.setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }
}
