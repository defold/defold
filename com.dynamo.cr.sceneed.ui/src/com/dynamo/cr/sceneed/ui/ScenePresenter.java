package com.dynamo.cr.sceneed.ui;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.google.inject.Inject;
import com.google.protobuf.Message;

public class ScenePresenter implements IPresenter {

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
        INodeLoader loader = this.nodeTypeRegistry.getLoader(type);
        Node node = loader.load(this.loaderContext, type, contents);
        this.model.setRoot(node);
    }

    @Override
    public void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        Node node = this.model.getRoot();
        INodeLoader loader = this.nodeTypeRegistry.getLoader(node.getClass());
        Message message = loader.buildMessage(this.loaderContext, node, monitor);
        SceneUtil.saveMessage(message, contents, monitor);
        this.model.setUndoRedoCounter(0);
    }

    @Override
    public void onResourceChanged(IResourceChangeEvent event) throws CoreException {
        this.model.handleResourceChanged(event);
    }

}
