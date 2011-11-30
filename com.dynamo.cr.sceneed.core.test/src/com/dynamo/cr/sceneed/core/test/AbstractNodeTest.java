package com.dynamo.cr.sceneed.core.test;

import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.IOException;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.junit.Before;

import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.Node;

public abstract class AbstractNodeTest {

    private IContainer contentRoot;
    private ISceneModel model;
    private ILoaderContext loaderContext;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private INodeTypeRegistry nodeTypeRegistry;

    private int updateCount;
    private int selectCount;

    @Before
    public void setup() throws CoreException, IOException {
        this.contentRoot = mock(IContainer.class);

        this.model = mock(ISceneModel.class);
        when(this.model.getContentRoot()).thenReturn(this.contentRoot);

        this.nodeTypeRegistry = mock(INodeTypeRegistry.class);

        this.loaderContext = mock(ISceneView.ILoaderContext.class);
        when(this.loaderContext.getNodeTypeRegistry()).thenReturn(this.nodeTypeRegistry);

        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();

        this.updateCount = 0;
        this.selectCount = 0;
    }

    // Accessors

    protected IContainer getContentRoot() {
        return this.contentRoot;
    }

    protected ISceneModel getModel() {
        return this.model;
    }

    protected INodeTypeRegistry getNodeTypeRegistry() {
        return this.nodeTypeRegistry;
    }

    protected ISceneView.ILoaderContext getLoaderContext() {
        return this.loaderContext;
    }

    // Helpers

    protected void execute(IUndoableOperation operation) throws ExecutionException {
        operation.addContext(this.undoContext);
        this.history.execute(operation, null, null);
    }

    protected void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    protected void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    protected void verifyUpdate() {
        ++this.updateCount;
        verify(this.model, times(this.updateCount)).notifyChange(any(Node.class));
    }

    protected void verifySelection() {
        ++this.selectCount;
        verify(this.model, times(this.selectCount)).setSelection(any(IStructuredSelection.class));
    }

    protected void setNodeProperty(Node node, Object id, Object value) throws ExecutionException {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        execute(propertyModel.setPropertyValue(id, value));
    }

    @SuppressWarnings("unchecked")
    protected IStatus getNodePropertyStatus(Node node, Object id) {
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        return propertyModel.getPropertyStatus(id);
    }

    protected void assertNodePropertyStatus(Node node, Object id, int severity, String message) {
        IStatus status = getNodePropertyStatus(node, id);
        assertTrue(testStatus(status, severity, message));
    }

    private boolean testStatus(IStatus status, int severity, String message) {
        if (status.isMultiStatus()) {
            for (IStatus child : status.getChildren()) {
                if (testStatus(child, severity, message)) {
                    return true;
                }
            }
            return false;
        } else {
            return status.getSeverity() == severity && (message == null || message.equals(status.getMessage()));
        }
    }

}
