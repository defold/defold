package com.dynamo.cr.contenteditor.scene;

public class ScenePropertyChangedEvent
{

    public Node m_Node;
    public IProperty m_Property;

    public ScenePropertyChangedEvent(Node node, IProperty property)
    {
        m_Node = node;
        m_Property = property;
    }

}
