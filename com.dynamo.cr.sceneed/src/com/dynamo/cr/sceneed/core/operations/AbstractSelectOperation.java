package com.dynamo.cr.sceneed.core.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public abstract class AbstractSelectOperation extends AbstractOperation {

    private IStructuredSelection oldSelection;
    private IStructuredSelection newSelection;
    private IPresenterContext presenterContext;

    public AbstractSelectOperation(String label, Node selected, IPresenterContext presenterContext) {
        super(label);

        this.oldSelection = presenterContext.getSelection();
        this.newSelection = new StructuredSelection(selected);
        this.presenterContext = presenterContext;
    }

    public AbstractSelectOperation(String label, List<Node> selected, IPresenterContext presenterContext) {
        super(label);

        this.oldSelection = presenterContext.getSelection();
        this.newSelection = new StructuredSelection(selected.toArray());
        this.presenterContext = presenterContext;
    }

    @Override
    public final IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = doExecute(monitor, info);
        this.presenterContext.setSelection(this.newSelection);
        return status;
    }

    @Override
    public final IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = doRedo(monitor, info);
        this.presenterContext.setSelection(this.newSelection);
        return status;
    }

    @Override
    public final IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
        IStatus status = doUndo(monitor, info);
        this.presenterContext.setSelection(this.oldSelection);
        return status;
    }

    protected abstract IStatus doExecute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException;
    protected abstract IStatus doRedo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException;
    protected abstract IStatus doUndo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException;
}
