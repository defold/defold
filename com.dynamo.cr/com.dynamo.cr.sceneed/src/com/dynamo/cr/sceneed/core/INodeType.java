package com.dynamo.cr.sceneed.core;

import java.util.List;

import com.dynamo.cr.editor.core.IResourceType;

public interface INodeType {

    String getExtension();

    INodeLoader<Node> getLoader();

    ISceneView.INodePresenter<?> getPresenter();

    /**
     * Create a new {@link INodeRenderer} instance
     * @note Node-render instances must be unique per OpenGL-context. See {@link INodeRenderer} for more information
     * @return {@link INodeRenderer} instance. null if no renderer is available
     */
    INodeRenderer<Node> createRenderer();

    IResourceType getResourceType();

    Class<?> getNodeClass();

    List<INodeType> getReferenceNodeTypes();

    String getDisplayGroup();

    boolean isCached();
}
