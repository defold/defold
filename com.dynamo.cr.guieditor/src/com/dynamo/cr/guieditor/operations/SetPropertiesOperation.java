package com.dynamo.cr.guieditor.operations;

import java.util.Arrays;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.cr.properties.IPropertyAccessor;

public class SetPropertiesOperation<T> extends AbstractOperation {

    private IPropertyAccessor<Object, GuiScene> accessor;
    private GuiScene scene;
    private String property;
    private List<T> oldValues;
    private List<T> newValues;
    private List<Object> nodes;

    public SetPropertiesOperation(List<Object> nodes, String property, IPropertyAccessor<Object, GuiScene> accessor, List<T> oldValues, List<T> newValues, GuiScene scene) {
        super("Set " + property);
        this.property = property;
        this.nodes = nodes;
        this.accessor = accessor;
        this.oldValues = oldValues;
        this.newValues = newValues;
        this.scene = scene;
    }

    @SuppressWarnings("unchecked")
    public SetPropertiesOperation(Object node, String property, IPropertyAccessor<Object, GuiScene> accessor, T oldValue, T newValue, GuiScene scene) {
        this(Arrays.asList(node), property, accessor, Arrays.asList(oldValue), Arrays.asList(newValue), scene);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {

        try {
            int i = 0;
            for (Object node : nodes) {
                accessor.setValue(node, property, newValues.get(i++), scene);
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
                accessor.setValue(node, property, newValues.get(i++), scene);
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
                accessor.setValue(node, property, oldValues.get(i++), scene);
            }
        } catch (Throwable e) {
            throw new ExecutionException("Failed to set property " + property, e);
        }
        return Status.OK_STATUS;
    }

}
