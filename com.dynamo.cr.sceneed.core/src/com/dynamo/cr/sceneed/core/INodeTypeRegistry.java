package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.sceneed.core.ISceneView.NodePresenter;

public interface INodeTypeRegistry {

    NodePresenter getPresenter(String extension);

    NodePresenter getPresenter(Class<? extends Node> c);

    INodeRenderer getRenderer(String extension);

    INodeRenderer getRenderer(Class<? extends Node> c);

    String getExtension(Class<? extends Node> c);

}
