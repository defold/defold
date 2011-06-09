package com.dynamo.cr.scene.graph;

import org.eclipse.core.commands.operations.IUndoableOperation;

public interface IExecuteOperationDelegate {
    void executeOperation(IUndoableOperation operation);
}
