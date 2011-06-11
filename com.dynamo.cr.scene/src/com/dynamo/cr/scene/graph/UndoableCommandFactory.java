package com.dynamo.cr.scene.graph;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.scene.operations.SetPropertiesOperation;

public class UndoableCommandFactory implements
        ICommandFactory<Object, Scene> {
    @Override
    public void createCommand(Object node, String property,
            IPropertyAccessor<Object, Scene> accessor, Object oldValue,
            Object newValue, Scene scene) {

        if (!newValue.equals(oldValue)) {
            SetPropertiesOperation<Object> operation = new SetPropertiesOperation<Object>(node,
                    property, accessor, oldValue,
                    newValue, scene);
            scene.executeOperation(operation);
        }
    }
}
