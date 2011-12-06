package com.dynamo.cr.sceneed.core;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;

public interface ISceneEditor {

    ISceneView.INodePresenter<? extends Node> getNodePresenter(Class<? extends Node> nodeClass);

    IPresenterContext getPresenterContext();

    ILoaderContext getLoaderContext();

}
