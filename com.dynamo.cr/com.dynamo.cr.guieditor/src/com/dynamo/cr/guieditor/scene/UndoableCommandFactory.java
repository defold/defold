package com.dynamo.cr.guieditor.scene;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.guieditor.operations.SetPropertiesOperation;
import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;

public class UndoableCommandFactory implements
        ICommandFactory<Object, GuiScene> {
    @Override
    public IUndoableOperation create(Object node, String property,
            IPropertyAccessor<Object, GuiScene> accessor, Object oldValue,
            Object newValue, boolean overridden, GuiScene scene, boolean force) {

        if (force || !newValue.equals(oldValue)) {
            SetPropertiesOperation<Object> operation = new SetPropertiesOperation<Object>(node,
                    property, accessor, oldValue,
                    newValue, scene);
            return operation;
        }
        return null;
    }

    @Override
    public IUndoableOperation createReset(Object obj, String property, IPropertyAccessor<Object, GuiScene> accessor, Object oldValue, GuiScene world) {
        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, GuiScene scene) {
        scene.getEditor().executeOperation(operation);
    }

}
