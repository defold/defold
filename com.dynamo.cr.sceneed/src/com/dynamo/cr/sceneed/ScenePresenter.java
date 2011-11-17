package com.dynamo.cr.sceneed;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.ILogger;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.cr.sceneed.core.ISceneView.Presenter;
import com.google.inject.Inject;
import com.google.protobuf.Message;

public class ScenePresenter implements Presenter {

    protected final ISceneModel model;
    protected final ISceneView view;
    private final NodeManager manager;
    private final Context context;

    private class Context implements ISceneView.Context {
        private ISceneModel model;
        private ISceneView view;
        private NodeManager manager;
        private ILogger logger;

        public Context(ISceneModel model, ISceneView view, NodeManager manager, ILogger logger) {
            this.model = model;
            this.view = view;
            this.manager = manager;
            this.logger = logger;
        }

        @Override
        public Node loadNode(String path) throws IOException, CoreException {
            IFile file = this.model.getContentRoot().getFile(new Path(path));
            return loadNode(file.getFileExtension(), file.getContents());
        }

        @Override
        public Node loadNode(String type, InputStream stream) throws IOException, CoreException {
            NodePresenter presenter = this.manager.getPresenter(type);
            if (presenter != null) {
                return presenter.onLoad(null, type, stream);
            } else {
                return null;
            }
        }

        @Override
        public Message buildMessage(Node node, IProgressMonitor monitor) throws IOException, CoreException {
            NodePresenter presenter = this.manager.getPresenter(node.getClass());
            if (presenter != null) {
                return presenter.onBuildMessage(this, node, monitor);
            } else {
                return null;
            }
        }

        @Override
        public ISceneView getView() {
            return this.view;
        }

        @Override
        public IStructuredSelection getSelection() {
            return this.model.getSelection();
        }

        @Override
        public void executeOperation(IUndoableOperation operation) {
            this.model.executeOperation(operation);
        }

        @Override
        public void logException(Throwable exception) {
            this.logger.logException(exception);
        }

        @Override
        public NodePresenter getPresenter(String type) {
            return this.manager.getPresenter(type);
        }

    }

    @Inject
    public ScenePresenter(ISceneModel model, ISceneView view, NodeManager manager, ILogger logger) {
        this.model = model;
        this.view = view;
        this.manager = manager;
        this.context = new Context(model, view, manager, logger);
    }

    @Override
    public void onSelect(IStructuredSelection selection) {
        this.model.setSelection(selection);
    }

    @Override
    public void onRefresh() {
        this.view.setRoot(this.model.getRoot());
        this.view.updateSelection(this.model.getSelection());
    }

    @Override
    public final void onLoad(String type, InputStream contents) throws IOException, CoreException {
        this.model.setRoot(this.manager.getPresenter(type).onLoad(this.context, type, contents));
    }

    @Override
    public void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        Node node = this.model.getRoot();
        NodePresenter nodePresenter = this.manager.getPresenter(node.getClass());
        Message message = nodePresenter.onBuildMessage(this.context, node, monitor);
        nodePresenter.onSaveMessage(this.context, message, contents, monitor);
        this.model.setUndoRedoCounter(0);
    }

    @Override
    public void onResourceChanged(IResourceChangeEvent event) throws CoreException {
        this.model.handleResourceChanged(event);
    }

    @Override
    public ISceneView.Context getContext() {
        return this.context;
    }

}
