package com.dynamo.cr.goprot.core;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.goprot.operations.SetPropertiesOperation;
import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;

public class NodeUndoableCommandFactory implements
ICommandFactory<Object, NodeModel> {
    @Override
    public IUndoableOperation create(Object node, String property,
            IPropertyAccessor<Object, NodeModel> accessor, Object oldValue,
            Object newValue, NodeModel model) {

        if (!newValue.equals(oldValue)) {
            SetPropertiesOperation<Object, NodeModel> operation = new SetPropertiesOperation<Object, NodeModel>(node,
                    property, accessor, oldValue,
                    newValue, model);
            return operation;
        }
        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, NodeModel world) {
        world.executeOperation(operation);
    }
}
