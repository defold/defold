package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;

public interface ISceneEditor {

    ISceneView.IPresenter getScenePresenter();

    ISceneView.INodePresenter<? extends Node> getNodePresenter(Class<? extends Node> nodeClass);

    IPresenterContext getPresenterContext();

    ILoaderContext getLoaderContext();

    ISceneModel getModel();
}
