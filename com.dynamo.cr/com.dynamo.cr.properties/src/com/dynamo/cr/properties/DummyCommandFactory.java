package com.dynamo.cr.properties;

import org.eclipse.core.commands.operations.IUndoableOperation;

public class DummyCommandFactory implements ICommandFactory<Object, IPropertyObjectWorld> {

    @Override
    public IUndoableOperation create(Object obj, String property,
            IPropertyAccessor<Object, IPropertyObjectWorld> accesor,
            Object oldValue, Object newValue, IPropertyObjectWorld world) {

        return null;
    }

    @Override
    public void execute(IUndoableOperation operation,
            IPropertyObjectWorld world) {
    }

}
