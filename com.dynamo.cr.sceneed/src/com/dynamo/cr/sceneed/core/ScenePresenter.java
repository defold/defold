package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.google.inject.Inject;
import com.google.protobuf.Message;

public class ScenePresenter implements IPresenter, IModelListener {

    private final ISceneModel model;
    private final ISceneView view;
    private final INodeTypeRegistry nodeTypeRegistry;
    private final ILoaderContext loaderContext;

    @Inject
    public ScenePresenter(ISceneModel model, ISceneView view, INodeTypeRegistry manager, ILogger logger, ILoaderContext loaderContext) {
        this.model = model;
        this.view = view;
        this.nodeTypeRegistry = manager;
        this.loaderContext = loaderContext;
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
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeFromExtension(type);
        INodeLoader<? super Node> loader = nodeType.getLoader();
        Node node = loader.load(this.loaderContext, contents);
        this.model.setRoot(node);
    }

    @Override
    public void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        Node node = this.model.getRoot();
        INodeType nodeType = this.nodeTypeRegistry.getNodeType(node.getClass());
        INodeLoader<? super Node> loader = nodeType.getLoader();
        Message message = loader.buildMessage(this.loaderContext, node, monitor);
        SceneUtil.saveMessage(message, contents, monitor);
        this.model.setUndoRedoCounter(0);
    }

    @Override
    public void onResourceChanged(IResourceChangeEvent event) throws CoreException {
        this.model.handleResourceChanged(event);
    }

    @Override
    public void rootChanged(Node root) {
        this.view.setRoot(root);
    }

    @Override
    public void selectionChanged(IStructuredSelection selection) {
        this.view.updateSelection(selection);
    }

    @Override
    public void nodeChanged(Node node) {
        this.view.updateNode(node);
    }

    @Override
    public void dirtyChanged(boolean dirty) {
        this.view.setDirty(dirty);
    }

}
