package com.dynamo.cr.tileeditor.core;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.tileeditor.operations.SetPropertiesOperation;

public class TileSetUndoableCommandFactory implements
ICommandFactory<Object, TileSetModel> {
    @Override
    public IUndoableOperation create(Object node, String property,
            IPropertyAccessor<Object, TileSetModel> accessor, Object oldValue,
            Object newValue, boolean overridden, TileSetModel model, boolean force) {

        if (force || !newValue.equals(oldValue)) {
            SetPropertiesOperation<Object, TileSetModel> operation = new SetPropertiesOperation<Object, TileSetModel>(node,
                    property, accessor, oldValue,
                    newValue, model);
            return operation;
        }
        return null;
    }

    @Override
    public IUndoableOperation createReset(Object node, String property,
            IPropertyAccessor<Object, TileSetModel> accessor, Object oldValue,
            TileSetModel model) {
        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, TileSetModel world) {
        world.executeOperation(operation);
    }
}
