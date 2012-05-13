package com.dynamo.cr.sceneed.core;

import java.util.List;

import com.dynamo.cr.editor.core.IResourceType;

public interface INodeType {

    String getExtension();

    INodeLoader<Node> getLoader();

    ISceneView.INodePresenter<?> getPresenter();

    INodeRenderer<Node> getRenderer();

    IResourceType getResourceType();

    Class<?> getNodeClass();
    List<INodeType> getReferenceNodeTypes();

    String getDisplayGroup();

}
