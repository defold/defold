package com.dynamo.cr.scene.graph;

public interface ISceneListener
{
    void sceneChanged(SceneEvent event);
    void propertyChanged(ScenePropertyChangedEvent event);
}
