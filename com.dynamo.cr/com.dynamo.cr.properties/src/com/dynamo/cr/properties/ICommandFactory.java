package com.dynamo.cr.properties;

import org.eclipse.core.commands.operations.IUndoableOperation;

public interface ICommandFactory<T, U extends IPropertyObjectWorld> {

    public IUndoableOperation create(T obj, String property,
            IPropertyAccessor<T, U> accessor,
            Object oldValue, Object newValue, boolean overridden, U world, boolean force);

    public IUndoableOperation createReset(T obj, String property,
            IPropertyAccessor<T, U> accessor,
            Object oldValue, U world);

    public void execute(IUndoableOperation operation, U world);

}
