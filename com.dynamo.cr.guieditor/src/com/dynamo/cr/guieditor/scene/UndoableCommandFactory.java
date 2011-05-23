package com.dynamo.cr.guieditor.scene;

import com.dynamo.cr.guieditor.operations.SetPropertiesOperation;
import com.dynamo.cr.guieditor.property.ICommandFactory;
import com.dynamo.cr.guieditor.property.IPropertyAccessor;

public class UndoableCommandFactory implements
        ICommandFactory<Object, GuiScene> {
    @Override
    public void createCommand(Object node, String property,
            IPropertyAccessor<Object, GuiScene> accessor, Object oldValue,
            Object newValue, GuiScene scene) {

        if (!newValue.equals(oldValue)) {
            SetPropertiesOperation<Object> operation = new SetPropertiesOperation<Object>(node,
                    property, accessor, oldValue,
                    newValue, scene);
            scene.getEditor().executeOperation(operation);
        }
    }
}
