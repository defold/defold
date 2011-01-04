package com.dynamo.cr.contenteditor.editors;

import javax.media.opengl.GLException;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.openmali.vecmath2.Point3d;

import com.dynamo.cr.contenteditor.manipulator.IManipulator;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.Scene;

public interface IEditor {

    void executeOperation(IUndoableOperation operation);

    void viewToWorld(int x, int y, Vector4d world_point, Vector4d world_vector);

    void setManipulator(String manipulator);

    IManipulator getManipulator();

    double[] worldToView(Point3d point);

    Camera getCamera();

    Node[] getSelectedNodes();

    Scene getScene();

    Node getRoot();

    ResourceLoaderFactory getLoaderFactory();

    Node getPasteTarget();
    void setPasteTarget(Node node);

    int[] getViewPort();

    Node[] selectNode(int x, int y, int w, int h, boolean multi_select, boolean add_to_selection, boolean update_ui) throws GLException;

    void setSelectedNodes(Node[] nodes);

    UndoContext getUndoContext();

    boolean isSelecting();
}
