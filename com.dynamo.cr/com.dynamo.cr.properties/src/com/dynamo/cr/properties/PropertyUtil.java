package com.dynamo.cr.properties;

import java.util.ArrayList;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;
import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;
import com.dynamo.cr.properties.types.ValueSpread;

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

    public static <T, U extends IPropertyObjectWorld> IUndoableOperation setProperties(IPropertyModel<T, U> model, Object[] ids, Object[] values, boolean force, Type mergeType) {
        ArrayList<IUndoableOperation> operations = new ArrayList<IUndoableOperation>();
        if (ids.length != values.length) {
            throw new IllegalArgumentException("ids and values must have the same lengths");
        }
        int idCount = ids.length;
        for (int i = 0; i < idCount; ++i) {
            Object id = ids[i];
            Object value = values[i];
            IUndoableOperation operation = model.setPropertyValue(id, value, force);
            if (operation != null) {
                operations.add(operation);
            }
        }
        if (operations.size() == 0)
            return null;
        else
            return new CompositeOperation(operations.toArray(new IUndoableOperation[operations.size()]), mergeType);
    }

    public static <T, U extends IPropertyObjectWorld> IUndoableOperation setProperties(IPropertyModel<T, U> model, Object[] ids, Object[] values, boolean force) {
        Type type = Type.OPEN;
        if (force) {
            type = Type.CLOSE;
        }
        return setProperties(model, ids, values, force, type);
    }

    public static Object mergeValue(Object oldValue, Object newValue) {
        if (oldValue instanceof Vector4d && newValue instanceof Double[]) {
            return mergeVector4dValue((Vector4d) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof Vector3d && newValue instanceof Double[]) {
            return mergeVector3dValue((Vector3d) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof Point3d && newValue instanceof Double[]) {
            return mergePoint3dValue((Point3d) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof Quat4d && newValue instanceof Double[]) {
            return mergeQuat4dValue((Quat4d) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof RGB && newValue instanceof Double[]) {
            return mergeRGBValue((RGB) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof ValueSpread && newValue instanceof Double[]) {
            return mergeValueSpread((ValueSpread) oldValue, (Double[]) newValue);
        } else {
            // No merge
            return newValue;
        }
    }

    private static Object mergeVector4dValue(Vector4d oldValue, Double[] delta) {
        Vector4d toSet = new Vector4d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        toSet.w = delta[3] != null ? delta[3] : oldValue.w;
        return toSet;
    }

    private static Object mergeVector3dValue(Vector3d oldValue, Double[] delta) {
        Vector3d toSet = new Vector3d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        return toSet;
    }

    private static Object mergePoint3dValue(Point3d oldValue, Double[] delta) {
        Point3d toSet = new Point3d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        return toSet;
    }

    private static Object mergeQuat4dValue(Quat4d oldValue, Double[] delta) {
        Quat4d toSet = new Quat4d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        toSet.w = delta[3] != null ? delta[3] : oldValue.w;
        return toSet;
    }

    private static Object mergeRGBValue(RGB oldValue, Double[] delta) {
        RGB toSet = new RGB(0, 0, 0);

        toSet.red = (int) (delta[0] != null ? delta[0] : oldValue.red);
        toSet.green = (int) (delta[1] != null ? delta[1] : oldValue.green);
        toSet.blue = (int) (delta[2] != null ? delta[2] : oldValue.blue);
        return toSet;
    }

    private static Object mergeValueSpread(ValueSpread oldValue, Double[] delta) {
        ValueSpread toSet = new ValueSpread(oldValue);
        if (delta[0] != null) {
            toSet.setValue(delta[0]);
        }
        if (delta[1] != null) {
            toSet.setSpread(delta[1]);
        }
        return toSet;
    }
}
