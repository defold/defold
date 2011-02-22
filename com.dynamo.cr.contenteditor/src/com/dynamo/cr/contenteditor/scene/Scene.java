package com.dynamo.cr.contenteditor.scene;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.ui.services.IDisposable;

public class Scene implements IDisposable
{
    private final List<Node> m_RootNodes = new ArrayList<Node>();
    private final List<ISceneListener> m_Listeners = new ArrayList<ISceneListener>();
    private Map<String, Node> collectionInstanceIdToNode = new HashMap<String, Node>();
    private Map<String, Node> instanceIdToNode = new HashMap<String, Node>();

    public Scene()
    {
    }

    protected void doValidate(Node node)
    {
        assert node.getScene() == this;

        for (Node n : node.getChilden())
        {
            doValidate(n);
            assert n.getParent() == node;
        }
    }

    public void validate()
    {
        for (Node n : m_RootNodes)
        {
            doValidate(n);
        }
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
        SceneEvent e = new SceneEvent(SceneEvent.PROPERTY_CHANGED);
        fireSceneEvent(e);
    }

    public void propertyChanged(Node node, IProperty property)
    {
        ScenePropertyChangedEvent e = new ScenePropertyChangedEvent(node, property);
        fireScenePropertyChangedEvent(e);
    }

    public void collectionInstanceIdChanged(Node node) {
        SceneEvent e = new SceneEvent(SceneEvent.COLLECTION_INSTANCE_NODE_IDENTIFIER_CHANGED);
        fireSceneEvent(e);
    }

    public void instanceIdChanged(Node node) {
        SceneEvent e = new SceneEvent(SceneEvent.INSTANCE_NODE_IDENTIFIER_CHANGED);
        fireSceneEvent(e);
    }

    public void setCollectionInstanceId(Node node, String key) {
        collectionInstanceIdToNode.put(node.getIdentifier(), node);
    }

    public void setInstanceId(Node node, String key) {
        instanceIdToNode.put(node.getIdentifier(), node);
    }

    public Node getCollectionInstanceNodeFromId(String ident) {
        return instanceIdToNode.get(ident);
    }

    public Node getInstanceNodeFromId(String ident) {
        return instanceIdToNode.get(ident);
    }

    public void nodeAdded(Node node)
    {
        validate();

        if (node instanceof CollectionInstanceNode)
            collectionInstanceIdToNode.put(node.getIdentifier(), node);
        if (node instanceof InstanceNode)
            instanceIdToNode.put(node.getIdentifier(), node);

        SceneEvent e = new SceneEvent(SceneEvent.NODE_ADDED);
        fireSceneEvent(e);
    }

    public void nodeRemoved(Node node)
    {
        if (node instanceof CollectionInstanceNode)
            collectionInstanceIdToNode.remove(node.getIdentifier());
        if (node instanceof InstanceNode)
            instanceIdToNode.remove(node.getIdentifier());
        //identifiers.remove(node);
        SceneEvent e = new SceneEvent(SceneEvent.NODE_REMOVED);
        fireSceneEvent(e);
    }

    public void nodeReparented(Node node, Node parent) {
        SceneEvent e = new SceneEvent(SceneEvent.NODE_REPARENTED);
        fireSceneEvent(e);
    }

    public String getUniqueCollectionInstanceId(String base) {
        int id = 0;

        int i = base.length()-1;
        while (i >= 0) {
            char c = base.charAt(i);
            if (!(c >= '0' && c <= '9'))
                break;
            --i;
        }
        int leading_zeros = 0;
        if (i < base.length()-1) {
            id = Integer.parseInt(base.substring(i+1));
            leading_zeros = base.substring(i+1).length();
            base = base.substring(0, i+1);
        }
        String format_string;
        if (leading_zeros > 0)
            format_string = String.format("%%s%%0%dd", leading_zeros);
        else
            format_string = "%s%d";

        while (true) {
            String s = String.format(format_string, base, id);
            if (!collectionInstanceIdToNode.containsKey(s))
                return s;
            ++id;
        }
    }

    public String getUniqueInstanceId(String base) {
        int id = 0;

        int i = base.length()-1;
        while (i >= 0) {
            char c = base.charAt(i);
            if (!(c >= '0' && c <= '9'))
                break;
            --i;
        }
        int leading_zeros = 0;
        if (i < base.length()-1) {
            id = Integer.parseInt(base.substring(i+1));
            leading_zeros = base.substring(i+1).length();
            base = base.substring(0, i+1);
        }
        String format_string;
        if (leading_zeros > 0)
            format_string = String.format("%%s%%0%dd", leading_zeros);
        else
            format_string = "%s%d";

        while (true) {
            String s = String.format(format_string, base, id);
            if (!instanceIdToNode.containsKey(s))
                return s;
            ++id;
        }
    }
}

