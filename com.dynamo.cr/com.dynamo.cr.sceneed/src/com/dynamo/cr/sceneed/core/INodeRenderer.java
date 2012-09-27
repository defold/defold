package com.dynamo.cr.sceneed.core;


/**
 * Node render class. Instances are created on a per context basis, e.g. a render-view,
 * and should correspond to an OpenGL-context so that a node-renderer can cache OpenGL-resources.
 * OpenGL-resources can't be shared (by default) between contexts.
 *
 * @author chmu
 *
 * @param <T>
 */
public interface INodeRenderer<T extends Node> {
    void dispose();
    void setup(RenderContext renderContext, T node);
    void render(RenderContext renderContext, T node, RenderData<T> renderData);
}
