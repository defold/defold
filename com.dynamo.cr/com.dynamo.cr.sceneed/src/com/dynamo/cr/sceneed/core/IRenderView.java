package com.dynamo.cr.sceneed.core;

import java.util.List;

import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point2i;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.widgets.Composite;

public interface IRenderView  {
    void addRenderProvider(IRenderViewProvider provider);
    void removeRenderProvider(IRenderViewProvider provider);

    void addRenderController(IRenderViewController controller);
    void removeRenderController(IRenderViewController controller);

    void setupNode(RenderContext renderContext, Node node);

    void dispose();
    void setFocus();
    void createControls(Composite parent);
    void refresh();
    void setSimulating(boolean simulating);

    List<Node> findNodesBySelection(Point2i start, Point2i end);

    Node getInput();
    void setInput(Node input);
    void setSelection(IStructuredSelection selection);
    void viewToWorld(int x, int y, Vector4d clickPos, Vector4d clickDir);
    double[] worldToView(Point3d point);
    Camera getCamera();
    void setCamera(Camera camera);
    Vector4d getCameraFocusPoint();
    Matrix4d getViewTransform();
    Matrix4d getProjectionTransform();
    void frameSelection();

    // TODO This is part of a "quick-fix" to enable disposal of graphics resources inside nodes
    // See SceneEditor#dispose for more info
    GL2 activateGLContext();
    void releaseGLContext();

    void setNodeTypeVisible(INodeType nodeType, boolean visible);

    SceneGrid getGrid();
    void toggleRecord();

    boolean isGridShown();
    void setGridShown(boolean gridShown);

    boolean isOutlineShown();
    void setOutlineShown(boolean outlineShown);
}
