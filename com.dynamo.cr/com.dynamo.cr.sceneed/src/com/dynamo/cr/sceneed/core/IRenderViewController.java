package com.dynamo.cr.sceneed.core;

import java.util.List;

import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseWheelListener;

public interface IRenderViewController extends MouseListener, MouseMoveListener, MouseWheelListener, KeyListener {
    public enum FocusType {
        NONE,
        SELECTION,
        MANIPULATOR,
        CAMERA
    }

    FocusType getFocusType(List<Node> nodes, MouseEvent event);

    void initControl(List<Node> nodes);
    void finalControl();
}
