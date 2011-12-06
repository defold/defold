package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.editor.core.IResourceType;

public interface INodeType {

    String getExtension();

    ISceneView.INodeLoader<Node> getLoader();

    ISceneView.INodePresenter<?> getPresenter();

    INodeRenderer<Node> getRenderer();

    IResourceType getResourceType();

    Class<?> getNodeClass();

}
