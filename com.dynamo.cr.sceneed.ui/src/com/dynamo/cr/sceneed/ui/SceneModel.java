package com.dynamo.cr.sceneed.ui;

import java.io.IOException;

import javax.annotation.PreDestroy;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.sceneed.core.ILogger;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneUndoableCommandFactory;
import com.google.inject.Inject;

@Entity(commandFactory = SceneUndoableCommandFactory.class)
public class SceneModel implements IAdaptable, IOperationHistoryListener, IResourceDeltaVisitor, ISceneModel {

    private final ISceneView view;
    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final ILogger logger;
    private final IContainer contentRoot;
    private final ILoaderContext loaderContext;

    private Node root;
    private IStructuredSelection selection;
    private int undoRedoCounter;

    private static PropertyIntrospector<SceneModel, SceneModel> introspector = new PropertyIntrospector<SceneModel, SceneModel>(SceneModel.class, Messages.class);

    @Inject
    public SceneModel(ISceneView view, IOperationHistory history, IUndoContext undoContext, ILogger logger, IContainer contentRoot, ILoaderContext loaderContext) {
        this.view = view;
        this.history = history;
        this.undoContext = undoContext;
        this.logger = logger;
        this.contentRoot = contentRoot;
        this.loaderContext = loaderContext;
        this.selection = new StructuredSelection();
        this.undoRedoCounter = 0;
    }

    @PreDestroy
    public void dispose() {
        this.history.removeOperationHistoryListener(this);
    }

    @Inject
    public void init() {
        this.history.addOperationHistoryListener(this);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getRoot()
     */
    @Override
    public Node getRoot() {
        return this.root;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#setRoot(com.dynamo.cr.sceneed.core.Node)
     */
    @Override
    public void setRoot(Node root) {
        if (this.root != root) {
            this.root = root;
            if (root != null) {
                root.setModel(this);
            }
            this.view.setRoot(root);
            if (root != null) {
                setSelection(new StructuredSelection(this.root));
            } else {
                setSelection(new StructuredSelection());
            }
            setUndoRedoCounter(0);
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getSelection()
     */
    @Override
    public IStructuredSelection getSelection() {
        return this.selection;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#setSelection(org.eclipse.jface.viewers.IStructuredSelection)
     */
    @Override
    public void setSelection(IStructuredSelection selection) {
        if (!this.selection.equals(selection)) {
            this.selection = selection;
            this.view.updateSelection(this.selection);
        }
    }

    @Override
    public void notifyChange(Node node) {
        // Always update the root for now
        this.view.updateNode(this.root);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#executeOperation(org.eclipse.core.commands.operations.IUndoableOperation)
     */
    @Override
    public void executeOperation(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.history.execute(operation, null, null);
        } catch (final ExecutionException e) {
            this.logger.logException(e);
        }

        if (status != Status.OK_STATUS) {
            this.logger.logException(status.getException());
        }
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<SceneModel, SceneModel>(this, this, introspector);
        }
        return null;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getContentRoot()
     */
    @Override
    public IContainer getContentRoot() {
        return this.contentRoot;
    }

    @Override
    public void setUndoRedoCounter(int undoRedoCounter) {
        boolean prevDirty = this.undoRedoCounter != 0;
        boolean dirty = undoRedoCounter != 0;
        // NOTE: We must set undoRedoCounter before we call setDirty.
        // The "framework" might as for isDirty()
        this.undoRedoCounter = undoRedoCounter;
        if (prevDirty != dirty) {
            this.view.setDirty(dirty);
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (!event.getOperation().hasContext(this.undoContext)) {
            // Only handle operations related to this editor
            return;
        }
        int type = event.getEventType();
        switch (type) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.REDONE:
            setUndoRedoCounter(undoRedoCounter + 1);
            break;
        case OperationHistoryEvent.UNDONE:
            setUndoRedoCounter(undoRedoCounter - 1);
            break;
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#handleResourceChanged(org.eclipse.core.resources.IResourceChangeEvent)
     */
    @Override
    public void handleResourceChanged(IResourceChangeEvent event) throws CoreException {
        if (this.root != null) {
            event.getDelta().accept(this);
        }
    }

    @Override
    public boolean visit(IResourceDelta delta) throws CoreException {
        IResource resource = delta.getResource();
        if (resource instanceof IFile) {
            handleReload(this.root, (IFile)resource);
            return false;
        }
        return true;
    }

    private void handleReload(Node node, IFile file) {
        for (Node child : node.getChildren()) {
            handleReload(child, file);
        }
        node.handleReload(file);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getFile(java.lang.String)
     */
    @Override
    public IFile getFile(String path) {
        return this.contentRoot.getFile(new Path(path));
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#loadNode(java.lang.String)
     */
    @Override
    public Node loadNode(String path) throws IOException, CoreException {
        return this.loaderContext.loadNode(path);
    }

    @Override
    public Image getImage(Class<? extends Node> nodeClass) {
        String extension = this.loaderContext.getNodeTypeRegistry().getExtension(nodeClass);
        if (extension != null) {
            return Activator.getDefault().getImage(extension);
        }
        return null;
    }

}
