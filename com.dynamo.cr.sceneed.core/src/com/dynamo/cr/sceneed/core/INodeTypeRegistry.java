package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.sceneed.core.ISceneView.NodePresenter;

public interface INodeTypeRegistry {

    NodePresenter getPresenter(String type);

    NodePresenter getPresenter(Class<? extends Node> c);

    INodeRenderer getRenderer(String type);

    INodeRenderer getRenderer(Class<? extends Node> c);

}
