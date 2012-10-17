package com.dynamo.cr.properties;

import java.util.ArrayList;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;
import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;

public class PropertyUtil {

    private static class CompositeOperation extends AbstractOperation implements IMergeableOperation {

        private IUndoableOperation[] operations;
        private boolean allMergable;
        private Type type = Type.OPEN;

        public CompositeOperation(IUndoableOperation[] operations, Type type) {
            super(operations[0].getLabel());
            this.operations = operations;
            this.allMergable = true;
            this.type = type;

            for (IUndoableOperation o : operations) {
                if (o instanceof IMergeableOperation) {
                    ((IMergeableOperation) o).setType(type);
                }
                else {
                    allMergable = false;
                    break;
                }
            }
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


        @Override
        public void setType(Type type) {
            this.type  = type;
        }

        @Override
        public Type getType() {
            return type;
        }

        @Override
        public boolean canMerge(IMergeableOperation operation) {
            if (allMergable && operation instanceof CompositeOperation) {
                CompositeOperation co = (CompositeOperation) operation;
                if (co.allMergable && operations.length == co.operations.length) {
                    for (int i = 0; i < operations.length; ++i) {
                        if (!((IMergeableOperation) operations[i]).canMerge((IMergeableOperation) co.operations[i])) {
                            return false;
                        }
                    }
                    return true;
                }
            }
            return false;
        }

        @Override
        public void mergeWith(IMergeableOperation operation) {
            CompositeOperation co = (CompositeOperation) operation;
            for (int i = 0; i < operations.length; ++i) {
                ((IMergeableOperation) operations[i]).mergeWith((IMergeableOperation) co.operations[i]);
            }
        }
    }

    public static <T, U extends IPropertyObjectWorld> IUndoableOperation setProperty(IPropertyModel<T, U>[] models, Object id, Object value, Type mergeType) {
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
            return new CompositeOperation(operations.toArray(new IUndoableOperation[operations.size()]), mergeType);
    }

    public static <T, U extends IPropertyObjectWorld> IUndoableOperation setProperty(IPropertyModel<T, U>[] models, Object id, Object value) {
        return setProperty(models, id, value, Type.OPEN);
    }

}
