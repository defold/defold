package com.dynamo.cr.sceneed.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;

public class SetPropertiesOperation<T, U extends IPropertyObjectWorld> extends AbstractOperation implements IMergeableOperation {

    private IPropertyAccessor<Object, U> accessor;
    private U model;
    private String property;
    private T oldValue;
    private T newValue;
    private Object node;
    private boolean overrides;
    private boolean resets;
    private Type type = Type.OPEN;

    public SetPropertiesOperation(Object node, String property, IPropertyAccessor<Object, U> accessor, T oldValue, T newValue, boolean overridden, U scene) {
        super("Set " + property);
        this.property = property;
        this.node = node;
        this.accessor = accessor;
        this.oldValue = oldValue;
        this.newValue = newValue;
        this.model = scene;
        this.overrides = overridden;
    }

    public String getProperty() {
        return this.property;
    }

    private void set() throws ExecutionException {
        try {
            accessor.setValue(node, property, newValue, model);
        } catch (Throwable e) {
            throw new ExecutionException("Failed to set property " + property, e);
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        set();
        if (!this.overrides && accessor.isOverridden(node, property, model)) {
            this.resets = true;
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        set();
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        try {
            if (this.resets) {
                accessor.resetValue(node, property, model);
            } else {
                accessor.setValue(node, property, oldValue, model);
            }
        } catch (Throwable e) {
            throw new ExecutionException("Failed to set property " + property, e);
        }
        return Status.OK_STATUS;
    }

    @Override
    public void setType(Type type) {
        this.type = type;
    }

    @Override
    public Type getType() {
        return type;
    }

    @Override
    public boolean canMerge(IMergeableOperation operation) {
        if (operation instanceof SetPropertiesOperation<?, ?>) {
            SetPropertiesOperation<?, ?> o = (SetPropertiesOperation<?, ?>) operation;
            return o.node == node &&
                   o.property.equals(property) &&
                   o.newValue.getClass() == newValue.getClass();
        }
        return false;
    }

    @SuppressWarnings("unchecked")
    @Override
    public void mergeWith(IMergeableOperation operation) {
        SetPropertiesOperation<?, ?> o = (SetPropertiesOperation<?, ?>) operation;
        this.newValue = (T) o.newValue;
    }

}
