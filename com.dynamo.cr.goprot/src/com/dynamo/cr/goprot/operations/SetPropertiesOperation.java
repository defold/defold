package com.dynamo.cr.goprot.operations;

import java.util.Arrays;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;

public class SetPropertiesOperation<T, U extends IPropertyObjectWorld> extends AbstractOperation {

    private IPropertyAccessor<Object, U> accessor;
    private U model;
    private String property;
    private List<T> oldValues;
    private List<T> newValues;
    private List<Object> nodes;

    public SetPropertiesOperation(List<Object> nodes, String property, IPropertyAccessor<Object, U> accessor, List<T> oldValues, List<T> newValues, U model) {
        super("Set " + property);
        this.property = property;
        this.nodes = nodes;
        this.accessor = accessor;
        this.oldValues = oldValues;
        this.newValues = newValues;
        this.model = model;
    }

    public List<Object> getNodes() {
        return this.nodes;
    }

    public String getProperty() {
        return this.property;
    }

    public List<T> getOldValues() {
        return this.oldValues;
    }

    public List<T> getNewValues() {
        return this.newValues;
    }

    @SuppressWarnings("unchecked")
    public SetPropertiesOperation(Object node, String property, IPropertyAccessor<Object, U> accessor, T oldValue, T newValue, U scene) {
        this(Arrays.asList(node), property, accessor, Arrays.asList(oldValue), Arrays.asList(newValue), scene);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {

        try {
            int i = 0;
            for (Object node : nodes) {
                accessor.setValue(node, property, newValues.get(i), model);
                ++i;
            }
        } catch (Throwable e) {
            throw new ExecutionException("Failed to set property " + property, e);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        try {
            int i = 0;
            for (Object node : nodes) {
                accessor.setValue(node, property, newValues.get(i), model);
                ++i;
            }
        } catch (Throwable e) {
            throw new ExecutionException("Failed to set property " + property, e);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        try {
            int i = 0;
            for (Object node : nodes) {
                accessor.setValue(node, property, oldValues.get(i), model);
                ++i;
            }
        } catch (Throwable e) {
            throw new ExecutionException("Failed to set property " + property, e);
        }
        return Status.OK_STATUS;
    }

}
