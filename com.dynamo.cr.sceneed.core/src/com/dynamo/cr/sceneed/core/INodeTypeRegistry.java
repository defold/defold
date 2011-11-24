package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.ISceneView.INodePresenter;

public interface INodeTypeRegistry {

    INodeLoader getLoader(String extension);
    INodeLoader getLoader(Class<? extends Node> c);

    INodePresenter getPresenter(String extension);
    INodePresenter getPresenter(Class<? extends Node> c);

    INodeRenderer<Node> getRenderer(String extension);
    INodeRenderer<Node> getRenderer(Class<? extends Node> c);

    String getExtension(Class<? extends Node> c);

}
