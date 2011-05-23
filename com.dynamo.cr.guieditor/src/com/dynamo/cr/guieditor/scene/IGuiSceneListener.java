package com.dynamo.cr.guieditor.scene;


public interface IGuiSceneListener {
    public void propertyChanged(GuiNode node, String property);

    public void nodeRemoved(GuiNode node);
}
