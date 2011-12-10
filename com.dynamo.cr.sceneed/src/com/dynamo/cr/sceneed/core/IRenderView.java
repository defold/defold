package com.dynamo.cr.sceneed.core;

import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.widgets.Composite;

public interface IRenderView  {
    void addRenderProvider(IRenderViewProvider provider);
    void removeRenderProvider(IRenderViewProvider provider);

    void addMouseListener(MouseListener listener);
    void removeMouseListener(MouseListener listener);

    void addMouseMoveListener(MouseMoveListener listener);
    void removeMouseMoveListener(MouseMoveListener listener);
    void setupNode(RenderContext renderContext, Node node);

    void dispose();
    void setFocus();
    void createControls(Composite parent);
    void refresh();
    void viewToWorld(int x, int y, Vector4d clickPos, Vector4d clickDir);
}
