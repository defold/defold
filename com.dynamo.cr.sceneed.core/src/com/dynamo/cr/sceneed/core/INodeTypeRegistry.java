package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.ISceneView.INodePresenter;

public interface INodeTypeRegistry {

    INodeLoader<Node> getLoader(String extension);
    INodeLoader<Node> getLoader(Class<? extends Node> c);

    INodePresenter<? extends Node> getPresenter(String extension);
    INodePresenter<? extends Node> getPresenter(Class<? extends Node> c);

    INodeRenderer<Node> getRenderer(String extension);
    INodeRenderer<Node> getRenderer(Class<? extends Node> c);

    Class<?> getNodeClass(String extension);
    String getExtension(Class<? extends Node> c);
    IResourceType getResourceType(Class<? extends Node> c);
    IResourceType getResourceType(String extension);
}
