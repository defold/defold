package com.dynamo.cr.properties.test;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;

public class DummyCommandFactory implements ICommandFactory<Object, DummyWorld> {

    @Override
    public IUndoableOperation create(Object obj, String property,
            IPropertyAccessor<Object, DummyWorld> accessor,
            Object oldValue, Object newValue, DummyWorld world) {

        // Increase total number of commands created
        ++world.totalCommands;

        // Increase command count for this property
        Integer count = world.commandsCreated.get(property);
        if (count == null)
            count = 0;
        count++;
        world.commandsCreated.put(property, count);

        // Set actual value
        try {
            accessor.setValue(obj, property, newValue, world);
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }

        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, DummyWorld world) {
    }
}