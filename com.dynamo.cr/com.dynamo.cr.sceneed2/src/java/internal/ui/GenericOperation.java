package internal.ui;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import clojure.lang.IFn;

public class GenericOperation extends AbstractOperation {

    private IFn undoFunction;
    private IFn redoFunction;

    public GenericOperation(String label, IFn undoFunction, IFn redoFunction) {
        super(label);
        this.undoFunction = undoFunction;
        this.redoFunction = redoFunction;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        redoFunction.invoke();
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        undoFunction.invoke();
        return Status.OK_STATUS;
    }

}
