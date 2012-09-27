package com.dynamo.cr.sceneed.core;

import java.util.List;

import org.eclipse.swt.events.MouseEvent;

public interface IRenderViewProvider {
    public enum MouseEventType {
        MOUSE_DOWN,
        MOUSE_UP,
        MOUSE_MOVE
    }

    void setup(RenderContext renderContext);

    void onNodeHit(List<Node> nodes, MouseEvent event, MouseEventType mouseEventType);

    boolean hasFocus(List<Node> nodes);
}
