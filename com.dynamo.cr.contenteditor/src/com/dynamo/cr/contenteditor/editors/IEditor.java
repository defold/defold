package com.dynamo.cr.contenteditor.editors;

import javax.media.opengl.GLException;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;

import com.dynamo.cr.contenteditor.manipulator.IManipulator;
import com.dynamo.cr.scene.graph.INodeFactory;
import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.graph.Scene;
import com.dynamo.cr.scene.resource.IResourceFactory;

public interface IEditor {

    void executeOperation(IUndoableOperation operation);

    void viewToWorld(int x, int y, Vector4d world_point, Vector4d world_vector);

    void setManipulator(String manipulator);

    IManipulator getManipulator();

    void setManipulatorOrientation(String manipulatorOrientation);

    String getManipulatorOrientationName();

    void setCamera(String cameraName);

    String getCameraName();

    double[] worldToView(Point3d point);

    Camera getCamera();

    Node[] getSelectedNodes();

    Scene getScene();

    Node getRoot();

    IResourceFactory getResourceFactory();
    INodeFactory getNodeFactory();

    Node getPasteTarget();
    void setPasteTarget(Node node);

    int[] getViewPort();

    Node[] selectNode(int x, int y, int w, int h, boolean multi_select, boolean add_to_selection, boolean update_ui) throws GLException;

    void setSelectedNodes(Node[] nodes);

    UndoContext getUndoContext();

    boolean isSelecting();

    void resetCamera();

    void frameObjects();
}
