package com.dynamo.cr.contenteditor.scene;

public class SceneEvent
{
    public static final int NODE_ADDED = 1;
    public static final int NODE_REMOVED = 2;
    public static final int PROPERTY_CHANGED = 3;
    public static final int NODE_REPARENTED = 4;
    public static final int NODE_IDENTIFIER_CHANGED = 5;

    public int m_Type;

    public SceneEvent(int type)
    {
        m_Type = type;
    }
}
