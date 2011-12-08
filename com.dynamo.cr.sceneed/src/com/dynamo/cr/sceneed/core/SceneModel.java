package com.dynamo.cr.sceneed.core;

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

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.google.inject.Inject;

@Entity(commandFactory = SceneUndoableCommandFactory.class)
public class SceneModel implements IAdaptable, IOperationHistoryListener, ISceneModel {

    private final IModelListener listener;
    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final ILogger logger;
    private final IContainer contentRoot;
    private final ILoaderContext loaderContext;
    private final IImageProvider imageProvider;

    private Node root;
    private IStructuredSelection selection;
    private int undoRedoCounter;

    private static PropertyIntrospector<SceneModel, SceneModel> introspector = new PropertyIntrospector<SceneModel, SceneModel>(SceneModel.class);

    @Inject
    public SceneModel(IModelListener listener, IOperationHistory history, IUndoContext undoContext, ILogger logger, IContainer contentRoot, ILoaderContext loaderContext, IImageProvider imageProvider) {
        this.listener = listener;
        this.history = history;
        this.undoContext = undoContext;
        this.logger = logger;
        this.contentRoot = contentRoot;
        this.loaderContext = loaderContext;
        this.imageProvider = imageProvider;

        this.selection = new StructuredSelection();
        this.undoRedoCounter = 0;
    }

    @Inject
    public void init() {
        this.history.addOperationHistoryListener(this);
    }

    @PreDestroy
    public void dispose() {
        if (this.root != null) {
            this.root.dispose();
        }
        this.history.removeOperationHistoryListener(this);
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
            this.listener.rootChanged(root);
            if (root != null) {
                setSelection(new StructuredSelection(this.root));
            } else {
                setSelection(new StructuredSelection());
            }
            clearDirty();
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
        this.selection = selection;
    }

    private void notifyChange() {
        this.listener.stateChanged(this.selection, isDirty());
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
    public boolean isDirty() {
        return this.undoRedoCounter != 0;
    }

    @Override
    public void clearDirty() {
        this.undoRedoCounter = 0;
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (!event.getOperation().hasContext(this.undoContext)) {
            // Only handle operations related to this editor
            return;
        }
        boolean change = false;
        int type = event.getEventType();
        switch (type) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.REDONE:
            change = true;
            ++this.undoRedoCounter;
            break;
        case OperationHistoryEvent.UNDONE:
            change = true;
            --this.undoRedoCounter;
            break;
        }
        if (change && this.root != null) {
            notifyChange();
        }
    }

    private static class ResourceDeltaVisitor implements IResourceDeltaVisitor {
        private final Node root;
        private boolean reloaded;

        public ResourceDeltaVisitor(Node root) {
            this.root = root;
            this.reloaded = false;
        }

        public boolean isReloaded() {
            return this.reloaded;
        }

        @Override
        public boolean visit(IResourceDelta delta) throws CoreException {
            IResource resource = delta.getResource();
            if (resource instanceof IFile) {
                if (handleReload(this.root, (IFile)resource)) {
                    this.reloaded = true;
                }
                return false;
            }
            return true;
        }

        private boolean handleReload(Node node, IFile file) {
            boolean reloaded = false;
            for (Node child : node.getChildren()) {
                if (handleReload(child, file)) {
                    reloaded = true;
                }
            }
            if (node.handleReload(file)) {
                reloaded = true;
            }
            return reloaded;
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#handleResourceChanged(org.eclipse.core.resources.IResourceChangeEvent)
     */
    @Override
    public void handleResourceChanged(IResourceChangeEvent event) throws CoreException {
        if (this.root != null) {
            ResourceDeltaVisitor visitor = new ResourceDeltaVisitor(this.root);
            event.getDelta().accept(visitor);
            if (visitor.isReloaded()) {
                notifyChange();
            }
        }
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
    public String getExtension(Class<? extends Node> nodeClass) {
        INodeType nodeType = this.loaderContext.getNodeTypeRegistry().getNodeType(nodeClass);
        if (nodeType != null) {
            return nodeType.getExtension();
        }
        return null;
    }

    @Override
    public String getTypeName(Class<? extends Node> nodeClass) {
        INodeType nodeType = this.loaderContext.getNodeTypeRegistry().getNodeType(nodeClass);
        if (nodeType != null) {
            return nodeType.getResourceType().getName();
        }
        return null;
    }

    @Override
    public Image getIcon(Class<? extends Node> nodeClass) {
        String extension = getExtension(nodeClass);
        return this.imageProvider.getImage(extension);
    }

}
