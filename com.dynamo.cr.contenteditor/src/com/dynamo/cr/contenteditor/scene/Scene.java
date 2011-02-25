package com.dynamo.cr.contenteditor.scene;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.ui.services.IDisposable;

public class Scene implements IDisposable
{
    private final List<ISceneListener> m_Listeners = new ArrayList<ISceneListener>();

    public Scene()
    {
    }

    @Override
    public void dispose()
    {
    }

    void fireSceneEvent(SceneEvent e) {
        // Make a copy of listeners. We could get concurrent modification problems
        // if a listener would respond by added/removing a listener.
        ArrayList<ISceneListener> listeners = new ArrayList<ISceneListener>(m_Listeners);
        for (ISceneListener l : listeners)
        {
            l.sceneChanged(e);
        }
    }

    void fireScenePropertyChangedEvent(ScenePropertyChangedEvent e) {
        // Make a copy of listeners. We could get concurrent modification problems
        // if a listener would respond by added/removing a listener.
        ArrayList<ISceneListener> listeners = new ArrayList<ISceneListener>(m_Listeners);
        for (ISceneListener l : listeners)
        {
            l.propertyChanged(e);
        }
    }

    public void addSceneListener(ISceneListener listener)
    {
        m_Listeners.add(listener);
    }

    public void removeSceneListener(ISceneListener listener)
    {
        m_Listeners.remove(listener);
    }

    public void nodeTransformChanged(Node node)
    {
        SceneEvent e = new SceneEvent(SceneEvent.PROPERTY_CHANGED, node);
        fireSceneEvent(e);
    }

    public void propertyChanged(Node node, IProperty property)
    {
        ScenePropertyChangedEvent e = new ScenePropertyChangedEvent(node, property);
        fireScenePropertyChangedEvent(e);
    }

    public void nodeAdded(Node node)
    {
        fireSceneEvent(new SceneEvent(SceneEvent.NODE_ADDED, node));
    }

    public void nodeRemoved(Node node)
    {
        fireSceneEvent(new SceneEvent(SceneEvent.NODE_REMOVED, node));
    }

    public void nodeReparented(Node node, Node parent) {
        fireSceneEvent(new SceneEvent(SceneEvent.NODE_REPARENTED, node));
    }

    public void nodeChanged(Node node) {
        fireSceneEvent(new SceneEvent(SceneEvent.NODE_CHANGED, node));
    }
}

