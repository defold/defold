package com.dynamo.cr.sceneed.core;


public interface INodeRenderer<T extends Node> {
    void setup(RenderContext renderContext, T node);
    void render(RenderContext renderContext, T node, RenderData<T> renderData);
}
