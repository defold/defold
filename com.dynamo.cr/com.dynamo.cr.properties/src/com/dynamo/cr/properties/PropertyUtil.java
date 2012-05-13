package com.dynamo.cr.properties;

import java.util.ArrayList;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class PropertyUtil {

    private static class CompositeOperation extends AbstractOperation {

        private IUndoableOperation[] operations;

        public CompositeOperation(IUndoableOperation[] operations) {
            super(operations[0].getLabel());
            this.operations = operations;
        }

        @Override
        public IStatus execute(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {

            for (IUndoableOperation o : operations) {
                IStatus s = o.execute(monitor, info);
                if (!s.isOK()) {
                    return s;
                }
            }
            return Status.OK_STATUS;
        }

        @Override
        public IStatus redo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            for (IUndoableOperation o : operations) {
                IStatus s = o.redo(monitor, info);
                if (!s.isOK()) {
                    return s;
                }
            }
            return Status.OK_STATUS;
        }

        @Override
        public IStatus undo(IProgressMonitor monitor, IAdaptable info)
                throws ExecutionException {
            for (IUndoableOperation o : operations) {
                IStatus s = o.undo(monitor, info);
                if (!s.isOK()) {
                    return s;
                }
            }
            return Status.OK_STATUS;
        }

    }

    public static <T, U extends IPropertyObjectWorld> IUndoableOperation setProperty(IPropertyModel<T, U>[] models, Object id, Object value) {
        ArrayList<IUndoableOperation> operations = new ArrayList<IUndoableOperation>();
        for (IPropertyModel<T, U> model : models) {
            IUndoableOperation operation = model.setPropertyValue(id, value);
            if (operation != null) {
                operations.add(operation);
            }
        }
        if (operations.size() == 0)
            return null;
        else
            return new CompositeOperation(operations.toArray(new IUndoableOperation[operations.size()]));
    }

}
