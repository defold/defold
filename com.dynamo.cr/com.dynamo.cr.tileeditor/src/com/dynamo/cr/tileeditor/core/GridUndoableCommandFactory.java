package com.dynamo.cr.tileeditor.core;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.tileeditor.operations.SetPropertiesOperation;

public class GridUndoableCommandFactory implements
ICommandFactory<Object, GridModel> {
    @Override
    public IUndoableOperation create(Object node, String property,
            IPropertyAccessor<Object, GridModel> accessor, Object oldValue,
            Object newValue, boolean overridden, GridModel model, boolean force) {

        if (force || !newValue.equals(oldValue)) {
            SetPropertiesOperation<Object, GridModel> operation = new SetPropertiesOperation<Object, GridModel>(node,
                    property, accessor, oldValue,
                    newValue, model);
            return operation;
        }
        return null;
    }

    @Override
    public IUndoableOperation createReset(Object node, String property,
            IPropertyAccessor<Object, GridModel> accessor, Object oldValue, GridModel model) {
        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, GridModel world) {
        world.executeOperation(operation);
    }
}
