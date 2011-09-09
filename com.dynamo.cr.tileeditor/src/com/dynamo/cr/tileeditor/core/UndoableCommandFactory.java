package com.dynamo.cr.tileeditor.core;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.tileeditor.operations.SetPropertiesOperation;

public class UndoableCommandFactory implements
ICommandFactory<Object, TileSetModel> {
    @Override
    public IUndoableOperation create(Object node, String property,
            IPropertyAccessor<Object, TileSetModel> accessor, Object oldValue,
            Object newValue, TileSetModel model) {

        if (!newValue.equals(oldValue)) {
            SetPropertiesOperation<Object> operation = new SetPropertiesOperation<Object>(node,
                    property, accessor, oldValue,
                    newValue, model);
            return operation;
        }
        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, TileSetModel world) {
        world.executeOperation(operation);
    }
}
