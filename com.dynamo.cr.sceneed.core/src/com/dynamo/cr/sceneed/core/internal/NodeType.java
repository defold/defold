package com.dynamo.cr.sceneed.core.internal;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.ISceneView.INodePresenter;

public class NodeType implements INodeType {

    private final String extension;
    private final ISceneView.INodeLoader<Node> loader;
    private final ISceneView.INodePresenter<Node> presenter;
    private final INodeRenderer<Node> renderer;
    private final IResourceType resourceType;
    private final Class<?> nodeClass;

    public NodeType(String extension, ISceneView.INodeLoader<Node> loader, ISceneView.INodePresenter<Node> presenter, INodeRenderer<Node> renderer, IResourceType resourceType, Class<?> nodeClass) {
        this.extension = extension;
        this.loader = loader;
        this.presenter = presenter;
        this.renderer = renderer;
        this.resourceType = resourceType;
        this.nodeClass = nodeClass;
    }

    @Override
    public String getExtension() {
        return this.extension;
    }

    @Override
    public INodeLoader<Node> getLoader() {
        return this.loader;
    }

    @Override
    public INodePresenter<?> getPresenter() {
        return this.presenter;
    }

    @Override
    public INodeRenderer<Node> getRenderer() {
        return this.renderer;
    }

    @Override
    public IResourceType getResourceType() {
        return this.resourceType;
    }

    @Override
    public Class<?> getNodeClass() {
        return this.nodeClass;
    }

}
