package com.dynamo.cr.sceneed.core;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.sceneed.core.operations.ResetPropertiesOperation;
import com.dynamo.cr.sceneed.core.operations.SetPropertiesOperation;

public class SceneUndoableCommandFactory implements
ICommandFactory<Object, ISceneModel> {
    @Override
    public IUndoableOperation create(Object object, String property,
            IPropertyAccessor<Object, ISceneModel> accessor, Object oldValue,
            Object newValue, boolean overridden, ISceneModel model, boolean force) {

        if (force || !newValue.equals(oldValue)) {
            SetPropertiesOperation<Object, ISceneModel> operation = new SetPropertiesOperation<Object, ISceneModel>(object,
                    property, accessor, oldValue,
                    newValue, overridden, model);
            return operation;
        }
        return null;
    }

    @Override
    public IUndoableOperation createReset(Object object, String property,
            IPropertyAccessor<Object, ISceneModel> accessor, Object oldValue,
            ISceneModel model) {

        return new ResetPropertiesOperation<Object, ISceneModel>(object,
                property, accessor, oldValue, model);
    }

    @Override
    public void execute(IUndoableOperation operation, ISceneModel world) {
        world.executeOperation(operation);
    }
}
