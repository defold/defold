package com.dynamo.cr.properties.test;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;

public class DummyCommandFactory implements ICommandFactory<Object, DummyWorld> {

    @Override
    public IUndoableOperation create(Object obj, String property,
            IPropertyAccessor<Object, DummyWorld> accessor,
            Object oldValue, Object newValue, boolean overridden, DummyWorld world, boolean force) {

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
    public IUndoableOperation createReset(final Object obj, final String property,
            final IPropertyAccessor<Object, DummyWorld> accessor,
            final Object oldValue, final DummyWorld world) {

        // Increase total number of commands created
        ++world.totalCommands;

        // Increase command count for this property
        Integer count = world.commandsCreated.get(property);
        if (count == null)
            count = 0;
        count++;
        world.commandsCreated.put(property, count);

        // Reset actual value
        try {
            accessor.resetValue(obj, property, world);
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }

        return null;
    }

    @Override
    public void execute(IUndoableOperation operation, DummyWorld world) {
    }
}