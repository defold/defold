package com.dynamo.cr.contenteditor.scene;

public interface ISceneListener
{
    void sceneChanged(SceneEvent event);
    void propertyChanged(ScenePropertyChangedEvent event);
}
