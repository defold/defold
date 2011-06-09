package com.dynamo.cr.scene.graph;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class Scene implements IPropertyObjectWorld
{
    private final List<ISceneListener> m_Listeners = new ArrayList<ISceneListener>();
    private IExecuteOperationDelegate executeOperationDelegate;

    public Scene(IExecuteOperationDelegate executeOperationDelegate)
    {
        this.executeOperationDelegate = executeOperationDelegate;
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

    public void addSceneListener(ISceneListener listener)
    {
        m_Listeners.add(listener);
    }

    public void removeSceneListener(ISceneListener listener)
    {
        m_Listeners.remove(listener);
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

    public void executeOperation(IUndoableOperation operation) {
        this.executeOperationDelegate.executeOperation(operation);
    }
}

