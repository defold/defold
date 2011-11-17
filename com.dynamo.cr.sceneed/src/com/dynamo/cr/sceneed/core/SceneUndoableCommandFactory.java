package com.dynamo.cr.sceneed.core;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.sceneed.operations.SetPropertiesOperation;

public class SceneUndoableCommandFactory implements
ICommandFactory<Object, ISceneModel> {
    @Override
    public IUndoableOperation create(Object object, String property,
            IPropertyAccessor<Object, ISceneModel> accessor, Object oldValue,
            Object newValue, ISceneModel model) {

        if (!newValue.equals(oldValue)) {
            SetPropertiesOperation<Object, ISceneModel> operation = new SetPropertiesOperation<Object, ISceneModel>(object,
                    property, accessor, oldValue,
                    newValue, model);
            return operation;
        }
        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, ISceneModel world) {
        world.executeOperation(operation);
    }
}
