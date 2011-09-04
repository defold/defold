package com.dynamo.cr.tileeditor.core;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.tileeditor.operations.SetPropertiesOperation;

public class UndoableCommandFactory implements
ICommandFactory<Object, TileSetModel> {
    @Override
    public void createCommand(Object node, String property,
            IPropertyAccessor<Object, TileSetModel> accessor, Object oldValue,
            Object newValue, TileSetModel model) {

        if (!newValue.equals(oldValue)) {
            SetPropertiesOperation<Object> operation = new SetPropertiesOperation<Object>(node,
                    property, accessor, oldValue,
                    newValue, model);
            model.executeOperation(operation);
        }
    }
}
